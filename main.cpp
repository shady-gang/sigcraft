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

int debug_mode = 0;

struct {
    mat4 matrix;
    float time;
    ivec3 camera_chunk_pos;
    int debug;
    int visible_chunks_radius;
    uint64_t visible_chunks_array;
    uint64_t scratch_buffer;
} ms_push_constants;

struct {
    mat4 matrix;
    Frustum frustum;
    ivec3 camera_chunk_pos;
    vec3 camera_pos;
    vec3 camera_dir;
    float time;
    int debug;
    int visible_chunks_radius;
    uint64_t visible_chunks_array;
} cpu_ms_push_constants;

struct {
    mat4 matrix;
    ivec3 camera_chunk_pos;
    float time;
    int debug;
    uint64_t vertex_positions;
    uint64_t face_data;
} vs_push_constants;

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
bool cache_meshlets = false;

float cpu_fps = 1;

enum Renderer {
    CPU_MESH,
    CPU_MESHLETS,
    GPU_MESHLETS,
    MAX_RENDERER = GPU_MESHLETS,
} renderer;

struct Shaders {
    std::vector<std::string> files;

    std::vector<std::unique_ptr<imr::ShaderModule>> modules;
    std::vector<std::unique_ptr<imr::ShaderEntryPoint>> entry_points;
    std::unique_ptr<imr::GraphicsPipeline> graphics_pipeline;
    std::unique_ptr<imr::ComputePipeline> compute_pipeline;

    Shaders(imr::Device& d, imr::Swapchain& swapchain, Renderer renderer) {
        switch (renderer) {
            case CPU_MESH:
                files = { "basic.vert.spv", "basic.frag.spv" };
                break;
            case CPU_MESHLETS:
                files = { "meshlet.task.spv", "meshlet.mesh.spv", "meshlet.frag.spv" };
                break;
            case GPU_MESHLETS:
                files = { "voxel.task.spv", "voxel.mesh.spv", "voxel.frag.spv" };
                break;
        }

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
            //{
            //    .location = 1,
            //    .binding = 0,
            //    .format = VK_FORMAT_R8G8B8_SNORM,
            //    .offset = offsetof(ChunkMesh::Vertex, nnx),
            //},
            //{
            //    .location = 2,
            //    .binding = 0,
            //    .format = VK_FORMAT_R8G8B8_UNORM,
            //    .offset = offsetof(ChunkMesh::Vertex, br),
            //},
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
        graphics_pipeline = std::make_unique<imr::GraphicsPipeline>(d, std::move(entry_point_ptrs), rts, stateBuilder);
        compute_pipeline = std::make_unique<imr::ComputePipeline>(d, "voxel.comp.spv");
    }
};

