#ifndef CHUNK_DATA_H
#define CHUNK_DATA_H

#include "morton.glsl"

#define CUNK_CHUNK_SIZE 16

layout(scalar, buffer_reference) readonly buffer CubicChunkRef {
    uint8_t data[];
};

const uint lod_offsets[] = { 0, 4096, 4608, 4672, 4680, 4681 };

uint encode_chunkcoord(uint x, uint y, uint z, uint lod) {
    return lod_offsets[lod] + encode_morton(x, y, z);
}

uint encode_chunkcoord(uvec3 v, uint lod) {
    return encode_chunkcoord(v.x, v.y, v.z, lod);
}

bool is_chunk_solid(CubicChunkRef chunk) {
    uint data = chunk.data[encode_chunkcoord(uvec3(0), 4)];
    return data == 2;
}

uint lod_access(CubicChunkRef chunk, uvec3 pos) {
    if (uint64_t(chunk) == 0)
        return 0;

    for (int lod = 4; lod >= 0; lod--) {
        uvec3 lod_pos = pos >> lod;
        uint data = chunk.data[encode_chunkcoord(lod_pos, lod)];
        if (lod == 0) {
            return data;
        }
        if (data == 0)
        return 0;
        // completely filled
        if (data == 2)
        return 2;
    }
    // unreachable
    return 0;
}

void decode_block_idx(int idx, out ivec3 block_pos) {
    uvec3 ublock_pos;
    decode_morton(idx, ublock_pos.x, ublock_pos.y, ublock_pos.z);
    block_pos = ivec3(ublock_pos);
}

void decode_block_idx(uint idx, out uvec3 block_pos) {
    decode_morton(idx, block_pos.x, block_pos.y, block_pos.z);
}

#endif
