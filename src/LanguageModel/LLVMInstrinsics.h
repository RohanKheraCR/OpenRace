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

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/ADT/StringRef.h>

#include <set>

namespace LLVMModel {

inline bool isDebug(const llvm::StringRef &funcName) {
  return funcName.equals("llvm.dbg.declare") || funcName.equals("llvm.dbg.value");
}
inline bool isLifetime(const llvm::StringRef &funcName) { return funcName.startswith("llvm.lifetime"); }
inline bool isStackSave(const llvm::StringRef &funcName) { return funcName.equals("llvm.stacksave"); }
inline bool isStackRestore(const llvm::StringRef &funcName) { return funcName.equals("llvm.stackrestore"); }
inline bool isMemcpy(const llvm::StringRef &funcName) { return funcName.startswith("llvm.memcpy"); }

// returns true for llvm APIS that have no effect on race detection
inline bool isNoEffect(const llvm::StringRef &funcName) {
  return isDebug(funcName) || isLifetime(funcName) || isStackSave(funcName) || isStackRestore(funcName) ||
         isMemcpy(funcName);
}

inline std::optional<const llvm::BasicBlock *> findAnyBlockSuccessorBFS(
    const std::set<const llvm::BasicBlock *> &starts, const std::function<bool(const llvm::BasicBlock *)> &matches,
    const std::function<bool(const llvm::BasicBlock *)> &isAvoided, std::set<const llvm::BasicBlock *> &visited) {
  std::deque<const llvm::BasicBlock *> queue;
  std::copy(starts.begin(), starts.end(), std::back_inserter(queue));

  while (!queue.empty()) {
    auto curr = queue.front();
    queue.pop_front();

    if (visited.find(curr) != visited.end()) {
      continue;
    }
    visited.insert(curr);

    if (matches(curr)) {
      return curr;
    }

    if (!isAvoided(curr)) {
      std::copy(succ_begin(curr), succ_end(curr), std::back_inserter(queue));
    }
  }

  return std::nullopt;
}

inline void findAllBlockSuccessorsBFS(const std::set<const llvm::BasicBlock *> &starts,
                                      const std::function<bool(const llvm::BasicBlock *)> &matches,
                                      const std::function<bool(const llvm::BasicBlock *)> &isAvoided,
                                      std::set<const llvm::BasicBlock *> &visited,
                                      std::set<const llvm::BasicBlock *> &found) {
  std::deque<const llvm::BasicBlock *> queue;
  std::copy(starts.begin(), starts.end(), std::back_inserter(queue));

  while (!queue.empty()) {
    auto curr = queue.front();
    queue.pop_front();

    if (visited.find(curr) != visited.end()) {
      continue;
    }
    visited.insert(curr);

    if (matches(curr)) {
      found.insert(curr);
    }

    if (!isAvoided(curr)) {
      std::copy(succ_begin(curr), succ_end(curr), std::back_inserter(queue));
    }
  }
}

}  // namespace LLVMModel