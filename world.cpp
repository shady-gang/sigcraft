#include "world.h"

#include <cassert>
#include <time.h>

World::World(const char* filename) {
    allocator = enkl_get_malloc_free_allocator();
    enkl_world = cunk_open_mcworld(filename, &allocator);
}

World::~World() {
    for (auto& c : loaded_chunks()) {
        unload_chunk(c.get());
    }
    while (true) {
        auto locked = regions.lock();
        if (locked->size() == 0) {
            break;
        }
        fprintf(stderr, "Some chunks are still loaded, waiting...");
        timespec t {
        };
        t.tv_nsec = 1000;
        nanosleep(&t, nullptr);
    }
    cunk_close_mcworld(enkl_world);
}

std::vector<std::shared_ptr<Chunk>> World::loaded_chunks() {
    std::vector<std::shared_ptr<Chunk>> list;
    auto guard = held_chunks.lock();
    for (auto& handle : *guard) {
        auto chunk_guard = handle.second->handle.lock();
        if (*chunk_guard)
            list.push_back(*chunk_guard);
    }
    //auto guard = regions.lock();
    //for (auto& [_, region] : *guard) {
    //    assert(region);
    //    for (auto& [_, chunk] : region->chunks) {
    //        list.push_back(chunk);
    //    }
    //}
    return list;
}

template<typename Guard>
Region* World::get_loaded_region(Guard& guard, int rx, int rz) {
    if (auto found = guard->find({ rx, rz}); found != guard->end()) {
        return &*found->second;
    }
    return nullptr;
}

template<typename Guard>
Region* World::load_region(Guard& locked_regions, int rx, int rz) {
    assert(!get_loaded_region(locked_regions, rx, rz));
    Int2 pos = {rx, rz};
    auto& r = (*locked_regions)[pos] = std::make_unique<Region>(*this, rx, rz);
    assert(!r->loaded);
    assert(!r->unloaded);
    r->loaded = true;
    return &*r;
}

static int to_region_coordinate(int c) {
    if (c < 0)
        return (c - 31) / 32;
    return c / 32;
}

static std::tuple<int, int> to_region_coordinates(int cx, int cz) {
    int rx = to_region_coordinate(cx);
    int rz = to_region_coordinate(cz);
    return { rx, rz };
}

std::shared_ptr<Chunk> World::get_loaded_chunk(int cx, int cz) {
    Int2 pos = { cx, cz };
    auto held_chunks_guard = held_chunks.lock();
    auto found = held_chunks_guard->find(pos);
    if (found != held_chunks_guard->end()) {
        auto& handle = found->second;
        auto chunk_guard = handle->handle.lock();
        if (*chunk_guard)
            return *chunk_guard;
    }
    //auto [rx, rz] = to_region_coordinates(cx, cz);
    //auto guard = regions.lock();
    //auto found = get_loaded_region(guard, rx, rz);
    //if (found)
    //    return found->get_chunk(cx & 0x1f, cz & 0x1f);
    return nullptr;
}

void World::load_chunk(int cx, int cz) {
    auto [rx, rz] = to_region_coordinates(cx, cz);
    auto held_guard = held_chunks.lock_mut();
    if (held_guard->find(Int2(cx, cz)) == held_guard->end()) {
        auto handle = (*held_guard)[Int2(cx, cz)] = std::make_shared<ChunkHandle>();
        tp.schedule([=, this] {
            auto regions_guard = regions.lock_mut();
            Region* r = get_loaded_region(regions_guard, rx, rz);
            if (!r)
                r = load_region(regions_guard, rx, rz);
            *handle->handle.lock_mut() = std::make_shared<Chunk>(*r, cx, cz);
            changes.fetch_add(1);
        });
    }
}

void World::unload_chunk(Chunk* chunk) {
    Int2 pos = { chunk->cx, chunk->cz };
    auto held_guard = held_chunks.lock_mut();
    held_guard->erase(pos);
    changes.fetch_add(1);
}

template<typename Guard>
void World::unload_region(Guard guard, Region* region) {
    //printf("unloaded region %d %d\n", region->rx, region->rz);
    //assert(region->loaded);
    //assert(!region->unloaded);
    //region->unloaded = true;
    int rx = region->rx;
    int rz = region->rz;
    Int2 pos = {rx, rz};
    guard->erase(pos);
}

Region::Region(World& w, int rx, int rz) : world(w), rx(rx), rz(rz) {
    //printf("! %d %d\n", rx, rz);
    enkl_region = cunk_open_mcregion(w.enkl_world, rx, rz);
}

Region::~Region() {
    //printf("~ %d %d %zu\n", rx, rz, (size_t) enkl_region);
    assert(chunks.empty());
    //chunks.clear();
    if (enkl_region)
        enkl_close_region(enkl_region);
}

Chunk* Region::get_chunk(unsigned int rcx, unsigned int rcz) {
    assert(rcx >= 0 && rcz >= 0);
    assert(rcx < 32 && rcz < 32);
    auto found = chunks.find({(int) rcx, (int) rcz});
    if (found != chunks.end())
        return found->second;
    return nullptr;
}

Chunk::Chunk(Region& region, int cx, int cz) : region(region), cx(cx), cz(cz) {
    unsigned rcx = cx & 0x1f;
    unsigned rcz = cz & 0x1f;
    Int2 pos = {(int)rcx, (int)rcz};
    //assert(region.chunks[pos] = nullptr);
    region.chunks[pos] = this;
    //printf("! %d %d\n", cx, cz);
    if (region.enkl_region) {
        enkl_chunk = cunk_open_mcchunk(region.enkl_region, rcx, rcz);
        if (enkl_chunk) {
            load_from_mcchunk(&data, enkl_chunk);
        }
    }
    region.users++;
}

Chunk::~Chunk() {
    enkl_destroy_chunk_data(&data);
    if (enkl_chunk)
        enkl_close_chunk(enkl_chunk);

    World& w = region.world;
    auto guard = w.regions.lock_mut();

    unsigned rcx = cx & 0x1f;
    unsigned rcz = cz & 0x1f;
    Int2 pos = {(int)rcx, (int)rcz};
    region.chunks.erase(pos);

    //printf("~ %d %d users: %d\n", cx, cz, region.users);

    if (--region.users <= 0)
        w.unload_region(std::move(guard), &region);
}
