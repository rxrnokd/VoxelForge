#include "world/World.hpp"

#include "render/Mesher.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>

namespace voxelforge {

namespace {
constexpr std::size_t kMeshPoolLimit = 64;
} // namespace

World::World(std::uint64_t seed, unsigned workerThreads)
    : m_generator(seed),
      m_pool(std::make_unique<ThreadPool>(workerThreads != 0 ? workerThreads : ThreadPool::defaultThreadCount())) {}

World::~World() {
    // Stop the workers before any data they might touch is destroyed.
    m_pool.reset();
}

// ---------------------------------------------------------------------------------------
// Streaming
// ---------------------------------------------------------------------------------------

void World::rebuildOffsets(int renderDistance) {
    m_renderDistance = renderDistance;
    m_meshOffsets.clear();
    m_generateOffsets.clear();
    const int meshLimit = renderDistance * renderDistance + renderDistance; // ~ (r + 0.5)^2
    const int reach = renderDistance + 2;
    for (int dz = -reach; dz <= reach; ++dz) {
        for (int dx = -reach; dx <= reach; ++dx) {
            if (dx * dx + dz * dz <= meshLimit) {
                m_meshOffsets.push_back({dx, dz});
            }
            // Generate every chunk that neighbours a meshable chunk.
            bool needed = false;
            for (int nz = -1; nz <= 1 && !needed; ++nz) {
                for (int nx = -1; nx <= 1 && !needed; ++nx) {
                    const int mx = dx + nx;
                    const int mz = dz + nz;
                    needed = mx * mx + mz * mz <= meshLimit;
                }
            }
            if (needed) {
                m_generateOffsets.push_back({dx, dz});
            }
        }
    }
    auto byDistance = [](const Offset& a, const Offset& b) {
        return a.dx * a.dx + a.dz * a.dz < b.dx * b.dx + b.dz * b.dz;
    };
    std::stable_sort(m_meshOffsets.begin(), m_meshOffsets.end(), byDistance);
    std::stable_sort(m_generateOffsets.begin(), m_generateOffsets.end(), byDistance);
}

void World::update(const Vec3& focus, int renderDistance) {
    renderDistance = std::clamp(renderDistance, 1, 32);
    const ChunkPos center = ChunkPos::fromWorld(fastFloor(focus.x), fastFloor(focus.z));
    if (renderDistance != m_renderDistance) {
        rebuildOffsets(renderDistance);
        m_scanNeeded = true;
    }
    if (center != m_center) {
        m_center = center;
        m_scanNeeded = true;
    }

    collectResults();
    if (m_scanNeeded) {
        m_scanNeeded = false;
        unloadFarChunks();
        scheduleGeneration();
        scheduleMeshing();
    }
}

void World::collectResults() {
    GenerationResult gen;
    while (m_generationResults.tryPop(gen)) {
        --m_generationInFlight;
        m_scanNeeded = true;
        const auto it = m_chunks.find(gen.pos);
        if (it == m_chunks.end() || it->second.id != gen.id) {
            continue; // unloaded while generating
        }
        Entry& e = it->second;
        e.chunk = std::move(gen.chunk);
        e.generating = false;
        e.meshDirty = true;
    }

    MeshResult mesh;
    while (m_meshResults.tryPop(mesh)) {
        --m_meshingInFlight;
        m_scanNeeded = true;
        const auto it = m_chunks.find(mesh.pos);
        if (it == m_chunks.end() || it->second.id != mesh.id) {
            recycleMesh(std::move(mesh.mesh));
            continue;
        }
        Entry& e = it->second;
        e.meshing = false;
        e.hasMesh = true;
        m_readyMeshes.push_back(std::move(mesh));
    }
}

void World::unloadFarChunks() {
    const int limit = (m_renderDistance + 3) * (m_renderDistance + 3);
    for (auto it = m_chunks.begin(); it != m_chunks.end();) {
        const int dx = it->first.x - m_center.x;
        const int dz = it->first.z - m_center.z;
        if (dx * dx + dz * dz <= limit) {
            ++it;
            continue;
        }
        const Entry& e = it->second;
        if (e.modified && e.chunk) {
            m_storedModified[it->first] = e.chunk;
        }
        if (e.hasMesh || e.meshing) {
            m_unloaded.push_back(it->first);
        }
        it = m_chunks.erase(it);
    }
}

void World::scheduleGeneration() {
    const std::size_t limit = static_cast<std::size_t>(m_pool->threadCount()) * 3;
    for (const Offset& o : m_generateOffsets) {
        const ChunkPos pos{m_center.x + o.dx, m_center.z + o.dz};
        if (m_chunks.contains(pos)) {
            continue;
        }
        const auto stored = m_storedModified.find(pos);
        if (stored != m_storedModified.end()) {
            // Player-modified chunks come back from memory instead of the generator.
            Entry& e = m_chunks[pos];
            e.id = ++m_nextId;
            e.chunk = std::move(stored->second);
            e.modified = true;
            e.meshDirty = true;
            m_storedModified.erase(stored);
            continue;
        }
        if (m_generationInFlight >= limit) {
            m_scanNeeded = true; // more to do next frame
            break;
        }
        Entry& e = m_chunks[pos];
        e.id = ++m_nextId;
        e.generating = true;
        ++m_generationInFlight;
        m_pool->submit([this, pos, id = e.id] {
            auto chunk = std::make_shared<Chunk>();
            m_generator.generate(pos, *chunk);
            m_generationResults.push({pos, id, std::move(chunk)});
        });
    }
}

void World::scheduleMeshing() {
    const std::size_t limit = static_cast<std::size_t>(m_pool->threadCount()) * 2;
    for (const Offset& o : m_meshOffsets) {
        const ChunkPos pos{m_center.x + o.dx, m_center.z + o.dz};
        const auto it = m_chunks.find(pos);
        if (it == m_chunks.end()) {
            continue;
        }
        Entry& e = it->second;
        if (!e.chunk || e.meshing || !e.meshDirty) {
            continue;
        }
        if (m_meshingInFlight >= limit && !e.urgent) {
            m_scanNeeded = true;
            continue; // keep looking for urgent (edited) chunks
        }

        // Snapshot the 3x3 neighbourhood; all neighbours must have block data.
        std::array<std::shared_ptr<const Chunk>, 9> snapshot;
        bool complete = true;
        for (int dz = -1; dz <= 1 && complete; ++dz) {
            for (int dx = -1; dx <= 1 && complete; ++dx) {
                const Entry* n = findEntry({pos.x + dx, pos.z + dz});
                complete = n != nullptr && n->chunk != nullptr;
                if (complete) {
                    snapshot[static_cast<std::size_t>((dz + 1) * 3 + (dx + 1))] = n->chunk;
                }
            }
        }
        if (!complete) {
            continue;
        }

        const ThreadPool::Priority priority = e.urgent ? ThreadPool::Priority::High : ThreadPool::Priority::Normal;
        e.meshing = true;
        e.meshDirty = false;
        e.urgent = false;
        ++m_meshingInFlight;
        m_pool->submit(
            [this, pos, id = e.id, chunks = std::move(snapshot), mesh = acquireMesh()]() mutable {
                thread_local ChunkMesher mesher; // per worker scratch buffers
                ChunkNeighborhood neighborhood;
                for (std::size_t i = 0; i < chunks.size(); ++i) {
                    neighborhood.chunks[i] = chunks[i].get();
                }
                mesher.build(neighborhood, *mesh);
                m_meshResults.push({pos, id, std::move(mesh)});
            },
            priority);
    }
}

std::unique_ptr<ChunkMesh> World::acquireMesh() {
    if (m_meshPool.empty()) {
        return std::make_unique<ChunkMesh>();
    }
    std::unique_ptr<ChunkMesh> mesh = std::move(m_meshPool.back());
    m_meshPool.pop_back();
    return mesh;
}

void World::recycleMesh(std::unique_ptr<ChunkMesh> mesh) {
    if (mesh && m_meshPool.size() < kMeshPoolLimit) {
        mesh->clear();
        m_meshPool.push_back(std::move(mesh));
    }
}

bool World::popMeshUpdate(MeshUpdate& out) {
    while (!m_readyMeshes.empty()) {
        MeshResult ready = std::move(m_readyMeshes.front());
        m_readyMeshes.pop_front();
        const Entry* e = findEntry(ready.pos);
        if (e == nullptr || e->id != ready.id) { // unloaded (or reloaded) before upload
            recycleMesh(std::move(ready.mesh));
            continue;
        }
        out.pos = ready.pos;
        out.mesh = std::move(ready.mesh);
        return true;
    }
    return false;
}

bool World::popUnloadedChunk(ChunkPos& out) {
    if (m_unloaded.empty()) {
        return false;
    }
    out = m_unloaded.back();
    m_unloaded.pop_back();
    return true;
}

bool World::isAreaReady(ChunkPos center, int radius) const {
    const int limit = radius * radius + radius;
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dz * dz > limit) {
                continue;
            }
            const Entry* e = findEntry({center.x + dx, center.z + dz});
            if (e == nullptr || !e->hasMesh || e->meshing || e->meshDirty) {
                return false;
            }
        }
    }
    return true;
}

