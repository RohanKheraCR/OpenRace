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
#include "DefaultHeapModel.h"

#include "PointerAnalysis/Program/CallSite.h"
#include "PointerAnalysis/Util/Util.h"

#define MORE_COMPLETE_TYPE_INFO

using namespace llvm;
using namespace pta;

Type *DefaultHeapModel::getNextBitCastDestType(const Instruction *allocSite) {
  // a call instruction
  const Instruction *nextInst = nullptr;
  if (auto call = dyn_cast<CallInst>(allocSite)) {
    nextInst = call->getNextNode();
  } else if (auto invoke = dyn_cast<InvokeInst>(allocSite)) {
    // skip the exception handler code
    nextInst = invoke->getNormalDest()->getFirstNonPHIOrDbgOrLifetime();
  }

  if (isa_and_nonnull<BitCastInst>(nextInst)) {
    Type *destTy = cast<BitCastInst>(nextInst)->getDestTy()->getPointerElementType();
    if (destTy->isSized()) {
      // only when the dest type is sized
      return destTy;
    }
  }

#ifdef MORE_COMPLETE_TYPE_INFO
  if (allocSite->getDebugLoc().get() != nullptr) {
    if (allocSite->getDebugLoc().getInlinedAt() != nullptr) {
      // else might be a inlined wrapper, try to find the bitcast user
      const BitCastInst *bitCastUser = nullptr;
      for (auto user : allocSite->users()) {
        if (auto bitcast = dyn_cast<BitCastInst>(user)) {
          if (bitCastUser == nullptr) {
            bitCastUser = bitcast;
          } else {
            // multiple bitcast user? which one to pick?
            return nullptr;
          }
        }
      }

      if (bitCastUser) {
        Type *destTy = bitCastUser->getDestTy()->getPointerElementType();
        if (destTy->isSized()) {
          return destTy;
        }
      }
    }
  }
#endif
  return nullptr;
}

// the signature of calloc is void *calloc(size_t elementNum, size_t
// elementSize);
Type *DefaultHeapModel::inferCallocType(const Function *fun, const Instruction *allocSite, int numArgNo,
                                        int sizeArgNo) {
  if (auto elemType = getNextBitCastDestType(allocSite)) {
    assert(elemType->isSized());

    pta::CallSite CS(allocSite);
    const DataLayout &DL = fun->getParent()->getDataLayout();
    const size_t elemSize = DL.getTypeAllocSize(elemType);
    const Value *elementNum = CS.getArgOperand(numArgNo);
    const Value *elementSize = CS.getArgOperand(sizeArgNo);

    if (auto size = dyn_cast<ConstantInt>(elementSize)) {
      if (elemSize == size->getSExtValue()) {
        // GREAT, we are sure that the element type is the bitcast type
        if (auto elemNum = dyn_cast<ConstantInt>(elementNum)) {
          if (elemNum->getSExtValue() == 1) {
            return inferMallocType(fun, allocSite, sizeArgNo);
          }
          return getBoundedArrayTy(elemType, elemNum->getSExtValue());
        } else {
          // the element number can not be determined
          return getUnboundedArrayTy(elemType);
        }
      } else {
        // TODO: maybe conservatively treat it as field insenstive object?
        return getUnboundedArrayTy(elemType);
      }
    }
  }
  return nullptr;
}

// the signature of malloc is void *malloc(size_t elementSize);
Type *DefaultHeapModel::inferMallocType(const Function *fun, const Instruction *allocSite, int sizeArgNo) {
  if (auto elemType = getNextBitCastDestType(allocSite)) {
    assert(elemType->isSized());

    if (auto ST = dyn_cast<StructType>(elemType)) {
      if (Type *lastElemTy = isStructWithFlexibleArray(ST)) {
        return getConvertedFlexibleArrayType(ST, lastElemTy);
      }
    }

    pta::CallSite CS(allocSite);
    const DataLayout &DL = fun->getParent()->getDataLayout();
    const size_t elemSize = DL.getTypeAllocSize(elemType);
    const Value *totalSize = nullptr;

    if (sizeArgNo >= 0) {
      totalSize = CS.getArgOperand(sizeArgNo);
    }

    // the allocated object size is known statically
    if (auto constSize = dyn_cast_or_null<ConstantInt>(totalSize)) {
      size_t memSize = constSize->getValue().getSExtValue();
      if (memSize == elemSize) {
        // GREAT!
        return elemType;
      } else if (memSize % elemSize == 0) {
        return getBoundedArrayTy(elemType, memSize / elemSize);
      }
      return nullptr;
    } else {
      // the size of allocated heap memory is unknown.
      // treat is an array with infinite elements and the ty
      if (DL.getTypeAllocSize(elemType) == 1) {
        // a int8_t[] is equal to a field-insensitive object.
        return nullptr;
      } else {
        return getUnboundedArrayTy(elemType);
      }
    }
  } else {
    // could it be a boolean type? it also uses i8* as the type
    if (sizeArgNo >= 0) {
      auto call = cast<CallBase>(allocSite);
      auto totalSize = call->getArgOperand(sizeArgNo);
      if (auto constSize = dyn_cast<ConstantInt>(totalSize)) {
        if (constSize->getSExtValue() == 1) {
          return call->getType()->getPointerElementType();  // i8* as the type
        }
      }
    }
  }

  // we can not resolve the type
  return nullptr;
}
