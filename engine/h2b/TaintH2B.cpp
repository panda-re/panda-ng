// SPDX-License-Identifier: GPL-2.0-or-later
//
// taint-h2b: build-time shadow helpers for the taint mode (PLAN §4, H2b).
//
// For each TCG helper in a module that touches no guest memory and makes no calls left after
// inlining (locals, constant tables, and memory reached through its pointer arguments, which the
// engine passes only when they point into the CPU state, are fine), emit
//
//     uint64_t taint_h2b_<name>(const uint64_t *in)
//
// in[0..n): the arguments, in[n..2n): their shadows, in[2n]: shadow env minus env. It returns the
// shadow of the helper's result and stores the shadow of everything the helper stores through its
// pointer arguments, computed with the same bit rules as the TCG pass (Memcheck V-bits). A table
// `taint_h2b_table` of {name, fn, nargs, pointer-argument mask} lists them, null-terminated.
// Loads through a tainted index taint the loaded value, and a branch on a tainted condition taints
// the whole result: inside a helper, a table or a branch is part of the computation.
//
//   opt -load-pass-plugin=libTaintH2B.so -passes='taint-h2b,internalize,globaldce'
//       -internalize-public-api-list=taint_h2b_table in.bc -o out.bc

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

namespace {

Type *shadowTy(Type *T)
{
    LLVMContext &C = T->getContext();
    if (T->isIntegerTy()) {
        return T;
    }
    if (T->isFloatingPointTy()) {
        return IntegerType::get(C, T->getPrimitiveSizeInBits().getFixedValue());
    }
    if (T->isPointerTy()) {
        return Type::getInt64Ty(C);
    }
    if (auto *V = dyn_cast<FixedVectorType>(T)) {
        Type *E = shadowTy(V->getElementType());
        return E ? FixedVectorType::get(E, V->getNumElements()) : nullptr;
    }
    if (auto *S = dyn_cast<StructType>(T)) {
        SmallVector<Type *, 4> el;
        for (Type *E : S->elements()) {
            if (!(E = shadowTy(E))) {
                return nullptr;
            }
            el.push_back(E);
        }
        return StructType::get(C, el);
    }
    if (auto *A = dyn_cast<ArrayType>(T)) {
        Type *E = shadowTy(A->getElementType());
        return E ? ArrayType::get(E, A->getNumElements()) : nullptr;
    }
    return nullptr;
}

// Memory the shadow helper can mirror: locals, pointer arguments (the engine maps them into the
// shadow env), and, for reads, constant tables.
bool ownMemory(Value *P, bool write)
{
    const Value *U = getUnderlyingObject(P, 0);
    auto *G = dyn_cast<GlobalVariable>(U);
    return isa<AllocaInst>(U) || (isa<Argument>(U) && U->getType()->isPointerTy()) ||
           (!write && G && G->isConstant() && G->hasDefinitiveInitializer());
}

// Is the helper simple enough for this pass? Explains the first reason it is not.
bool qualifies(Function &F, std::string &why)
{
    if (F.isDeclaration() || F.isVarArg()) {
        why = "declaration";
        return false;
    }
    bool ptrs = false;
    for (Argument &A : F.args()) {
        Type *T = A.getType();
        ptrs |= T->isPointerTy();
        if (!(T->isIntegerTy() && T->getIntegerBitWidth() <= 64) && !T->isFloatTy() &&
            !T->isDoubleTy() && !T->isPointerTy()) {
            why = "argument type";
            return false;
        }
    }
    Type *RT = F.getReturnType();
    if (!(RT->isIntegerTy() && RT->getIntegerBitWidth() <= 64) && !(RT->isVoidTy() && ptrs)) {
        why = "return type";
        return false;
    }
    if (F.arg_size() > 8) {
        why = "arguments";
        return false;
    }
    for (Instruction &I : instructions(F)) {
        if (!shadowTy(I.getType()) && !I.getType()->isVoidTy()) {
            why = "value type";
            return false;
        }
        if (auto *CB = dyn_cast<CallBase>(&I)) {
            Function *Callee = CB->getCalledFunction();
            if (Callee && CB->doesNotReturn() && !isa<InlineAsm>(CB->getCalledOperand())) {
                continue;               // an assert or a guest exception: the shadow stops there
            }
            if (!Callee || !Callee->isIntrinsic() || isa<InlineAsm>(CB->getCalledOperand())) {
                why = "call " + (Callee ? Callee->getName().str() : std::string("(indirect)"));
                return false;
            }
            if (auto *MI = dyn_cast<MemIntrinsic>(CB)) {
                if (!ownMemory(MI->getDest(), true) ||
                    (isa<MemTransferInst>(MI) &&
                     !ownMemory(cast<MemTransferInst>(MI)->getSource(), false))) {
                    why = "memory";
                    return false;
                }
            }
            continue;
        }
        if (I.isAtomic() || isa<VAArgInst>(I) || isa<LandingPadInst>(I) || isa<IndirectBrInst>(I) ||
            isa<CallBrInst>(I) || isa<InvokeInst>(I)) {
            why = "instruction";
            return false;
        }
        Value *P = nullptr;
        if (auto *L = dyn_cast<LoadInst>(&I)) {
            P = L->getPointerOperand();
        } else if (auto *S = dyn_cast<StoreInst>(&I)) {
            P = S->getPointerOperand();
            if (S->getValueOperand()->getType()->isPointerTy()) {
                why = "pointer store";
                return false;
            }
        }
        if (P) {
            if (!ownMemory(P, isa<StoreInst>(I))) {
                why = "memory";
                return false;
            }
        }
    }
    return true;
}

struct Shadower {
    Function &F;                        // the clone, being instrumented
    DenseMap<Value *, Value *> sh;      // value -> shadow
    DenseMap<AllocaInst *, AllocaInst *> sa;
    SmallVector<std::pair<PHINode *, PHINode *>, 16> phis;
    AllocaInst *ctl = nullptr;          // set once a branch's condition is tainted
    Type *I64;