bool World::waitForArea(const Vec3& focus, int renderDistance, int radius, double timeoutSeconds) {
    const auto start = std::chrono::steady_clock::now();
    const ChunkPos center = ChunkPos::fromWorld(fastFloor(focus.x), fastFloor(focus.z));
    for (;;) {
        update(focus, renderDistance);
        if (isAreaReady(center, radius)) {
            return true;
        }
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeoutSeconds) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

WorldStats World::stats() const {
    WorldStats s;
    for (const auto& [pos, e] : m_chunks) {
        s.loadedChunks += e.chunk ? 1 : 0;
        s.meshedChunks += (e.hasMesh && !e.meshDirty) ? 1 : 0;
        s.modifiedChunks += e.modified ? 1 : 0;
    }
    s.modifiedChunks += m_storedModified.size();
    s.generatingChunks = m_generationInFlight;
    s.meshingChunks = m_meshingInFlight;
    s.queuedJobs = m_pool->queuedJobs();
    return s;
}

// ---------------------------------------------------------------------------------------
// Blocks
// ---------------------------------------------------------------------------------------

const World::Entry* World::findEntry(ChunkPos pos) const {
    const auto it = m_chunks.find(pos);
    return it != m_chunks.end() ? &it->second : nullptr;
}

bool World::isChunkLoaded(ChunkPos pos) const {
    const Entry* e = findEntry(pos);
    return e != nullptr && e->chunk != nullptr;
}

std::shared_ptr<const Chunk> World::chunkSnapshot(ChunkPos pos) const {
    const Entry* e = findEntry(pos);
    return e != nullptr ? e->chunk : nullptr;
}

BlockType World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= kChunkHeight) {
        return BlockType::Air;
    }
    const ChunkPos pos = ChunkPos::fromWorld(x, z);
    const Entry* e = findEntry(pos);
    if (e == nullptr || !e->chunk) {
        return BlockType::Air;
    }
    return e->chunk->get(x - pos.originX(), y, z - pos.originZ());
}

