extern "C" {

#include "enklume/block_data.h"

}

#include "chunk_mesh.h"
#include "nasl/nasl.h"

#include <assert.h>
#include <vector>

using namespace nasl;

using add_vertex_fn_t = void(ivec3, vec2, ivec3);
using generate_face_fn_t = void(const std::function<add_vertex_fn_t>&);
using add_face_fn_t = void(ivec3, vec3, const std::function<generate_face_fn_t>&);

static auto minus_x_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(0, 0, 0), vec2(0, 0), ivec3(-1, 0, 0));
    add_vertex(ivec3(0, 1, 0), vec2(0, 1), ivec3(-1, 0, 0));
    add_vertex(ivec3(0, 1, 1), vec2(1, 1), ivec3(-1, 0, 0));
    add_vertex(ivec3(0, 0, 0), vec2(0, 0), ivec3(-1, 0, 0));
    add_vertex(ivec3(0, 1, 1), vec2(1, 1), ivec3(-1, 0, 0));
    add_vertex(ivec3(0, 0, 1), vec2(1, 0), ivec3(-1, 0, 0));
};

static auto plus_x_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(1, 1, 0), vec2(1, 1), ivec3(1, 0, 0));
    add_vertex(ivec3(1, 0, 0), vec2(1, 0), ivec3(1, 0, 0));
    add_vertex(ivec3(1, 1, 1), vec2(0, 1), ivec3(1, 0, 0));
    add_vertex(ivec3(1, 1, 1), vec2(0, 1), ivec3(1, 0, 0));
    add_vertex(ivec3(1, 0, 0), vec2(1, 0), ivec3(1, 0, 0));
    add_vertex(ivec3(1, 0, 1), vec2(0, 0), ivec3(1, 0, 0));
};

static auto minus_z_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(1, 1, 0), vec2(0, 1), ivec3(0, 0, -1));
    add_vertex(ivec3(0, 0, 0), vec2(1, 0), ivec3(0, 0, -1));
    add_vertex(ivec3(1, 0, 0), vec2(0, 0), ivec3(0, 0, -1));
    add_vertex(ivec3(0, 1, 0), vec2(1, 1), ivec3(0, 0, -1));
    add_vertex(ivec3(0, 0, 0), vec2(1, 0), ivec3(0, 0, -1));
    add_vertex(ivec3(1, 1, 0), vec2(0, 1), ivec3(0, 0, -1));
};

static auto plus_z_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(1, 0, 1), vec2(1, 0), ivec3(0, 0, 1));
    add_vertex(ivec3(0, 0, 1), vec2(0, 0), ivec3(0, 0, 1));
    add_vertex(ivec3(1, 1, 1), vec2(1, 1), ivec3(0, 0, 1));
    add_vertex(ivec3(1, 1, 1), vec2(1, 1), ivec3(0, 0, 1));
    add_vertex(ivec3(0, 0, 1), vec2(0, 0), ivec3(0, 0, 1));
    add_vertex(ivec3(0, 1, 1), vec2(0, 1), ivec3(0, 0, 1));
};

static auto minus_y_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(0, 0, 0), vec2(0, 0), ivec3(0, -1, 0));
    add_vertex(ivec3(1, 0, 1), vec2(1, 1), ivec3(0, -1, 0));
    add_vertex(ivec3(1, 0, 0), vec2(1, 0), ivec3(0, -1, 0));
    add_vertex(ivec3(1, 0, 1), vec2(1, 1), ivec3(0, -1, 0));
    add_vertex(ivec3(0, 0, 0), vec2(0, 0), ivec3(0, -1, 0));
    add_vertex(ivec3(0, 0, 1), vec2(0, 1), ivec3(0, -1, 0));
};

