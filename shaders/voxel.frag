#version 450
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

// layout(location = 0)
// perprimitiveEXT in vec3 color;
//
// layout(location = 1)
// perprimitiveEXT in vec3 normal;

layout(location = 0)
out vec4 colorOut;

void main() {
    //colorOut = vec4(normal * 0.5 + vec3(0.5), 1.0);
    //colorOut = vec4(color * 0.8 + 0.2 * dot(normal, normalize(vec3(1.0, 0.5, 0.1))), 1.0);
    colorOut = vec4(vec3(fract(gl_PrimitiveID * 0.5231), fract(gl_PrimitiveID * 0.252102), fract(gl_PrimitiveID * 0.333)), 1.0);
}