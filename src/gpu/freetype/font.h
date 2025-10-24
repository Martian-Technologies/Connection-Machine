#ifndef font_h
#define font_h

#include <ft2build.h>
#include FT_FREETYPE_H

#include "util/vec2.h"

class Font {
public:
	struct AtlasMetric {
		Vec2Int advance;
		Vec2Int size;
		Vec2Int bearing; // down is positive
		Vec2Int offset;
	};

	struct AtlasInfo {
		std::vector<AtlasMetric> metrics;
		Vec2Int textureSize;
		uint32_t newlineHeight;
	};

	Font(FT_Face face, uint32_t fontSize);
	~Font();

	const std::string& getFontFamily();

	void setSize(uint32_t ptSize);
	uint32_t getSize() const { return fontSize; }

	void createAtlas();

	const AtlasInfo& getAtlasInfo() const;
	const std::vector<uint8_t>& getTexture() const;

	std::optional<Font::AtlasInfo> getAtlasInfoDifferentSizeText(uint32_t ptSize);

private:
	FT_Face face;
	AtlasInfo atlas;
	std::vector<uint8_t> texture;
	uint32_t fontSize;
	std::string fontFamily;
};
#endif /* font_h */
