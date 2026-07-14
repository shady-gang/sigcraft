#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

// layout(location = 0)
// perprimitiveEXT in vec3 color;
//
// layout(location = 1)
// perprimitiveEXT in vec3 normal;

layout(scalar, buffer_reference) buffer VoxelDataRef {
    uint data[];
};

struct ChunkSections {
    VoxelDataRef sections[24];
};

layout(scalar, buffer_reference) buffer VisibleChunksArrayRef {
    ChunkSections visible[];
};

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

struct Plane {
    vec3 normal;
    float distance;
};

struct Frustum {
    Plane planes[6];
};

layout(scalar, push_constant) uniform T {
    mat4 matrix;
    Frustum frustum;
    ivec3 camera_chunk_pos;
    vec3 camera_pos;
    vec3 camera_dir;
    float time;
    int debug;
    int visible_chunk_radius;
    VisibleChunksArrayRef visible_chunks;
    ChunkTmpRef tmp;
} push_constants;

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

layout(location = 0)
out vec4 colorOut;

vec3 heatmap2(float lod) {
    if (lod < 2)
    return vec3(0.0, 0.0, 1.0);
    if (lod < 4)
    return vec3(0.0, 1.0, 1.0);
    if (lod < 8)
    return vec3(1.0, 1.0, 0.0);
    if (lod < 16)
    return vec3(1.0, 0.75, 0.0);
    return vec3(1.0, 0.0, 0.0);
}

float heat_temps[5] = {
    1, 2, 4, 8, 16
};

vec3 heat_colors[6] = {
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 1.0, 1.0),
    vec3(1.0, 1.0, 0.0),
    vec3(1.0, 0.75, 0.0),
    vec3(1.0, 0.5, 0.0),
    vec3(1.0, 0.0, 0.0)
};

vec3 heatmap(float lod) {
    float prev_temp = 0;
    for (int i = 0; i < 5; i++) {
        float range = heat_temps[i] - prev_temp;
        if (lod < heat_temps[i]) {
            float factor = (lod - prev_temp) / range;
            return mix(heat_colors[i], heat_colors[i + 1], factor);
        }
        prev_temp = heat_temps[i];
    }
    return heat_colors[5];
}

void main() {
    uint meshlet_idx = gl_PrimitiveID >> 20;
    uint face = gl_PrimitiveID & 0x7;
    vec3 normal = vec3(face2normal(face));
    vec3 color = vec3(fract(meshlet_idx * 0.5231), fract(meshlet_idx * 0.252102), fract(meshlet_idx * 0.333));

    if (push_constants.debug == 1) {
        color = heatmap((gl_PrimitiveID >> 3) / 32.0);
    }

    if (push_constants.debug == 2 || push_constants.debug == 3) {
        color = heatmap((gl_PrimitiveID >> 3) / 16.0);
    }

    //colorOut = vec4(normal * 0.5 + vec3(0.5), 1.0);
    colorOut = vec4(color * 0.8 + 0.2 * dot(normal, normalize(vec3(1.0, 0.5, 0.1))), 1.0);
    //colorOut = vec4(vec3(fract(gl_PrimitiveID * 0.5231), fract(gl_PrimitiveID * 0.252102), fract(gl_PrimitiveID * 0.333)), 1.0);
}