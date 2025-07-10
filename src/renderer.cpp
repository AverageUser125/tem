#include "renderer.h"
#include <glad/glad.h>
#include <stb_truetype.h>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <platform/tools.h>
#include <platform/window.h>
#include "utf8.h"

// ======= SETTINGS =======
constexpr int ATLAS_SIZE = 512;
constexpr int FIRST_CHAR = 32;
constexpr int CHAR_COUNT = 96;
constexpr float SDF_PADDING = 5.0f;
constexpr float SDF_SPREAD = 8.0f;
constexpr float SDF_SMOOTH = 0.25f;
static float fontSize = 0;

// monospace
#define CELL_WIDTH fontSize
#define CELL_HEIGHT fontSize

// ======= STRUCTS =======
static stbtt_bakedchar glyphs[CHAR_COUNT];
static GLuint atlasTexture = 0;
static GLuint shaderProgram = 0;
static GLuint vao = 0, vbo = 0;
static GLint uScreenSizeLoc, uTexLoc, uColorLoc;

// ======= SHADERS =======
static const char* vertexSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
uniform vec2 uScreenSize;
void main() {
	vec2 pos = aPos / uScreenSize * 2.0 - 1.0;
	gl_Position = vec4(pos.x, -pos.y, 0.0, 1.0);
	vUV = aUV;
}
)";

static const char* fragmentSrc = R"(
#version 330 core
#define SDF_SMOOTH 0.25
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec3 uColor;

void main() {
	float distance = texture(uTex, vUV).r;
	float alpha = smoothstep(0.5 - SDF_SMOOTH, 0.5 + SDF_SMOOTH, distance);
	FragColor = vec4(uColor, alpha);
}
)";

// ======= HELPERS =======
static GLuint compile(GLenum type, const char* src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);
	return shader;
}

static GLuint createShader() {
	GLuint vs = compile(GL_VERTEX_SHADER, vertexSrc);
	GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);
	return prog;
}

// ======= FONT INIT =======
static void loadFont(const char* ttfPath) {
	FILE* f = fopen(ttfPath, "rb");
	permaAssertComment(f, "Missing font file");
	fseek(f, 0, SEEK_END);
	int size = ftell(f);
	fseek(f, 0, SEEK_SET);
	auto* ttfBuffer = new unsigned char[size];
	fread(ttfBuffer, 1, size, f);
	fclose(f);

	constexpr int bakeW = ATLAS_SIZE, bakeH = ATLAS_SIZE;
	auto* bitmap = new unsigned char[bakeW * bakeH];
	permaAssertComment((stbtt_BakeFontBitmap(ttfBuffer, 0, fontSize, bitmap, bakeW, bakeH, FIRST_CHAR,
											 CHAR_COUNT,
											(stbtt_bakedchar*)glyphs) > 0), "Failed to bake font");

	glGenTextures(1, &atlasTexture);
	glBindTexture(GL_TEXTURE_2D, atlasTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, bakeW, bakeH, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	delete[] bitmap;
	delete[] ttfBuffer;
}

// ======= INIT =======
void startRender(float _fontSize) {
	fontSize = _fontSize;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	loadFont(RESOURCES_PATH "FiraCode-Regular.ttf");
	shaderProgram = createShader();

	uScreenSizeLoc = glGetUniformLocation(shaderProgram, "uScreenSize");
	uTexLoc = glGetUniformLocation(shaderProgram, "uTex");
	uColorLoc = glGetUniformLocation(shaderProgram, "uColor");

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
}

// ======= RENDER TEXT =======
static void drawChar(char c, float x, float y) {
	if (c < FIRST_CHAR || c >= FIRST_CHAR + CHAR_COUNT)
		c = '?';
	stbtt_bakedchar& g = glyphs[c - FIRST_CHAR];

    float x0 = x + g.xoff;
	float y0 = y + g.yoff;
	float x1 = x0 + (g.x1 - g.x0);
	float y1 = y0 + (g.y1 - g.y0);

	// Texture coordinates (normalized atlas UV)
	float s0 = g.x0 / (float)ATLAS_SIZE; // assuming 512x512 atlas
	float t0 = g.y0 / (float)ATLAS_SIZE;
	float s1 = g.x1 / (float)ATLAS_SIZE;
	float t1 = g.y1 / (float)ATLAS_SIZE;

	// Vertices: x, y, s, t (6 vertices for two triangles)
	float vertices[6][4] = {
		{x0, y0, s0, t0}, {x1, y0, s1, t0}, {x1, y1, s1, t1}, {x0, y0, s0, t0}, {x1, y1, s1, t1}, {x0, y1, s0, t1},
	};

	glBindTexture(GL_TEXTURE_2D, atlasTexture);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ======= MAIN RENDER =======
void render(const std::vector<std::string>& lines, int startLineIndex, float _fontSize, int screenW, int screenH) {
	fontSize = _fontSize;
	glUseProgram(shaderProgram);

	glUniform2f(uScreenSizeLoc, (float)screenW, (float)screenH);
	glUniform3f(uColorLoc, 1.0f, 1.0f, 1.0f);
	glUniform1i(uTexLoc, 0);

	int maxRows = screenH / CELL_HEIGHT;

	for (int i = 0; i < maxRows; ++i) {
		int lineIndex = startLineIndex + i;
		float y = (i + 1) * CELL_HEIGHT;
		float x = 0;

		std::string_view line;
		if (lineIndex < (int)lines.size())
			line = lines[lineIndex];
		else
			line = ""; // Clear empty rows

		const char* p = line.data();
		const char* end = p + line.size();

		while (p < end) {
			uint32_t cp;
			int len = decode_utf8(p, &cp);
			if (len > 0 && cp < 128) {
				drawChar((char)cp, x, y);
				p += len;
			} else {
				drawChar('?', x, y);
				++p;
			}
			x += CELL_WIDTH;
		}
	}
}

void renderCursor(int cursorX, int cursorY, float deltaTime) {
	static float time = 0.0f;
	time += deltaTime;

	// Blink every ~1 second
	float blink = fmod(time, 1.0f);
	if (blink > 0.5f)
		return;

	// Use a solid block character ('█') as cursor symbol
	drawChar((char)219, cursorX * CELL_WIDTH, (cursorY + 1) * CELL_HEIGHT);
}
