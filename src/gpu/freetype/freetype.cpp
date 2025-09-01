#include "gpu/freetype/freetype.h"

Freetype::Freetype() {
	FT_Error error = FT_Init_FreeType(&ftLibrary);
	if (error) {
		logError("Failed to Initalize FreeType: {}", "FreeType", FT_Error_String(error));
		exit(1);
	}
}

Freetype::~Freetype() {
	FT_Error error = FT_Done_FreeType(ftLibrary);
	if (error) {
		logInfo("Failed to Deinitalize FreeType: {}", "FreeType", FT_Error_String(error));
	}
}

const Freetype& Freetype::get() {
	static Freetype freetype;
	return freetype;
}

std::shared_ptr<Font> Freetype::loadFont(const std::string& fontFile, uint32_t ptSize) {
	FT_Face face;

	FT_Error error = FT_New_Face(ftLibrary, fontFile.c_str(), 0, &face);
	if (error) {
		logError("Failed to Create FreeType Face: {}", "FreeType", FT_Error_String(error));
		exit(1);
	}
	error = FT_Set_Char_Size(face, 0, ptSize << 6, 0, 0);
	if (error) {
		logInfo("Failed to Set FreeType Font Size: {}", "FreeType", FT_Error_String(error));
	}
	return std::make_shared<Font>(face, ptSize);
}