    explicit Shadower(Function &F) : F(F), I64(Type::getInt64Ty(F.getContext())) {}

    Value *get(Value *V)
    {
        auto it = sh.find(V);
        if (it != sh.end()) {
            return it->second;
        }
        Type *T = shadowTy(V->getType());
        return Constant::getNullValue(T);   // constants, globals, undef: clean
    }

    // Is any bit of shadow S set? (i1)
    Value *any(IRBuilder<> &B, Value *S)
    {
        Type *T = S->getType();
        if (T->isIntegerTy()) {
            return B.CreateICmpNE(S, Constant::getNullValue(T));
        }
        if (T->isVectorTy()) {
            return B.CreateICmpNE(B.CreateOrReduce(S), Constant::getNullValue(T->getScalarType()));
        }
        Value *r = B.getFalse();
        unsigned n = isa<StructType>(T) ? T->getStructNumElements() : T->getArrayNumElements();
        for (unsigned i = 0; i < n; i++) {
            r = B.CreateOr(r, any(B, B.CreateExtractValue(S, i)));
        }
        return r;
    }

    // All ones of shadow type T if C, else zero.
    Value *all(IRBuilder<> &B, Type *T, Value *C)
    {
        if (T->isIntegerTy() || T->isVectorTy()) {
            return B.CreateSelect(C, Constant::getAllOnesValue(T), Constant::getNullValue(T));
        }
        Value *r = PoisonValue::get(T);
        unsigned n = isa<StructType>(T) ? T->getStructNumElements() : T->getArrayNumElements();
        for (unsigned i = 0; i < n; i++) {
            Type *E = isa<StructType>(T) ? T->getStructElementType(i) : T->getArrayElementType();
            r = B.CreateInsertValue(r, all(B, E, C), i);
        }
        return r;
    }

    Value *ones(IRBuilder<> &B, Type *T)
    {
        if (T->isIntegerTy() || T->isVectorTy()) {
            return Constant::getAllOnesValue(T);
        }
        return all(B, T, B.getTrue());
    }

