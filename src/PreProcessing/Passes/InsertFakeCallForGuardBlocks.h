/* Copyright 2021 Coderrect Inc. All Rights Reserved.
Licensed under the GNU Affero General Public License, version 3 or later (“AGPL”), as published by the Free Software
Foundation. You may not use this file except in compliance with the License. You may obtain a copy of the License at
https://www.gnu.org/licenses/agpl-3.0.en.html
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an “AS IS” BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#pragma once

#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Module.h>

#include <set>

#include "Analysis/OpenMP/OpenMP.h"

struct GuardBlockState {
  // a map of omp_get_thread_num calls who's guarded blocks have already been computed
  // and the call HAS a corresponding guarded block
  std::map<llvm::CallBase *, std::set<llvm::BasicBlock *>> existGuards;

  // a map of blocks to the tid they are guarded by omp_get_thread_num
  // TODO: simple implementation can only handle one block being guarded
  std::map<const llvm::BasicBlock *, size_t> block2TID;

  // set of omp_get_thread_num calls who's guarded blocks have already been computed
  std::set<llvm::CallBase *> visited;

  // find a cmp IR and its guarded blocks after this call to omp_get_thread_num
  void computeGuardedBlocks(llvm::CallBase *call) {
    // Check if we have already computed block2TID for this omp_get_thread_num call
    if (visited.find(call) != visited.end()) {
      return;
    }

    // Find all cmpInsts that compare the omp_get_thread_num call to a const value
    auto const cmpInsts = race::getConstCmpEqInsts(call);
    for (auto const &pair : cmpInsts) {
      auto const cmpInst = pair.first;
      auto const tid = pair.second;

      // Find all branches that use the result of the cmp inst
      for (auto user : cmpInst->users()) {
        auto branch = llvm::dyn_cast<llvm::BranchInst>(user);
        if (branch == nullptr) continue;

        // Find all the blocks guarded by this branch
        auto guarded = race::getGuardedBlocks(branch);

        // insert the blocks into the block2TID map
        for (auto const block : guarded) {
          block2TID[block] = tid;
        }

        // cache the result
        existGuards.insert(std::make_pair(call, guarded));
      }
    }

    // Mark this omp_get_thread_num call as visited
    visited.insert(call);
  }

  // the fake function declarations of the guard start and end
  llvm::Function *guardStartFn = nullptr;
  llvm::Function *guardEndFn = nullptr;

  // create a function declaration
  // the following code is from https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl03.html
  // and https://freecompilercamp.org/llvm-ir-func1/
  llvm::Function *generateFakeFn(std::string fnName, llvm::LLVMContext &context, llvm::Module &module) {
    // Make the function type: void(i32)
    std::vector<llvm::Type *> Params(1, Type::getInt32Ty(context));
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context), Params, false);

    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, fnName, module);
    assert(F);

    llvm::Value *guardTID = F->arg_begin();
    guardTID->setName("guardTID");  // to match the param in the call

    return F;
  }

  // create the fake functions, once
  void createFakeGuardFn(llvm::LLVMContext &context, llvm::Module &module) {
    guardStartFn = generateFakeFn("omp_get_thread_num_guard_start", context, module);
    guardEndFn = generateFakeFn("omp_get_thread_num_guard_end", context, module);
  }
};

// insert fake external calls for the guarded blocks by omp_get_thread_num
void insertFakeCallForGuardBlocks(llvm::Module &module);
