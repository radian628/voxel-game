#include "draw.h"
#include <iostream>
#include <fstream>

vec2 rotation;

namespace matrix {
	mat4 view;
	mat4 projection;
}

namespace program {
	GLuint chunk;
	GLuint chunk2;
}

namespace fbo {
}

namespace vao {
	GLuint chunk;
}

namespace vbo {
	GLuint chunkPanel;
	GLuint chunkPanelIndex;
}

std::vector<uint32_t> chunkIndexBufferDataSource;

std::string getTextFile(std::string fileName) {
	std::ifstream file(fileName);
	if (file.is_open()) {
		std::string fileString = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		file.close();
		return fileString;
	}
	else {
		throw "Failed to open file " + fileName;
	}
}

GLuint makeShaderFromString(std::string shaderString, GLuint shaderType) {
	GLuint shader = glCreateShader(shaderType);

	const char* source = shaderString.c_str();
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		std::cout << "shader compile error: \n" << infoLog << std::endl;
	}
	return shader;
}

GLuint makeShaderProgramFromStrings(std::string vertexShaderString, std::string fragmentShaderString) {
	GLuint vertexShader = makeShaderFromString(vertexShaderString, GL_VERTEX_SHADER);
	GLuint fragmentShader = makeShaderFromString(fragmentShaderString, GL_FRAGMENT_SHADER);

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	GLint success;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "program linking failed:\n" << infoLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

GLuint makeShaderProgramFromFiles(std::string vertexShaderFileName, std::string fragmentShaderFileName) {
	std::string vertexShaderString = getTextFile(vertexShaderFileName);
	std::string fragmentShaderString = getTextFile(fragmentShaderFileName);
	return makeShaderProgramFromStrings(vertexShaderString, fragmentShaderString);
}

void drawSetup() {
	program::chunk = makeShaderProgramFromFiles("shader/chunk.vert", "shader/chunk.frag");
	program::chunk2 = makeShaderProgramFromFiles("shader/chunk2.vert", "shader/chunk.frag");

	glGenVertexArrays(1, &vao::chunk);
	glBindVertexArray(vao::chunk);

	glGenBuffers(1, &vbo::chunkPanel);
	glBindBuffer(GL_ARRAY_BUFFER, vbo::chunkPanel);
	glBufferData(GL_ARRAY_BUFFER, sizeof(CHUNK_PANEL_VERTS), CHUNK_PANEL_VERTS.data(), GL_STATIC_DRAW);

	chunkIndexBufferDataSource.reserve(3 * BLOCKS_PER_SIDE * BLOCKS_PER_SIDE * (BLOCKS_PER_SIDE+1) * 6);
	for (int i = 0; i < 3 * BLOCKS_PER_SIDE * BLOCKS_PER_SIDE * (BLOCKS_PER_SIDE + 1) * 6; i += 6) {
		chunkIndexBufferDataSource.push_back(0 + i / 6 * 4);
		chunkIndexBufferDataSource.push_back(1 + i / 6 * 4);
		chunkIndexBufferDataSource.push_back(2 + i / 6 * 4);
		chunkIndexBufferDataSource.push_back(2 + i / 6 * 4);
		chunkIndexBufferDataSource.push_back(1 + i / 6 * 4);
		chunkIndexBufferDataSource.push_back(3 + i / 6 * 4);
	}

	glGenBuffers(1, &vbo::chunkPanelIndex);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo::chunkPanelIndex);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, chunkIndexBufferDataSource.size() * sizeof(uint32_t), chunkIndexBufferDataSource.data(), GL_STATIC_DRAW);
}

void drawFrame() {
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	//========================= DRAW CHUNKS =============================
	glBindVertexArray(vao::chunk);

	glUseProgram(program::chunk2);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo::chunkPanelIndex);

	matrix::view = mat4(1.0f);
	matrix::view = glm::rotate(matrix::view, rotation.y, { 1.f, 0.f, 0.f });
	matrix::view = glm::rotate(matrix::view, rotation.x, { 0.f, 1.f, 0.f });
	matrix::view = glm::translate(matrix::view, -viewerPosition);

	updateChunkGLBuffers();
	for (auto chunkGLStatePair : chunkGLBuffers) {
		auto& chunkGLState = chunkGLStatePair.second;
		auto& posAndLOD = chunkGLStatePair.first;
		if (chunksThatShouldBeDrawn.find(posAndLOD) != chunksThatShouldBeDrawn.end()) {
			glBindBuffer(GL_ARRAY_BUFFER, /*chunkGLBuffers[{0, 0, 0, 0}].buffer*/chunkGLState.buffer);

			glVertexAttribPointer(0, 4, GL_HALF_FLOAT, false, 12, 0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 4, GL_BYTE, true, 12, (GLvoid*)8);
			glEnableVertexAttribArray(1);

			vec3 floatPos = vec3{ posAndLOD.x, posAndLOD.y, posAndLOD.z } *static_cast<float>(BLOCKS_PER_SIDE);

			auto model = glm::mat4(1.0f);
			model = glm::translate(model, floatPos);
			//model = glm::scale(model, glm::vec3(pow(2, posAndLOD.w)));

			auto mvp = matrix::projection * matrix::view * model;

			glUniform1ui(0, 15u);
			glUniform1ui(1, 4);
			glUniform1ui(2, 8);
			glUniformMatrix4fv(3, 1, false, glm::value_ptr(mvp));

			glDrawElements(GL_TRIANGLES, chunkGLState.vertexCount, GL_UNSIGNED_INT, 0);
		}
	}
}