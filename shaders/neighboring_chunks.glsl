#ifndef NEIGHBORING_CHUNKS_H
#define NEIGHBORING_CHUNKS_H

struct ChunkSections {
    CubicChunkRef sections[24];
};

layout(scalar, buffer_reference) buffer VisibleChunksArrayRef {
    ChunkSections ref[];
};

uint64_t neighboring_chunks_2[27];

int encode_neighboring_chunk_idx(int dx, int dy, int dz) {
    return ((dx + 1) * 3 + (dy + 1)) * 3 + (dz + 1);
}

void decode_neighboring_chunk_idx(int idx, out int dx, out int dy, out int dz) {
    dz = (idx % 3) - 1;
    dy = ((idx / 3) % 3) - 1;
    dx = ((idx / 3) / 3) - 1;
}

uint safe_access(int x, int y, int z) {
    int ox = 0, oy = 0, oz = 0;
    if (x >= 16) {
        ox = 1;
        x -= 16;
    }
    if (y >= 16) {
        oy = 1;
        y -= 16;
    }
    if (z >= 16) {
        oz = 1;
        z -= 16;
    }
    if (x < 0) {
        ox = -1;
        x += 16;
    }
    if (y < 0) {
        oy = -1;
        y += 16;
    }
    if (z < 0) {
        oz = -1;
        z += 16;
    }

    if (x >= 0 && x < CUNK_CHUNK_SIZE && y >= 0 && y < CUNK_CHUNK_SIZE && z >= 0 && z < CUNK_CHUNK_SIZE) {
        CubicChunkRef voxel_data = CubicChunkRef(neighboring_chunks_2[encode_neighboring_chunk_idx(ox, oy, oz)]);
        return lod_access(voxel_data, uvec3(x, y, z));
    }
    return 0;
}

uint safe_access(ivec3 v) {
    return safe_access(v.x, v.y, v.z);
}

bool is_solid_block(ivec3 pos) {
    return safe_access(pos) != 0;
}

uint load_neighboring_chunks(ivec3 chunk_position, ivec3 camera_chunk_pos, int visible_chunk_radius, VisibleChunksArrayRef visible_chunks) {
    int visible_chunks_array_size = visible_chunk_radius * 2 + 1;

    int occluded_direct_neighbors = 0;
#define PARALLEL_NEIGHBORS_GATHER 0
#if PARALLEL_NEIGHBORS_GATHER
    int idx = int(gl_SubgroupInvocationID);
    if (idx < 27) {
        int dx, dy, dz;
        decode_neighboring_chunk_idx(idx, dx, dy, dz);
#else
    for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dz = -1; dz <= 1; dz++) {
        int idx = encode_neighboring_chunk_idx(dx, dy, dz);
#endif
        ivec3 neighboring_chunk_position = chunk_position + ivec3(dx, dy, dz);
        if (neighboring_chunk_position.y < 0 || neighboring_chunk_position.y >= 24) {
            neighboring_chunks_2[idx] = 0;
        } else {
            ivec3 delta = neighboring_chunk_position - camera_chunk_pos;
            if (abs(delta.x) > visible_chunk_radius || abs(delta.z) > visible_chunk_radius) {
                neighboring_chunks_2[idx] = 0;
            } else {
                uint visible_chunks_array_neighbor_x = delta.x + visible_chunk_radius;
                uint visible_chunks_array_neighbor_z = delta.z + visible_chunk_radius;
                uint visible_chunks_array_neighbor_idx = visible_chunks_array_neighbor_x * visible_chunks_array_size + visible_chunks_array_neighbor_z;

                CubicChunkRef chunk = visible_chunks.ref[visible_chunks_array_neighbor_idx].sections[chunk_position.y + dy];
                neighboring_chunks_2[idx] = uint64_t(chunk);

                bool is_direct_neighbor = abs(dx) + abs(dy) + abs(dz) <= 1;
                if (is_direct_neighbor) {
                    if (uint64_t(chunk) != 0 && is_chunk_solid(chunk))
                    occluded_direct_neighbors += 1;
                }
            }
        }
    }

#if PARALLEL_NEIGHBORS_GATHER
    occluded_direct_neighbors = subgroupAdd(occluded_direct_neighbors);
#endif

    return occluded_direct_neighbors;
}

#endif
