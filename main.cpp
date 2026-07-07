#include "imr/imr.h"
#include "imr/util.h"

#include "world.h"
#include "chunk_mesh.h"

#include <cmath>
#include "nasl/nasl.h"
#include "nasl/nasl_mat.h"

#include "camera.h"
#include "threadpool.h"

using namespace nasl;

struct {
    mat4 matrix;
    float time;
    ivec3 camera_chunk_pos;
    int debug;
    int visible_chunks_radius;
    uint64_t visible_chunks_array;
} push_constants;

Camera camera = {
    .position = {
        0, 128, 0,
    },
};
CameraFreelookState camera_state = {
    .fly_speed = 100.0f,
    .mouse_sensitivity = 1,
};
CameraInput camera_input;

void camera_update(GLFWwindow*, CameraInput* input);

bool reload_shaders = false;
bool wireframe = false;

struct Shaders {
    //std::vector<std::string> files = { "basic.vert.spv", "basic.frag.spv" };
    std::vector<std::string> files = { "voxel.task.spv", "voxel.mesh.spv", "voxel.frag.spv" };

    std::vector<std::unique_ptr<imr::ShaderModule>> modules;
    std::vector<std::unique_ptr<imr::ShaderEntryPoint>> entry_points;
    std::unique_ptr<imr::GraphicsPipeline> pipeline;

