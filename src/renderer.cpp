#include "renderer.h"
#include <glad/glad.h>
#include <stb_truetype.h>
#include <platform/tools.h>
#include "utf8.h"
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include "styledScreen.h"

static void uploadAtlasTexture();

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
	if (frag_uv == vec2(0.0, 0.0)) {
        out_color = frag_color;
    } else {
		float alpha = texture(tex, frag_uv).r;
		vec3 premultiplied_rgb = frag_color.rgb * alpha;
		out_color = vec4(premultiplied_rgb, alpha);
    }
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


static constexpr int ATLAS_WIDTH = 2048;
static constexpr int ATLAS_HEIGHT = ATLAS_WIDTH;
static constexpr int CURSOR_CODEPOINT = 0x2588;

static stbtt_fontinfo fontInfo;
static unsigned char* ttfBuffer = nullptr;

static unsigned char atlasBitmap[ATLAS_WIDTH * ATLAS_HEIGHT];
static GLuint atlasTexture = 0;
static float scale = 0.0f;
static int atlasX = 0;
static int atlasY = 0;
static int atlasRowHeight = 0;

static std::unordered_map<char32_t, Glyph> glyphs;

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

static constexpr vec4 termColorToRGBA(TermColor color) {
	return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, 1.0f};
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

static bool tryPackGlyph(char32_t cp) {
	if (glyphs.find(cp) != glyphs.end())
		return true;

	int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, cp);
	if (glyphIndex == 0)
		return false;

	int advance, lsb;
	stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advance, &lsb);

	int glyphW, glyphH, xoff, yoff;
	unsigned char* bitmap = stbtt_GetGlyphBitmap(&fontInfo, scale, scale, glyphIndex, &glyphW, &glyphH, &xoff, &yoff);
	defer(stbtt_FreeBitmap(bitmap, nullptr));
	if (atlasX + glyphW >= ATLAS_WIDTH) {
		atlasX = 0;
		atlasY += atlasRowHeight;
		atlasRowHeight = 0;
	}

	if (atlasY + glyphH >= ATLAS_HEIGHT) {
		return false; // atlas full
	}

	for (int i = 0; i < glyphH; i++) {
		memcpy(atlasBitmap + (atlasY + i) * ATLAS_WIDTH + atlasX, bitmap + i * glyphW, glyphW);
	}

	Glyph g;
	g.ax = advance * scale;
	g.ay = 0;
	g.bw = (float)glyphW;
	g.bh = (float)glyphH;
	g.bl = (float)xoff;
	g.bt = (float)yoff;
	g.tx = (float)atlasX / ATLAS_WIDTH;

	glyphs[cp] = g;

	atlasX += glyphW + 1;
	atlasRowHeight = std::max(atlasRowHeight, glyphH);

	return true;
}

static void loadGlyphIfNeeded(char32_t cp) {
	if (glyphs.find(cp) != glyphs.end())
		return;

	if (!tryPackGlyph(cp)) {
		if (glyphs.find('?') == glyphs.end())
			tryPackGlyph('?');
		glyphs[cp] = glyphs.at('?');
		return;
	}

	uploadAtlasTexture();
}

static void buildAtlasIncremental(const std::vector<char32_t>& codepoints) {
	int x = 0, y = 0, rowHeight = 0;

	memset(atlasBitmap, 0, sizeof(atlasBitmap));
	glyphs.clear();

	for (char32_t cp : codepoints) {
		int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, cp);
		if (glyphIndex == 0) {
			// Skip missing glyphs
			continue;
		}

		int advance, lsb;
		stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advance, &lsb);

		int glyphW, glyphH, xoff, yoff;
		unsigned char* bitmap =
			stbtt_GetGlyphBitmap(&fontInfo, scale, scale, glyphIndex, &glyphW, &glyphH, &xoff, &yoff);
		defer(stbtt_FreeBitmap(bitmap, nullptr));

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
	}

	uploadAtlasTexture();

	// Initialize incremental packing state
	atlasX = x;
	atlasY = y;
	atlasRowHeight = rowHeight;
}

static void uploadAtlasTexture() {
	if (atlasTexture == 0)
		glGenTextures(1, &atlasTexture);

	glBindTexture(GL_TEXTURE_2D, atlasTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, atlasBitmap);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// Internal constant from startRender
static float charWidth = 0;
static float charHeight = 0;

void startRender() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	FILE* f = fopen(RESOURCES_PATH "MesloLGMNerdFontPropo-Regular.ttf", "rb");
	permaAssertComment(f, "Failed to open font file");
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	ttfBuffer = new unsigned char[size];
	fread(ttfBuffer, 1, size, f);
	fclose(f);

	permaAssertComment(stbtt_InitFont(&fontInfo, ttfBuffer, 0), "Failed to init font");
	scale = stbtt_ScaleForPixelHeight(&fontInfo, o.fontSize);

	stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

	memset(atlasBitmap, 0, sizeof(atlasBitmap));
	glyphs.clear();
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	shaderProgram = createShaderProgram();

	// Prebake ASCII range 32..126 at start
	std::vector<char32_t> asciiRange;
	for (char32_t cp = 32; cp <= 126; cp++)
		asciiRange.push_back(cp);
	asciiRange.push_back(CURSOR_CODEPOINT);

	buildAtlasIncremental(asciiRange);
	uploadAtlasTexture();
	glyphs['\0'] = glyphs.at(' ');

	int glyphIndexSpace = stbtt_FindGlyphIndex(&fontInfo, ' ');
	int advanceSpace, lsb;
	stbtt_GetGlyphHMetrics(&fontInfo, glyphIndexSpace, &advanceSpace, &lsb);
	charWidth = advanceSpace * scale;
	charHeight = (float)(ascent - descent + lineGap) * scale;
}

