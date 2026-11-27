#pragma once

#include "physics/AABB.hpp"
#include "render/ChunkMesh.hpp"
#include "threading/ConcurrentQueue.hpp"
#include "threading/ThreadPool.hpp"
#include "world/Chunk.hpp"
#include "world/TerrainGenerator.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace voxelforge {

// A finished chunk mesh waiting to be uploaded to the GPU by the main thread.
struct MeshUpdate {
    ChunkPos pos;
    std::unique_ptr<ChunkMesh> mesh;
};

struct WorldStats {
    std::size_t loadedChunks = 0;     // chunks with block data
    std::size_t generatingChunks = 0; // generation jobs in flight
    std::size_t meshingChunks = 0;    // meshing jobs in flight
    std::size_t meshedChunks = 0;     // chunks with an up to date mesh
    std::size_t modifiedChunks = 0;   // edited by the player (loaded + unloaded)
    std::size_t queuedJobs = 0;       // jobs waiting in the thread pool
};

// The voxel world: a streaming set of chunks around a focus point.
//
// Threading model: all World methods are called from the main thread. Generation and
// meshing run on a worker pool. Chunk block data is immutable once published
// (std::shared_ptr<const Chunk>); editing a block replaces the chunk with a modified copy
// (copy-on-write). Workers therefore only ever read immutable snapshots, and results
// travel back through mutex protected queues, so there is no shared mutable state.
class World : public VoxelQuery {
public:
    explicit World(std::uint64_t seed, unsigned workerThreads = 0);
    ~World() override;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    std::uint64_t seed() const { return m_generator.seed(); }
    const TerrainGenerator& generator() const { return m_generator; }

    // --- Streaming -------------------------------------------------------------------
    // Applies finished jobs, unloads distant chunks and schedules generation/meshing
    // around `focus`, nearest chunks first. Call once per frame.
    void update(const Vec3& focus, int renderDistance);

    // Finished meshes in completion order (stale results are filtered out).
    bool popMeshUpdate(MeshUpdate& out);
    // Hands a mesh buffer back for reuse by future meshing jobs.
    void recycleMesh(std::unique_ptr<ChunkMesh> mesh);
    // Chunks whose GPU mesh should be released.
    bool popUnloadedChunk(ChunkPos& out);

    // True when every chunk within `radius` of `center` has an up to date mesh.
    bool isAreaReady(ChunkPos center, int radius) const;
    // Keeps updating (sleeping briefly) until isAreaReady or the timeout expires.
    bool waitForArea(const Vec3& focus, int renderDistance, int radius, double timeoutSeconds);

    WorldStats stats() const;
    unsigned workerThreads() const { return m_pool->threadCount(); }

    // --- Blocks ----------------------------------------------------------------------
    BlockType getBlock(int x, int y, int z) const;
    BlockType blockAt(int x, int y, int z) const override { return getBlock(x, y, z); }
    // Edits a block (copy-on-write) and schedules high priority re-meshing of every
    // chunk that can see it. Returns false if the chunk is not loaded or nothing changed.
    bool setBlock(int x, int y, int z, BlockType type);
    bool isChunkLoaded(ChunkPos pos) const;
    std::shared_ptr<const Chunk> chunkSnapshot(ChunkPos pos) const;

    // Chunks whose mesh depends on the block at (x, z): its own chunk plus neighbours
    // touching it (border faces and ambient occlusion). Returns the number written.
    static int chunksAffectedBy(int x, int z, ChunkPos out[4]);

    // --- Persistence -----------------------------------------------------------------
    // Every player-modified chunk, loaded or not.
    std::vector<std::pair<ChunkPos, std::shared_ptr<const Chunk>>> modifiedChunks() const;
    // Registers a modified chunk loaded from a save file; it replaces generation.
    void addModifiedChunk(ChunkPos pos, std::shared_ptr<const Chunk> chunk);

    // A walkable spawn point (feet position) near the given column.
    Vec3 findSpawnPoint(int nearX, int nearZ) const;

private:
    struct Entry {
        std::uint64_t id = 0;                // unique per load, rejects stale job results
        std::shared_ptr<const Chunk> chunk;  // null while generating
        bool generating = false;
        bool meshing = false;
        bool meshDirty = false;              // needs a (new) mesh
        bool urgent = false;                 // edited: mesh with high priority
        bool hasMesh = false;
        bool modified = false;               // must be saved
    };
    struct GenerationResult {
        ChunkPos pos;
        std::uint64_t id = 0;
        std::shared_ptr<const Chunk> chunk;
    };
    struct MeshResult {
        ChunkPos pos;
        std::uint64_t id = 0;
        std::unique_ptr<ChunkMesh> mesh;
    };
    struct Offset {
        int dx;
        int dz;
    };

    void rebuildOffsets(int renderDistance);
    void collectResults();
    void unloadFarChunks();
    void scheduleGeneration();
    void scheduleMeshing();
    std::unique_ptr<ChunkMesh> acquireMesh();
    const Entry* findEntry(ChunkPos pos) const;

    TerrainGenerator m_generator;
    std::unordered_map<ChunkPos, Entry, ChunkPosHash> m_chunks;
    std::unordered_map<ChunkPos, std::shared_ptr<const Chunk>, ChunkPosHash> m_storedModified; // unloaded edits

    std::vector<Offset> m_generateOffsets; // sorted by distance
    std::vector<Offset> m_meshOffsets;     // sorted by distance
    ChunkPos m_center{0, 0};
    int m_renderDistance = -1;
    bool m_scanNeeded = true;
    std::uint64_t m_nextId = 0;
    std::size_t m_generationInFlight = 0;
    std::size_t m_meshingInFlight = 0;

    std::deque<MeshResult> m_readyMeshes;
    std::vector<ChunkPos> m_unloaded;
    std::vector<std::unique_ptr<ChunkMesh>> m_meshPool;

    ConcurrentQueue<GenerationResult> m_generationResults;
    ConcurrentQueue<MeshResult> m_meshResults;

    // Declared last: destroyed first, so no job outlives the data it references.
    std::unique_ptr<ThreadPool> m_pool;
};

} // namespace voxelforge
