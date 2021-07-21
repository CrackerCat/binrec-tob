#include "RemoveBlocks.h"
#include "IR/Selectors.h"
#include "PassUtils.h"

using namespace binrec;
using namespace llvm;

char RemoveBlocks::ID = 0;
static RegisterPass<RemoveBlocks> x("remove-blocks", "S2E Remove specific blocks specified with -remove-block", false,
                                    false);

static cl::list<unsigned> blockAddresses("remove-block", cl::desc("Basic block addresses to remove"),
                                         cl::value_desc("address"));

auto RemoveBlocks::runOnModule(Module &m) -> bool {
    if (blockAddresses.empty())
        return false;

    std::vector<Function *> eraseList;

    for (Function &f : m) {
        if (!f.hasName() || blockAddresses.empty())
            continue;

        unsigned address = 0;

        if (f.getName().startswith("tcg-llvm-tb-"))
            address = hexToInt(f.getName().substr(f.getName().rfind('-') + 1));
        else if (isLiftedFunction(f))
            address = getBlockAddress(&f);
        else
            continue;

        if (vectorContains(*(&blockAddresses), address))
            eraseList.push_back(&f);
    }

    for (Function *f : eraseList) {
        INFO("Remove " << f->getName());
        f->eraseFromParent();
    }

    return !eraseList.empty();
}