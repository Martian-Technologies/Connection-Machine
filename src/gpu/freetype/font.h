#ifndef font_h_
#define font_h_

#include <ft2build.h>
#include FT_FREETYPE_H


class Font {
public:
	struct AtlasMetric {
		uint32_t advance_x 	= 0;
		uint32_t advance_y 	= 0;
		uint32_t width 		= 0;
		uint32_t height 	= 0;
		uint32_t left 		= 0;
		uint32_t top 		= 0;
		uint32_t offset 	= 0;
	};

	struct AtlasInfo {
		std::vector<AtlasMetric> metrics;
		uint32_t textureWidth = 0;
		uint32_t textureHeight = 0;
	};

	Font(FT_Face face, uint32_t fontSize);
	~Font();

	const std::string& getFontFamily();

	void setSize(uint32_t ptSize);

	void createAtlas();

	const AtlasInfo& getAtlasInfo();
	const std::vector<uint8_t>& getTexture();

private:
	FT_Face face;
	AtlasInfo atlas;
	std::vector<uint8_t> texture;
	uint32_t fontSize;
	std::string fontFamily;
};
#endif
