#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// layout(location = 0)
// in ivec3 vertexIn;

// layout(location = 1)
// in vec3 normalIn;
//
// layout(location = 2)
// in vec3 colorIn;
//
// layout(location = 0)
// out vec3 color;
//
// layout(location = 1)
// out vec3 normal;

layout(scalar, buffer_reference) buffer VertexPosRef {
    uint ref[];
};

layout(scalar, push_constant) uniform T {
    mat4 matrix;
    ivec3 chunk_position;
    float time;
    int debug;
    VertexPosRef vertex_positions;
    uint64_t paddd;
} push_constants;

void main() {
    mat4 matrix = push_constants.matrix;
    uint vertex_idx = uint(gl_VertexIndex);
    uint vertex_pos;
    if (push_constants.debug == 1) {
        vertex_pos = vertex_idx;
    } else {
        vertex_pos = push_constants.vertex_positions.ref[vertex_idx];
    }
    uvec3 vertex = uvec3(vertex_pos & 0x1Fu, (vertex_pos >> 10), (vertex_pos >> 5) & 0x1Fu);
    gl_Position = matrix * vec4(vec3(ivec3(vertex) + push_constants.chunk_position * 16), 1.0);
    // int primid = gl_VertexIndex / 6;
    // color = colorIn;
    // normal = normalIn;
}