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

#include "ProgramTrace.h"

#include "PreProcessing/PreProcessing.h"
#include "Trace/Event.h"

using namespace race;

ProgramTrace::ProgramTrace(llvm::Module *module, llvm::StringRef entryName) : module(module) {
  // Run preprocessing on module
  preprocess(*module);

  // Run pointer analysis
  pta.analyze(module, entryName);

  TraceBuildState state;

  // build all threads starting from this main func
  auto const mainEntry = pta::GT::getEntryNode(pta.getCallGraph());
  auto mainThread = std::make_unique<ThreadTrace>(*this, mainEntry, state);
  // insert at front because main thread is always first
  threads.insert(threads.begin(), std::move(mainThread));
}

llvm::raw_ostream &race::operator<<(llvm::raw_ostream &os, const ProgramTrace &trace) {
  os << "===== Program Trace =====\n";

  // the order is a little reversed for parallel omp forks after changing the traversal order
  auto const &threads = trace.getThreads();
  for (auto const &thread : threads) {
    os << *thread;
  }

  os << "========================\n";
  return os;
}