static std::function<generate_face_fn_t> plus_y_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(1, 1, 1), vec2(1, 0), ivec3(0, 1, 0));
    add_vertex(ivec3(0, 1, 0), vec2(0, 1), ivec3(0, 1, 0));
    add_vertex(ivec3(1, 1, 0), vec2(1, 1), ivec3(0, 1, 0));
    add_vertex(ivec3(0, 1, 1), vec2(0, 0), ivec3(0, 1, 0));
    add_vertex(ivec3(0, 1, 0), vec2(0, 1), ivec3(0, 1, 0));
    add_vertex(ivec3(1, 1, 1), vec2(1, 0), ivec3(0, 1, 0));
};

static BlockData access_safe(const ChunkData* chunk, ChunkNeighborsUnsafe& neighbours, int x, int y, int z) {
    unsigned int i, k;
    if (x < 0) {
        i = 0;
    } else if (x < CUNK_CHUNK_SIZE) {
        i = 1;
    } else {
        i = 2;
    }

    if (y < 0 || y > CUNK_CHUNK_MAX_HEIGHT)
        return BlockAir;

    if (z < 0) {
        k = 0;
    } else if (z < CUNK_CHUNK_SIZE) {
        k = 1;
    } else {
        k = 2;
    }
    //assert(!neighbours || neighbours[1][1][1] == chunk);
    if (i == 1 && k == 1) {
        return chunk_get_block_data(chunk, x, y, z);
    } else {
        if (neighbours.neighbours[i][k])
            return chunk_get_block_data(neighbours.neighbours[i][k], x & 15, y, z & 15);
    }

    return BlockAir;
}

void traverse_chunk_mesh(const ChunkData* chunk, ChunkNeighborsUnsafe& neighbours, const std::function<add_face_fn_t>& add_face) {
    for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++) {
        for (int x = 0; x < CUNK_CHUNK_SIZE; x++)
            for (int y = 0; y < CUNK_CHUNK_SIZE; y++)
                for (int z = 0; z < CUNK_CHUNK_SIZE; z++) {
                    int world_y = y + section * CUNK_CHUNK_SIZE;
                    BlockData block_data = access_safe(chunk, neighbours, x, world_y, z);
                    if (block_data != BlockAir) {
                        vec3 color;
                        color.x = block_colors[block_data].r;
                        color.y = block_colors[block_data].g;
                        color.z = block_colors[block_data].b;
                        if (access_safe(chunk, neighbours, x, world_y + 1, z) == BlockAir) {
                            add_face(ivec3(x, world_y, z), color, plus_y_face);
                        }
                        if (access_safe(chunk, neighbours, x, world_y - 1, z) == BlockAir) {
                            add_face(ivec3(x, world_y, z), color, minus_y_face);
                        }

                        if (access_safe(chunk, neighbours, x + 1, world_y, z) == BlockAir) {
                            add_face(ivec3(x, world_y, z), color, plus_x_face);
                        }
                        if (access_safe(chunk, neighbours, x - 1, world_y, z) == BlockAir) {
                            add_face(ivec3(x, world_y, z), color, minus_x_face);
                        }

                        if (access_safe(chunk, neighbours, x, world_y, z + 1) == BlockAir) {
                            add_face(ivec3(x, world_y, z), color, plus_z_face);
                        }
                        if (access_safe(chunk, neighbours, x, world_y, z - 1) == BlockAir) {
                            add_face(ivec3(x, world_y, z), color, minus_z_face);
                        }
                    }
                }
    }
}

auto make_vertex_encoder(const ivec3& block_position, const vec3& color) {
    return [&](ivec3 position, vec2 uv, ivec3 normal) {
        ChunkMesh::Vertex v;
        v.vx = block_position.x + position.x;
        v.vy = block_position.y + position.y;
        v.vz = block_position.z + position.z;
        v.tt = uv.x * 255;
        v.ss = uv.y * 255;
        v.nnx = normal.x * 127 + 128;
        v.nny = normal.y * 127 + 128;
        v.nnz = normal.z * 127 + 128;
        v.br = color.x * 255;
        v.bg = color.y * 255;
        v.bb = color.z * 255;
        return v;
    };
}

