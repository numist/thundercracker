/*
 * This file is part of the Sifteo VM (SVM) Target for LLVM
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "Transforms/LogTransform.h"
#include "Transforms/MetadataTransform.h"
#include "Support/ErrorReporter.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/Type.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Analysis/ValueTracking.h"
using namespace llvm;

namespace llvm {
    BasicBlockPass *createLateLTIPass();
}

namespace {
    class LateLTIPass : public BasicBlockPass {
    public:
        static char ID;
        LateLTIPass()
            : BasicBlockPass(ID) {}

        virtual bool runOnBasicBlock (BasicBlock &BB);

        virtual const char *getPassName() const {
            return "Late link-time intrinsics";
        }
        
    private:
        bool runOnCall(CallSite &CS, StringRef Name);
        void handleAbort(CallSite &CS);
    };
}

char LateLTIPass::ID = 0;

BasicBlockPass *llvm::createLateLTIPass()
{
    return new LateLTIPass();
}

bool LateLTIPass::runOnBasicBlock (BasicBlock &BB)
{
    bool Changed = false;

    for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E;) {
        CallSite CS(cast<Value>(I));
        ++I;

        if (CS) {
            Function *F = CS.getCalledFunction();
            if (F && runOnCall(CS, F->getName()))
                Changed = true;
        }
    }

    return Changed;
}

void LateLTIPass::handleAbort(CallSite &CS)
{
    Instruction *I = CS.getInstruction();
    std::string msg;

    if (CS.arg_size() != 1)
        report_fatal_error(I, "_SYS_lti_abort requires exactly one arg");

    if (!GetConstantStringInfo(CS.getArgument(0), msg))
        report_fatal_error(I, "_SYS_lti_abort message must be a constant string.");

    report_fatal_error(I, Twine(msg));
}

bool LateLTIPass::runOnCall(CallSite &CS, StringRef Name)
{
    const TargetData *TD = getAnalysisIfAvailable<TargetData>();
    assert(TD);

    if (Name == "_SYS_lti_log") {
        LogTransform(CS);
        return true;
    }
    
    if (Name == "_SYS_lti_metadata") {
        MetadataTransform(CS, TD);
        return true;
    }
    
    if (Name == "_SYS_lti_abort") {
        handleAbort(CS);
        return true;
    }

    return false;
}