int radius = 64;
std::optional<ivec3> old_camera_chunk_pos;

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
        if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
            renderer = CPU_MESH;
            reload_shaders = true;
        }
        if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
            renderer = CPU_MESHLETS;
            reload_shaders = true;
        }
        if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
            renderer = GPU_MESHLETS;
            reload_shaders = true;
        }
        if (key == GLFW_KEY_M && action == GLFW_PRESS)
            debug_mode = (debug_mode + 1) % 16;
        if (key == GLFW_KEY_F5 && action == GLFW_PRESS) {
            wireframe ^= true;
            reload_shaders = true;
        }
        if (key == GLFW_KEY_N && action == GLFW_PRESS) {
            cache_meshlets ^= true;
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

    camera = {{0, 160, 0}, {0, 0}, 60};

    std::unique_ptr<imr::Image> depthBuffer;

    std::shared_ptr<imr::Buffer> scratchBuffer;
    bool meshlets_baked = false;

    auto shaders = std::make_unique<Shaders>(device, swapchain, renderer);

    std::vector<std::shared_ptr<Chunk>> renderable_chunks;

    struct {
        std::shared_ptr<imr::Buffer> visible_chunks_buffer;
        std::vector<std::shared_ptr<ChunkMeshlets>> used_meshlets;
    } cpu_meshlet_renderer;

    struct {
        std::vector<std::tuple<int, int, std::shared_ptr<ChunkMesh>>> used_meshes;
    } cpu_vs_renderer;

    auto& vk = device.dispatch;
    while (!glfwWindowShouldClose(window)) {
        fps_counter.tick();
        std::string s;
        s += "radius: ";
        s += std::to_string(radius);
        s += ", debug = ";
        s += std::to_string(debug_mode);
        s += ", cpu_fps = ";
        s += std::to_string(cpu_fps);
        fps_counter.updateGlfwWindowTitle(window, s);

        if (reload_shaders) {
            swapchain.drain();
            shaders = std::make_unique<Shaders>(device, swapchain, renderer);
            reload_shaders = false;
        }

        auto began_recording_cmds = imr_get_time_nano();

        swapchain.renderFrameSimplified([&](imr::Swapchain::SimplifiedRenderContext& context) {
            camera_update(window, &camera_input);
            camera_move_freelook(&camera, &camera_input, &camera_state, delta);

            auto& image = context.image();
            auto& cmdbuf = context.cmdbuf();

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
                        .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
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

            Frustum frustum;
            frustum.extractFromMatrix(m);

            int player_chunk_x = camera.position.x / 16;
            int player_chunk_y = camera.position.y / 16;
            int player_chunk_z = camera.position.z / 16;

            ivec3 camera_chunk_pos = ivec3(player_chunk_x, player_chunk_y, player_chunk_z);
            if (!old_camera_chunk_pos || ((old_camera_chunk_pos->x) != camera_chunk_pos.x) || ((old_camera_chunk_pos->z) != camera_chunk_pos.z)) {
                old_camera_chunk_pos = camera_chunk_pos;
                auto load_chunk = [&](int cx, int cz) {
                    auto loaded = world.get_loaded_chunk(cx, cz);
                    if (!loaded)
                        world.load_chunk(cx, cz);
                };

                for (int dx = -radius; dx <= radius; dx++) {
                    for (int dz = -radius; dz <= radius; dz++) {
                        load_chunk(player_chunk_x + dx, player_chunk_z + dz);
                    }
                }

                for (auto& chunk : renderable_chunks) {
                    if (abs(chunk->cx - player_chunk_x) > radius || abs(chunk->cz - player_chunk_z) > radius) {
                        world.unload_chunk(chunk.get());
                        continue;
                    }
                }
            }

            if (!cache_meshlets)
                renderable_chunks = world.loaded_chunks();

            switch (renderer) {
                case CPU_MESHLETS: {
                    int visible_chunks_array_size = radius * 2 + 1;

                    if (!cache_meshlets) {
                        cmdbuf.addCleanupAction([=, used_meshes = cpu_meshlet_renderer.used_meshlets]() {

                        });
                        cpu_meshlet_renderer.used_meshlets.clear();
                        std::vector<uint64_t> visible_chunks;
                        visible_chunks.resize(visible_chunks_array_size * visible_chunks_array_size);
                        memset(visible_chunks.data(), 0, sizeof(uint64_t) * visible_chunks.size());
                        for (auto& chunk: renderable_chunks) {
                            unsigned visible_chunks_array_coord_x = (chunk->cx - player_chunk_x) + radius;
                            unsigned visible_chunks_array_coord_z = (chunk->cz - player_chunk_z) + radius;
                            assert(visible_chunks_array_coord_x < visible_chunks_array_size);
                            assert(visible_chunks_array_coord_z < visible_chunks_array_size);
                            auto& visible_chunks_array_cell = visible_chunks[visible_chunks_array_coord_x * visible_chunks_array_size + visible_chunks_array_coord_z];

                            auto mesh_lock = chunk->meshlets.lock_mut();
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
                                    tp.schedule([n, &device, chunk = chunk]() {
                                        auto nn = n;
                                        auto mesh = std::make_shared<ChunkMeshlets>(device, nn);
                                        auto mesh_lock = chunk->meshlets.lock_mut();
                                        mesh_lock->mesh = mesh;
                                        mesh_lock->task_spawned = false;
                                    });
                                }
                                continue;
                            }
                            auto mesh = mesh_container.mesh;
                            if (!mesh->buffer)
                                continue;

                            visible_chunks_array_cell = mesh->buffer->device_address();

                            cpu_meshlet_renderer.used_meshlets.push_back(mesh);
                        }

                        cpu_meshlet_renderer.visible_chunks_buffer = std::make_shared<imr::Buffer>(device, sizeof(uint64_t) * visible_chunks_array_size * visible_chunks_array_size,
                                                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
                        cpu_meshlet_renderer.visible_chunks_buffer->uploadDataSync(0, sizeof(uint64_t) * visible_chunks_array_size * visible_chunks_array_size, visible_chunks.data());
                    }

                    cpu_ms_push_constants.camera_chunk_pos = { player_chunk_x, 0 /* the visible chunks array is always offset at Y=0 player_chunk_y*/, player_chunk_z };
                    cpu_ms_push_constants.visible_chunks_radius = radius;
                    cpu_ms_push_constants.visible_chunks_array = cpu_meshlet_renderer.visible_chunks_buffer->device_address();
                    cpu_ms_push_constants.debug = debug_mode;
                    cpu_ms_push_constants.matrix = m;
                    cpu_ms_push_constants.frustum = frustum;
                    cpu_ms_push_constants.camera_pos = camera.position;
                    cpu_ms_push_constants.camera_dir = camera_get_forward_vec(&camera);

                    context.frame().withRenderTargets(cmdbuf, { &image }, &*depthBuffer, [&]() {
                        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shaders->graphics_pipeline->pipeline());
                        vkCmdPushConstants(cmdbuf, shaders->graphics_pipeline->layout(), VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(cpu_ms_push_constants), &cpu_ms_push_constants);

                        device.dispatch.cmdDrawMeshTasksEXT(cmdbuf, visible_chunks_array_size, 1, visible_chunks_array_size);
                    });

                    cmdbuf.addCleanupAction([=, visible_chunks_array_gpu = cpu_meshlet_renderer.visible_chunks_buffer]() {

                    });
                    break;
                }
                case GPU_MESHLETS: {
                    int visible_chunks_array_size = radius * 2 + 1;
                    std::vector<std::array<uint64_t, CUNK_CHUNK_SECTIONS_COUNT>> visible_chunks;
                    visible_chunks.resize(visible_chunks_array_size * visible_chunks_array_size);
                    memset(visible_chunks.data(), 0, sizeof(uint64_t) * CUNK_CHUNK_SECTIONS_COUNT * visible_chunks.size());

                    for (auto& chunk : renderable_chunks) {
                        unsigned visible_chunks_array_coord_x = (chunk->cx - player_chunk_x) + radius;
                        unsigned visible_chunks_array_coord_z = (chunk->cz - player_chunk_z) + radius;
                        assert(visible_chunks_array_coord_x < visible_chunks_array_size);
                        assert(visible_chunks_array_coord_z < visible_chunks_array_size);
                        auto& visible_chunks_array_cell = visible_chunks[visible_chunks_array_coord_x * visible_chunks_array_size + visible_chunks_array_coord_z];

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

                        cmdbuf.addCleanupAction([=, data = data]() {

                        });
                    }

                    auto visible_chunks_array_gpu = std::make_shared<imr::Buffer>(device, sizeof(uint64_t) * CUNK_CHUNK_SECTIONS_COUNT * visible_chunks_array_size * visible_chunks_array_size,
                                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
                    visible_chunks_array_gpu->uploadDataSync(0, sizeof(uint64_t) * CUNK_CHUNK_SECTIONS_COUNT * visible_chunks_array_size * visible_chunks_array_size, visible_chunks.data());
                    ms_push_constants.matrix = m;
                    ms_push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
                    ms_push_constants.camera_chunk_pos = { player_chunk_x, 0 /* the visible chunks array is always offset at Y=0 player_chunk_y*/, player_chunk_z };
                    ms_push_constants.visible_chunks_radius = radius;
                    ms_push_constants.visible_chunks_array = visible_chunks_array_gpu->device_address();
                    ms_push_constants.debug = debug_mode;

                    size_t required_scratch_buffer_size = 135264 * visible_chunks_array_size * visible_chunks_array_size;
                    if (!scratchBuffer || scratchBuffer->size != required_scratch_buffer_size) {
                        if (scratchBuffer) {
                            // hold onto it till the frame is done
                            cmdbuf.addCleanupAction([=, scratchBuffer = scratchBuffer]() {});
                            scratchBuffer = nullptr;
                        }
                        scratchBuffer = std::make_shared<imr::Buffer>(device, required_scratch_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                        meshlets_baked = false;
                    }
                    ms_push_constants.scratch_buffer = scratchBuffer->device_address();

                    if (!cache_meshlets || !meshlets_baked) {
                        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, shaders->compute_pipeline->pipeline());
                        vkCmdPushConstants(cmdbuf, shaders->compute_pipeline->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ms_push_constants), &ms_push_constants);
                        vkCmdDispatch(cmdbuf, visible_chunks_array_size, CUNK_CHUNK_SECTIONS_COUNT, visible_chunks_array_size);
                        device.dispatch.cmdPipelineBarrier2(cmdbuf, tmpPtr<VkDependencyInfo>({
                            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                            .memoryBarrierCount = 1,
                            .pMemoryBarriers = tmpPtr<VkMemoryBarrier2>({
                                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                .srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                .dstStageMask = VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT,
                                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                            }),
                        }));
                    }
                    meshlets_baked = true;

                    context.frame().withRenderTargets(cmdbuf, { &image }, &*depthBuffer, [&]() {
                        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shaders->graphics_pipeline->pipeline());
                        vkCmdPushConstants(cmdbuf, shaders->graphics_pipeline->layout(), VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0, sizeof(ms_push_constants), &ms_push_constants);

                        device.dispatch.cmdDrawMeshTasksEXT(cmdbuf, visible_chunks_array_size, CUNK_CHUNK_SECTIONS_COUNT, visible_chunks_array_size);
                    });

                    cmdbuf.addCleanupAction([=, visible_chunks_array_gpu = visible_chunks_array_gpu]() {

                    });
                    break;
                }
                case CPU_MESH: {
                    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shaders->graphics_pipeline->pipeline());
                    context.frame().withRenderTargets(cmdbuf, { &image }, &*depthBuffer, [&]() {
                        if (!cache_meshlets) {
                            cmdbuf.addCleanupAction([=, used_meshes = std::move(cpu_vs_renderer.used_meshes)]() {

                            });
                            cpu_vs_renderer.used_meshes.clear();

                            for (auto& chunk : renderable_chunks) {
                                auto mesh_lock = chunk->mesh.lock_mut();
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
                                    continue;

                                vec3 min = { chunk->cx * 16.0f, 0, chunk->cz * 16.0f };
                                vec3 max = min + vec3(16, 384, 16);

                                if (!frustum.isAABBInsideFrustum(min, max))
                                    continue;

                                cpu_vs_renderer.used_meshes.push_back({chunk->cx, chunk->cz, mesh});
                            }
                        }

                        vs_push_constants.debug = debug_mode;
                        vs_push_constants.matrix = m;
                        vs_push_constants.time = ((imr_get_time_nano() / 1000) % 10000000000) / 1000000.0f;
                        for (auto& [cx, cz, mesh] : cpu_vs_renderer.used_meshes) {
                            vs_push_constants.camera_chunk_pos = { cx, 0, cz };
                            vs_push_constants.vertex_positions = debug_mode == 2 ? mesh->vertices_as_indices->device_address() : mesh->vertices->device_address();
                            vs_push_constants.face_data = mesh->faces->device_address();
                            vkCmdPushConstants(cmdbuf, shaders->graphics_pipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vs_push_constants), &vs_push_constants);
                            vkCmdBindIndexBuffer(cmdbuf, debug_mode == 1 ? mesh->vertices_as_indices->handle : mesh->indices->handle, 0, VK_INDEX_TYPE_UINT32);
                            //VkBuffer vb = mesh->vertices->handle;
                            //VkDeviceSize offset = 0;
                            //vkCmdBindVertexBuffers(cmdbuf, 0, 1, &vb, &offset);
                            if (debug_mode == 2)
                                device.dispatch.cmdDraw(cmdbuf, mesh->num_verts, 1, 0, 0);
                            else
                                device.dispatch.cmdDrawIndexed(cmdbuf, mesh->num_verts, 1, 0, 0, 0);
                        }
                    });
                }
                break;
            }

            auto now = imr_get_time_nano();
            delta = ((float) ((now - prev_frame) / 1000L)) / 1000000.0f;
            prev_frame = now;

            auto cpu_overhead_us = ((now - began_recording_cmds) / 1000L);
            if (context.frame().id % 100 == 0)
                cpu_fps = 1000000.0f / cpu_overhead_us;

            glfwPollEvents();
        });
    }

    swapchain.drain();
    return 0;
}
