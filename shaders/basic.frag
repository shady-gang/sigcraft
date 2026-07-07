#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

// layout(location = 0)
// in vec3 color;
//
// layout(location = 1)
// in vec3 normal;

layout(location = 0)
out vec4 colorOut;

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

struct Face {
    uint8_t r, g, b, orientation;
};

layout(scalar, buffer_reference) buffer FacesRef {
    Face ref[];
};

layout(scalar, push_constant) uniform T {
    mat4 matrix;
    ivec3 chunk_position;
    float time;
    FacesRef faces;
} push_constants;

void main() {
    Face face = push_constants.faces.ref[gl_PrimitiveID / 2];
    vec3 color = vec3(face.r / 255.0, face.g / 255.0, face.b / 255.0);
    vec3 normal = face2normal(face.orientation);

    //colorOut = vec4(normal * 0.5 + vec3(0.5), 1.0);
    colorOut = vec4(color * 0.8 + 0.2 * dot(normal, normalize(vec3(1.0, 0.5, 0.1))), 1.0);
    //colorOut = vec4(vec3(fract(gl_PrimitiveID * 0.5231), fract(gl_PrimitiveID * 0.252102), fract(gl_PrimitiveID * 0.333)), 1.0);
}