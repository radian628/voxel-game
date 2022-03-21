
#define GLM_FORCE_RADIANS
#include "draw.h"
#include "glad.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <array>
#include <fstream>
#include <cstdlib>

#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/noise.hpp"

namespace input {
    bool FORWARD;
    bool BACKWARD;
    bool LEFT;
    bool RIGHT;
    bool UP;
    bool DOWN;
    bool TOGGLE_CURSOR_MODE;
    std::unordered_map<int, bool*> keyBindings = {
        {GLFW_KEY_W, &FORWARD},
        {GLFW_KEY_S, &BACKWARD},
        {GLFW_KEY_A, &LEFT},
        {GLFW_KEY_D, &RIGHT},
        {GLFW_KEY_SPACE, &UP},
        {GLFW_KEY_LEFT_SHIFT, &DOWN},
        {GLFW_KEY_ESCAPE, &TOGGLE_CURSOR_MODE}
    };

    bool isCursorLocked = false;
    glm::vec2 mousePos;
}

using namespace glm;

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

const std::array<vec3, 6> FULLSCREEN_QUAD = std::array<vec3, 6>{
    vec3{ -1.0f, -1.0f, 0.5f},
    vec3{ 1.0f, -1.0f, 0.5f},
    vec3{ -1.0f, 1.0f, 0.5f},
    vec3{ 1.0f, -1.0f, 0.5f},
    vec3{ 1.0f, 1.0f, 0.5f},
    vec3{ -1.0f, 1.0f, 0.5f}
};

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto keyMapping = input::keyBindings.find(key);
    if (keyMapping != input::keyBindings.end()) {
        switch (action) {
        case GLFW_PRESS:
            *(keyMapping->second) = true;
            break;
        case GLFW_RELEASE:
            *(keyMapping->second) = false;
            break;
        }
    }
}

void windowSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    matrix::projection = perspective(2.1f, glm::max(static_cast<float>(width), 1.f) / glm::max(static_cast<float>(height), 1.f), 0.1f, 300.f);
}

int main() {
    viewerPosition = glm::vec3{ 128.f, 128.f, 128.f };

    std::cout << "Waiting for RenderDoc... (press Enter to continue)" << std::endl;
    std::cin.get();
    matrix::projection = perspective(2.1f, glm::max(static_cast<float>(WINDOW_WIDTH), 1.f) / glm::max(static_cast<float>(WINDOW_HEIGHT), 1.f), 0.1f, 300.f);
    matrix::view = glm::translate(glm::mat4(1.0f), { -8.0f, -8.0f, -60.0f });

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "voxel game", NULL, NULL);
    if (window == nullptr) {
        std::cout << "Window failed to load." << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, keyCallback);
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowSizeCallback(window, windowSizeCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    std::array<std::array<float, 48 * BLOCKS_PER_SIDE>, 48 * BLOCKS_PER_SIDE>* perlins = new std::array<std::array<float, 48 * BLOCKS_PER_SIDE>, 48 * BLOCKS_PER_SIDE>();
    for (int z = 0; z < 48 * BLOCKS_PER_SIDE; z++) {
        for (int x = 0; x < 48 * BLOCKS_PER_SIDE; x++) {
            (*perlins)[z][x] = glm::perlin((vec2{ x, z }) * 0.16f) * 0.5f
                + glm::perlin((vec2{ x, z }) * 0.04f)
                + glm::perlin((vec2{ x, z }) * 0.01f) * 1.5f
                + glm::perlin((vec2{ x, z }) * 0.0025f) * 5.0f
                + glm::perlin((vec2{ x, z }) * 0.0007f) * 25.0f;
        }
    }

    static3DLoop<0, 0, 0, 32, BLOCKS_PER_SIDE, 32>([&](auto chunkCoords) {
        //uint32_t x = 3;
        addChunkAt({ chunkCoords.xyz });
        auto& blocks = perChunkState[{ chunkCoords.xyz }].blocks;
        static3DLoop<0, 0, 0, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE, BLOCKS_PER_SIDE>([&](auto coords) {
            float noiseSample = (*perlins)[coords.z + BLOCKS_PER_SIDE * chunkCoords.z][coords.x + BLOCKS_PER_SIDE * chunkCoords.x];
            blocks[getChunkIndex(coords)] = (coords.y + BLOCKS_PER_SIDE * chunkCoords.y) < (noiseSample * 8.0f + 128.0f);
        });
    });

    delete perlins;

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);


    drawSetup();

    GLuint chunkShaderProgram = program::chunk;
    glUseProgram(chunkShaderProgram);

    GLuint fullscreenQuadBuffer;
    glGenBuffers(1, &fullscreenQuadBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(FULLSCREEN_QUAD), FULLSCREEN_QUAD.data(), GL_STATIC_DRAW);

    double prevTime = glfwGetTime();

    int framesRendered = 0;
    while (!glfwWindowShouldClose(window)) {
        drawFrame();
        if (framesRendered % 10 == 0) {
            freeFarawayDrawChunksFromGPU(512 * 31);
            setChunksToDraw();
        }
        //for (int i = 0; i < 10; i++)
        //notUpdated.push_back({ i, 0, 0, 0 });

        framesRendered++;
        double currentTime = glfwGetTime();
        if (framesRendered % 60 == 0) {
            printf("FPS: %f\n", static_cast<double>(framesRendered) / (currentTime - prevTime));
        }

        double mousePosX;
        double mousePosY;
        glfwGetCursorPos(window, &mousePosX, &mousePosY);

        glm::vec2 mouseMovement = glm::vec2{ mousePosX, mousePosY } - input::mousePos;
        input::mousePos = { mousePosX, mousePosY };

        rotation += mouseMovement * 0.003f;

        rotation.y = glm::clamp(rotation.y, -glm::pi<float>() / 2.0f, glm::pi<float>() / 2.0f);

        if (input::FORWARD) {
            viewerPosition.z -= 1.2 * cosf(rotation.x);
            viewerPosition.x -= 1.2 * -sinf(rotation.x);
        }
        if (input::BACKWARD) {
            viewerPosition.z += 1.2 * cosf(rotation.x);
            viewerPosition.x += 1.2 * -sinf(rotation.x);
        }
        if (input::LEFT) {
            viewerPosition.z -= 1.2 * sinf(rotation.x);
            viewerPosition.x -= 1.2 * cosf(rotation.x);
        }
        if (input::RIGHT) {
            viewerPosition.z += 1.2 * sinf(rotation.x);
            viewerPosition.x += 1.2 * cosf(rotation.x);
        }
        if (input::UP) {
            viewerPosition.y += 1.2;
        }
        if (input::DOWN) {
            viewerPosition.y -= 1.2;
        }
        if (input::TOGGLE_CURSOR_MODE) {
            input::TOGGLE_CURSOR_MODE = false;
            input::isCursorLocked = !input::isCursorLocked;
            glfwSetInputMode(window, GLFW_CURSOR, input::isCursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}