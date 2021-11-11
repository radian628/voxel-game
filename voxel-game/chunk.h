#pragma once
#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/hash.hpp"

#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <array>
#include <atomic>

using namespace glm;

const int BLOCKS_PER_SIDE = 16;
const int VOLUME = BLOCKS_PER_SIDE * BLOCKS_PER_SIDE * BLOCKS_PER_SIDE;
const int STARTING_CHUNK_ATTRIB_BUFFER_SIZE = 1024;

const std::array<vec2, 4> CHUNK_PANEL_VERTS = std::array<vec2, 4>{
    vec2{ 0.f, 0.f },
    vec2{ 0.f, 1.f },
    vec2{ 1.f, 0.f },
    vec2{ 1.f, 1.f }
};

const std::array<uint8_t, 6> CHUNK_PANEL_INDICES = { 0,1,2,2,1,3 };

typedef glm::ivec3 ChunkKey; //fine to use this as primary key because # of chunks will stay quite small
typedef uint16_t Block;
typedef std::array<uint16_t, VOLUME> BlockList;
struct PerChunkState {
    BlockList blocks;
};

struct TemporaryChunksSnapshot {
    std::unordered_map<ChunkKey, PerChunkState> chunks;
    std::atomic<int> users;
};


float chunkCloseness(ChunkKey chunkKey);
bool isChunkCloser(const ChunkKey& chunkKey1, const ChunkKey& chunkKey2);

struct BufferAndPanelCount {
    GLuint buffer;
    unsigned int vertexCount;
    BufferAndPanelCount(GLuint b, unsigned int p) : buffer{ b }, vertexCount{ p } {};
    BufferAndPanelCount() {};
};

extern std::unordered_map<ChunkKey, PerChunkState> perChunkState;
extern std::unordered_map<ChunkKey, BufferAndPanelCount> chunkGLBuffers;
extern std::unordered_set<ChunkKey> chunksRequiringBufferUpdates;
extern std::unordered_set<ChunkKey> chunksThatShouldBeDrawn;
extern std::unordered_set<ChunkKey> notUpdated;
extern vec3 viewerPosition;



template <int x1, int y1, int z1, int x2, int y2, int z2>
void static3DLoop(std::function<void(ivec3 coords)> callback) {
    ivec3 coords;
    for (coords.z = z1; coords.z < z2; coords.z++) {
        for (coords.y = y1; coords.y < y2; coords.y++) {
            for (coords.x = x1; coords.x < x2; coords.x++) {
                callback(coords);
            }
        }
    }
}

int getChunkIndex(ivec3 coords);

struct ChunkVertexFormat {
    uint32_t posXY;
    uint32_t posZAndMaterial;
    glm::i8vec3 normalOut;
    glm::i8 padding;
    ChunkVertexFormat(glm::vec3 position, glm::i8vec3 normal, uint16_t material) {
        posXY = glm::packHalf2x16(position.xy);
        posZAndMaterial = glm::packHalf2x16(glm::vec2{ position.z });
        normalOut = normal;
    }
};

std::vector<ChunkVertexFormat> getChunkGLBuffer(PerChunkState chunk, std::array<bool, 6> doAdjacentsExist, std::array<PerChunkState*, 6> adjacents, TemporaryChunksSnapshot* tcs);

void updateChunkGLBuffers();

void addChunkAt(ChunkKey posAndLod);

void setChunksToDraw();


void mergeChunksIntoHigherLOD(ChunkKey posAndLod);

void freeFarawayDrawChunksFromGPU(int limit);