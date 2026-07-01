extern "C" {

#include "enklume/block_data.h"

}

#include "chunk_mesh.h"
#include "nasl/nasl.h"

#include <assert.h>
#include <vector>

#define MINUS_X_FACE(V) \
V(-1, -1, -1,   0, 0, -1, 0, 0) \
V(-1,  1, -1,   0, 1, -1, 0, 0) \
V(-1,  1,  1,   1, 1, -1, 0, 0) \
V(-1, -1, -1,   0, 0, -1, 0, 0) \
V(-1,  1,  1,   1, 1, -1, 0, 0) \
V(-1, -1,  1,   1, 0, -1, 0, 0)

#define PLUS_X_FACE(V) \
V(1,   1, -1,   1, 1, 1, 0, 0) \
V(1,  -1, -1,   1, 0, 1, 0, 0) \
V(1,   1,  1,   0, 1, 1, 0, 0) \
V(1,   1,  1,   0, 1, 1, 0, 0) \
V(1,  -1, -1,   1, 0, 1, 0, 0) \
V(1,  -1,  1,   0, 0, 1, 0, 0)

#define MINUS_Z_FACE(V) \
V(1,   1, -1,   0, 1, 0, 0, -1) \
V(-1, -1, -1,   1, 0, 0, 0, -1) \
V(1,  -1, -1,   0, 0, 0, 0, -1) \
V(-1,  1, -1,   1, 1, 0, 0, -1) \
V(-1, -1, -1,   1, 0, 0, 0, -1) \
V(1,   1, -1,   0, 1, 0, 0, -1)

#define PLUS_Z_FACE(V) \
V(1,  -1,  1,   1, 0, 0, 0, 1) \
V(-1, -1,  1,   0, 0, 0, 0, 1) \
V(1,   1,  1,   1, 1, 0, 0, 1) \
V(1,   1,  1,   1, 1, 0, 0, 1) \
V(-1, -1,  1,   0, 0, 0, 0, 1) \
V(-1,  1,  1,   0, 1, 0, 0, 1)

#define MINUS_Y_FACE(V) \
V(-1, -1, -1,   0, 0, 0, -1, 0) \
V(1,  -1,  1,   1, 1, 0, -1, 0) \
V(1,  -1, -1,   1, 0, 0, -1, 0) \
V(1,  -1,  1,   1, 1, 0, -1, 0) \
V(-1, -1, -1,   0, 0, 0, -1, 0) \
V(-1, -1,  1,   0, 1, 0, -1, 0)

#define PLUS_Y_FACE(V) \
V(1,   1,  1,   1, 0, 0, 1, 0) \
V(-1,  1, -1,   0, 1, 0, 1, 0) \
V(1,   1, -1,   1, 1, 0, 1, 0) \
V(-1,  1,  1,   0, 0, 0, 1, 0) \
V(-1,  1, -1,   0, 1, 0, 1, 0) \
V(1,   1,  1,   1, 0, 0, 1, 0)

#define CUBE(V) \
MINUS_X_FACE(V) \
PLUS_X_FACE(V)  \
MINUS_Z_FACE(V) \
PLUS_Z_FACE(V)  \
MINUS_Y_FACE(V) \
PLUS_Y_FACE(V)\

//#define V(cx, cy, cz, t, s, nx, ny, nz) tmp[0] = (int) ((cx + 1) / 2) + (float) x; tmp[1] = (int) ((cy + 1) / 2) + (float) y; tmp[2] = (int) ((cz + 1) / 2) + (float) z; tmp[3] = t; tmp[4] = s; g.push_back(tmp[0]); g.push_back(tmp[1]); g.push_back(tmp[2]); g.push_back(tmp[3]); g.push_back(tmp[4]);
#define V(cx, cy, cz, t, s, nx, ny, nz) \
v.vx = ((cx + 1) / 2) + x;             \
v.vy = ((cy + 1) / 2) + y;             \
v.vz = ((cz + 1) / 2) + z;             \
v.tt = t * 255;             \
v.ss = t * 255;             \
v.nnx = nx * 127 + 128;            \
v.nny = ny * 127 + 128;            \
v.nnz = nz * 127 + 128;            \
v.br = color.x * 255;            \
v.bg = color.y * 255;            \
v.bb = color.z * 255;            \
add_vertex();

