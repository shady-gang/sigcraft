#ifndef VOXEL_MESH_H
#define VOXEL_MESH_H

const int face_mask_to_vertex_mask_arr[64] = {
0, 240, 15, 255, 170, 250, 175, 255, 85, 245, 95, 255, 255, 255, 255, 255, 204,
252, 207, 255, 238, 254, 239, 255, 221, 253, 223, 255, 255, 255, 255, 255, 51,
243, 63, 255, 187, 251, 191, 255, 119, 247, 127, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

uint8_t face_mask_to_vertex_mask(int faces) {
    /*9int bit_0 = 0x1;
    int bit_1 = 0x2;
    int bit_2 = 0x4;
    int bit_3 = 0x8;
    int bit_4 = 0x10;
    int bit_5 = 0x20;
    int bit_6 = 0x40;
    int bit_7 = 0x80;
    int result = 0;
    if ((faces & 0x1) != 0) {
        result |= bit_4 | bit_5 | bit_6 | bit_7;
    }
    if ((faces & 0x2) != 0) {
        result |= bit_0 | bit_1 | bit_2 | bit_3;
    }
    if ((faces & 0x4) != 0) {
        result |= bit_1 | bit_3 | bit_5 | bit_7;
    }
    if ((faces & 0x8) != 0) {
        result |= bit_0 | bit_2 | bit_4 | bit_6;
    }
    if ((faces & 0x10) != 0) {
        result |= bit_2 | bit_3 | bit_6 | bit_7;
    }
    if ((faces & 0x20) != 0) {
        result |= bit_0 | bit_1 | bit_4 | bit_5;
    }
    return uint8_t(result);*/
    return uint8_t(face_mask_to_vertex_mask_arr[faces]);
}

int approx_unique_cube_vertices(uint8_t faces_mask) {
    return bitCount(face_mask_to_vertex_mask(int(faces_mask)));
}

ivec3 face2normal(uint face) {
    switch (face) {
        case 0: return ivec3(0, 1, 0);
        case 1: return ivec3(0, -1, 0);
        case 2: return ivec3(0, 0, 1);
        case 3: return ivec3(0, 0, -1);
        case 4: return ivec3(1, 0, 0);
        case 5: return ivec3(-1, 0, 0);
    }
    return ivec3(0);
}

const vec3 vertex_offsets[8] = {
    vec3(0, 0, 0),
    vec3(0, 0, 1),
    vec3(1, 0, 0),
    vec3(1, 0, 1),
    vec3(0, 1, 0),
    vec3(0, 1, 1),
    vec3(1, 1, 0),
    vec3(1, 1, 1),
};

#endif
