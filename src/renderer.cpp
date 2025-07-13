#include "renderer.h"

#include <glad/glad.h>
#include <stb_truetype.h>
#include <platform/tools.h> // permaAssert and defer macros
#include "utf8.h"
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstring> // memset
#include <cstdio>  // fprintf, stderr

struct Glyph {
	float ax; // advance.x
	float ay; // advance.y
	float bw; // bitmap width
	float bh; // bitmap height
	float bl; // bitmap left
	float bt; // bitmap top
	float tx; // x offset in atlas
};

static const char* fragmentShaderSrc = R"glsl(
#version 330 core
in vec2 frag_uv;
in vec4 frag_color;
out vec4 out_color;

uniform sampler2D tex;

void main() {
	float alpha = texture(tex, frag_uv).r;
	out_color = vec4(frag_color.rgb, frag_color.a * alpha);
}
)glsl";

static const char* vertexShaderSrc = R"glsl(
#version 330 core

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

out vec2 frag_uv;
out vec4 frag_color;

uniform vec2 screenSize;

void main() {
    vec2 pos = in_pos / screenSize * 2.0 - 1.0;
    pos.y = -pos.y;
    gl_Position = vec4(pos, 0.0, 1.0);
    frag_uv = in_uv;
    frag_color = in_color;
}

)glsl";


static constexpr int ATLAS_WIDTH = 1024;
static constexpr int ATLAS_HEIGHT = 1024;
static constexpr int CURSOR_CODEPOINT = 0x2588;

static stbtt_fontinfo fontInfo;
static unsigned char* ttfBuffer = nullptr;

static unsigned char atlasBitmap[ATLAS_WIDTH * ATLAS_HEIGHT];
static GLuint atlasTexture = 0;
static float fontSizeGlobal = 0.0f;
static float scale = 0.0f;

static std::unordered_map<uint32_t, Glyph> glyphs;

static int ascent = 0;
static int descent = 0;
static int lineGap = 0;

static GLuint vao = 0, vbo = 0, shaderProgram = 0;

// Vertex data: x, y, u, v
struct Vertex {
	float x, y, u, v;
	float r, g, b, a;
};

struct vec4 { 
	union {
		struct {
			float x, y, z, w;
		};

		struct {
			float r, g, b, a;
		};
	};
};


static vec4 termColorToRGBA(TermColor color) {
	switch (color) {
	case TermColor::Black:
		return {0.0f, 0.0f, 0.0f, 1.0f};
	case TermColor::Red:
		return {0.8f, 0.0f, 0.0f, 1.0f};
	case TermColor::Green:
		return {0.0f, 0.8f, 0.0f, 1.0f};
	case TermColor::Yellow:
		return {0.8f, 0.8f, 0.0f, 1.0f};
	case TermColor::Blue:
		return {0.0f, 0.0f, 0.8f, 1.0f};
	case TermColor::Magenta:
		return {0.8f, 0.0f, 0.8f, 1.0f};
	case TermColor::Cyan:
		return {0.0f, 0.8f, 0.8f, 1.0f};
	case TermColor::White:
		return {0.8f, 0.8f, 0.8f, 1.0f};
	case TermColor::BrightBlack:
		return {0.2f, 0.2f, 0.2f, 1.0f};
	case TermColor::BrightRed:
		return {1.0f, 0.0f, 0.0f, 1.0f};
	case TermColor::BrightGreen:
		return {0.0f, 1.0f, 0.0f, 1.0f};
	case TermColor::BrightYellow:
		return {1.0f, 1.0f, 0.0f, 1.0f};
	case TermColor::BrightBlue:
		return {0.0f, 0.0f, 1.0f, 1.0f};
	case TermColor::BrightMagenta:
		return {1.0f, 0.0f, 1.0f, 1.0f};
	case TermColor::BrightCyan:
		return {0.0f, 1.0f, 1.0f, 1.0f};
	case TermColor::BrightWhite:
		return {1.0f, 1.0f, 1.0f, 1.0f};
	default:
		return {1.0f, 1.0f, 1.0f, 1.0f};
	}
}

static GLuint compileShader(GLenum type, const char* src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);
	return shader;
}

static GLuint createShaderProgram() {
	GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
	GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glDeleteShader(vs);
	glDeleteShader(fs);
	return program;
}