bool World::setBlock(int x, int y, int z, BlockType type) {
    if (y < 0 || y >= kChunkHeight) {
        return false;
    }
    const ChunkPos pos = ChunkPos::fromWorld(x, z);
    const auto it = m_chunks.find(pos);
    if (it == m_chunks.end() || !it->second.chunk) {
        return false;
    }
    Entry& e = it->second;
    const int lx = x - pos.originX();
    const int lz = z - pos.originZ();
    if (e.chunk->get(lx, y, lz) == type) {
        return false;
    }
    // Copy-on-write: running mesh jobs keep reading the old immutable snapshot.
    auto copy = std::make_shared<Chunk>(*e.chunk);
    copy->set(lx, y, lz, type);
    e.chunk = std::move(copy);
    e.modified = true;

    ChunkPos affected[4];
    const int count = chunksAffectedBy(x, z, affected);
    for (int i = 0; i < count; ++i) {
        const auto n = m_chunks.find(affected[i]);
        if (n != m_chunks.end()) {
            n->second.meshDirty = true;
            n->second.urgent = true;
        }
    }
    m_scanNeeded = true;
    return true;
}

int World::chunksAffectedBy(int x, int z, ChunkPos out[4]) {
    const ChunkPos pos = ChunkPos::fromWorld(x, z);
    const int lx = x - pos.originX();
    const int lz = z - pos.originZ();
    const int dx = lx == 0 ? -1 : (lx == kChunkSize - 1 ? 1 : 0);
    const int dz = lz == 0 ? -1 : (lz == kChunkSize - 1 ? 1 : 0);
    int n = 0;
    out[n++] = pos;
    if (dx != 0) out[n++] = {pos.x + dx, pos.z};
    if (dz != 0) out[n++] = {pos.x, pos.z + dz};
    if (dx != 0 && dz != 0) out[n++] = {pos.x + dx, pos.z + dz};
    return n;
}

// ---------------------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------------------

std::vector<std::pair<ChunkPos, std::shared_ptr<const Chunk>>> World::modifiedChunks() const {
    std::vector<std::pair<ChunkPos, std::shared_ptr<const Chunk>>> result;
    for (const auto& [pos, e] : m_chunks) {
        if (e.modified && e.chunk) {
            result.emplace_back(pos, e.chunk);
        }
    }
    for (const auto& [pos, chunk] : m_storedModified) {
        result.emplace_back(pos, chunk);
    }
    return result;
}

void World::addModifiedChunk(ChunkPos pos, std::shared_ptr<const Chunk> chunk) {
    const auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        // Already loaded: replace the generated content (a fresh id drops stale jobs).
        Entry& e = it->second;
        if (e.generating) {
            e.generating = false;
        }
        e.chunk = std::move(chunk);
        e.modified = true;
        e.meshDirty = true;
        e.id = ++m_nextId;
        e.meshing = false;
        m_scanNeeded = true;
        return;
    }
    m_storedModified[pos] = std::move(chunk);
}

Vec3 World::findSpawnPoint(int nearX, int nearZ) const {
    // Walk an outward square spiral until we find dry land.
    int x = nearX;
    int z = nearZ;
    int dx = 1;
    int dz = 0;
    int legLength = 1;
    int legProgress = 0;
    int turns = 0;
    for (int i = 0; i < 4096; ++i) {
        const int height = m_generator.surfaceHeight(x, z);
        if (height > kSeaLevel) {
            return {static_cast<float>(x) + 0.5f, static_cast<float>(height + 1), static_cast<float>(z) + 0.5f};
        }
        x += dx * 8;
        z += dz * 8;
        if (++legProgress == legLength) {
            legProgress = 0;
            const int t = dx;
            dx = -dz;
            dz = t;
            if (++turns % 2 == 0) {
                ++legLength;
            }
        }
    }
    return {static_cast<float>(nearX) + 0.5f, static_cast<float>(kSeaLevel + 1), static_cast<float>(nearZ) + 0.5f};
}

} // namespace voxelforge
