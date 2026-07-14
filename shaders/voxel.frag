#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "frustum.glsl"
#include "chunk_data.glsl"
//#include "neighboring_chunks.glsl"
#include "voxel_range_meshlet.glsl"
#include "voxel_mesh.glsl"
#include "heatmap.glsl"

// layout(location = 0)
// perprimitiveEXT in vec3 color;
//
// layout(location = 1)
// perprimitiveEXT in vec3 normal;

layout(scalar, push_constant) uniform T {
    mat4 matrix;
    Frustum frustum;
    ivec3 camera_chunk_pos;
    vec3 camera_pos;
    vec3 camera_dir;
    float time;
    int debug;
    int visible_chunk_radius;
    uint64_t visible_chunks;
    uint64_t tmp;
} push_constants;

layout(location = 0)
out vec4 colorOut;

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