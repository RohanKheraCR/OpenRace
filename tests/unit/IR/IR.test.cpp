#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/NoFolder.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

#include <catch2/catch.hpp>

#include "IR/Builder.h"
#include "IR/InfoImpls.h"

TEST_CASE("Manually Constructed IR", "[unit][IR]") {
  llvm::LLVMContext ctx;
  auto module = std::make_unique<llvm::Module>("testmodule", ctx);
  auto func = llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false),
      llvm::Function::ExternalLinkage, "testfunction", module.get());

  auto BB = llvm::BasicBlock::Create(ctx, "testblock", func);
  llvm::IRBuilder<llvm::NoFolder> IRB(BB);

  SECTION("Construct LoadInfo") {
    auto alloca = IRB.CreateAlloca(IRB.getInt32Ty());
    auto loadInst = IRB.CreateLoad(IRB.getInt32Ty(), alloca);

    auto load = std::make_unique<race::LoadInfo>(loadInst);
    REQUIRE(load->getInst() == loadInst);
    REQUIRE(llvm::isa<llvm::LoadInst>(load->getInst()));
    REQUIRE(load->getAccessedValue() == loadInst->getPointerOperand());
    REQUIRE(llvm::isa<race::ReadInfo>(load));
    REQUIRE(llvm::isa<race::MemAccessInfo>(load));
  }

  SECTION("Construct StoreInfo") {
    auto alloca = IRB.CreateAlloca(IRB.getInt32Ty());
    auto val = IRB.getInt32(42);
    auto storeInst = IRB.CreateStore(val, alloca);

    auto store = std::make_unique<race::StoreInfo>(storeInst);
    REQUIRE(store->getInst() == storeInst);
    REQUIRE(llvm::isa<llvm::StoreInst>(store->getInst()));
    REQUIRE(store->getAccessedValue() == storeInst->getPointerOperand());
    REQUIRE(llvm::isa<race::WriteInfo>(store));
    REQUIRE(llvm::isa<race::MemAccessInfo>(store));
  }

  SECTION("Construct CallInfo") {
    auto callInst = IRB.CreateCall(func);
    REQUIRE(callInst != nullptr);

    auto call = std::make_unique<race::CallInfo>(callInst);
    REQUIRE(call->getInst() == callInst);
    REQUIRE(llvm::isa<llvm::CallBase>(call->getInst()));
    REQUIRE(llvm::isa<race::CallInfo>(call));
  }
}

TEST_CASE("Builder Read/Write/Call", "[unit][IR]") {
  const char *ModuleString = R"(
declare void @bar()

define void @hello() {
    ret void
}

define void @foo(i32* %x) {
  %read = load i32, i32* %x
  %add = add nsw i32 %read, 42
  call void @bar()
  store i32 %add, i32* %x
  call void @hello()
  ret void
})";
  llvm::LLVMContext Ctx;
  llvm::SMDiagnostic Err;
  auto M = llvm::parseAssemblyString(ModuleString, Err, Ctx);
  // if (!M) {
  //     Err.print("error", llvm::errs());
  // }
  auto func = M->getFunction("foo");

  auto racefunc = race::generateRaceFunction(func);
  REQUIRE(racefunc.size() == 4);

  auto read = llvm::dyn_cast<race::ReadInfo>(racefunc.at(0).get());
  REQUIRE(read);
  REQUIRE(read->getAccessedValue()->getName() == "x");

  auto externcall = llvm::dyn_cast<race::CallInfo>(racefunc.at(1).get());
  REQUIRE(externcall);
  REQUIRE(externcall->getInst()->getCalledFunction()->getName() == "bar");

  auto write = llvm::dyn_cast<race::WriteInfo>(racefunc.at(2).get());
  REQUIRE(write);
  REQUIRE(write->getAccessedValue()->getName() == "x");

  auto call = llvm::dyn_cast<race::CallInfo>(racefunc.at(3).get());
  REQUIRE(call);
  REQUIRE(call->getInst()->getCalledFunction()->getName() == "hello");
}

TEST_CASE("Builder pthread create/join", "[unit][IR]") {
  const char *ModuleString = R"(
%union.pthread_attr_t = type { i64, [48 x i8] }

define i8* @entry() {
    ret i8* null
}

define void @foo() {
  %p_thread = alloca i64
  %1 = call i32 @pthread_create(i64* %p_thread, %union.pthread_attr_t* null, i8* (i8*)* bitcast (i8* ()* @entry to i8* (i8*)*), i8* null)
  %thread = load i64, i64* %p_thread
  %2 = call i32 @pthread_join(i64 %thread, i8** null)
  ret void
}

declare i32 @pthread_create(i64*, %union.pthread_attr_t*, i8* (i8*)*, i8*)
declare i32 @pthread_join(i64, i8**)
)";

  llvm::LLVMContext Ctx;
  llvm::SMDiagnostic Err;
  auto module = llvm::parseAssemblyString(ModuleString, Err, Ctx);
  auto func = module->getFunction("foo");

  auto racefunc = race::generateRaceFunction(func);
  REQUIRE(racefunc.size() == 3);

  auto pthreadcreate = llvm::dyn_cast<race::ForkInfo>(racefunc.at(0).get());
  REQUIRE(pthreadcreate);
  REQUIRE(pthreadcreate->getThreadHandle()->getName() == "p_thread");
  REQUIRE(pthreadcreate->getThreadEntry()->getName() == "entry");

  auto pthreadjoin = llvm::dyn_cast<race::JoinInfo>(racefunc.at(2).get());
  REQUIRE(pthreadjoin);
  REQUIRE(pthreadjoin->getThreadHandle()->getName() == "thread");
}

TEST_CASE("Builder pthread mutex", "[unit][IR]") {
  const char *ModuleString = R"(
%union.pthread_mutex_t = type { %struct.__pthread_mutex_s }
%struct.__pthread_mutex_s = type { i32, i32, i32, i32, i32, i16, i16, %struct.__pthread_internal_list }
%struct.__pthread_internal_list = type { %struct.__pthread_internal_list*, %struct.__pthread_internal_list* }

define dso_local i32 @main() #0 {
  %mutex = alloca %union.pthread_mutex_t
  %call = call i32 @pthread_mutex_lock(%union.pthread_mutex_t* %mutex)
  %call1 = call i32 @pthread_mutex_unlock(%union.pthread_mutex_t* %mutex)
  ret i32 0
}

declare i32 @pthread_mutex_lock(%union.pthread_mutex_t*) #1
declare i32 @pthread_mutex_unlock(%union.pthread_mutex_t*) #1
)";

  llvm::LLVMContext Ctx;
  llvm::SMDiagnostic Err;
  auto module = llvm::parseAssemblyString(ModuleString, Err, Ctx);
  auto func = module->getFunction("main");

  auto const racefunc = race::generateRaceFunction(func);
  REQUIRE(racefunc.size() == 2);

  SECTION("pthread_mutex_lock") {
    auto lock = llvm::dyn_cast<race::LockInfo>(racefunc.at(0).get());
    REQUIRE(lock != nullptr);
    CHECK(lock->getLockValue()->getName() == "mutex");
  }

  SECTION("pthread_mutex_unlock") {
    auto unlock = llvm::dyn_cast<race::UnlockInfo>(racefunc.at(1).get());
    REQUIRE(unlock != nullptr);
    CHECK(unlock->getLockValue()->getName() == "mutex");
  }
}