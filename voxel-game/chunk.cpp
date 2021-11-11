#include <future>
#include <chrono>
#include "chunk.h"

std::unordered_map<ChunkKey, PerChunkState> perChunkState; //chunk blocks
std::unordered_map<ChunkKey, BufferAndPanelCount> chunkGLBuffers; //opengl buffer objects
std::unordered_set<ChunkKey> chunksRequiringBufferUpdates; //chunks selected for drawing
std::unordered_set<ChunkKey> chunksThatShouldBeDrawn; //ready-to-draw chunks that should be drawn
std::unordered_set<ChunkKey> notUpdated; //chunks that need to have their draw meshes updated

struct KeyAndChunkFuture {
    ChunkKey key;
    std::future<std::vector<ChunkVertexFormat>> chunkFuture;
};
std::list <KeyAndChunkFuture> pendingChunkPolygonizations;
vec3 viewerPosition = { 0,0,0 };

float chunkCloseness(ChunkKey chunkKey) {
    vec3 chunkKeyPos = {
        static_cast<float>(chunkKey.x),
        static_cast<float>(chunkKey.y),
        static_cast<float>(chunkKey.z)
    };
    return glm::distance(viewerPosition, chunkKeyPos * static_cast<float>(BLOCKS_PER_SIDE));
}
bool isChunkCloser(const ChunkKey& chunkKey1, const ChunkKey& chunkKey2) {
    return chunkCloseness(chunkKey1) < chunkCloseness(chunkKey2);
}
const std::array<glm::i8vec3, 6> normalTable = {
    glm::i8vec3{ -127, 0, 0 },
    { 127, 0, 0 },
    { 0, -127, 0 },
    { 0, 127, 0 },
    { 0, 0, -127 },
    { 0, 0, 127 }
};

const std::array<glm::vec3, 6> panelVertex1 = {
    glm::vec3{ 0.f, 0.f, 0.f },
    { 1.f, 0.f, 0.f },
    { 0.f, 0.f, 0.f },
    { 0.f, 1.f, 0.f },
    { 0.f, 0.f, 0.f },
    { 0.f, 0.f, 1.f }
};

const std::array<glm::vec3, 6> panelVertex2 = {
    glm::vec3{ 0.f, 0.f, 1.f },
    { 1.f, 1.f, 0.f },
    { 1.f, 0.f, 0.f },
    { 0.f, 1.f, 1.f },
    { 0.f, 1.f, 0.f },
    { 1.f, 0.f, 1.f }
};

const std::array<glm::vec3, 6> panelVertex3 = {
    glm::vec3{ 0.f, 1.f, 0.f },
    { 1.f, 0.f, 1.f },
    { 0.f, 0.f, 1.f },
    { 1.f, 1.f, 0.f },
    { 1.f, 0.f, 0.f },
    { 0.f, 1.f, 1.f }
};

const std::array<glm::vec3, 6> panelVertex4 = {
    glm::vec3{ 0.f, 1.f, 1.f },
    { 1.f, 1.f, 1.f },
    { 1.f, 0.f, 1.f },
    { 1.f, 1.f, 1.f },
    { 1.f, 1.f, 0.f },
    { 1.f, 1.f, 1.f }
};