    Shaders(imr::Device& d, imr::Swapchain& swapchain) {
        imr::GraphicsPipeline::RenderTargetsState rts;
        rts.color.push_back((imr::GraphicsPipeline::RenderTarget) {
            .format = swapchain.format(),
            .blending = {
                .blendEnable = false,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            }
        });
        imr::GraphicsPipeline::RenderTarget depth = {
            .format = VK_FORMAT_D32_SFLOAT
        };
        rts.depth = depth;

        VkVertexInputBindingDescription bindings[] = {
            {
                .binding = 0,
                .stride = sizeof(ChunkMesh::Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
        };

        VkVertexInputAttributeDescription attributes[] = {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R16G16B16_SINT,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R8G8B8_SNORM,
                .offset = offsetof(ChunkMesh::Vertex, nnx),
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R8G8B8_UNORM,
                .offset = offsetof(ChunkMesh::Vertex, br),
            },
        };

        VkPipelineVertexInputStateCreateInfo vertex_input {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = sizeof(bindings) / sizeof(bindings[0]),
            .pVertexBindingDescriptions = &bindings[0],
            .vertexAttributeDescriptionCount = sizeof(attributes) / sizeof(attributes[0]),
            .pVertexAttributeDescriptions = &attributes[0],
        };

        VkPipelineRasterizationStateCreateInfo rasterization {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,

            .polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,

            .lineWidth = 1.0f,
        };

        imr::GraphicsPipeline::StateBuilder stateBuilder = {
            .vertexInputState = vertex_input,
            .inputAssemblyState = imr::GraphicsPipeline::simple_triangle_input_assembly(),
            .viewportState = imr::GraphicsPipeline::one_dynamically_sized_viewport(),
            .rasterizationState = rasterization,
            .multisampleState = imr::GraphicsPipeline::one_spp(),
            .depthStencilState = imr::GraphicsPipeline::simple_depth_testing(),
        };

        std::vector<imr::ShaderEntryPoint*> entry_point_ptrs;
        for (auto filename : files) {
            VkShaderStageFlagBits stage;
            if (filename.ends_with("vert.spv"))
                stage = VK_SHADER_STAGE_VERTEX_BIT;
            else if (filename.ends_with("frag.spv"))
                stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            else if (filename.ends_with("mesh.spv"))
                stage = VK_SHADER_STAGE_MESH_BIT_EXT;
            else if (filename.ends_with("task.spv"))
                stage = VK_SHADER_STAGE_TASK_BIT_EXT;
            else
                throw std::runtime_error("Unknown suffix");
            modules.push_back(std::make_unique<imr::ShaderModule>(d, std::move(filename)));
            entry_points.push_back(std::make_unique<imr::ShaderEntryPoint>(*modules.back(), stage, "main"));
            entry_point_ptrs.push_back(entry_points.back().get());
        }
        pipeline = std::make_unique<imr::GraphicsPipeline>(d, std::move(entry_point_ptrs), rts, stateBuilder);
    }
};

int radius = 16;

int main(int argc, char** argv) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1024, 1024, "Example", nullptr, nullptr);

    ThreadPool tp(std::thread::hardware_concurrency());

    if (argc < 2)
        return 0;

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL))
            reload_shaders = true;
        if (key == GLFW_KEY_PAGE_UP && action == GLFW_PRESS)
            radius++;
        if (key == GLFW_KEY_PAGE_DOWN && action == GLFW_PRESS)
            radius--;
        if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
            push_constants.debug = (push_constants.debug + 1) % 16;
        if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
            wireframe ^= true;
            reload_shaders = true;
        }
    });

    imr::Context context;
    imr::Device device(context, [&](vkb::PhysicalDeviceSelector& b) {
        b.add_required_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        b.add_required_extension(VK_KHR_SHADER_MAXIMAL_RECONVERGENCE_EXTENSION_NAME);
    });
    imr::Swapchain swapchain(device, window);
    imr::FpsCounter fps_counter;

    auto world = World(argv[1]);

    auto prev_frame = imr_get_time_nano();
    float delta = 0;

    camera = {{0, 0, 3}, {0, 0}, 60};

    std::unique_ptr<imr::Image> depthBuffer;

    auto shaders = std::make_unique<Shaders>(device, swapchain);

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        std::string s;
        s += "radius: ";
        s += std::to_string(radius);
        s += ", debug = ";
        s += std::to_string(push_constants.debug);
        fps_counter.updateGlfwWindowTitle(window, s);

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            if (reload_shaders) {
                swapchain.drain();
                shaders = std::make_unique<Shaders>(device, swapchain);
                reload_shaders = false;
            }

            auto& image = context.image();
            auto cmdbuf = context.cmdbuf();

            if (!depthBuffer || depthBuffer->size().width != context.image().size().width || depthBuffer->size().height != context.image().size().height) {
                VkImageUsageFlagBits depthBufferFlags = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
                depthBuffer = std::make_unique<imr::Image>(device, VK_IMAGE_TYPE_2D, context.image().size(), VK_FORMAT_D32_SFLOAT, depthBufferFlags);

                vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .dependencyFlags = 0,
                    .imageMemoryBarrierCount = 1,
                    .pImageMemoryBarriers = tmpPtr((VkImageMemoryBarrier2) {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = 0,
                        .srcAccessMask = 0,
                        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                        .image = depthBuffer->handle(),
                        .subresourceRange = depthBuffer->whole_image_subresource_range()
                    })
                }));
            }

            vk.cmdClearColorImage(cmdbuf, image.handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearColorValue) {
                .float32 = { 0.0f, 0.0f, 0.3f, 1.0f },
            }), 1, tmpPtr(image.whole_image_subresource_range()));

            vk.cmdClearDepthStencilImage(cmdbuf, depthBuffer->handle(), VK_IMAGE_LAYOUT_GENERAL, tmpPtr((VkClearDepthStencilValue) {
                .depth = 1.0f,
                .stencil = 0,
            }), 1, tmpPtr(depthBuffer->whole_image_subresource_range()));

            // This barrier ensures that the clear is finished before we run the dispatch.
            // before: all writes from the "transfer" stage (to which the clear command belongs)
            // after: all writes from the "compute" stage
            vk.cmdPipelineBarrier2KHR(cmdbuf, tmpPtr((VkDependencyInfo) {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .dependencyFlags = 0,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = tmpPtr((VkMemoryBarrier2) {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                })
            }));

            // update the push constant data on the host...
            mat4 m = identity_mat4;
            mat4 flip_y = identity_mat4;
            flip_y.rows[1][1] = -1;
            m = m * flip_y;
            mat4 view_mat = camera_get_view_mat4(&camera, context.image().size().width, context.image().size().height);
            m = m * view_mat;
            m = m * translate_mat4(vec3(-0.5, -0.5f, -0.5f));

            auto& pipeline = shaders->pipeline;
            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline());

            push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;

            context.frame().withRenderTargets(cmdbuf, { &image }, &*depthBuffer, [&]() {
                //for (auto pos : positions) {
                //    mat4 cube_matrix = m;
                //    cube_matrix = cube_matrix * translate_mat4(pos);

                //    push_constants_batched.matrix = cube_matrix;
                //    vkCmdPushConstants(cmdbuf, pipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants_batched), &push_constants_batched);
                //    vkCmdDraw(cmdbuf, 12 * 3, 1, 0, 0);
                //}

                push_constants.matrix = m;

                auto load_chunk = [&](int cx, int cz) {
                    auto loaded = world.get_loaded_chunk(cx, cz);
                    if (!loaded)
                        world.load_chunk(cx, cz);
                };

                int player_chunk_x = camera.position.x / 16;
                int player_chunk_y = camera.position.y / 16;
                int player_chunk_z = camera.position.z / 16;

                for (int dx = -radius; dx <= radius; dx++) {
                    for (int dz = -radius; dz <= radius; dz++) {
                        load_chunk(player_chunk_x + dx, player_chunk_z + dz);
                    }
                }

                int visible_chunks_array_size = radius * 2 + 1;
                std::vector<std::array<uint64_t, CUNK_CHUNK_SECTIONS_COUNT>> visible_chunks;
                visible_chunks.resize(visible_chunks_array_size * visible_chunks_array_size);

                for (auto chunk : world.loaded_chunks()) {
                    if (abs(chunk->cx - player_chunk_x) > radius || abs(chunk->cz - player_chunk_z) > radius) {
                        world.unload_chunk(chunk.get());
                        continue;
                    }

                    unsigned visible_chunks_array_coord_x = (chunk->cx - player_chunk_x) + radius;
                    unsigned visible_chunks_array_coord_z = (chunk->cz - player_chunk_z) + radius;
                    assert(visible_chunks_array_coord_x < visible_chunks_array_size);
                    assert(visible_chunks_array_coord_z < visible_chunks_array_size);
                    auto& visible_chunks_array_cell = visible_chunks[visible_chunks_array_coord_x * visible_chunks_array_size + visible_chunks_array_coord_z];

                    /*auto mesh_lock = chunk->mesh.lock_mut();
                    auto& mesh_container = *mesh_lock;
                    if (!mesh_container.mesh) {
                        if (mesh_container.task_spawned)
                            continue;

                        bool all_neighbours_loaded = true;
                        ChunkNeighbors n = {};
                        for (int dx = -1; dx < 2; dx++) {
                            for (int dz = -1; dz < 2; dz++) {
                                int nx = chunk->cx + dx;
                                int nz = chunk->cz + dz;

                                auto neighborChunk = world.get_loaded_chunk(nx, nz);
                                if (neighborChunk)
                                    n.neighbours[dx + 1][dz + 1] = neighborChunk;
                                else
                                    all_neighbours_loaded = false;
                            }
                        }
                        if (all_neighbours_loaded) {
                            mesh_container.task_spawned = true;
                            tp.schedule([n,&device,chunk = chunk]() {
                                auto nn = n;
                                auto mesh = std::make_shared<ChunkMesh>(device, nn);
                                auto mesh_lock = chunk->mesh.lock_mut();
                                mesh_lock->mesh = mesh;
                                mesh_lock->task_spawned = false;
                            });
                        }
                        continue;
                    }
                    auto mesh = mesh_container.mesh;
                    if (mesh->num_verts == 0)
                        continue;*/

                    auto data_lock = chunk->gpu_data.lock_mut();
                    auto& data_container = *data_lock;
                    if (!data_container.data) {
                        if (data_container.task_spawned)
                            continue;

                        data_container.task_spawned = true;
                        tp.schedule([&device, chunk]() {
                           auto data = std::make_shared<ChunkVoxelData>(device, chunk);
                           auto data_lock = chunk->gpu_data.lock_mut();
                           data_lock->data = data;
                           data_lock->task_spawned = false;
                        });
                        continue;
                    }
                    auto data = data_container.data;

                    for (int section = 0; section < CUNK_CHUNK_SECTIONS_COUNT; section++) {
                        if (!data->buf[section]) {
                            visible_chunks_array_cell[section] = 0;
                            continue;
                        }

                        visible_chunks_array_cell[section] = data->buf[section]->device_address();
                    }

                    context.frame().addCleanupAction([=, data = data]() {

                    });
                }

                auto visible_chunks_array_gpu = std::make_shared<imr::Buffer>(device, sizeof(uint64_t) * CUNK_CHUNK_SECTIONS_COUNT * visible_chunks_array_size * visible_chunks_array_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                visible_chunks_array_gpu->uploadDataSync(0, sizeof(uint64_t) * CUNK_CHUNK_SECTIONS_COUNT * visible_chunks_array_size * visible_chunks_array_size, visible_chunks.data());
                push_constants.camera_chunk_pos = { player_chunk_x, 0 /* the visible chunks array is always offset at Y=0 player_chunk_y*/, player_chunk_z };
                push_constants.visible_chunks_radius = radius;
                push_constants.visible_chunks_array = visible_chunks_array_gpu->device_address();
                vkCmdPushConstants(cmdbuf, pipeline->layout(), VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0, sizeof(push_constants), &push_constants);

                device.dispatch.cmdDrawMeshTasksEXT(cmdbuf, visible_chunks_array_size * 4, CUNK_CHUNK_SECTIONS_COUNT * 4, visible_chunks_array_size * 4);

                context.frame().addCleanupAction([=, visible_chunks_array_gpu = visible_chunks_array_gpu]() {

                });
            });

            auto now = imr_get_time_nano();
            delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
            prev_frame = now;

            glfwPollEvents();
        });
    }

    swapchain.drain();
    return 0;
}
