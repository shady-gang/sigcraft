#include <unordered_set>

extern "C" {

#include "enklume/block_data.h"

}

#include "chunk_mesh.h"
#include "nasl/nasl.h"

#include <assert.h>
#include <vector>

using namespace nasl;

enum class BlockFace {
    Top,
    Bottom,
    PlusZ,
    MinusZ,
    PlusX,
    MinusX,
};

uint blockface2axis(BlockFace bf) {
    return ((uint)bf / 2 + 1) % 3;
    /*switch (bf) {
        case BlockFace::Top:
        case BlockFace::Bottom: return 1;
        case BlockFace::PlusZ:
        case BlockFace::MinusZ: return 2;
        case BlockFace::PlusX:
        case BlockFace::MinusX: return 0;
    }*/
}

using add_face_fn_t = void(ivec3, vec3, BlockFace);

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
        int x, y, z;
        for (int block_idx = 0; block_idx < CUNK_CHUNK_SIZE * CUNK_CHUNK_SIZE * CUNK_CHUNK_SIZE; block_idx++) {
            decode_morton(block_idx, (uint&) x, (uint&) y, (uint&) z);
            int world_y = y + section * CUNK_CHUNK_SIZE;
            BlockData block_data = access_safe(chunk, neighbours, x, world_y, z);
            if (block_data != BlockAir) {
                vec3 color;
                color.x = block_colors[block_data].r;
                color.y = block_colors[block_data].g;
                color.z = block_colors[block_data].b;

                if (access_safe(chunk, neighbours, x, world_y + 1, z) == BlockAir) {
                    add_face(ivec3(x, world_y, z), color, BlockFace::Top);
                }
                if (access_safe(chunk, neighbours, x, world_y - 1, z) == BlockAir) {
                    add_face(ivec3(x, world_y, z), color, BlockFace::Bottom);
                }
                if (access_safe(chunk, neighbours, x, world_y, z + 1) == BlockAir) {
                    add_face(ivec3(x, world_y, z), color, BlockFace::PlusZ);
                }
                if (access_safe(chunk, neighbours, x, world_y, z - 1) == BlockAir) {
                    add_face(ivec3(x, world_y, z), color, BlockFace::MinusZ);
                }
                if (access_safe(chunk, neighbours, x + 1, world_y, z) == BlockAir) {
                    add_face(ivec3(x, world_y, z), color, BlockFace::PlusX);
                }
                if (access_safe(chunk, neighbours, x - 1, world_y, z) == BlockAir) {
                    add_face(ivec3(x, world_y, z), color, BlockFace::MinusX);
                }
            }
        }
    }
}

using add_vertex_fn_t = void(ivec3, vec2);
using generate_face_vertices_fn_t = void(const std::function<add_vertex_fn_t>&);

ivec3 face2normal(BlockFace face) {
    switch (face) {
        case BlockFace::Top: return ivec3(0, 1, 0);
        case BlockFace::Bottom: return ivec3(0, -1, 0);
        case BlockFace::PlusZ: return ivec3(0, 0, 1);
        case BlockFace::MinusZ: return ivec3(0, 0, -1);
        case BlockFace::PlusX: return ivec3(1, 0, 0);
        case BlockFace::MinusX: return ivec3(-1, 0, 0);
    }
}

static auto plus_y_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(1, 1, 1), vec2(1, 0));
    add_vertex(ivec3(0, 1, 0), vec2(0, 1));
    add_vertex(ivec3(1, 1, 0), vec2(1, 1));
    add_vertex(ivec3(0, 1, 1), vec2(0, 0));
    add_vertex(ivec3(0, 1, 0), vec2(0, 1));
    add_vertex(ivec3(1, 1, 1), vec2(1, 0));
};

static auto minus_y_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(0, 0, 0), vec2(0, 0));
    add_vertex(ivec3(1, 0, 1), vec2(1, 1));
    add_vertex(ivec3(1, 0, 0), vec2(1, 0));
    add_vertex(ivec3(1, 0, 1), vec2(1, 1));
    add_vertex(ivec3(0, 0, 0), vec2(0, 0));
    add_vertex(ivec3(0, 0, 1), vec2(0, 1));
};

static auto plus_z_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(1, 0, 1), vec2(1, 0));
    add_vertex(ivec3(0, 0, 1), vec2(0, 0));
    add_vertex(ivec3(1, 1, 1), vec2(1, 1));
    add_vertex(ivec3(1, 1, 1), vec2(1, 1));
    add_vertex(ivec3(0, 0, 1), vec2(0, 0));
    add_vertex(ivec3(0, 1, 1), vec2(0, 1));
};