std::vector<ChunkVertexFormat> getChunkGLBuffer(PerChunkState chunk, std::array<bool, 6> doAdjacentsExist, std::array<PerChunkState*, 6> adjacents, TemporaryChunksSnapshot* tcs) {
    //PerChunkState chunk = perChunkState[chunkKey];
    std::vector<ChunkVertexFormat> chunkGLBufferData;
    chunkGLBufferData.reserve(STARTING_CHUNK_ATTRIB_BUFFER_SIZE);

    auto addPanelIfNoAdjacent = [&](ivec3 coords, int indexOffset, uint8_t orientation) {
        int index = getChunkIndex(coords);
        Block block = chunk.blocks[index];
        Block adjacent = chunk.blocks[index + indexOffset];
        if (block && !adjacent) {
            glm::vec3 baseVertexPos = glm::vec3{ coords.x, coords.y, coords.z };
            chunkGLBufferData.push_back(ChunkVertexFormat(baseVertexPos + panelVertex1[orientation], normalTable[orientation], block));
            chunkGLBufferData.push_back(ChunkVertexFormat(baseVertexPos + panelVertex2[orientation], normalTable[orientation], block));
            chunkGLBufferData.push_back(ChunkVertexFormat(baseVertexPos + panelVertex3[orientation], normalTable[orientation], block));
            chunkGLBufferData.push_back(ChunkVertexFormat(baseVertexPos + panelVertex4[orientation], normalTable[orientation], block));
        }
    };

    auto addPanelIfNoAdjacentWithAdjacentChunk = [&](BlockList adjacentBlockList, ivec3 coords, int indexOffset, uint8_t orientation) {
        int index = getChunkIndex(coords);
        Block block = chunk.blocks[index];
        Block adjacent = adjacentBlockList[index + indexOffset];
        if (block && !adjacent) {
            glm::vec3 baseVertexPos = glm::vec3{ coords.x, coords.y, coords.z };
            chunkGLBufferData.push_back(ChunkVertexFormat(baseVertexPos + panelVertex1[orientation], normalTable[orientation], block));
            chunkGLBufferData.push_back(ChunkVertexFormat(baseVertexPos + panelVertex2[orientation], normalTable[orientation], block));
            chunkGLBufferData.push_back(ChunkVertexFormat(baseVertexPos + panelVertex3[orientation], normalTable[orientation], block));
            chunkGLBufferData.push_back(ChunkVertexFormat(baseVertexPos + panelVertex4[orientation], normalTable[orientation], block));
        }
    };

    static3DLoop<1, 0, 0, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE>([&](ivec3 coords) {
        addPanelIfNoAdjacent(coords, -1, 0);
    });
    //auto adjacentChunkIter = perChunkState.find(chunkKey + ivec4{ -1, 0, 0, 0 });
    if (doAdjacentsExist[0]) {
        PerChunkState& adjacentChunk = *adjacents[0];
        static3DLoop<0, 0, 0, 1, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE>([&](ivec3 coords) {
            addPanelIfNoAdjacentWithAdjacentChunk(adjacentChunk.blocks, coords, BLOCKS_PER_SIDE - 1, 0);
        });
    }
    static3DLoop<0, 0, 0, BLOCKS_PER_SIDE - 1, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE>([&](ivec3 coords) {
        addPanelIfNoAdjacent(coords, 1, 1);
    });
    //adjacentChunkIter = perChunkState.find(chunkKey + ivec4{ 1, 0, 0, 0 });
    if (doAdjacentsExist[1]) {
        PerChunkState& adjacentChunk = *adjacents[1];
        static3DLoop<BLOCKS_PER_SIDE - 1, 0, 0, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE>([&](ivec3 coords) {
            addPanelIfNoAdjacentWithAdjacentChunk(adjacentChunk.blocks, coords, -(BLOCKS_PER_SIDE - 1), 1);
        });
    }
    static3DLoop<0, 1, 0, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE>([&](ivec3 coords) {
        addPanelIfNoAdjacent(coords, -BLOCKS_PER_SIDE, 2);
    });
    //adjacentChunkIter = perChunkState.find(chunkKey + ivec4{ 0, -1, 0, 0 });
    if (doAdjacentsExist[2]) {
        PerChunkState& adjacentChunk = *adjacents[2];
        static3DLoop<0, 0, 0, BLOCKS_PER_SIDE, 1, BLOCKS_PER_SIDE>([&](ivec3 coords) {
            addPanelIfNoAdjacentWithAdjacentChunk(adjacentChunk.blocks, coords, BLOCKS_PER_SIDE * (BLOCKS_PER_SIDE - 1), 2);
        });
    }
    static3DLoop<0, 0, 0, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE - 1, BLOCKS_PER_SIDE>([&](ivec3 coords) {
        addPanelIfNoAdjacent(coords, BLOCKS_PER_SIDE, 3);
    });
    //adjacentChunkIter = perChunkState.find(chunkKey + ivec4{ 0, 1, 0, 0 });
    if (doAdjacentsExist[3]) {
        PerChunkState& adjacentChunk = *adjacents[3];
        static3DLoop<0, BLOCKS_PER_SIDE - 1, 0, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE>([&](ivec3 coords) {
            addPanelIfNoAdjacentWithAdjacentChunk(adjacentChunk.blocks, coords, -BLOCKS_PER_SIDE * (BLOCKS_PER_SIDE - 1), 3);
        });
    }
    static3DLoop<0, 0, 1, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE>([&](ivec3 coords) {
        addPanelIfNoAdjacent(coords, -BLOCKS_PER_SIDE * BLOCKS_PER_SIDE, 4);
    });
    //adjacentChunkIter = perChunkState.find(chunkKey + ivec4{ 0, 0, -1, 0 });
    if (doAdjacentsExist[4]) {
        PerChunkState& adjacentChunk = *adjacents[4];
        static3DLoop<0, 0, 0, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE, 1>([&](ivec3 coords) {
            addPanelIfNoAdjacentWithAdjacentChunk(adjacentChunk.blocks, coords, BLOCKS_PER_SIDE * BLOCKS_PER_SIDE * (BLOCKS_PER_SIDE - 1), 4);
        });
    }
    static3DLoop<0, 0, 0, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE - 1>([&](ivec3 coords) {
        addPanelIfNoAdjacent(coords, BLOCKS_PER_SIDE * BLOCKS_PER_SIDE, 5);
    });
    //adjacentChunkIter = perChunkState.find(chunkKey + ivec4{ 0, 0, 1, 0 });
    if (doAdjacentsExist[5]) {
        PerChunkState& adjacentChunk = *adjacents[5];
        static3DLoop<0, 0, BLOCKS_PER_SIDE - 1, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE>([&](ivec3 coords) {
            addPanelIfNoAdjacentWithAdjacentChunk(adjacentChunk.blocks, coords, -BLOCKS_PER_SIDE * BLOCKS_PER_SIDE * (BLOCKS_PER_SIDE - 1), 5);
        });
    }

    tcs->users--;
    if (tcs->users == 0) {
        delete tcs;
    }

    return chunkGLBufferData;
}

