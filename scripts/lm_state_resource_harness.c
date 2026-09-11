#include "susamune/lm_state_resource.h"
#include "susamune/lm_state_model.h"

int equivalent(unsigned int savedId, unsigned int liveId,
               const unsigned char *saved, const unsigned char *live) {
    return LmStateIdleResourceRecordEquivalent(savedId, liveId, saved, live);
}

int changes_match(unsigned int activeMask, unsigned int recordMask,
                  const unsigned long *savedIds, const unsigned long *liveIds,
                  const unsigned char *saved, const unsigned char *live) {
    return LmStateResourceChangesMatch(activeMask, recordMask, savedIds, liveIds,
                                     saved, live);
}

int settled(unsigned long id, unsigned int slot, unsigned long backing,
            unsigned long start, unsigned long end, const unsigned char *record) {
    return LmStateSettledRoomRecord(id, slot, backing, start, end, record);
}

int reload_changes(unsigned int activeMask, unsigned int recordMask,
    const unsigned long *savedIds, const unsigned long *liveIds,
    const unsigned char *saved, const unsigned char *live,
    unsigned long savedBulk, unsigned long liveBulk, unsigned long slotSize,
    unsigned long start, unsigned long end) {
    return LmStateResourceReloadChangesMatch(activeMask, recordMask, savedIds,
        liveIds, saved, live, savedBulk, liveBulk, slotSize, start, end);
}

int model_endpoint(unsigned long state, unsigned long handle,
    unsigned long registryHandle, unsigned long root, unsigned long registryRoot,
    unsigned long archive, unsigned long size) {
    return LmStateModelEndpointValid(state, handle, registryHandle, root,
                                      registryRoot, archive, size);
}

int model_changes(const unsigned long *saved, const unsigned long *live,
    const unsigned long *savedRegistry, const unsigned long *liveRegistry) {
    return LmStateModelChangesKnown(saved, live, savedRegistry, liveRegistry);
}