static auto minus_z_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(1, 1, 0), vec2(0, 1));
    add_vertex(ivec3(0, 0, 0), vec2(1, 0));
    add_vertex(ivec3(1, 0, 0), vec2(0, 0));
    add_vertex(ivec3(0, 1, 0), vec2(1, 1));
    add_vertex(ivec3(0, 0, 0), vec2(1, 0));
    add_vertex(ivec3(1, 1, 0), vec2(0, 1));
};

static auto plus_x_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(1, 1, 0), vec2(1, 1));
    add_vertex(ivec3(1, 0, 0), vec2(1, 0));
    add_vertex(ivec3(1, 1, 1), vec2(0, 1));
    add_vertex(ivec3(1, 1, 1), vec2(0, 1));
    add_vertex(ivec3(1, 0, 0), vec2(1, 0));
    add_vertex(ivec3(1, 0, 1), vec2(0, 0));
};

static auto minus_x_face = [](const std::function<add_vertex_fn_t>& add_vertex) {
    add_vertex(ivec3(0, 0, 0), vec2(0, 0));
    add_vertex(ivec3(0, 1, 0), vec2(0, 1));
    add_vertex(ivec3(0, 1, 1), vec2(1, 1));
    add_vertex(ivec3(0, 0, 0), vec2(0, 0));
    add_vertex(ivec3(0, 1, 1), vec2(1, 1));
    add_vertex(ivec3(0, 0, 1), vec2(1, 0));
};

static std::array<std::function<generate_face_vertices_fn_t>, 6> generate_face_vertices = {
    plus_y_face,
    minus_y_face,
    plus_z_face,
    minus_z_face,
    plus_x_face,
    minus_x_face,
};

ChunkMesh::Vertex encode_vertex(ivec3 position, vec2 uv, ivec3 normal, const vec3& color) {
    ChunkMesh::Vertex v;
    v.vx = position.x;
    v.vy = position.y;
    v.vz = position.z;
    v.tt = uv.x * 255;
    v.ss = uv.y * 255;
    // v.nnx = normal.x * 127 + 128;
    // v.nny = normal.y * 127 + 128;
    // v.nnz = normal.z * 127 + 128;
    // v.br = color.x * 255;
    // v.bg = color.y * 255;
    // v.bb = color.z * 255;
    return v;
}

ChunkMesh::Face encode_face(vec3 color, BlockFace face) {
    ChunkMesh::Face f;
    f.r = color.x * 255;
    f.g = color.y * 255;
    f.b = color.z * 255;
    f.orientation = (int) face;
    return f;
}

