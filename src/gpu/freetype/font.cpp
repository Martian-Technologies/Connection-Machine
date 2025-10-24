#include "font.h"

Font::Font(FT_Face face, uint32_t fontSize) : face(face), fontSize(fontSize) {
	fontFamily = face->family_name;
	createAtlas();
}

Font::~Font() {
	FT_Error error = FT_Done_Face(face);
	if (error) {
		logInfo("Failed to free FreeType face: {}", "FreeType", FT_Error_String(error));
	}
}

void Font::setSize(uint32_t ptSize) {
	if (ptSize == fontSize) return;
	FT_Error error = FT_Set_Char_Size(face, 0, ptSize << 6, 0, 0);
	if (error) {
		logInfo("Failed to set FreeType font size: {}", "FreeType", FT_Error_String(error));
		return;
	}
	fontSize = ptSize;
	createAtlas();
}

const std::string& Font::getFontFamily() { return fontFamily; }

std::optional<Font::AtlasInfo> Font::getAtlasInfoDifferentSizeText(uint32_t ptSize) {
	if (fontSize == ptSize) return getAtlasInfo();
	FT_Error error = FT_Set_Char_Size(face, 0, ptSize << 6, 0, 0);
	if (error) {
		logInfo("Failed to set FreeType font size: {}", "FreeType::getAtlasInfoDifferentSizeText", FT_Error_String(error));
		return std::nullopt;
	}
	AtlasInfo newAtlas;
	newAtlas.metrics.resize(128 - 32);
	newAtlas.newlineHeight = face->size->metrics.height >> 6;
	FT_Int32 load_flags = FT_LOAD_RENDER;
	for (int i = 32; i < 128; ++i) {
		error = FT_Load_Char(face, i, load_flags);
		if (error) {
			logInfo("Failed to load glyph {}: {}", "FreeType", (char)i, FT_Error_String(error));
			continue;
		}
		AtlasMetric& metric = newAtlas.metrics[i - 32];
		metric.advance = Vec2Int(face->glyph->advance.x >> 6, face->glyph->advance.y >> 6);
		metric.size = Vec2Int(face->glyph->bitmap.width, face->glyph->bitmap.rows);
		metric.bearing = Vec2Int(face->glyph->bitmap_left, -face->glyph->bitmap_top);
		metric.offset = Vec2Int(newAtlas.textureSize.x, 0);
		newAtlas.textureSize.x += face->glyph->bitmap.width;
		newAtlas.textureSize.y = max(newAtlas.textureSize.y, face->glyph->bitmap.rows);
	}
	error = FT_Set_Char_Size(face, 0, this->fontSize << 6, 0, 0);
	if (error) {
		logInfo("Failed to reset FreeType font size: {}", "FreeType::getAtlasInfoDifferentSizeText", FT_Error_String(error));
		return std::nullopt;
	}
	return newAtlas;
}

void Font::createAtlas() {
	atlas.textureSize = {0, 0};
	atlas.metrics.resize(128 - 32);
	atlas.newlineHeight = face->size->metrics.height >> 6;
	FT_Int32 load_flags = FT_LOAD_RENDER;
	FT_Error error;
	for (int i = 32; i < 128; ++i) {
		error = FT_Load_Char(face, i, load_flags);
		if (error) {
			logInfo("Failed to load glyph {}: {}", "FreeType", (char)i, FT_Error_String(error));
			continue;
		}
		AtlasMetric& metric = atlas.metrics[i - 32];
		metric.advance = Vec2Int(face->glyph->advance.x >> 6, face->glyph->advance.y >> 6);
		metric.size = Vec2Int(face->glyph->bitmap.width, face->glyph->bitmap.rows);
		metric.bearing = Vec2Int(face->glyph->bitmap_left, -face->glyph->bitmap_top);
		metric.offset = Vec2Int(atlas.textureSize.x, 0);
		atlas.textureSize.x += face->glyph->bitmap.width;
		atlas.textureSize.y = max(atlas.textureSize.y, face->glyph->bitmap.rows);
	}

	texture.resize(atlas.textureSize.x * atlas.textureSize.y);
	for (int i = 32; i < 128; ++i) {
		error = FT_Load_Char(face, i, load_flags);
		if (error) {
			logInfo("Failed to load glyph {}: {}", "FreeType", (char)i, FT_Error_String(error));
			continue;
		}
		AtlasMetric& metric = atlas.metrics[i - 32];
		for (int j = 0; j < metric.size.y; j++) {
			memcpy(&texture[metric.offset.x + j * atlas.textureSize.x], &face->glyph->bitmap.buffer[j * metric.size.x], metric.size.x);
		}
	}
}

const Font::AtlasInfo& Font::getAtlasInfo() const { return atlas; }

const std::vector<uint8_t>& Font::getTexture() const { return texture; }
