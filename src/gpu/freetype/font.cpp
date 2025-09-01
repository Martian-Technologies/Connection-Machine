#include "font.h"

Font::Font(FT_Face face, uint32_t fontSize) : face(face), fontSize(fontSize){
	fontFamily = face->family_name;
	createAtlas();
}

Font::~Font() {
	FT_Error error = FT_Done_Face(face);
	if (error) {
		logInfo("Failed to Free FreeType Face: {}", "FreeType", FT_Error_String(error));
	}
}

void Font::setSize(uint32_t ptSize) {
	if (ptSize == fontSize) return;
	FT_Error error = FT_Set_Char_Size(face, 0, ptSize << 6, 0, 0);
	if (error) {
		logInfo("Failed to Set FreeType Font Size: {}", "FreeType", FT_Error_String(error));
	}
	createAtlas();
}

const std::string& Font::getFontFamily() {
	return fontFamily;
}

void Font::createAtlas() {
	atlas.metrics.resize(128 - 32);
    FT_Int32 load_flags = FT_LOAD_RENDER | FT_LOAD_TARGET_(FT_RENDER_MODE_SDF);
	FT_Error error;
    for (int i = 32; i < 128; ++i) {
		error = FT_Load_Char(face, i, load_flags);
        if (error) {
            logInfo("Failed to load glyph {}: {}", "FreeType", (char)i, FT_Error_String(error));
			continue;
        }
		AtlasMetric& metric = atlas.metrics[i - 32];
		metric.advance_x = face->glyph->advance.x;
		metric.advance_y = face->glyph->advance.y;
		metric.width = face->glyph->bitmap.width;
		metric.height = face->glyph->bitmap.rows;
		metric.top = face->glyph->bitmap_top;
		metric.left = face->glyph->bitmap_left;
		metric.offset = atlas.textureWidth;
		atlas.textureHeight = std::max(atlas.textureHeight, face->glyph->bitmap.rows);
		atlas.textureWidth += face->glyph->bitmap.width;
    }

	texture.resize(atlas.textureWidth * atlas.textureHeight);
    for (int i = 32; i < 128; ++i) {
		error = FT_Load_Char(face, i, load_flags);
        if (error) {
            logInfo("Failed to load glyph {}: {}", "FreeType", (char)i, FT_Error_String(error));
			continue;
        }
		AtlasMetric& metric = atlas.metrics[i - 32];
		for (int j = 0; j < metric.height; i++) {
			memcpy(&texture[metric.offset + j * atlas.textureWidth], &face->glyph->bitmap.buffer[j * metric.width], metric.width);
		}
	}
}

const Font::AtlasInfo& Font::getAtlasInfo() {
	return atlas;
}

const std::vector<uint8_t>& Font::getTexture() {
	return texture;
}
