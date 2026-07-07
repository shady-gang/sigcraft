cmake_minimum_required(VERSION 3.28)
project(sigcraft)

set(CMAKE_CXX_STANDARD 20)

set(IMR_FOLDER "" CACHE STRING "Folder where IMR is found")
add_subdirectory(${IMR_FOLDER} imr_build)

if (NOT TARGET imr)
    message("IMR not found. Please set IMR_FOLDER accordingly.")
endif ()

add_subdirectory(enklume)

find_package(nasl)

add_executable(sigcraft main.cpp camera.cpp chunk_mesh.cpp world.cpp)
target_link_libraries(sigcraft imr enklume nasl::nasl)

target_include_directories(sigcraft PUBLIC "thirdparty/rustex/")

add_custom_target(basic_vert_spv COMMAND ${GLSLANG_EXE} -g -gV -V -S vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/basic.vert -o ${CMAKE_CURRENT_BINARY_DIR}/basic.vert.spv)
add_dependencies(sigcraft basic_vert_spv)
add_custom_target(basic_frag_spv COMMAND ${GLSLANG_EXE} -g -gV -V -S frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/basic.frag -o ${CMAKE_CURRENT_BINARY_DIR}/basic.frag.spv --target-env spirv1.6)
add_dependencies(sigcraft basic_frag_spv)

add_custom_target(voxel_frag_spv COMMAND ${GLSLANG_EXE} -g -gV -V -S frag ${CMAKE_CURRENT_SOURCE_DIR}/shaders/voxel.frag -o ${CMAKE_CURRENT_BINARY_DIR}/voxel.frag.spv --target-env spirv1.6 ${CMAKE_CURRENT_SOURCE_DIR}/shaders/limits.conf)
add_dependencies(sigcraft voxel_frag_spv)
add_custom_target(voxel_mesh_spv COMMAND ${GLSLANG_EXE} -g -gV -V -S mesh ${CMAKE_CURRENT_SOURCE_DIR}/shaders/voxel.mesh -o ${CMAKE_CURRENT_BINARY_DIR}/voxel.mesh.spv --target-env spirv1.6 ${CMAKE_CURRENT_SOURCE_DIR}/shaders/limits.conf)
add_dependencies(sigcraft voxel_mesh_spv)
add_custom_target(voxel_task_spv COMMAND ${GLSLANG_EXE} -g -gV -V -S task ${CMAKE_CURRENT_SOURCE_DIR}/shaders/voxel.task -o ${CMAKE_CURRENT_BINARY_DIR}/voxel.task.spv --target-env spirv1.6 ${CMAKE_CURRENT_SOURCE_DIR}/shaders/limits.conf)
add_dependencies(sigcraft voxel_task_spv)
