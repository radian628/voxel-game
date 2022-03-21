#pragma once
#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glad.h"

#include "chunk.h"

#include <unordered_map>
#include <map>
#include <unordered_set>
#include <set>

extern vec2 rotation;

namespace matrix {
	extern mat4 view;
	extern mat4 projection;
}

namespace program {
	extern GLuint chunk;
	extern GLuint chunk2;
}

namespace fbo {
}

namespace vao {
	extern GLuint chunk;
}

namespace vbo {
	extern GLuint chunkPanel;
	extern GLuint chunkPanelIndex;
}


void drawSetup();

void drawFrame();