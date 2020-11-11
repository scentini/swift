//===--- OwnershipUtils.cpp -----------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/SIL/OwnershipUtils.h"
#include "swift/Basic/Defer.h"
#include "swift/SIL/InstructionUtils.h"
#include "swift/SIL/LinearLifetimeChecker.h"
#include "swift/SIL/Projection.h"
#include "swift/SIL/SILArgument.h"
#include "swift/SIL/SILInstruction.h"

using namespace swift;

bool swift::isValueAddressOrTrivial(SILValue v) {
  return v->getType().isAddress() ||
         v.getOwnershipKind() == OwnershipKind::None;
}

// These operations forward both owned and guaranteed ownership.
bool swift::isOwnershipForwardingValueKind(SILNodeKind kind) {
  switch (kind) {
  case SILNodeKind::TupleInst:
  case SILNodeKind::StructInst:
  case SILNodeKind::EnumInst:
  case SILNodeKind::DifferentiableFunctionInst:
  case SILNodeKind::LinearFunctionInst:
  case SILNodeKind::OpenExistentialRefInst:
  case SILNodeKind::UpcastInst:
  case SILNodeKind::UncheckedValueCastInst:
  case SILNodeKind::UncheckedRefCastInst:
  case SILNodeKind::ConvertFunctionInst:
  case SILNodeKind::RefToBridgeObjectInst:
  case SILNodeKind::BridgeObjectToRefInst:
  case SILNodeKind::UnconditionalCheckedCastInst:
  case SILNodeKind::UncheckedEnumDataInst:
  case SILNodeKind::MarkUninitializedInst:
  case SILNodeKind::SelectEnumInst:
  case SILNodeKind::SwitchEnumInst:
  case SILNodeKind::CheckedCastBranchInst:
  case SILNodeKind::DestructureStructInst:
  case SILNodeKind::DestructureTupleInst:
  case SILNodeKind::MarkDependenceInst:
  case SILNodeKind::InitExistentialRefInst:
    return true;
  default:
    return false;
  }
}

// These operations forward guaranteed ownership, but don't necessarily forward
// owned values.
bool swift::isGuaranteedForwardingValueKind(SILNodeKind kind) {
  switch (kind) {
  case SILNodeKind::TupleExtractInst:
  case SILNodeKind::StructExtractInst:
  case SILNodeKind::DifferentiableFunctionExtractInst:
  case SILNodeKind::LinearFunctionExtractInst:
  case SILNodeKind::OpenExistentialValueInst:
  case SILNodeKind::OpenExistentialBoxValueInst:
    return true;
  default:
    return isOwnershipForwardingValueKind(kind);
  }
}

bool swift::isOwnedForwardingValueKind(SILNodeKind kind) {
  switch (kind) {
  case SILNodeKind::BranchInst:
    return true;
  default:
    return isOwnershipForwardingValueKind(kind);
  }
}

bool swift::isOwnedForwardingInstruction(SILInstruction *inst) {
  auto kind = inst->getKind();
  switch (kind) {
  case SILInstructionKind::BranchInst:
    return true;
  default:
    return isOwnershipForwardingValueKind(SILNodeKind(kind));
  }
}

bool swift::isOwnedForwardingValue(SILValue value) {
  switch (value->getKind()) {
  // Phi arguments always forward ownership.
  case ValueKind::SILPhiArgument:
    return true;
  default:
    return isOwnedForwardingValueKind(
        value->getKindOfRepresentativeSILNodeInObject());
  }
}

bool swift::isGuaranteedForwardingValue(SILValue value) {
  // If we have an argument from a transforming terminator, we can forward
  // guaranteed.
  if (auto *arg = dyn_cast<SILArgument>(value)) {
    if (auto *ti = arg->getSingleTerminator()) {
      if (ti->isTransformationTerminator()) {
        return true;
      }
    }
  }

  return isGuaranteedForwardingValueKind(
      value->getKindOfRepresentativeSILNodeInObject());
}

bool swift::isGuaranteedForwardingInst(SILInstruction *i) {
  return isGuaranteedForwardingValueKind(SILNodeKind(i->getKind()));
}

bool swift::isOwnershipForwardingInst(SILInstruction *i) {
  return isOwnershipForwardingValueKind(SILNodeKind(i->getKind()));
}

bool swift::isReborrowInstruction(const SILInstruction *i) {
  switch (i->getKind()) {
  case SILInstructionKind::BranchInst:
    return true;
  default:
    return false;
  }
}