    // Any shadow among operands -> all ones of I's shadow type.
    Value *anyAll(IRBuilder<> &B, Instruction &I)
    {
        Value *c = B.getFalse();
        for (Value *O : I.operands()) {
            if (isa<BasicBlock>(O) || isa<Function>(O) || O->getType()->isMetadataTy()) {
                continue;
            }
            if (shadowTy(O->getType())) {
                c = B.CreateOr(c, any(B, get(O)));
            }
        }
        return all(B, shadowTy(I.getType()), c);
    }

    // x | -x: every bit at or above the lowest set bit (carries only move up).
    Value *left(IRBuilder<> &B, Value *S)
    {
        return B.CreateOr(S, B.CreateNeg(S));
    }

    // A shadow cast to an integer shadow type of another width or kind.
    Value *fit(IRBuilder<> &B, Value *S, Type *T)
    {
        if (S->getType() == T) {
            return S;
        }
        if (S->getType()->isIntegerTy() && T->isIntegerTy()) {
            return B.CreateZExtOrTrunc(S, T);
        }
        return all(B, T, any(B, S));
    }

    Value *delta = nullptr;             // shadow env minus env

    // The shadow of an address: beside a local, or in the shadow env for a pointer argument.
    Value *shadowAddr(IRBuilder<> &B, Value *P)
    {
        Value *U = const_cast<Value *>(getUnderlyingObject(P, 0));
        if (isa<Argument>(U)) {
            return B.CreateIntToPtr(B.CreateAdd(B.CreatePtrToInt(P, I64), delta), P->getType());
        }
        auto *A = cast<AllocaInst>(U);
        AllocaInst *S = sa.lookup(A);
        Value *off = B.CreateSub(B.CreatePtrToInt(P, I64), B.CreatePtrToInt(A, I64));
        return B.CreateGEP(B.getInt8Ty(), S, off);
    }

