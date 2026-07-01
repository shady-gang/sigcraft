#ifndef SIGCRAFT_CHUNK_MESH_H
#define SIGCRAFT_CHUNK_MESH_H

#include "world.h"

#include "imr/imr.h"

#include <cstddef>

struct ChunkNeighbors {
    std::shared_ptr<Chunk> neighbours[3][3];
};

struct ChunkNeighborsUnsafe {
    ChunkData* neighbours[3][3];
};

struct ChunkMesh {
    std::unique_ptr<imr::Buffer> buf;
    size_t num_verts;

    ChunkMesh(imr::Device&, ChunkNeighbors& n);

    struct Vertex {
        int16_t vx, vy, vz;
        uint8_t tt;
        uint8_t ss;
        uint8_t nnx, nny, nnz;
        uint8_t pad;
        uint8_t br, bg, bb, pad2;
    };

    static_assert(sizeof(Vertex) == sizeof(uint8_t) * 16);
};

constexpr static unsigned lod_res[] = { 16, 8, 4, 2, 1 };
constexpr static unsigned lod_offsets[] = { 0, 4096, 4608, 4672, 4680, 4681 };

inline unsigned encode_chunkcoord(unsigned x, unsigned y, unsigned z, int lod) {
    return lod_offsets[lod] + x + (y + z * lod_res[lod]) * lod_res[lod];
}

inline std::tuple<unsigned, unsigned, unsigned> decode_chunkcoord(unsigned coord, int lod) {
    unsigned x = coord % lod_res[lod];
    coord /= lod_res[lod];
    unsigned y = coord % lod_res[lod];
    coord /= lod_res[lod];
    unsigned z = coord;
    return {x, y, z};
}

struct ChunkVoxelData {
    std::unique_ptr<imr::Buffer> buf[CUNK_CHUNK_SECTIONS_COUNT];

    ChunkVoxelData(imr::Device&, std::shared_ptr<Chunk>);

    constexpr static size_t buffer_size = lod_offsets[5] * sizeof(int);
};

#endif