//===----------------------------------------------------------------------===//
//                           Borrowing Operand
//===----------------------------------------------------------------------===//

void BorrowingOperandKind::print(llvm::raw_ostream &os) const {
  switch (value) {
  case Kind::BeginBorrow:
    os << "BeginBorrow";
    return;
  case Kind::BeginApply:
    os << "BeginApply";
    return;
  case Kind::Branch:
    os << "Branch";
    return;
  case Kind::Apply:
    os << "Apply";
    return;
  case Kind::TryApply:
    os << "TryApply";
    return;
  case Kind::Yield:
    os << "Yield";
    return;
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

llvm::raw_ostream &swift::operator<<(llvm::raw_ostream &os,
                                     BorrowingOperandKind kind) {
  kind.print(os);
  return os;
}

void BorrowingOperand::print(llvm::raw_ostream &os) const {
  os << "BorrowScopeOperand:\n"
        "Kind: " << kind << "\n"
        "Value: " << op->get()
     << "User: " << *op->getUser();
}

llvm::raw_ostream &swift::operator<<(llvm::raw_ostream &os,
                                     const BorrowingOperand &operand) {
  operand.print(os);
  return os;
}

void BorrowingOperand::visitLocalEndScopeInstructions(
    function_ref<void(Operand *)> func) const {
  switch (kind) {
  case BorrowingOperandKind::BeginBorrow:
    for (auto *use : cast<BeginBorrowInst>(op->getUser())->getUses()) {
      if (use->isLifetimeEnding()) {
        func(use);
      }
    }
    return;
  case BorrowingOperandKind::BeginApply: {
    auto *user = cast<BeginApplyInst>(op->getUser());
    for (auto *use : user->getTokenResult()->getUses()) {
      func(use);
    }
    return;
  }
  // These are instantaneous borrow scopes so there aren't any special end
  // scope instructions.
  case BorrowingOperandKind::Apply:
  case BorrowingOperandKind::TryApply:
  case BorrowingOperandKind::Yield:
    return;
  case BorrowingOperandKind::Branch:
    return;
  }
}

void BorrowingOperand::visitBorrowIntroducingUserResults(
    function_ref<void(BorrowedValue)> visitor) const {
  switch (kind) {
  case BorrowingOperandKind::Apply:
  case BorrowingOperandKind::TryApply:
  case BorrowingOperandKind::BeginApply:
  case BorrowingOperandKind::Yield:
    llvm_unreachable("Never has borrow introducer results!");
  case BorrowingOperandKind::BeginBorrow: {
    auto value = *BorrowedValue::get(cast<BeginBorrowInst>(op->getUser()));
    return visitor(value);
  }
  case BorrowingOperandKind::Branch: {
    auto *bi = cast<BranchInst>(op->getUser());
    for (auto *succBlock : bi->getSuccessorBlocks()) {
      auto value =
          *BorrowedValue::get(succBlock->getArgument(op->getOperandNumber()));
      visitor(value);
    }
    return;
  }
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

void BorrowingOperand::visitConsumingUsesOfBorrowIntroducingUserResults(
    function_ref<void(Operand *)> func) const {
  // First visit all of the results of our user that are borrow introducing
  // values.
  visitBorrowIntroducingUserResults([&](BorrowedValue value) {
    // Visit the scope ending instructions of this value. If any of them are
    // consuming borrow scope operands, visit the consuming uses of the
    // results or successor arguments.
    //
    // This enables one to walk the def-use chain of guaranteed phis for a
    // single guaranteed scope.
    value.visitLocalScopeEndingUses([&](Operand *valueUser) {
      if (auto subBorrowScopeOp = BorrowingOperand::get(valueUser)) {
        if (subBorrowScopeOp->consumesGuaranteedValues()) {
          subBorrowScopeOp->visitUserResultConsumingUses(func);
          return;
        }
      }

      // Otherwise, if we don't have a borrow scope operand that consumes
      // guaranteed values, just visit value user.
      func(valueUser);
    });
  });
}

void BorrowingOperand::visitUserResultConsumingUses(
    function_ref<void(Operand *)> visitor) const {
  auto *ti = dyn_cast<TermInst>(op->getUser());
  if (!ti) {
    for (SILValue result : op->getUser()->getResults()) {
      for (auto *use : result->getUses()) {
        if (use->isLifetimeEnding()) {
          visitor(use);
        }
      }
    }
    return;
  }

  for (auto *succBlock : ti->getSuccessorBlocks()) {
    auto *arg = succBlock->getArgument(op->getOperandNumber());
    for (auto *use : arg->getUses()) {
      if (use->isLifetimeEnding()) {
        visitor(use);
      }
    }
  }
}

void BorrowingOperand::getImplicitUses(
    SmallVectorImpl<Operand *> &foundUses,
    std::function<void(Operand *)> *errorFunction) const {
  visitLocalEndScopeInstructions([&](Operand *op) { foundUses.push_back(op); });
}

//===----------------------------------------------------------------------===//
//                             Borrow Introducers
//===----------------------------------------------------------------------===//

void BorrowedValueKind::print(llvm::raw_ostream &os) const {
  switch (value) {
  case BorrowedValueKind::SILFunctionArgument:
    os << "SILFunctionArgument";
    return;
  case BorrowedValueKind::BeginBorrow:
    os << "BeginBorrowInst";
    return;
  case BorrowedValueKind::LoadBorrow:
    os << "LoadBorrowInst";
    return;
  case BorrowedValueKind::Phi:
    os << "Phi";
    return;
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

void BorrowedValue::print(llvm::raw_ostream &os) const {
  os << "BorrowScopeIntroducingValue:\n"
    "Kind: " << kind << "\n"
    "Value: " << value;
}

void BorrowedValue::getLocalScopeEndingInstructions(
    SmallVectorImpl<SILInstruction *> &scopeEndingInsts) const {
  assert(isLocalScope() && "Should only call this given a local scope");

  switch (kind) {
  case BorrowedValueKind::SILFunctionArgument:
    llvm_unreachable("Should only call this with a local scope");
  case BorrowedValueKind::BeginBorrow:
  case BorrowedValueKind::LoadBorrow:
  case BorrowedValueKind::Phi:
    for (auto *use : value->getUses()) {
      if (use->isLifetimeEnding()) {
        scopeEndingInsts.push_back(use->getUser());
      }
    }
    return;
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

void BorrowedValue::visitLocalScopeEndingUses(
    function_ref<void(Operand *)> visitor) const {
  assert(isLocalScope() && "Should only call this given a local scope");
  switch (kind) {
  case BorrowedValueKind::SILFunctionArgument:
    llvm_unreachable("Should only call this with a local scope");
  case BorrowedValueKind::LoadBorrow:
  case BorrowedValueKind::BeginBorrow:
  case BorrowedValueKind::Phi:
    for (auto *use : value->getUses()) {
      if (use->isLifetimeEnding()) {
        visitor(use);
      }
    }
    return;
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

llvm::raw_ostream &swift::operator<<(llvm::raw_ostream &os,
                                     BorrowedValueKind kind) {
  kind.print(os);
  return os;
}

llvm::raw_ostream &swift::operator<<(llvm::raw_ostream &os,
                                     const BorrowedValue &value) {
  value.print(os);
  return os;
}

bool BorrowedValue::areUsesWithinScope(
    ArrayRef<Operand *> uses, SmallVectorImpl<Operand *> &scratchSpace,
    SmallPtrSetImpl<SILBasicBlock *> &visitedBlocks,
    DeadEndBlocks &deadEndBlocks) const {
  // Make sure that we clear our scratch space/utilities before we exit.
  SWIFT_DEFER {
    scratchSpace.clear();
    visitedBlocks.clear();
  };

  // First make sure that we actually have a local scope. If we have a non-local
  // scope, then we have something (like a SILFunctionArgument) where a larger
  // semantic construct (in the case of SILFunctionArgument, the function
  // itself) acts as the scope. So we already know that our passed in
  // instructions must be in the same scope.
  if (!isLocalScope())
    return true;

  // Otherwise, gather up our local scope ending instructions, looking through
  // guaranteed phi nodes.
  visitLocalScopeTransitiveEndingUses(
      [&scratchSpace](Operand *op) { scratchSpace.emplace_back(op); });

  LinearLifetimeChecker checker(visitedBlocks, deadEndBlocks);
  return checker.validateLifetime(value, scratchSpace, uses);
}

bool BorrowedValue::visitLocalScopeTransitiveEndingUses(
    function_ref<void(Operand *)> visitor) const {
  assert(isLocalScope());

  SmallVector<Operand *, 32> worklist;
  SmallPtrSet<Operand *, 16> beenInWorklist;
  for (auto *use : value->getUses()) {
    if (!use->isLifetimeEnding())
      continue;
    worklist.push_back(use);
    beenInWorklist.insert(use);
  }

  bool foundError = false;
  while (!worklist.empty()) {
    auto *op = worklist.pop_back_val();
    assert(op->isLifetimeEnding() && "Expected only consuming uses");

    // See if we have a borrow scope operand. If we do not, then we know we are
    // a final consumer of our borrow scope introducer. Visit it and continue.
    auto scopeOperand = BorrowingOperand::get(op);
    if (!scopeOperand) {
      visitor(op);
      continue;
    }

    scopeOperand->visitConsumingUsesOfBorrowIntroducingUserResults(
        [&](Operand *op) {
          assert(op->isLifetimeEnding() && "Expected only consuming uses");
          // Make sure we haven't visited this consuming operand yet. If we
          // have, signal an error and bail without re-visiting the operand.
          if (!beenInWorklist.insert(op).second) {
            foundError = true;
            return;
          }
          worklist.push_back(op);
        });
  }

  return foundError;
}

bool BorrowedValue::visitInteriorPointerOperands(
    function_ref<void(const InteriorPointerOperand &)> func) const {
  SmallVector<Operand *, 32> worklist(value->getUses());
  while (!worklist.empty()) {
    auto *op = worklist.pop_back_val();

    if (auto interiorPointer = InteriorPointerOperand::get(op)) {
      func(*interiorPointer);
      continue;
    }

    auto *user = op->getUser();
    if (isa<BeginBorrowInst>(user) || isa<DebugValueInst>(user) ||
        isa<SuperMethodInst>(user) || isa<ClassMethodInst>(user) ||
        isa<CopyValueInst>(user) || isa<EndBorrowInst>(user) ||
        isa<ApplyInst>(user) || isa<StoreBorrowInst>(user) ||
        isa<StoreInst>(user) || isa<PartialApplyInst>(user) ||
        isa<UnmanagedRetainValueInst>(user) ||
        isa<UnmanagedReleaseValueInst>(user) ||
        isa<UnmanagedAutoreleaseValueInst>(user)) {
      continue;
    }

    // These are interior pointers that have not had support yet added for them.
    if (isa<OpenExistentialBoxInst>(user) ||
        isa<ProjectExistentialBoxInst>(user)) {
      continue;
    }

    // Look through object.
    if (auto *svi = dyn_cast<SingleValueInstruction>(user)) {
      if (Projection::isObjectProjection(svi)) {
        for (SILValue result : user->getResults()) {
          llvm::copy(result->getUses(), std::back_inserter(worklist));
        }
        continue;
      }
    }

    return false;
  }

  return true;
}

//===----------------------------------------------------------------------===//
//                           InteriorPointerOperand
//===----------------------------------------------------------------------===//

bool InteriorPointerOperand::getImplicitUses(
    SmallVectorImpl<Operand *> &foundUses,
    std::function<void(Operand *)> *onError) {
  SILValue projectedAddress = getProjectedAddress();
  SmallVector<Operand *, 8> worklist(projectedAddress->getUses());

  bool foundError = false;

  while (!worklist.empty()) {
    auto *op = worklist.pop_back_val();

    // Skip type dependent operands.
    if (op->isTypeDependent())
      continue;

    // Before we do anything, add this operand to our implicit regular user
    // list.
    foundUses.push_back(op);

    // Then update the worklist with new things to find if we recognize this
    // inst and then continue. If we fail, we emit an error at the bottom of the
    // loop that we didn't recognize the user.
    auto *user = op->getUser();

    // First, eliminate "end point uses" that we just need to check liveness at
    // and do not need to check transitive uses of.
    if (isa<LoadInst>(user) || isa<CopyAddrInst>(user) ||
        isIncidentalUse(user) || isa<StoreInst>(user) ||
        isa<StoreBorrowInst>(user) || isa<PartialApplyInst>(user) ||
        isa<DestroyAddrInst>(user) || isa<AssignInst>(user) ||
        isa<AddressToPointerInst>(user) || isa<YieldInst>(user) ||
        isa<LoadUnownedInst>(user) || isa<StoreUnownedInst>(user) ||
        isa<EndApplyInst>(user) || isa<LoadWeakInst>(user) ||
        isa<StoreWeakInst>(user) || isa<AssignByWrapperInst>(user) ||
        isa<BeginUnpairedAccessInst>(user) ||
        isa<EndUnpairedAccessInst>(user) || isa<WitnessMethodInst>(user) ||
        isa<SwitchEnumAddrInst>(user) || isa<CheckedCastAddrBranchInst>(user) ||
        isa<SelectEnumAddrInst>(user) || isa<InjectEnumAddrInst>(user)) {
      continue;
    }

    // Then handle users that we need to look at transitive uses of.
    if (Projection::isAddressProjection(user) ||
        isa<ProjectBlockStorageInst>(user) ||
        isa<OpenExistentialAddrInst>(user) ||
        isa<InitExistentialAddrInst>(user) ||
        isa<InitEnumDataAddrInst>(user) || isa<BeginAccessInst>(user) ||
        isa<TailAddrInst>(user) || isa<IndexAddrInst>(user) ||
        isa<UnconditionalCheckedCastAddrInst>(user)) {
      for (SILValue r : user->getResults()) {
        llvm::copy(r->getUses(), std::back_inserter(worklist));
      }
      continue;
    }

    if (auto *builtin = dyn_cast<BuiltinInst>(user)) {
      if (auto kind = builtin->getBuiltinKind()) {
        if (*kind == BuiltinValueKind::TSanInoutAccess) {
          continue;
        }
      }
    }

    // If we have a load_borrow, add it's end scope to the liveness requirement.
    if (auto *lbi = dyn_cast<LoadBorrowInst>(user)) {
      transform(lbi->getEndBorrows(), std::back_inserter(foundUses),
                [](EndBorrowInst *ebi) { return &ebi->getAllOperands()[0]; });
      continue;
    }

    // TODO: Merge this into the full apply site code below.
    if (auto *beginApply = dyn_cast<BeginApplyInst>(user)) {
      // TODO: Just add this to implicit regular user list?
      llvm::copy(beginApply->getTokenResult()->getUses(),
                 std::back_inserter(foundUses));
      continue;
    }

    if (auto fas = FullApplySite::isa(user)) {
      continue;
    }

    if (auto *mdi = dyn_cast<MarkDependenceInst>(user)) {
      // If this is the base, just treat it as a liveness use.
      if (op->get() == mdi->getBase()) {
        continue;
      }

      // If we are the value use, look through it.
      llvm::copy(mdi->getValue()->getUses(), std::back_inserter(worklist));
      continue;
    }

    // We were unable to recognize this user, so return true that we failed.
    if (onError) {
      (*onError)(op);
    }
    foundError = true;
  }

  // We were able to recognize all of the uses of the address, so return false
  // that we did not find any errors.
  return foundError;
}

//===----------------------------------------------------------------------===//
//                          Owned Value Introducers
//===----------------------------------------------------------------------===//

void OwnedValueIntroducerKind::print(llvm::raw_ostream &os) const {
  switch (value) {
  case OwnedValueIntroducerKind::Apply:
    os << "Apply";
    return;
  case OwnedValueIntroducerKind::BeginApply:
    os << "BeginApply";
    return;
  case OwnedValueIntroducerKind::TryApply:
    os << "TryApply";
    return;
  case OwnedValueIntroducerKind::Copy:
    os << "Copy";
    return;
  case OwnedValueIntroducerKind::LoadCopy:
    os << "LoadCopy";
    return;
  case OwnedValueIntroducerKind::LoadTake:
    os << "LoadTake";
    return;
  case OwnedValueIntroducerKind::Phi:
    os << "Phi";
    return;
  case OwnedValueIntroducerKind::Struct:
    os << "Struct";
    return;
  case OwnedValueIntroducerKind::Tuple:
    os << "Tuple";
    return;
  case OwnedValueIntroducerKind::FunctionArgument:
    os << "FunctionArgument";
    return;
  case OwnedValueIntroducerKind::PartialApplyInit:
    os << "PartialApplyInit";
    return;
  case OwnedValueIntroducerKind::AllocBoxInit:
    os << "AllocBoxInit";
    return;
  case OwnedValueIntroducerKind::AllocRefInit:
    os << "AllocRefInit";
    return;
  }
  llvm_unreachable("Covered switch isn't covered");
}

//===----------------------------------------------------------------------===//
//                       Introducer Searching Routines
//===----------------------------------------------------------------------===//

bool swift::getAllBorrowIntroducingValues(SILValue inputValue,
                                          SmallVectorImpl<BorrowedValue> &out) {
  if (inputValue.getOwnershipKind() != OwnershipKind::Guaranteed)
    return false;

  SmallVector<SILValue, 32> worklist;
  worklist.emplace_back(inputValue);

  while (!worklist.empty()) {
    SILValue value = worklist.pop_back_val();

    // First check if v is an introducer. If so, stash it and continue.
    if (auto scopeIntroducer = BorrowedValue::get(value)) {
      out.push_back(*scopeIntroducer);
      continue;
    }

    // If v produces .none ownership, then we can ignore it. It is important
    // that we put this before checking for guaranteed forwarding instructions,
    // since we want to ignore guaranteed forwarding instructions that in this
    // specific case produce a .none value.
    if (value.getOwnershipKind() == OwnershipKind::None)
      continue;

    // Otherwise if v is an ownership forwarding value, add its defining
    // instruction
    if (isGuaranteedForwardingValue(value)) {
      if (auto *i = value->getDefiningInstruction()) {
        llvm::copy(i->getOperandValues(true /*skip type dependent ops*/),
                   std::back_inserter(worklist));
        continue;
      }

      // Otherwise, we should have a block argument that is defined by a single
      // predecessor terminator.
      auto *arg = cast<SILPhiArgument>(value);
      auto *termInst = arg->getSingleTerminator();
      assert(termInst && termInst->isTransformationTerminator());
      assert(termInst->getNumOperands() == 1 &&
             "Transforming terminators should always have a single operand");
      worklist.push_back(termInst->getAllOperands()[0].get());
      continue;
    }

    // Otherwise, this is an introducer we do not understand. Bail and return
    // false.
    return false;
  }

  return true;
}

Optional<BorrowedValue>
swift::getSingleBorrowIntroducingValue(SILValue inputValue) {
  if (inputValue.getOwnershipKind() != OwnershipKind::Guaranteed)
    return None;

  SILValue currentValue = inputValue;
  while (true) {
    // First check if our initial value is an introducer. If we have one, just
    // return it.
    if (auto scopeIntroducer = BorrowedValue::get(currentValue)) {
      return scopeIntroducer;
    }

    // Otherwise if v is an ownership forwarding value, add its defining
    // instruction
    if (isGuaranteedForwardingValue(currentValue)) {
      if (auto *i = currentValue->getDefiningInstruction()) {
        auto instOps = i->getOperandValues(true /*ignore type dependent ops*/);
        // If we have multiple incoming values, return .None. We can't handle
        // this.
        auto begin = instOps.begin();
        if (std::next(begin) != instOps.end()) {
          return None;
        }
        // Otherwise, set currentOp to the single operand and continue.
        currentValue = *begin;
        continue;
      }

      // Otherwise, we should have a block argument that is defined by a single
      // predecessor terminator.
      auto *arg = cast<SILPhiArgument>(currentValue);
      auto *termInst = arg->getSingleTerminator();
      assert(termInst && termInst->isTransformationTerminator());
      assert(termInst->getNumOperands() == 1 &&
             "Transformation terminators should only have single operands");
      currentValue = termInst->getAllOperands()[0].get();
      continue;
    }

    // Otherwise, this is an introducer we do not understand. Bail and return
    // None.
    return None;
  }

  llvm_unreachable("Should never hit this");
}

bool swift::getAllOwnedValueIntroducers(
    SILValue inputValue, SmallVectorImpl<OwnedValueIntroducer> &out) {
  if (inputValue.getOwnershipKind() != OwnershipKind::Owned)
    return false;

  SmallVector<SILValue, 32> worklist;
  worklist.emplace_back(inputValue);

  while (!worklist.empty()) {
    SILValue value = worklist.pop_back_val();

    // First check if v is an introducer. If so, stash it and continue.
    if (auto introducer = OwnedValueIntroducer::get(value)) {
      out.push_back(*introducer);
      continue;
    }

    // If v produces .none ownership, then we can ignore it. It is important
    // that we put this before checking for guaranteed forwarding instructions,
    // since we want to ignore guaranteed forwarding instructions that in this
    // specific case produce a .none value.
    if (value.getOwnershipKind() == OwnershipKind::None)
      continue;

    // Otherwise if v is an ownership forwarding value, add its defining
    // instruction
    if (isOwnedForwardingValue(value)) {
      if (auto *i = value->getDefiningInstruction()) {
        llvm::copy(i->getOperandValues(true /*skip type dependent ops*/),
                   std::back_inserter(worklist));
        continue;
      }

      // Otherwise, we should have a block argument that is defined by a single
      // predecessor terminator.
      auto *arg = cast<SILPhiArgument>(value);
      auto *termInst = arg->getSingleTerminator();
      assert(termInst && termInst->isTransformationTerminator());
      assert(termInst->getNumOperands() == 1 &&
             "Transforming terminators should always have a single operand");
      worklist.push_back(termInst->getAllOperands()[0].get());
      continue;
    }

    // Otherwise, this is an introducer we do not understand. Bail and return
    // false.
    return false;
  }

  return true;
}

Optional<OwnedValueIntroducer>
swift::getSingleOwnedValueIntroducer(SILValue inputValue) {
  if (inputValue.getOwnershipKind() != OwnershipKind::Owned)
    return None;

  SILValue currentValue = inputValue;
  while (true) {
    // First check if our initial value is an introducer. If we have one, just
    // return it.
    if (auto introducer = OwnedValueIntroducer::get(currentValue)) {
      return introducer;
    }

    // Otherwise if v is an ownership forwarding value, add its defining
    // instruction
    if (isOwnedForwardingValue(currentValue)) {
      if (auto *i = currentValue->getDefiningInstruction()) {
        auto instOps = i->getOperandValues(true /*ignore type dependent ops*/);
        // If we have multiple incoming values, return .None. We can't handle
        // this.
        auto begin = instOps.begin();
        if (std::next(begin) != instOps.end()) {
          return None;
        }
        // Otherwise, set currentOp to the single operand and continue.
        currentValue = *begin;
        continue;
      }

      // Otherwise, we should have a block argument that is defined by a single
      // predecessor terminator.
      auto *arg = cast<SILPhiArgument>(currentValue);
      auto *termInst = arg->getSingleTerminator();
      assert(termInst && termInst->isTransformationTerminator());
      assert(termInst->getNumOperands()
             - termInst->getNumTypeDependentOperands() == 1 &&
             "Transformation terminators should only have single operands");
      currentValue = termInst->getAllOperands()[0].get();
      continue;
    }

    // Otherwise, this is an introducer we do not understand. Bail and return
    // None.
    return None;
  }

  llvm_unreachable("Should never hit this");
}

//===----------------------------------------------------------------------===//
//                             Forwarding Operand
//===----------------------------------------------------------------------===//

Optional<ForwardingOperand> ForwardingOperand::get(Operand *use) {
  auto *user = use->getUser();
  if (isa<OwnershipForwardingTermInst>(user))
    return ForwardingOperand(use);
  if (isa<OwnershipForwardingSingleValueInst>(user))
    return ForwardingOperand(use);
  if (isa<OwnershipForwardingConversionInst>(user))
    return ForwardingOperand(use);
  if (isa<OwnershipForwardingSelectEnumInstBase>(user))
    return ForwardingOperand(use);
  if (isa<OwnershipForwardingMultipleValueInstruction>(user))
    return ForwardingOperand(use);
  return None;
}

ValueOwnershipKind ForwardingOperand::getOwnershipKind() const {
  auto *user = use->getUser();
  if (auto *ofti = dyn_cast<OwnershipForwardingTermInst>(user))
    return ofti->getOwnershipKind();
  if (auto *ofsvi = dyn_cast<OwnershipForwardingSingleValueInst>(user))
    return ofsvi->getOwnershipKind();
  if (auto *ofci = dyn_cast<OwnershipForwardingConversionInst>(user))
    return ofci->getOwnershipKind();
  if (auto *ofseib = dyn_cast<OwnershipForwardingSelectEnumInstBase>(user))
    return ofseib->getOwnershipKind();
  if (auto *ofmvi = dyn_cast<OwnershipForwardingMultipleValueInstruction>(user))
    return ofmvi->getOwnershipKind();
  llvm_unreachable("Out of sync with ForwardingOperand::get?!");
}

void ForwardingOperand::setOwnershipKind(ValueOwnershipKind newKind) const {
  auto *user = use->getUser();
  if (auto *ofsvi = dyn_cast<OwnershipForwardingSingleValueInst>(user))
    if (!ofsvi->getType().isTrivial(*ofsvi->getFunction()))
      return ofsvi->setOwnershipKind(newKind);
  if (auto *ofci = dyn_cast<OwnershipForwardingConversionInst>(user))
    if (!ofci->getType().isTrivial(*ofci->getFunction()))
      return ofci->setOwnershipKind(newKind);
  if (auto *ofseib = dyn_cast<OwnershipForwardingSelectEnumInstBase>(user))
    if (!ofseib->getType().isTrivial(*ofseib->getFunction()))
      return ofseib->setOwnershipKind(newKind);

  if (auto *ofmvi = dyn_cast<OwnershipForwardingMultipleValueInstruction>(user)) {
    assert(ofmvi->getNumOperands() == 1);
    if (!ofmvi->getOperand(0)->getType().isTrivial(*ofmvi->getFunction())) {
      ofmvi->setOwnershipKind(newKind);
      // TODO: Refactor this better.
      if (auto *dsi = dyn_cast<DestructureStructInst>(ofmvi)) {
        for (auto &result : dsi->getAllResultsBuffer()) {
          if (result.getType().isTrivial(*dsi->getFunction()))
            continue;
          result.setOwnershipKind(newKind);
        }
      } else {
        auto *dti = cast<DestructureTupleInst>(ofmvi);
        for (auto &result : dti->getAllResultsBuffer()) {
          if (result.getType().isTrivial(*dti->getFunction()))
            continue;
          result.setOwnershipKind(newKind);
        }
      }
    }
    return;
  }

  if (auto *ofti = dyn_cast<OwnershipForwardingTermInst>(user)) {
    assert(ofti->getNumOperands() == 1);
    if (!ofti->getOperand(0)->getType().isTrivial(*ofti->getFunction())) {
      ofti->setOwnershipKind(newKind);

      // Then convert all of its incoming values that are owned to be guaranteed.
      for (auto &succ : ofti->getSuccessors()) {
        auto *succBlock = succ.getBB();

        // If we do not have any arguments, then continue.
        if (succBlock->args_empty())
          continue;

        for (auto *succArg : succBlock->getSILPhiArguments()) {
          // If we have an any value, just continue.
          if (!succArg->getType().isTrivial(*ofti->getFunction()))
            continue;
          succArg->setOwnershipKind(newKind);
        }
      }
    }
    return;
  }

  llvm_unreachable("Out of sync with ForwardingOperand::get?!");
}

void ForwardingOperand::replaceOwnershipKind(ValueOwnershipKind oldKind,
                                             ValueOwnershipKind newKind) const {
  auto *user = use->getUser();

  if (auto *ofsvi = dyn_cast<OwnershipForwardingSingleValueInst>(user))
    if (ofsvi->getOwnershipKind() == oldKind)
      return ofsvi->setOwnershipKind(newKind);

  if (auto *ofci = dyn_cast<OwnershipForwardingConversionInst>(user))
    if (ofci->getOwnershipKind() == oldKind)
      return ofci->setOwnershipKind(newKind);

  if (auto *ofseib = dyn_cast<OwnershipForwardingSelectEnumInstBase>(user))
    if (ofseib->getOwnershipKind() == oldKind)
      return ofseib->setOwnershipKind(newKind);

  if (auto *ofmvi = dyn_cast<OwnershipForwardingMultipleValueInstruction>(user)) {
    if (ofmvi->getOwnershipKind() == oldKind) {
      ofmvi->setOwnershipKind(newKind);
    }
    // TODO: Refactor this better.
    if (auto *dsi = dyn_cast<DestructureStructInst>(ofmvi)) {
      for (auto &result : dsi->getAllResultsBuffer()) {
        if (result.getOwnershipKind() != oldKind)
          continue;
        result.setOwnershipKind(newKind);
      }
    } else {
      auto *dti = cast<DestructureTupleInst>(ofmvi);
      for (auto &result : dti->getAllResultsBuffer()) {
        if (result.getOwnershipKind() != oldKind)
          continue;
        result.setOwnershipKind(newKind);
      }
    }
    return;
  }

  if (auto *ofti = dyn_cast<OwnershipForwardingTermInst>(user)) {
    if (ofti->getOwnershipKind() == oldKind) {
      ofti->setOwnershipKind(newKind);
      // Then convert all of its incoming values that are owned to be guaranteed.
      for (auto &succ : ofti->getSuccessors()) {
        auto *succBlock = succ.getBB();

        // If we do not have any arguments, then continue.
        if (succBlock->args_empty())
          continue;

        for (auto *succArg : succBlock->getSILPhiArguments()) {
          // If we have an any value, just continue.
          if (succArg->getOwnershipKind() == oldKind) {
            succArg->setOwnershipKind(newKind);
          }
        }
      }
    }
    return;
  }
  llvm_unreachable("Out of sync with ForwardingOperand::get?!");
}