    Value *visit(Instruction &I)
    {
        if (I.isTerminator()) {
            Value *c = nullptr;
            if (auto *Br = dyn_cast<BranchInst>(&I); Br && Br->isConditional()) {
                c = Br->getCondition();
            } else if (auto *Sw = dyn_cast<SwitchInst>(&I)) {
                c = Sw->getCondition();
            }
            if (c) {
                IRBuilder<> B(&I);
                B.CreateStore(B.CreateOr(B.CreateLoad(B.getInt1Ty(), ctl), any(B, get(c))), ctl);
            }
            return nullptr;
        }
        IRBuilder<> B(I.getNextNode());
        Type *ST = I.getType()->isVoidTy() ? nullptr : shadowTy(I.getType());

        if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
            Value *a = BO->getOperand(0), *b = BO->getOperand(1);
            Value *sa_ = get(a), *sb = get(b);
            switch (BO->getOpcode()) {
            case Instruction::Add:
            case Instruction::Sub:
            case Instruction::Mul:
                return left(B, B.CreateOr(sa_, sb));
            case Instruction::And:      // a clean 0 on either side forces a clean 0
                return B.CreateOr(B.CreateOr(B.CreateAnd(sa_, sb), B.CreateAnd(sa_, b)),
                                  B.CreateAnd(a, sb));
            case Instruction::Or:       // a clean 1 on either side forces a clean 1
                return B.CreateOr(B.CreateOr(B.CreateAnd(sa_, sb), B.CreateAnd(sa_, B.CreateNot(b))),
                                  B.CreateAnd(B.CreateNot(a), sb));
            case Instruction::Xor:
                return B.CreateOr(sa_, sb);
            case Instruction::Shl:
            case Instruction::LShr:
            case Instruction::AShr: {
                Value *moved = B.CreateBinOp(BO->getOpcode(), sa_, b);
                return B.CreateSelect(B.CreateICmpNE(sb, Constant::getNullValue(sb->getType())),
                                      Constant::getAllOnesValue(ST), moved);
            }
            case Instruction::FSub:
                if (match_fneg(BO)) {
                    return sb;
                }
                return anyAll(B, I);
            default:                    // div, rem, floating point
                return anyAll(B, I);
            }
        }
        if (auto *U = dyn_cast<UnaryOperator>(&I)) {    // fneg: sign flip only
            return get(U->getOperand(0));
        }
        if (isa<CmpInst>(I)) {
            Value *s = B.CreateOr(fit(B, get(I.getOperand(0)), get(I.getOperand(1))->getType()),
                                  get(I.getOperand(1)));
            if (s->getType()->isVectorTy()) {
                return B.CreateICmpNE(s, Constant::getNullValue(s->getType()));
            }
            return any(B, s);
        }
        if (auto *S = dyn_cast<SelectInst>(&I)) {
            Value *pick = B.CreateSelect(S->getCondition(), get(S->getTrueValue()),
                                         get(S->getFalseValue()));
            Value *sc = get(S->getCondition());
            return B.CreateSelect(sc, ones(B, ST), pick);  // per lane for a vector condition
        }
        if (auto *C = dyn_cast<CastInst>(&I)) {
            Value *s = get(C->getOperand(0));
            switch (C->getOpcode()) {
            case Instruction::Trunc:
                return B.CreateTrunc(s, ST);
            case Instruction::ZExt:
                return B.CreateZExt(s, ST);
            case Instruction::SExt:
                return B.CreateSExt(s, ST);
            case Instruction::BitCast:
                return B.CreateBitCast(s, ST);
            case Instruction::PtrToInt:
            case Instruction::IntToPtr:
                return B.CreateZExtOrTrunc(s, ST);
            default:                    // fp conversions
                return all(B, ST, any(B, s));
            }
        }
        if (auto *P = dyn_cast<PHINode>(&I)) {
            IRBuilder<> PB(P);
            PHINode *s = PB.CreatePHI(ST, P->getNumIncomingValues());
            phis.push_back({P, s});
            return s;
        }
        if (auto *A = dyn_cast<AllocaInst>(&I)) {
            IRBuilder<> AB(A);
            AllocaInst *s = AB.CreateAlloca(A->getAllocatedType(), A->getArraySize());
            s->setAlignment(A->getAlign());
            sa[A] = s;
            return Constant::getNullValue(I64);
        }
        if (auto *G = dyn_cast<GetElementPtrInst>(&I)) {
            Value *s = get(G->getPointerOperand());
            for (Value *Ix : G->indices()) {
                s = B.CreateOr(s, fit(B, get(Ix), I64));
            }
            return all(B, I64, any(B, s));
        }
        if (auto *L = dyn_cast<LoadInst>(&I)) {
            Value *ps = get(L->getPointerOperand());
            Value *mem;
            const Value *U = getUnderlyingObject(L->getPointerOperand(), 0);
            if (isa<AllocaInst>(U) || isa<Argument>(U)) {
                mem = B.CreateAlignedLoad(ST, shadowAddr(B, L->getPointerOperand()), L->getAlign());
            } else {
                mem = Constant::getNullValue(ST);   // a constant table
            }
            return B.CreateSelect(any(B, ps), ones(B, ST), mem);
        }
        if (auto *S = dyn_cast<StoreInst>(&I)) {
            B.CreateAlignedStore(get(S->getValueOperand()), shadowAddr(B, S->getPointerOperand()),
                                 S->getAlign());
            return nullptr;
        }
        if (auto *E = dyn_cast<ExtractValueInst>(&I)) {
            return B.CreateExtractValue(get(E->getAggregateOperand()), E->getIndices());
        }
        if (auto *E = dyn_cast<InsertValueInst>(&I)) {
            return B.CreateInsertValue(get(E->getAggregateOperand()),
                                       get(E->getInsertedValueOperand()), E->getIndices());
        }
        if (auto *E = dyn_cast<ExtractElementInst>(&I)) {
            Value *s = B.CreateExtractElement(get(E->getVectorOperand()), E->getIndexOperand());
            return B.CreateSelect(any(B, get(E->getIndexOperand())), ones(B, ST), s);
        }
        if (auto *E = dyn_cast<InsertElementInst>(&I)) {
            Value *s = B.CreateInsertElement(get(E->getOperand(0)), get(E->getOperand(1)),
                                             E->getOperand(2));
            return B.CreateSelect(any(B, get(E->getOperand(2))), Constant::getAllOnesValue(ST), s);
        }
        if (auto *V = dyn_cast<ShuffleVectorInst>(&I)) {
            return B.CreateShuffleVector(get(V->getOperand(0)), get(V->getOperand(1)),
                                         V->getShuffleMask());
        }
        if (isa<FreezeInst>(I)) {
            return get(I.getOperand(0));
        }
        if (auto *II = dyn_cast<IntrinsicInst>(&I)) {
            return intrinsic(B, *II, ST);
        }
        if (I.isTerminator()) {
            return nullptr;             // control flow is not data flow
        }
        errs() << "taint-h2b: unhandled " << I << "\n";
        return ST ? anyAll(B, I) : nullptr;
    }

