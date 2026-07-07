#ifndef SIGCRAFT_WORLD_H
#define SIGCRAFT_WORLD_H

extern "C" {

#include "enklume/block_data.h"
#include "enklume/enklume.h"

}

struct ChunkMesh;
struct ChunkVoxelData;

#include "rustex.h"
#include "threadpool.h"

#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>

struct Int2 {
    int32_t x, z;
    bool operator==(const Int2 &other) const {
        return x == other.x && other.z == z;
    };
};

template <>
struct std::hash<Int2> {
    std::size_t operator()(const Int2& k) const {
        using std::size_t;
        using std::hash;
        using std::string;
        return (hash<uint32_t>()(k.x) ^ (hash<uint32_t>()(k.z) << 1));
    }
};

struct World;
struct Region;

template<typename T>
using Mutex = rustex::mutex<T>;

struct Chunk {
    Region& region;
    int cx, cz;
    McChunk* enkl_chunk = nullptr;
    ChunkData data = {};
    struct MeshContainer {
        std::shared_ptr<ChunkMesh> mesh;
        bool task_spawned = false;
    };
    Mutex<MeshContainer> mesh;
    struct DataContainer {
        std::shared_ptr<ChunkVoxelData> data;
        bool task_spawned = false;
    };
    Mutex<DataContainer> gpu_data;

    Chunk(Region&, int x, int z);
    Chunk(const Chunk&) = delete;
    ~Chunk();
};

struct Region {
    World& world;
    int rx, rz;
    McRegion* enkl_region = nullptr;
    bool loaded = false;
    bool unloaded = false;
    std::unordered_map<Int2, Chunk*> chunks;

    Region(World&, int rx, int rz);
    Region(const Region&) = delete;
    ~Region();

protected:
    int users = 0;

    Chunk* get_chunk(unsigned rcx, unsigned rcz);
    friend World;
    friend Chunk;
};

struct World {
    Enkl_Allocator allocator;
    McWorld* enkl_world;
    Mutex<std::unordered_map<Int2, std::unique_ptr<Region>>> regions;

    struct ChunkHandle {
        Mutex<std::shared_ptr<Chunk>> handle;
    };
    Mutex<std::unordered_map<Int2, std::shared_ptr<ChunkHandle>>> held_chunks;

    explicit World(const char*);
    World(const World&) = delete;
    ~World();

    void load_chunk(int chunk_x, int chunk_z);
    void unload_chunk(Chunk*);
    std::shared_ptr<Chunk> get_loaded_chunk(int x, int z);
    std::vector<std::shared_ptr<Chunk>> loaded_chunks();
private:
    ThreadPool tp { 1 };

    template<typename Guard>
    Region* get_loaded_region(Guard&, int rx, int rz);
    template<typename Guard>
    Region* load_region(Guard&, int rx, int rz);
    template<typename Guard>
    void unload_region(Guard, Region*);

    friend Region;
    friend Chunk;
};

//WorldChunk world[WORLD_SIZE][WORLD_SIZE];

#endif