template<typename T>
void upload(imr::Device& d, std::unique_ptr<imr::Buffer>& dst, const std::vector<T>& src, VkBufferUsageFlags flags) {
    size_t buffer_size = src.size() * sizeof(T);
    dst = std::make_unique<imr::Buffer>(d, buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dst->uploadDataSync(0, buffer_size, (void*) src.data());
}

ChunkMesh::ChunkMesh(imr::Device& d, ChunkNeighbors& n) {
    std::vector<uint32_t> idx_data;
    // std::vector<Vertex> vertex_data;
    std::vector<Face> face_data;

    ChunkNeighborsUnsafe unsafe {};
    for (size_t x = 0; x < 3; x++) {
        for (size_t z = 0; z < 3; z++) {
            unsafe.neighbours[x][z] = &n.neighbours[x][z].get()->data;
        }
    }

    num_verts = 0;
    std::function<add_face_fn_t> add_face = [&](ivec3 block_position, vec3 color, BlockFace face) {
        std::function<add_vertex_fn_t> add_vertex = [&](ivec3 position, vec2 uv){
            //vertex_data.push_back(encode_vertex(block_position + position, uv, face2normal(face), color));
            ivec3 p = block_position + position;
            assert(p.x >= 0);
            idx_data.push_back((p.x) | (p.z << 5) | (p.y << 10));
            num_verts += 1;
        };
        generate_face_vertices[(int)face](add_vertex);
        Face f = encode_face(color, face);
        face_data.push_back(f);
    };
    traverse_chunk_mesh(unsafe.neighbours[1][1], unsafe, add_face);

    //fprintf(stderr, "%zu vertices, totalling %zu KiB of data\n", num_verts, num_verts * sizeof(float) * 5 / 1024);
    //fflush(stderr);

    if (num_verts > 0) {
        // upload(d, vertices, vertex_data, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        upload(d, indices, idx_data, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        upload(d, faces, face_data, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }
}

struct FormattedBuffer {
    std::vector<uint8_t> data_;

    void append_bytes(size_t size, void* data) {
        size_t old_size = data_.size();
        data_.resize(data_.size() + size);
        memcpy(data_.data() + old_size, data, size);
    }

    template<typename T>
    void append(T t) {
        append_bytes(sizeof(T), &t);
    }

    template<typename T>
    void concatenate(const std::vector<T>& t) {
        append_bytes(sizeof(T) * t.size(), (void*) t.data());
    }
};

struct MeshletBuilder {
    size_t max_verts = 128, max_faces = 128;

    std::vector<uint32_t> vertices;
    std::unordered_map<uint32_t, uint8_t> vertices_map;

    std::vector<ChunkMeshlets::Face> faces;

    bool add_face(std::array<uint32_t, 4> face_vertices, vec3 color, BlockFace f) {
        if (faces.size() +1  > max_faces)
            return false;

        int space_needed = 0;
        for (auto v : face_vertices) {
            if (vertices_map.contains(v))
                continue;
            space_needed++;
        }

        if (vertices.size() + space_needed > max_verts) {
            return false;
        }

        std::array<uint8_t, 4> idx;
        for (int i = 0; i < 4; i++) {
            auto found = vertices_map.find(face_vertices[i]);
            if (found != vertices_map.end()) {
                idx[i] = found->second;
            } else {
                idx[i] = vertices.size();
                vertices.push_back(face_vertices[i]);
                vertices_map[face_vertices[i]] = idx[i];
            }
        }
        std::array<uint8_t, 4> color_and_face;
        color_and_face[0] = color.x * 255;
        color_and_face[1] = color.y * 255;
        color_and_face[2] = color.z * 255;
        color_and_face[3] = (uint8_t) f;
        faces.push_back(ChunkMeshlets::Face(idx, color_and_face));
        assert(vertices.size() <= max_verts);
        assert(faces.size() <= max_faces);
        return true;
    }

    std::vector<uint8_t> materialize() {
        FormattedBuffer output;

        output.append<uint16_t>(vertices.size());
        output.append<uint16_t>(faces.size());
        output.concatenate(vertices);
        output.concatenate(faces);
        return std::move(output.data_);
    }
};

uint32_t pack_vertex_position(uvec3 v) {
    return uint32_t(v.y << 10) | (v.z << 5) | (v.x);
}

uvec3 unpack_vertex_position(uint idx) {
    return uvec3(idx & 0x1F, (idx >> 10), (idx >> 5) & 0x1F);
}

ChunkMeshlets::ChunkMeshlets(imr::Device& d, ChunkNeighbors& n) {
    ChunkNeighborsUnsafe unsafe {};
    for (size_t x = 0; x < 3; x++) {
        for (size_t z = 0; z < 3; z++) {
            unsafe.neighbours[x][z] = &n.neighbours[x][z].get()->data;
        }
    }

    std::vector<MeshletBuilder> meshlets;
    std::function<add_face_fn_t> add_face = [&](ivec3 block_position, vec3 color, BlockFace face) {
        uvec3 v0 = uvec3(block_position.x, block_position.y, block_position.z);
        uint axis = blockface2axis(face);
        uint u = (axis + 1) % 3;
        uint v = (axis + 2) % 3;
        bool positive = ((uint) face) % 2 == 0;
        if (positive)
            v0[axis] += 1;
        uvec3 v1 = v0;
        v1[u] += 1;
        uvec3 v2 = v0;
        v2[v] += 1;
        uvec3 v3 = v1;
        v3[v] += 1;
        if (!positive)
            std::swap(v1, v2);
        std::array<uint32_t, 4> face_arr = { pack_vertex_position(v0), pack_vertex_position(v1), pack_vertex_position(v2), pack_vertex_position(v3) };
        while (meshlets.empty() || !meshlets.back().add_face(face_arr, color, face)) {
            meshlets.push_back({});
        }
    };
    traverse_chunk_mesh(unsafe.neighbours[1][1], unsafe, add_face);

    if (meshlets.empty())
        return;

    std::vector<std::vector<uint8_t>> materialized_meshlets;
    for (auto& meshlet : meshlets) {
        materialized_meshlets.push_back(meshlet.materialize());
    }

    FormattedBuffer output;
    output.append<uint32_t>(meshlets.size());
    size_t offset = output.data_.size() + materialized_meshlets.size() * sizeof(uint32_t);
    for (auto& mat : materialized_meshlets) {
        output.append<uint32_t>(offset);
        offset += mat.size();
    }
    for (auto& mat : materialized_meshlets) {
        output.concatenate(mat);
    }
    assert(output.data_.size() == offset);
    upload(d, buffer, output.data_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
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