    static bool match_fneg(BinaryOperator *BO)
    {
        auto *C = dyn_cast<ConstantFP>(BO->getOperand(0));
        return C && C->isNegativeZeroValue();
    }

    Value *intrinsic(IRBuilder<> &B, IntrinsicInst &II, Type *ST)
    {
        switch (II.getIntrinsicID()) {
        case Intrinsic::lifetime_start:
        case Intrinsic::lifetime_end:
        case Intrinsic::assume:
        case Intrinsic::experimental_noalias_scope_decl:
            return nullptr;
        case Intrinsic::bswap:
        case Intrinsic::bitreverse:
            return B.CreateUnaryIntrinsic(II.getIntrinsicID(), get(II.getArgOperand(0)));
        case Intrinsic::fshl:
        case Intrinsic::fshr: {
            Value *s = B.CreateIntrinsic(II.getIntrinsicID(), {ST},
                                         {get(II.getArgOperand(0)), get(II.getArgOperand(1)),
                                          II.getArgOperand(2)});
            return B.CreateSelect(any(B, get(II.getArgOperand(2))), Constant::getAllOnesValue(ST),
                                  s);
        }
        case Intrinsic::uadd_with_overflow:
        case Intrinsic::sadd_with_overflow:
        case Intrinsic::usub_with_overflow:
        case Intrinsic::ssub_with_overflow:
        case Intrinsic::umul_with_overflow:
        case Intrinsic::smul_with_overflow: {
            Value *s = B.CreateOr(get(II.getArgOperand(0)), get(II.getArgOperand(1)));
            Value *r = B.CreateInsertValue(PoisonValue::get(ST), left(B, s), 0);
            return B.CreateInsertValue(r, any(B, s), 1);
        }
        case Intrinsic::memset: {
            Value *v = get(II.getArgOperand(1));
            B.CreateMemSet(shadowAddr(B, II.getArgOperand(0)), v, II.getArgOperand(2),
                           MaybeAlign());
            return nullptr;
        }
        case Intrinsic::memcpy:
        case Intrinsic::memmove: {
            Value *src = II.getArgOperand(1);
            Value *dst = shadowAddr(B, II.getArgOperand(0));
            const Value *U = getUnderlyingObject(src, 0);
            if (isa<AllocaInst>(U) || isa<Argument>(U)) {
                B.CreateMemMove(dst, MaybeAlign(), shadowAddr(B, src), MaybeAlign(),
                                II.getArgOperand(2));
            } else {
                B.CreateMemSet(dst, B.getInt8(0), II.getArgOperand(2), MaybeAlign());
            }
            return nullptr;
        }
        default:                        // ctpop, ctlz, min/max, abs, saturating, fp: conservative
            return ST ? anyAll(B, II) : nullptr;
        }
    }

    void run()
    {
        ReversePostOrderTraversal<Function *> rpo(&F);
        SmallVector<Instruction *, 256> todo;
        for (BasicBlock *BB : rpo) {
            for (Instruction &I : *BB) {
                todo.push_back(&I);
            }
        }
        IRBuilder<> EB(&*F.getEntryBlock().getFirstInsertionPt());   // not visited itself
        ctl = EB.CreateAlloca(EB.getInt1Ty());
        EB.CreateStore(EB.getFalse(), ctl);
        for (Instruction *I : todo) {
            if (Value *s = visit(*I)) {
                sh[I] = s;
            }
        }
        for (auto &[P, S] : phis) {
            for (unsigned i = 0; i < P->getNumIncomingValues(); i++) {
                BasicBlock *In = P->getIncomingBlock(i);
                Value *v = get(P->getIncomingValue(i));
                S->addIncoming(v, In);
            }
        }
    }
};