void render(const std::vector<StyledLine>& screen, int screenW, int screenH) {
	glViewport(0, 0, screenW, screenH);
	glClear(GL_COLOR_BUFFER_BIT);
	permaAssert(o.fontSize > 0.0f);

	glUseProgram(shaderProgram);
	defer(glUseProgram(0));
	GLint screenSizeLoc = glGetUniformLocation(shaderProgram, "screenSize");
	glUniform2f(screenSizeLoc, float(screenW), float(screenH));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, atlasTexture);
	GLint texLoc = glGetUniformLocation(shaderProgram, "tex");
	glUniform1i(texLoc, 0);

	glBindVertexArray(vao);
	defer(glBindVertexArray(0));
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

	for (int lineIndex = 0; lineIndex < screen.size(); ++lineIndex) {
		float penX = 0;
		float baselineY = yStart + ascent * scale + lineIndex * lineHeight;
		StyledLine line = screen[lineIndex];
		for (const StyledChar& stc : line) {
			if (stc.ch == '\r')
				continue;

			loadGlyphIfNeeded(stc.ch);
			const Glyph& g = glyphs.at(stc.ch);

			float x0 = std::round(penX + g.bl);
			float y0 = std::round(baselineY + g.bt);
			float x1 = x0 + g.bw;
			float y1 = y0 + g.bh;

			if (stc.bg != TermColor::DefaultBackGround()) {
				float bgX0 = penX;
				float bgY0 = baselineY - ascent * scale;
				float bgX1 = bgX0 + charWidth;
				float bgY1 = bgY0 + charHeight;

				vec4 bgColor = termColorToRGBA(stc.bg);
				vertices.push_back({bgX0, bgY0, 0, 0, bgColor.r, bgColor.g, bgColor.b, bgColor.a});
				vertices.push_back({bgX1, bgY0, 0, 0, bgColor.r, bgColor.g, bgColor.b, bgColor.a});
				vertices.push_back({bgX0, bgY1, 0, 0, bgColor.r, bgColor.g, bgColor.b, bgColor.a});
				vertices.push_back({bgX1, bgY0, 0, 0, bgColor.r, bgColor.g, bgColor.b, bgColor.a});
				vertices.push_back({bgX1, bgY1, 0, 0, bgColor.r, bgColor.g, bgColor.b, bgColor.a});
				vertices.push_back({bgX0, bgY1, 0, 0, bgColor.r, bgColor.g, bgColor.b, bgColor.a});
			}

			float tx0 = g.tx;
			float tx1 = tx0 + g.bw / ATLAS_WIDTH;
			float ty0 = 0.0f;
			float ty1 = g.bh / ATLAS_HEIGHT;
			vec4 fgColor = termColorToRGBA(stc.fg);

			vertices.push_back({x0, y0, tx0, ty0, fgColor.r, fgColor.g, fgColor.b, fgColor.a});
			vertices.push_back({x1, y0, tx1, ty0, fgColor.r, fgColor.g, fgColor.b, fgColor.a});
			vertices.push_back({x0, y1, tx0, ty1, fgColor.r, fgColor.g, fgColor.b, fgColor.a});
			vertices.push_back({x1, y0, tx1, ty0, fgColor.r, fgColor.g, fgColor.b, fgColor.a});
			vertices.push_back({x1, y1, tx1, ty1, fgColor.r, fgColor.g, fgColor.b, fgColor.a});
			vertices.push_back({x0, y1, tx0, ty1, fgColor.r, fgColor.g, fgColor.b, fgColor.a});

			penX += g.ax;
		}
	}
	// TOFIX: Segfault when exiting nano
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
}

void renderCursor(int cursorX, int cursorY, float deltaTime, int screenW, int screenH) {
	bool visible = true;
	if (o.flags.has(TermFlags::CURSOR_BLINK)) {
		static float time = 0.0f;
		time += deltaTime;

		// Blink on/off every 0.5s
		visible = fmod(time, 1.0f) < 0.5f;
		if (!visible)
			return;
	}

	auto it = glyphs.find(CURSOR_CODEPOINT);
	if (it == glyphs.end())
		return;

	const Glyph& g = it->second;

	float cursorWidth = 2.0f;
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

	constexpr vec4 color = termColorToRGBA(TermColor::DefaultForeGround());
	Vertex verts[6] = {
		{x0, y0, tx0, ty0, color.r, color.g, color.b, color.a}, {x1, y0, tx1, ty0, color.r, color.g, color.b, color.a},
		{x0, y1, tx0, ty1, color.r, color.g, color.b, color.a}, {x1, y0, tx1, ty0, color.r, color.g, color.b, color.a},
		{x1, y1, tx1, ty1, color.r, color.g, color.b, color.a}, {x0, y1, tx0, ty1, color.r, color.g, color.b, color.a},
	};

	glUseProgram(shaderProgram);
	defer(glUseProgram(0));
	GLint screenSizeLoc = glGetUniformLocation(shaderProgram, "screenSize");
	GLint texLoc = glGetUniformLocation(shaderProgram, "tex");

	glUniform2f(screenSizeLoc, float(screenW), float(screenH));
	glUniform1i(texLoc, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, atlasTexture);

	glBindVertexArray(vao);
	defer(glBindVertexArray(0));
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));

	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, 6);
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
}