static void paste_minus_x_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z) {
    ChunkMesh::Vertex v;
    auto add_vertex = [&](){
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    MINUS_X_FACE(V)
}

static void paste_plus_x_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z) {
    ChunkMesh::Vertex v;
    auto add_vertex = [&](){
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    PLUS_X_FACE(V)
}

static void paste_minus_y_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z) {
    ChunkMesh::Vertex v;
    auto add_vertex = [&](){
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    MINUS_Y_FACE(V)
}

static void paste_plus_y_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z) {
    ChunkMesh::Vertex v;
    auto add_vertex = [&](){
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    PLUS_Y_FACE(V)
}

static void paste_minus_z_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z) {
    ChunkMesh::Vertex v;
    auto add_vertex = [&](){
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    MINUS_Z_FACE(V)
}

static void paste_plus_z_face(std::vector<uint8_t>& g, nasl::vec3 color, unsigned x, unsigned y, unsigned z) {
    float tmp[5];
    ChunkMesh::Vertex v;
    auto add_vertex = [&](){
        uint8_t tmp[sizeof(v)];
        memcpy(tmp, &v, sizeof(v));
        for (auto b : tmp)
            g.push_back(b);
    };
    PLUS_Z_FACE(V)
}

#undef V

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

void chunk_mesh(const ChunkData* chunk, ChunkNeighborsUnsafe& neighbours, std::vector<uint8_t>& g, size_t* num_verts) {
    *num_verts = 0;
    for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++) {
        for (int x = 0; x < CUNK_CHUNK_SIZE; x++)
            for (int y = 0; y < CUNK_CHUNK_SIZE; y++)
                for (int z = 0; z < CUNK_CHUNK_SIZE; z++) {
                    int world_y = y + section * CUNK_CHUNK_SIZE;
                    BlockData block_data = access_safe(chunk, neighbours, x, world_y, z);
                    if (block_data != BlockAir) {
                        nasl::vec3 color;
                        color.x = block_colors[block_data].r;
                        color.y = block_colors[block_data].g;
                        color.z = block_colors[block_data].b;
                        if (access_safe(chunk, neighbours, x, world_y + 1, z) == BlockAir) {
                            paste_plus_y_face(g, color, x, world_y, z);
                            *num_verts += 6;
                        }
                        if (access_safe(chunk, neighbours, x, world_y - 1, z) == BlockAir) {
                            paste_minus_y_face(g, color, x, world_y, z);
                            *num_verts += 6;
                        }

                        if (access_safe(chunk, neighbours, x + 1, world_y, z) == BlockAir) {
                            paste_plus_x_face(g, color, x, world_y, z);
                            *num_verts += 6;
                        }
                        if (access_safe(chunk, neighbours, x - 1, world_y, z) == BlockAir) {
                            paste_minus_x_face(g, color, x, world_y, z);
                            *num_verts += 6;
                        }

                        if (access_safe(chunk, neighbours, x, world_y, z + 1) == BlockAir) {
                            paste_plus_z_face(g, color, x, world_y, z);
                            *num_verts += 6;
                        }
                        if (access_safe(chunk, neighbours, x, world_y, z - 1) == BlockAir) {
                            paste_minus_z_face(g, color, x, world_y, z);
                            *num_verts += 6;
                        }
                    }
                }
    }
}

ChunkMesh::ChunkMesh(imr::Device& d, ChunkNeighbors& n) {
    std::vector<uint8_t> g;
    ChunkNeighborsUnsafe unsafe {};
    for (size_t x = 0; x < 3; x++) {
        for (size_t z = 0; z < 3; z++) {
            unsafe.neighbours[x][z] = &n.neighbours[x][z].get()->data;
        }
    }
    chunk_mesh(unsafe.neighbours[1][1], unsafe, g, &num_verts);

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
    unsigned buffer[CUNK_CHUNK_SIZE * CUNK_CHUNK_SIZE * CUNK_CHUNK_MAX_HEIGHT];
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
                        bool completely_filled = true, empty = true;
                        for (int rx = x * 2; rx < x * 2 + 1; rx++)
                            for (int ry = y * 2; ry < y * 2 + 1; ry++)
                                for (int rz = z * 2; rz < z * 2 + 1; rz++) {
                                    if (chunk_get_block_data(&c->data, x, baseY + y, z) != 0)
                                        empty = false;
                                    else
                                        completely_filled = false;
                                }

                        if (completely_filled)
                            buffer[encode_chunkcoord(x, y, z, lod)] = 2;
                        else if (empty)
                            buffer[encode_chunkcoord(x, y, z, lod)] = 0;
                        else
                            buffer[encode_chunkcoord(x, y, z, lod)] = 1;
                    }
        }

        buf[section] = std::make_unique<imr::Buffer>(d, buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        buf[section]->uploadDataSync(0, buffer_size, buffer);
    }
}