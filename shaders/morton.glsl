#ifndef MORTON_H
#define MORTON_H

uint encode_morton(uint x, uint y, uint z) {
    uint acc = 0;
    for (uint bit = 0; bit < 4; bit++) {
        uint xb = (x >> bit) & 0x1u;
        uint yb = (y >> bit) & 0x1u;
        uint zb = (z >> bit) & 0x1u;
        acc |= (zb << 2 | yb << 1 | xb << 0) << (3 * bit);
    }
    return acc;
}

void decode_morton(uint xyz, out uint x, out uint y, out uint z) {
    x = 0;
    y = 0;
    z = 0;
    for (uint bit = 0; bit < 4; bit++) {
        uint xyzb = (xyz >> (3 * bit)) & 0x7;
        uint xb = xyzb & 0x1;
        uint yb = (xyzb >> 1) & 0x1;
        uint zb = (xyzb >> 2) & 0x1;
        x |= (xb << bit);
        y |= (yb << bit);
        z |= (zb << bit);
    }
}

#endif
