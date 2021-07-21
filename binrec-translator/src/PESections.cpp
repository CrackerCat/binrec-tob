/*
 * Create global array variables all sections except .idata (since a new symbol
 * table is generated)
 */

#include "PESections.h"
#include "PEReader.h"
#include "PassUtils.h"
#include "SectionUtils.h"
#include <cmath>

using namespace llvm;

char PESections::ID = 0;
static RegisterPass<PESections> X("pe-sections",
                                  "S2E Add global arrays for the old PE file sections, and print their "
                                  "loadbase for linking",
                                  false, false);

static auto patchSectionName(const std::string &oldName) -> std::string {
    std::string name(oldName);

    if (name[0] == '.' || name[0] == '/')
        name.replace(0, 1, "__");

    return name;
}

static auto shouldCopySection(const std::string name) -> bool { return name != ".idata"; }

auto PESections::runOnModule(Module &m) -> bool {
    PEReader reader(getSourcePath(m).data());

    // combine raw section table data with runtime load base metadata
    NamedMDNode *mdSections = m.getNamedMetadata("sections");
    assert(mdSections);
    section_meta_t s;
    const Section *lastSection = nullptr;
    size_t lastLoadBase = 0;

    for (unsigned i = 0, iUpperBound = mdSections->getNumOperands(); i < iUpperBound; ++i) {
        readSectionMeta(s, mdSections->getOperand(i));

        const Section *section = reader.findSectionByName(s.name.data());
        assert(section);

        s.fileOffset = section->offset;
        s.size = section->virtualSize;

        if (shouldCopySection(s.name)) {
            std::vector<std::byte> data;

            if (s.fileOffset &&
                section->flags & COFF::IMAGE_SCN_MEM_READ) { // XXX: && !(section->flags & IMAGE_SCN_MEM_DISCARDABLE) {
                auto optData = reader.readSection(*section);
                assert(optData.has_value());
                optData->swap(data);
            }

            s.name = patchSectionName(s.name);
            copySection(m, s, data.data(), !(section->flags & COFF::IMAGE_SCN_MEM_WRITE));
            writeSectionConfig(s.name, s.loadBase);
        }

        writeSectionMeta(m, s);
        lastSection = section;
        lastLoadBase = s.loadBase;
    }

    // move wrapper function to custom section to avoid overlap
    assert(lastSection);
    assert(lastSection->flags & COFF::IMAGE_SCN_ALIGN_4096BYTES);
    unsigned nchunks = ceilf((float)lastSection->rawSize / 0x1000);

    writeSectionConfig(WRAPPER_SECTION, lastLoadBase + nchunks * 0x1000);

    return true;
}