// Build or update atlas for the given new codepoints (append-only)
static void buildAtlasIncremental(const std::vector<uint32_t>& newCodepoints) {
	int x = 0, y = 0, rowHeight = 0;

	// Find max y from existing glyphs to continue packing
	if (!glyphs.empty()) {
		// Find bottom row (max y + height)
		// We'll repack all glyphs to keep things simple (optional optimization)
		x = 0;
		y = 0;
		rowHeight = 0;
		// We'll rebuild entire atlas for simplicity
		memset(atlasBitmap, 0, sizeof(atlasBitmap));
		std::unordered_map<uint32_t, Glyph> oldGlyphs = glyphs;
		glyphs.clear();

		// Insert old glyphs first
		for (auto& [cp, g] : oldGlyphs) {
			int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, cp);
			permaAssertComment(glyphIndex != 0, "Glyph must exist");

			int advance, lsb;
			stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advance, &lsb);

			int glyphW = (int)g.bw;
			int glyphH = (int)g.bh;
			int xoff = (int)g.bl;
			int yoff = (int)g.bt;

			if (x + glyphW >= ATLAS_WIDTH) {
				x = 0;
				y += rowHeight;
				rowHeight = 0;
			}

			permaAssert(y + glyphH < ATLAS_HEIGHT);

			// Bake bitmap again for old glyphs (since we cleared atlas)
			unsigned char* bitmap =
				stbtt_GetGlyphBitmap(&fontInfo, scale, scale, glyphIndex, &glyphW, &glyphH, &xoff, &yoff);

			for (int i = 0; i < glyphH; i++) {
				memcpy(atlasBitmap + (y + i) * ATLAS_WIDTH + x, bitmap + i * glyphW, glyphW);
			}

			Glyph newG;
			newG.ax = advance * scale;
			newG.ay = 0;
			newG.bw = (float)glyphW;
			newG.bh = (float)glyphH;
			newG.bl = (float)xoff;
			newG.bt = (float)yoff;
			newG.tx = (float)x / ATLAS_WIDTH;

			glyphs[cp] = newG;

			x += glyphW + 1;
			if (glyphH > rowHeight)
				rowHeight = glyphH;

			stbtt_FreeBitmap(bitmap, nullptr);
		}
	}

	// Now bake new glyphs
	for (uint32_t cp : newCodepoints) {
		if (glyphs.find(cp) != glyphs.end()) // already cached
			continue;

		int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, cp);
		if (glyphIndex == 0) {
			// Unknown glyph, skip here (handled in render)
			continue;
		}

		int advance, lsb;
		stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advance, &lsb);

		int glyphW, glyphH, xoff, yoff;
		unsigned char* bitmap =
			stbtt_GetGlyphBitmap(&fontInfo, scale, scale, glyphIndex, &glyphW, &glyphH, &xoff, &yoff);

		if (x + glyphW >= ATLAS_WIDTH) {
			x = 0;
			y += rowHeight;
			rowHeight = 0;
		}

		permaAssert(y + glyphH < ATLAS_HEIGHT);

		for (int i = 0; i < glyphH; i++) {
			memcpy(atlasBitmap + (y + i) * ATLAS_WIDTH + x, bitmap + i * glyphW, glyphW);
		}

		Glyph g;
		g.ax = advance * scale;
		g.ay = 0;
		g.bw = (float)glyphW;
		g.bh = (float)glyphH;
		g.bl = (float)xoff;
		g.bt = (float)yoff;
		g.tx = (float)x / ATLAS_WIDTH;

		glyphs[cp] = g;

		x += glyphW + 1;
		if (glyphH > rowHeight)
			rowHeight = glyphH;

		stbtt_FreeBitmap(bitmap, nullptr);
	}
}

static void uploadAtlasTexture() {
	if (atlasTexture == 0)
		glGenTextures(1, &atlasTexture);

	glBindTexture(GL_TEXTURE_2D, atlasTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, atlasBitmap);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

// Internal constant from startRender
static float charWidth = 0;
static float charHeight = 0;

void startRender(float fontSize) {
	fontSizeGlobal = fontSize;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	FILE* f = fopen(RESOURCES_PATH "FiraCode-Regular.ttf", "rb");
	permaAssertComment(f, "Failed to open font file");
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	ttfBuffer = new unsigned char[size];
	fread(ttfBuffer, 1, size, f);
	fclose(f);

	permaAssertComment(stbtt_InitFont(&fontInfo, ttfBuffer, 0), "Failed to init font");
	scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSizeGlobal);

	stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

	memset(atlasBitmap, 0, sizeof(atlasBitmap));
	glyphs.clear();
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	shaderProgram = createShaderProgram();

	// Prebake ASCII range 32..126 at start
	std::vector<uint32_t> asciiRange;
	for (uint32_t cp = 32; cp <= 126; cp++)
		asciiRange.push_back(cp);
	asciiRange.push_back(CURSOR_CODEPOINT);

	buildAtlasIncremental(asciiRange);
	uploadAtlasTexture();

	int glyphIndexSpace = stbtt_FindGlyphIndex(&fontInfo, ' ');
	int advanceSpace, lsb;
	stbtt_GetGlyphHMetrics(&fontInfo, glyphIndexSpace, &advanceSpace, &lsb);
	charWidth = advanceSpace * scale;
	charHeight = (float)(ascent - descent + lineGap) * scale;
}

