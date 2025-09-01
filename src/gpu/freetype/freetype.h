#ifndef freetype_h
#define freetype_h

#include <ft2build.h>
#include FT_FREETYPE_H
#include "font.h"

class Freetype {
public:
	Freetype();
	~Freetype();

	static const Freetype& get();

	std::shared_ptr<Font> loadFont(const std::string& fontFile, uint32_t ptSize = 12);

private:
	FT_Library ftLibrary;
};

#endif /* freetype_h */
