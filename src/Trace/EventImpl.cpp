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

#include "Trace/EventImpl.h"

#include "Trace/ProgramTrace.h"
#include "Trace/ThreadTrace.h"

using namespace race;

std::vector<const pta::ObjTy *> ReadEventImpl::getAccessedMemory() const {
  std::vector<const pta::ObjTy *> pts;
  info->thread.program.pta.getPointsTo(info->context, read->getAccessedValue(), pts);
  return pts;
}

std::vector<const pta::ObjTy *> WriteEventImpl::getAccessedMemory() const {
  std::vector<const pta::ObjTy *> pts;
  info->thread.program.pta.getPointsTo(info->context, write->getAccessedValue(), pts);
  return pts;
}

std::vector<const pta::CallGraphNodeTy *> ForkEventImpl::getThreadEntry() const {
  auto entryVal = fork->getThreadEntry();
  if (auto entryFunc = llvm::dyn_cast<llvm::Function>(entryVal)) {
    auto const newContext = pta::CT::contextEvolve(info->context, fork->getInst());
    auto const entryNode = info->thread.program.pta.getDirectNodeOrNull(newContext, entryFunc);
    return {entryNode};
  } else if (auto entryInst = llvm::dyn_cast<llvm::Instruction>(entryVal)) {
    // we do NOT evolve the context here because to do so would lead to only negative results
    // we need the scopes to be the same so that the arrays written to are the same
    auto const newContext = info->context;
    auto const entryNode = info->thread.program.pta.getDirectNodeOrNull(newContext, entryInst->getFunction());
    return {entryNode};
  }
  // the entry is indirect and we need to figure out where the real function is
  auto callsite = info->thread.program.pta.getInDirectCallSite(info->context, fork->getInst());
  auto const &nodes = callsite->getResolvedNode();
  return std::vector<const pta::CallGraphNodeTy *>(nodes.begin(), nodes.end());
}