// Clone F as (args..., shadows..., delta) -> shadow of the result, instrumented.
Function *makeShadow(Function &F)
{
    Module &M = *F.getParent();
    SmallVector<Type *, 8> params;
    for (Argument &A : F.args()) {
        params.push_back(A.getType());
    }
    for (Argument &A : F.args()) {
        params.push_back(shadowTy(A.getType()));
    }
    params.push_back(Type::getInt64Ty(F.getContext()));
    bool isVoid = F.getReturnType()->isVoidTy();
    Type *RT = isVoid ? F.getReturnType() : shadowTy(F.getReturnType());
    Function *NF = Function::Create(FunctionType::get(RT, params, false),
                                    GlobalValue::InternalLinkage, "taint_h2b_body_" + F.getName(),
                                    M);
    ValueToValueMapTy VM;
    auto ai = NF->arg_begin();
    for (Argument &A : F.args()) {
        VM[&A] = &*ai++;
    }
    SmallVector<ReturnInst *, 4> rets;
    CloneFunctionInto(NF, &F, VM, CloneFunctionChangeType::LocalChangesOnly, rets);
    NF->setLinkage(GlobalValue::InternalLinkage);   // the clone copied the original's
    NF->setVisibility(GlobalValue::DefaultVisibility);
    NF->setDSOLocal(true);
    NF->setSubprogram(nullptr);
    for (Instruction &I : instructions(*NF)) {
        I.setDebugLoc(DebugLoc());
    }
    // the original's value attributes (range, noundef, ...) do not describe shadows
    NF->setAttributes(AttributeList::get(F.getContext(), NF->getAttributes().getFnAttrs(),
                                         AttributeSet(), {}));
    NF->setMemoryEffects(MemoryEffects::unknown());
    NF->removeFnAttr(Attribute::NoInline);
    NF->removeFnAttr(Attribute::OptimizeNone);

    // A call that does not return (an assertion, a guest exception) ends the shadow helper with a
    // fully tainted result: the real helper does not complete either.
    SmallPtrSet<ReturnInst *, 4> traps;
    SmallVector<CallBase *, 4> nr;      // the first per block: the rest of the block goes
    SmallPtrSet<BasicBlock *, 4> seen;
    for (Instruction &I : instructions(*NF)) {
        if (auto *CB = dyn_cast<CallBase>(&I); CB && CB->doesNotReturn() &&
                                               seen.insert(CB->getParent()).second) {
            nr.push_back(CB);
        }
    }
    for (CallBase *CB : nr) {
        BasicBlock *BB = CB->getParent();
        BasicBlock *rest = BB->splitBasicBlock(CB->getIterator());
        BB->getTerminator()->eraseFromParent();
        Type *OT = F.getReturnType();
        ReturnInst *R = OT->isVoidTy() ? ReturnInst::Create(F.getContext(), BB)
                                       : ReturnInst::Create(F.getContext(), PoisonValue::get(OT), BB);
        traps.insert(R);
        rets.push_back(R);
        DeleteDeadBlock(rest);
    }
    removeUnreachableBlocks(*NF);

    Shadower S(*NF);
    unsigned n = F.arg_size();
    for (unsigned i = 0; i < n; i++) {
        S.sh[NF->getArg(i)] = NF->getArg(n + i);
    }
    S.delta = NF->getArg(2 * n);
    S.run();
    for (ReturnInst *R : isVoid ? ArrayRef<ReturnInst *>() : ArrayRef<ReturnInst *>(rets)) {
        IRBuilder<> B(R);
        Value *s = traps.count(R) ? S.ones(B, RT)
                                  : B.CreateSelect(B.CreateLoad(B.getInt1Ty(), S.ctl),
                                                   S.ones(B, RT), S.get(R->getReturnValue()));
        ReturnInst::Create(F.getContext(), s, R->getIterator());
        R->eraseFromParent();
    }
    return NF;
}