void render(StyledScreen& screen, int startLineIndex, int screenW, int screenH) {
	glViewport(0, 0, screenW, screenH);
	glClear(GL_COLOR_BUFFER_BIT);
	permaAssert(fontSizeGlobal > 0.0f);
	if (screen.empty() || startLineIndex >= (int)screen.size())
		return;

	glUseProgram(shaderProgram);

	GLint screenSizeLoc = glGetUniformLocation(shaderProgram, "screenSize");
	glUniform2f(screenSizeLoc, float(screenW), float(screenH));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, atlasTexture);
	GLint texLoc = glGetUniformLocation(shaderProgram, "tex");
	glUniform1i(texLoc, 0);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));

	std::vector<Vertex> vertices;
	float lineHeight = (ascent - descent + lineGap) * scale;
	float yStart = 0;

	for (int lineIndex = startLineIndex; lineIndex < (int)screen.size(); ++lineIndex) {
		float penX = 0;
		float baselineY = yStart + ascent * scale + (lineIndex - startLineIndex) * lineHeight;

		for (StyledChar& stc : screen[lineIndex]) {
			if (stc.ch == '\r')
				continue;

			if (glyphs.find(stc.ch) == glyphs.end()) {
				glyphs.emplace(stc.ch, glyphs.at('?'));
			}

			const Glyph& g = glyphs.at(stc.ch);

			float x0 = penX + g.bl;
			float y0 = baselineY + g.bt;

			float x1 = x0 + g.bw;
			float y1 = y0 + g.bh;

			float tx0 = g.tx;
			float tx1 = tx0 + g.bw / ATLAS_WIDTH;
			float ty0 = 0.0f;
			float ty1 = g.bh / ATLAS_HEIGHT;
			vec4 color = termColorToRGBA(stc.fg);

			vertices.push_back({x0, y0, tx0, ty0, color.r, color.g, color.b, color.a});
			vertices.push_back({x1, y0, tx1, ty0, color.r, color.g, color.b, color.a});
			vertices.push_back({x0, y1, tx0, ty1, color.r, color.g, color.b, color.a});
			vertices.push_back({x1, y0, tx1, ty0, color.r, color.g, color.b, color.a});
			vertices.push_back({x1, y1, tx1, ty1, color.r, color.g, color.b, color.a});
			vertices.push_back({x0, y1, tx0, ty1, color.r, color.g, color.b, color.a});

			penX += g.ax;
		}
	}

	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());

	glBindVertexArray(0);
	glUseProgram(0);
}

void renderCursor(int cursorX, int cursorY, float deltaTime, int screenW, int screenH) {
	if (fontSizeGlobal <= 0)
		return;

	static float time = 0.0f;
	time += deltaTime;

	// Blink on/off every 0.5s
	bool visible = fmod(time, 1.0f) < 0.5f;
	if (!visible)
		return;

	auto it = glyphs.find(CURSOR_CODEPOINT);
	if (it == glyphs.end())
		return;

	const Glyph& g = it->second;

	float cursorWidth = 1.0f;
	float offsetX = 0.2f; // Shift left or right by modifying this value (pixels)

	float lineHeight = (ascent - descent + lineGap) * scale;
	float lineCenterY = cursorY * lineHeight + lineHeight * 0.5f;

	float glyphHeight = g.bh;
	float y0 = lineCenterY - glyphHeight * 0.5f;
	float y1 = y0 + glyphHeight;

	float x0 = cursorX * charWidth + offsetX;
	float x1 = x0 + cursorWidth;

	// Texture coords for thin vertical slice of full block glyph
	float tx0 = g.tx;
	float tx1 = tx0 + (cursorWidth / g.bw) * (g.bw / ATLAS_WIDTH);

	float ty0 = 0.0f;
	float ty1 = g.bh / ATLAS_HEIGHT;

	Vertex verts[6] = {
		{x0, y0, tx0, ty0}, {x1, y0, tx1, ty0}, {x0, y1, tx0, ty1},
		{x1, y0, tx1, ty0}, {x1, y1, tx1, ty1}, {x0, y1, tx0, ty1},
	};

	glUseProgram(shaderProgram);

	GLint screenSizeLoc = glGetUniformLocation(shaderProgram, "screenSize");
	GLint texLoc = glGetUniformLocation(shaderProgram, "tex");

	glUniform2f(screenSizeLoc, float(screenW), float(screenH));
	glUniform1i(texLoc, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, atlasTexture);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));

	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindVertexArray(0);
	glUseProgram(0);
}

void stopRender() {
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
	glDeleteProgram(shaderProgram);
	if (ttfBuffer) {
		delete[] ttfBuffer;
		ttfBuffer = nullptr;
	}
	if (atlasTexture) {
		glDeleteTextures(1, &atlasTexture);
		atlasTexture = 0;
	}
	glyphs.clear();
	fontSizeGlobal = 0.0f;
}
