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

#include <IR/Builder.h>

#include <vector>

#include "IR/IRImpls.h"
#include "LanguageModel/RaceModel.h"
#include "ThreadTrace.h"
#include "Trace/Event.h"

namespace race {

struct OpenMPState {
  // Track if we are currently in parallel region created from kmpc_fork_teams
  size_t teamsDepth = 0;
  bool inTeamsRegion() const { return teamsDepth > 0; }

  // Track if we are in single region
  bool inSingle = false;

  // Track the start/end instructions of master regions
  std::map<const llvm::CallBase *, const llvm::CallBase *> masterRegions;
  const llvm::CallBase *currentMasterStart = nullptr;

  // record the start of a master
  void markMasterStart(const llvm::CallBase *start) {
    assert(!currentMasterStart && "encountered two master starts in a row");
    currentMasterStart = start;
  }

  // mark the end of a master region
  void markMasterEnd(const llvm::CallBase *end) {
    assert(currentMasterStart && "encountered master end without start");
    masterRegions.insert({currentMasterStart, end});
    currentMasterStart = nullptr;
  }

  // Get the end of a previously encountered master region
  const llvm::CallBase *getMasterRegionEnd(const llvm::CallBase *start) const { return masterRegions.at(start); }

  // NOTE: this ugliness is only needed because there is no way to get the shared_ptr
  // from the forkEvent. forkEvent->getIRInst() returns a raw pointer instead.
  struct UnjoinedTask {
    const ForkEvent *forkEvent;
    std::shared_ptr<const OpenMPTaskFork> forkIR;

    UnjoinedTask(const ForkEvent *forkEvent, std::shared_ptr<const OpenMPTaskFork> forkIR)
        : forkEvent(forkEvent), forkIR(forkIR) {}
  };

  // List of unjoined OpenMP task threads
  std::vector<UnjoinedTask> unjoinedTasks;

  // map of blocks to the tid they are guarded by omp_get_thread_num
  // TODO: simple implementation can only handle one block being guarded
  std::map<const llvm::BasicBlock *, ThreadID> guardedBlocks;

  // set of omp_get_thread_num calls who's guarded blocks have already been computed
  std::set<const llvm::Instruction *> visited;

  // set of omp_get_thread_num calls who's guarded blocks have already been computed
  // and the call HAS a corresponding guarded block
  std::set<const llvm::Instruction *> existGuards;

  // whether we call updateGuardedBlocks to check if reach any guarded block entry/exit
  // update to true when see a compare IR after the call to omp_get_thread_num
  bool checkGuardedBlock = false;

  // Track the current guarded block and its thread ID if we are in any
  const llvm::BasicBlock *curGuardedBlock = nullptr;
  ThreadID guardedTID;  // this is not instantly updated with curGuardedBlock

  // the result of checkGuardedBlocks
  enum class GuardType {
    NoGuard,
    EnterGuard,
    Guarding,
    ExitGuard,
  };

  struct GuardResult {
    GuardType typ;
    std::optional<ThreadID> guardedTID;

    GuardResult(GuardType typ, ThreadID guardedTID) : typ(typ), guardedTID(guardedTID) {}
    GuardResult(GuardType typ) : typ(typ) {}
  };

  // check if we are entering/in/exiting any guarded blocks
  GuardResult checkGuardedBlocks(const llvm::Instruction *inst, bool isEnd) {
    // check if this is an exit
    if (curGuardedBlock) {
      if ((isEnd &&
           inst->getParent() ==
               curGuardedBlock)) {  // reach the end of basicblock/function, we close current guarded block if exists
        curGuardedBlock = nullptr;
        checkGuardedBlock = false;
        return GuardResult(GuardType::ExitGuard, guardedTID);
      } else {
        return GuardResult(GuardType::Guarding);
      }
    }

    if (isEnd) return GuardResult(GuardType::NoGuard);

    // check if this is an entry
    for (auto it = guardedBlocks.begin(); it != guardedBlocks.end(); it++) {
      auto guardedBlock = it->first;
      if (guardedBlock == inst->getParent()) {  // we enter a guarded block
        curGuardedBlock = guardedBlock;
        guardedTID = it->second;
        return GuardResult(GuardType::EnterGuard, guardedTID);
      }
    }

    return GuardResult(GuardType::NoGuard);
  }
};

// all included states are ONLY used when building ProgramTrace/ThreadTrace
struct TraceBuildState {
  // Cached function summaries
  FunctionSummaryBuilder builder;

  // the counter of thread id: since we are constructing ThreadTrace while building events,
  // pState.threads.size() will be updated after finishing the construction, we need such a counter
  ThreadID currentTID = 0;

  // When set, skip traversing until this instruction is reached
  const llvm::Instruction *skipUntil = nullptr;

  // Track state specific to OpenMP
  OpenMPState openmp;
};

class ProgramTrace {
  llvm::Module *module;
  std::unique_ptr<ThreadTrace> mainThread;
  std::vector<const ThreadTrace *> threads;

  friend class ThreadTrace;

 public:
  pta::PTA pta;

  [[nodiscard]] inline const std::vector<const ThreadTrace *> &getThreads() const { return threads; }

  [[nodiscard]] const Event *getEvent(ThreadID tid, EventID eid) { return threads.at(tid)->getEvent(eid); }

  // Get the module after preprocessing has been run
  [[nodiscard]] const Module &getModule() const { return *module; }

  explicit ProgramTrace(llvm::Module *module, llvm::StringRef entryName = "main");
  ~ProgramTrace() = default;
  ProgramTrace(const ProgramTrace &) = delete;
  ProgramTrace(ProgramTrace &&) = delete;  // Need to update threads because
                                           // they contain reference to parent
  ProgramTrace &operator=(const ProgramTrace &) = delete;
  ProgramTrace &operator=(ProgramTrace &&) = delete;
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const ProgramTrace &trace);

}  // namespace race
