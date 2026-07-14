#ifndef VOXEL_RANGE_MESHLET_H
#define VOXEL_RANGE_MESHLET_H

// sizeof = 6
struct MeshletPayload {
    uint16_t start;
    uint16_t end;
    uint16_t num_verts;
    uint16_t num_prims;
};

// sizeof = 5636
struct ChunkTmp {
    uint8_t faces[4096];
    uint num_meshlets;
    MeshletPayload meshlets[256];
};

layout(scalar, buffer_reference) buffer ChunkTmpRef {
    ChunkTmp ref;
};

// sizeof = 135264
struct ChunkTmpSections {
    ChunkTmp sections[24];
};

layout(scalar, buffer_reference) buffer TmpRef {
    ChunkTmpSections visible[];
};

#endif
