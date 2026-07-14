#ifndef VOXEL_RANGE_MESHLET_PRODUCER_H
#define VOXEL_RANGE_MESHLET_PRODUCER_H

shared int meshlets_counter;

int allocate_next_meshlet_idx() {
    int idx;
    if (gl_SubgroupInvocationID == 0) {
        idx = atomicAdd(meshlets_counter, 1);
    }
    idx = subgroupShuffle(idx, 0);
    return idx;
}

void scan_chunk_produce_meshlets() {
    meshlets_counter = 0;
    barrier();

    // subgroup-coherent
    int primitives_budget = 0;
    int vertices_budget = 0;

    int num_subgroups = int(gl_WorkGroupSize.x / gl_SubgroupSize);
    int subgroup_slice = 4096 / num_subgroups;

    //int subchunk_offset = int(64 * encode_morton(sub_chunk.x, sub_chunk.y, sub_chunk.z));

    int scan_range_start = 0 + int(gl_SubgroupID.x * subgroup_slice + 0);
    int scan_range_end = 0 + int(gl_SubgroupID.x * subgroup_slice + subgroup_slice);

    int first = scan_range_start;
    int last = -1;

    for (int scan_idx = scan_range_start; scan_idx < scan_range_end; scan_idx += int(gl_SubgroupSize)) {
        ivec3 block_pos;
        decode_block_idx(scan_idx + int(gl_SubgroupInvocationID), block_pos);

        uint8_t faces = uint8_t(0);
        if (is_solid_block(block_pos)) {
            if (!is_solid_block(block_pos + ivec3(0, 1, 0))) {
                faces |= uint8_t(0x1);
            }
            if (!is_solid_block(block_pos + ivec3(0, -1, 0))) {
                faces |= uint8_t(0x2);
            }
            if (!is_solid_block(block_pos + ivec3(0, 0, 1))) {
                faces |= uint8_t(0x4);
            }
            if (!is_solid_block(block_pos + ivec3(0, 0, -1))) {
                faces |= uint8_t(0x8);
            }
            if (!is_solid_block(block_pos + ivec3(1, 0, 0))) {
                faces |= uint8_t(0x10);
            }
            if (!is_solid_block(block_pos + ivec3(-1, 0, 0))) {
                faces |= uint8_t(0x20);
            }
        }

        VOXEL_RANGE_MESHLET_OUTPUT.faces[encode_chunkcoord(uvec3(block_pos), 0)] = faces;

        int primitives_count = bitCount(uint(faces)) * 2;
        int vertices_count = approx_unique_cube_vertices(faces);

        bool has_faces = faces != 0;

        if (subgroupAny(has_faces)) {
            for (int i = 0; i < 8; i++) {
                //if (num_meshlets > 100)
                //    break;

                int new_primitives_budget = primitives_budget + subgroupInclusiveAdd(primitives_count);
                int new_vertices_budget = vertices_budget + subgroupInclusiveAdd(vertices_count);

                bool over_budget = new_vertices_budget > MAX_VERTS || new_primitives_budget > MAX_PRIMS;// || ((first >= 0) && ((scan_idx + int(gl_SubgroupInvocationID)) - first) > 32);
                if (subgroupAny(over_budget)) {
                    // the last scan iteration was under-budget but there's nothing we can add from the new one, complete it
                    if (subgroupAll(over_budget)) {

                        //if (subgroupElect())
                        //    debugPrintfEXT("meshlet %d %d has %d, %d vtx/prims in it\n", num_meshlets, scan_idx, vertices_budget, primitives_budget);

                        int meshlet_idx = allocate_next_meshlet_idx();
                        VOXEL_RANGE_MESHLET_OUTPUT.meshlets[meshlet_idx] = MeshletPayload(uint16_t(first), uint16_t(scan_idx), uint16_t(vertices_budget), uint16_t(primitives_budget));
                        primitives_budget = 0;
                        vertices_budget = 0;

                        first = -1;
                    } else {
                        uint furthest_not_over_budget = subgroupBallotFindMSB(subgroupBallot(!over_budget));

                        if (gl_SubgroupInvocationID <= furthest_not_over_budget) {
                            primitives_count = 0;
                            vertices_count = 0;
                        }

                        //if (subgroupElect())
                        //    debugPrintfEXT("meshlet %d %d has %d, %d vtx/prims in it\n", num_meshlets, scan_idx + furthest_not_over_budget + 1, vertices_budget, primitives_budget);

                        int biggest_viable_primitive_budget = subgroupMax(over_budget ? 0 : new_primitives_budget);
                        int biggest_viable_vertices_budget = subgroupMax(over_budget ? 0 : new_vertices_budget);

                        if (first == -1) {
                            uint first_thread_with_faces = subgroupBallotFindLSB(subgroupBallot(has_faces));
                            first = int(scan_idx + first_thread_with_faces);
                        }

                        int meshlet_idx = allocate_next_meshlet_idx();
                        VOXEL_RANGE_MESHLET_OUTPUT.meshlets[meshlet_idx] = MeshletPayload(uint16_t(first), uint16_t(scan_idx + furthest_not_over_budget + 1), uint16_t(biggest_viable_vertices_budget), uint16_t(biggest_viable_primitive_budget));
                        primitives_budget = 0;
                        vertices_budget = 0;

                        first = int(scan_idx + furthest_not_over_budget + 1);
                    }
                } else {
                    if (first == -1) {
                        uint first_thread_with_faces = subgroupBallotFindLSB(subgroupBallot(has_faces));
                        first = int(scan_idx + first_thread_with_faces);
                    }
                    uint last_thread_with_faces = subgroupBallotFindMSB(subgroupBallot(has_faces));
                    last = int(scan_idx + last_thread_with_faces + 1);

                    primitives_budget += subgroupAdd(primitives_count);
                    vertices_budget += subgroupAdd(vertices_count);
                    break;
                }
            }
        }
    }

    if (primitives_budget > 0) {
        int meshlet_idx = allocate_next_meshlet_idx();
        VOXEL_RANGE_MESHLET_OUTPUT.meshlets[meshlet_idx] = MeshletPayload(uint16_t(first), uint16_t(last), uint16_t(vertices_budget), uint16_t(primitives_budget));
    }

    barrier();
    VOXEL_RANGE_MESHLET_OUTPUT.num_meshlets = meshlets_counter;
    return;
}

#endif