int getChunkIndex(ivec3 coords) {
    return coords.x + BLOCKS_PER_SIDE * (coords.y + BLOCKS_PER_SIDE * coords.z);
}

enum AdjacentChunkState {
    USE_CHUNK, FILL, NO_FILL
};

void updateChunkGLBuffers() {
    pendingChunkPolygonizations.remove_if([](auto& futureAndKey) -> bool {
        //auto& futureAndKey = *iter;
        if (futureAndKey.chunkFuture.wait_for(std::chrono::nanoseconds(1)) == std::future_status::ready) {
            auto bufferData = futureAndKey.chunkFuture.get();
            GLuint buf;
            auto iter = chunkGLBuffers.find(futureAndKey.key);
            if (iter == chunkGLBuffers.end()) {
                glGenBuffers(1, &buf);
                chunkGLBuffers[futureAndKey.key] = BufferAndPanelCount(buf, 0);
                iter = chunkGLBuffers.find(futureAndKey.key);
            }
            else {
                buf = (*iter).second.buffer;
            }
            glBindBuffer(GL_ARRAY_BUFFER, buf);
            (*iter).second.vertexCount = bufferData.size() / 4 * 6;
            glBufferData(GL_ARRAY_BUFFER, bufferData.size() * sizeof(ChunkVertexFormat), bufferData.data(), GL_STATIC_DRAW);
            return true;
        }
        return false;
    });

    TemporaryChunksSnapshot* tcs = new TemporaryChunksSnapshot();
    for (auto iter = chunksRequiringBufferUpdates.begin(); iter != chunksRequiringBufferUpdates.end(); ++iter) {
        auto chunkKey = *iter;
        //auto chunk = perChunkState[chunkKey];

        PerChunkState chunkData = perChunkState[chunkKey];
        std::array<bool, 6> doAdjacentsExist;
        std::array<PerChunkState*, 6> adjacentChunks = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };


        const std::array<ivec3, 6> adjacentOffsets = {
            ivec3{ -1, 0, 0 },
            { 1, 0, 0 },
            { 0, -1, 0 },
            { 0, 1, 0 },
            { 0, 0, -1 },
            { 0, 0, 1 }
        };

        for (int i = 0; i < 6; i++) {
            auto adjacentCoords = chunkKey + adjacentOffsets[i];
            auto iter = perChunkState.find(adjacentCoords);
            if (iter != perChunkState.end()) {
                doAdjacentsExist[i] = true;
                auto iterToTempChunkCopy = tcs->chunks.find(adjacentCoords);
                if (iterToTempChunkCopy == tcs->chunks.end()) {
                    tcs->chunks[adjacentCoords] = PerChunkState((*iter).second);
                    adjacentChunks[i] = &tcs->chunks[adjacentCoords];
                }
                else {
                    adjacentChunks[i] = &tcs->chunks[adjacentCoords];
                }
            }
            else {
                doAdjacentsExist[i] = false;
            }
        }

        tcs->users += 1;
        pendingChunkPolygonizations.push_front({
            chunkKey, std::async(std::launch::async,&getChunkGLBuffer, chunkData, doAdjacentsExist, adjacentChunks, tcs)
        });

        //auto bufferData = getChunkGLBuffer(chunkData, doAdjacentsExist, adjacentChunks);
        //glBindBuffer(GL_ARRAY_BUFFER, chunkGLBuffers[chunkKey].buffer);
        //chunkGLBuffers[chunkKey].vertexCount = bufferData.size() / 4 * 6;
        //glBufferData(GL_ARRAY_BUFFER, bufferData.size() * sizeof(ChunkVertexFormat), bufferData.data(), GL_STATIC_DRAW);
    }
    chunksRequiringBufferUpdates.clear();
}