ChunkMesh::ChunkMesh(imr::Device& d, ChunkNeighbors& n) {
    std::vector<uint8_t> g;
    ChunkNeighborsUnsafe unsafe {};
    for (size_t x = 0; x < 3; x++) {
        for (size_t z = 0; z < 3; z++) {
            unsafe.neighbours[x][z] = &n.neighbours[x][z].get()->data;
        }
    }

    num_verts = 0;
    std::function<add_face_fn_t> add_face = [&](ivec3 block_position, vec3 color, const std::function<generate_face_fn_t>& gen_face) {
        auto encoder = make_vertex_encoder(block_position, color);
        std::function<add_vertex_fn_t> add_vertex = [&](ivec3 position, vec2 uv, ivec3 normal){
            ChunkMesh::Vertex v = encoder(position, uv, normal);
            uint8_t tmp[sizeof(v)];
            memcpy(tmp, &v, sizeof(v));
            for (auto b : tmp)
                g.push_back(b);
            num_verts += 1;
        };
        gen_face(add_vertex);
    };
    traverse_chunk_mesh(unsafe.neighbours[1][1], unsafe, add_face);

    //fprintf(stderr, "%zu vertices, totalling %zu KiB of data\n", num_verts, num_verts * sizeof(float) * 5 / 1024);
    //fflush(stderr);

    size_t buffer_size = g.size() * sizeof(uint8_t);
    void* buffer = g.data();

    if (buffer_size > 0) {
        buf = std::make_unique<imr::Buffer>(d, buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        buf->uploadDataSync(0, buffer_size, buffer);
    }
}

ChunkVoxelData::ChunkVoxelData(imr::Device& d, std::shared_ptr<Chunk> c) {
    uint8_t buffer[lod_offsets[5]];
    for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++) {
        int baseY = section * CUNK_CHUNK_SIZE;
        bool empty = true;
        for (int x = 0; x < CUNK_CHUNK_SIZE; x++)
            for (int y = 0; y < CUNK_CHUNK_SIZE; y++)
                for (int z = 0; z < CUNK_CHUNK_SIZE; z++) {
                    auto data = buffer[encode_chunkcoord(x, y, z, 0)] = chunk_get_block_data(&c->data, x, baseY + y, z);
                    if (data != 0)
                        empty = false;
                }

        if (empty)
            continue;

        // LOD computation
        for (int lod = 1; lod < 5; lod++) {
            for (int x = 0; x < lod_res[lod]; x++)
                for (int y = 0; y < lod_res[lod]; y++)
                    for (int z = 0; z < lod_res[lod]; z++) {
                        bool completely_filled = true, completely_empty = true;
                        for (int rx = x * 2; rx < x * 2 + 2; rx++)
                            for (int ry = y * 2; ry < y * 2 + 2; ry++)
                                for (int rz = z * 2; rz < z * 2 + 2; rz++) {
                                    auto data = buffer[encode_chunkcoord(rx, ry, rz, lod-1)];
                                    //printf("rx %d ry %d rz %d data %d\n", rx, ry, rz, data);
                                    if (data != 0)
                                        completely_empty = false;
                                    if (data == 0 || (lod > 1 && data != 2))
                                        completely_filled = false;
                                }

                        if (completely_filled)
                            buffer[encode_chunkcoord(x, y, z, lod)] = 2;
                        else if (completely_empty)
                            buffer[encode_chunkcoord(x, y, z, lod)] = 0;
                        else
                            buffer[encode_chunkcoord(x, y, z, lod)] = 1;
                    }
        }

        if (buffer[lod_offsets[4]] == 2) {
            assert(buffer[lod_offsets[4] - 1] == 2);
        }

        buf[section] = std::make_unique<imr::Buffer>(d, buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        buf[section]->uploadDataSync(0, buffer_size, buffer);
    }
}