// uint64_t taint_h2b_<name>(const uint64_t *in)
Function *makeEntry(Function &F, Function *Body)
{
    Module &M = *F.getParent();
    LLVMContext &C = M.getContext();
    Type *I64 = Type::getInt64Ty(C);
    StringRef name = F.getName();
    name.consume_front("helper_");
    Function *E = Function::Create(FunctionType::get(I64, {PointerType::get(C, 0)}, false),
                                   GlobalValue::InternalLinkage, "taint_h2b_" + name, M);
    E->setDSOLocal(true);
    IRBuilder<> B(BasicBlock::Create(C, "entry", E));
    unsigned n = F.arg_size();
    SmallVector<Value *, 16> args;
    for (unsigned i = 0; i <= 2 * n; i++) {
        Type *T = Body->getArg(i)->getType();
        Value *v = B.CreateLoad(I64, B.CreateConstGEP1_64(I64, E->getArg(0), i));
        if (T->isPointerTy()) {
            args.push_back(B.CreateIntToPtr(v, T));
            continue;
        }
        unsigned w = T->getPrimitiveSizeInBits().getFixedValue();
        v = B.CreateTrunc(v, IntegerType::get(C, w));
        args.push_back(B.CreateBitCast(v, T));
    }
    Value *r = B.CreateCall(Body, args);
    B.CreateRet(r->getType()->isVoidTy() ? ConstantInt::get(I64, 0) : B.CreateZExt(r, I64));
    return E;
}

struct TaintH2B : PassInfoMixin<TaintH2B> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &)
    {
        LLVMContext &C = M.getContext();
        SmallVector<Constant *, 64> rows;
        StructType *Row = StructType::get(C, {PointerType::get(C, 0), PointerType::get(C, 0),
                                              Type::getInt32Ty(C), Type::getInt32Ty(C)});
        SmallVector<Function *, 64> cands;
        for (Function &F : M) {
            if (F.getName().starts_with("helper_")) {
                cands.push_back(&F);
            }
        }
        for (Function *F : cands) {
            std::string why;
            if (!qualifies(*F, why)) {
                if (!F->isDeclaration()) {
                    errs() << "taint-h2b: skip " << F->getName() << " (" << why << ")\n";
                }
                continue;
            }
            Function *Body = makeShadow(*F);
            if (verifyFunction(*Body, &errs())) {
                errs() << "taint-h2b: dropped " << F->getName() << " (invalid shadow)\n";
                Body->eraseFromParent();
                continue;
            }
            Function *E = makeEntry(*F, Body);
            StringRef name = F->getName();
            name.consume_front("helper_");
            Constant *str = ConstantDataArray::getString(C, name);
            auto *G = new GlobalVariable(M, str->getType(), true, GlobalValue::PrivateLinkage, str,
                                         "taint_h2b_name");
            uint32_t ptrmask = 0;
            for (Argument &A : F->args()) {
                ptrmask |= A.getType()->isPointerTy() << A.getArgNo();
            }
            rows.push_back(ConstantStruct::get(
                Row, {G, E, ConstantInt::get(Type::getInt32Ty(C), F->arg_size()),
                      ConstantInt::get(Type::getInt32Ty(C), ptrmask)}));
            errs() << "taint-h2b: shadow " << F->getName() << "\n";
        }
        rows.push_back(Constant::getNullValue(Row));
        ArrayType *AT = ArrayType::get(Row, rows.size());
        new GlobalVariable(M, AT, true, GlobalValue::ExternalLinkage,
                           ConstantArray::get(AT, rows), "taint_h2b_table");
        return PreservedAnalyses::none();
    }
};

}   // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo()
{
    return {LLVM_PLUGIN_API_VERSION, "taint-h2b", "0.1", [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
                        if (name == "taint-h2b") {
                            MPM.addPass(TaintH2B());
                            return true;
                        }
                        return false;
                    });
            }};
}