void addChunkAt(ChunkKey posAndLod) {
    perChunkState[posAndLod] = PerChunkState();
    //printf("%d %d %d\n", posAndLod.x, posAndLod.y, posAndLod.z);
    notUpdated.insert(posAndLod);
    //GLuint buf;
    //glGenBuffers(1, &buf);
    //chunkGLBuffers[posAndLod] = BufferAndPanelCount(buf, 0);
}


void addChunkToDraw(ChunkKey posAndLod) {
    auto iter = notUpdated.find(posAndLod);
    if (iter != notUpdated.end()) {
        //notUpdated.insert(posAndLod);
        chunksRequiringBufferUpdates.insert(posAndLod);
        notUpdated.erase(iter);
    }
    chunksThatShouldBeDrawn.insert(posAndLod);
}

glm::ivec3 renderDistance = { 4, 4, 4 };
void setChunksToDraw() {
    chunksThatShouldBeDrawn.clear();
    glm::ivec3 chunkSpaceViewerPos = glm::ivec3{ viewerPosition.x, viewerPosition.y, viewerPosition.z } / BLOCKS_PER_SIDE;
    glm::ivec3 chunkCoord;
    for (chunkCoord.z = -renderDistance.z; chunkCoord.z < renderDistance.z + 1; chunkCoord.z++) {
        for (chunkCoord.y = -renderDistance.y; chunkCoord.y < renderDistance.y + 1; chunkCoord.y++) {
            for (chunkCoord.x = -renderDistance.x; chunkCoord.x < renderDistance.x + 1; chunkCoord.x++) {
                addChunkToDraw(ivec4{ chunkCoord + chunkSpaceViewerPos, 0 });
            }
        }
    }
}

std::array<uint32_t, 8> lodNoiseIndexOffsets = {
    0, 
    1, 
    BLOCKS_PER_SIDE, 
    BLOCKS_PER_SIDE + 1,
    BLOCKS_PER_SIDE * BLOCKS_PER_SIDE, 
    BLOCKS_PER_SIDE * BLOCKS_PER_SIDE + 1, 
    BLOCKS_PER_SIDE * BLOCKS_PER_SIDE + BLOCKS_PER_SIDE, 
    BLOCKS_PER_SIDE * BLOCKS_PER_SIDE + BLOCKS_PER_SIDE + 1
};

void freeFarawayDrawChunksFromGPU(int limit) {
    if (chunkGLBuffers.size() > limit) {
        struct PosAndLODAndDistance {
            ChunkKey posAndLod;
            float distance;
        };
        std::vector<PosAndLODAndDistance> distanceSortedChunks;
        distanceSortedChunks.reserve(chunkGLBuffers.size());
        for (const auto& kv : chunkGLBuffers) {
            auto posAndLod = kv.first;
            distanceSortedChunks.push_back({ posAndLod, glm::distance(
                {
                    posAndLod.x * BLOCKS_PER_SIDE,
                    posAndLod.y * BLOCKS_PER_SIDE,
                    posAndLod.z * BLOCKS_PER_SIDE
                },
                viewerPosition
            ) });
        }
        std::sort(distanceSortedChunks.begin(), distanceSortedChunks.end(), [](auto a, auto b) -> bool { return a.distance > b.distance; });
        printf("first elem: %f\n", distanceSortedChunks[0].distance);
        int chunksToFreeCount = chunkGLBuffers.size() - limit;
        for (int i = 0; i < chunksToFreeCount; i++) {
            auto bufferData = chunkGLBuffers[distanceSortedChunks[i].posAndLod];
            glDeleteBuffers(1, &(bufferData.buffer));
            chunkGLBuffers.erase(distanceSortedChunks[i].posAndLod);
            chunksThatShouldBeDrawn.erase(distanceSortedChunks[i].posAndLod);
        }
    }
}