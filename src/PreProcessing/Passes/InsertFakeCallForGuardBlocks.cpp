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

#include "InsertFakeCallForGuardBlocks.h"

#include <llvm/IR/IRBuilder.h>

namespace {

// we insert the call start at the beginning of each guarded block (for now, since only one block guarded
// by each call), and insert the call end at the end of the block
void insertFakeCall(llvm::LLVMContext &context, llvm::Module &module, std::set<llvm::BasicBlock *> &guardedBlocks,
                    GuardBlockState &state) {
  std::map<const BasicBlock *, size_t> &block2TID = state.block2TID;
  for (auto guardedBlock : guardedBlocks) {
    // pass the guarded TID as a constant to the only parameter of the fake function
    auto guardVal = llvm::ConstantInt::get(context, llvm::APInt(32, block2TID.find(guardedBlock)->second, true));
    std::vector<llvm::Value *> arg_list;
    arg_list.push_back(guardVal);

    // insert the call start
    llvm::Instruction *startcall = llvm::CallInst::Create(state.guardStartFn, arg_list);
    auto nonPhi = guardedBlock->getFirstNonPHI();
    if (llvm::isa<llvm::PHINode>(nonPhi)) {
      startcall->insertAfter(nonPhi);
    } else {
      startcall->insertBefore(nonPhi);
    }

    // insert the call end
    llvm::Instruction *endcall = llvm::CallInst::Create(state.guardEndFn, arg_list);
    llvm::Instruction *nonReturn = nullptr;
    for (auto it = guardedBlock->rbegin(); it != guardedBlock->rend(); it++) {
      if (llvm::isa<llvm::ReturnInst>(*it) || llvm::isa<llvm::BranchInst>(*it)) continue;
      nonReturn = &(*it);
      break;
    }
    endcall->insertAfter(nonReturn);
  }
}

}  // namespace

void insertFakeCallForGuardBlocks(llvm::Module &module) {
  GuardBlockState state;
  // find if exists any guarded block
  for (auto &function : module.getFunctionList()) {
    for (auto &basicblock : function.getBasicBlockList()) {
      for (auto &inst : basicblock.getInstList()) {
        auto call = llvm::dyn_cast<llvm::CallBase>(&inst);
        if (!call || !call->getCalledFunction() || !call->getCalledFunction()->hasName()) continue;
        auto const funcName = call->getCalledFunction()->getName();
        if (OpenMPModel::isGetThreadNum(funcName)) {
          state.computeGuardedBlocks(call);
        }
      }
    }
  }

  if (state.existGuards.empty()) return;

  // insert fake calls to fake functions
  for (auto guard : state.existGuards) {
    auto call = guard.first;
    auto blocks = guard.second;
    if (!state.guardStartFn && !state.guardEndFn) {  // create fake function declarations, only once
      state.createFakeGuardFn(call->getContext(), module);
    }
    insertFakeCall(call->getContext(), module, blocks, state);
  }
}
