#ifndef SUSAMUNE_LM_STATE_MODEL_H
#define SUSAMUNE_LM_STATE_MODEL_H

static inline int LmStateModelRootInside(unsigned long root,
    unsigned long archive, unsigned long size) {
    return root == 0u || ((root & 3u) == 0u && archive >= 0x80000000u &&
        archive < 0x81800000u && size <= 0x81800000u - archive &&
        size >= 4u && root >= archive && root - archive <= size - 4u);
}

static inline int LmStateModelEndpointValid(unsigned long state,
    unsigned long handle, unsigned long registryHandle, unsigned long root,
    unsigned long registryRoot, unsigned long archive, unsigned long size) {
    if (state == 0u) return handle == 0u && registryHandle == 0u &&
        root == 0u && registryRoot == 0u;
    return state == 3u && handle != 0u && handle == registryHandle &&
        LmStateModelRootInside(root, archive, size) &&
        LmStateModelRootInside(registryRoot, archive, size);
}

static inline int LmStateModelChangesKnown(const unsigned long *saved,
    const unsigned long *live, const unsigned long *savedRegistry,
    const unsigned long *liveRegistry) {
    unsigned int i;
    for (i = 0u; i < 13u; ++i) {
        if (i == 1u || i == 2u || i == 3u || i == 5u ||
            i == 11u || i == 12u) continue;
        if (saved[i] != live[i]) return 0;
    }
    for (i = 0u; i < 16u; ++i) {
        if (i == 1u || i == 3u) continue;
        if (savedRegistry[i] != liveRegistry[i]) return 0;
    }
    return 1;
}

#endif
