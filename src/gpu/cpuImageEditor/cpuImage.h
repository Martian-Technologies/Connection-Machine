#ifndef cpuImageEditor_h
#define cpuImageEditor_h

#include "gpu/freetype/font.h"
#include "util/vec2.h"

class CpuImage {
public:
	struct Pixel {
		Pixel(unsigned char r=0, unsigned char g=0, unsigned char b=0, unsigned char a=255) : r(r), g(g), b(b), a(a) {}
		unsigned char r;
		unsigned char g;
		unsigned char b;
		unsigned char a;
	};

	CpuImage(Vec2Int size, Pixel color = Pixel()) : img(size.x * size.y, color), imgSize(size) {}
	const unsigned char* getData() const { return (unsigned char*)img.data(); }

	Vec2Int getSize() const { return imgSize; }

	void addRect(Vec2Int pos, Vec2Int size, Pixel color, bool mix = true);
	void blitAlphaTexture(const unsigned char* texture, Vec2Int bufferSize, Vec2Int bufferTexturePos, Vec2Int size, Vec2Int pos, Pixel color, Rotation rotation = Rotation::ZERO);
	Vec2Int blitChar(std::shared_ptr<Font> font, char c, Vec2Int pos, Pixel color, Rotation rotation = Rotation::ZERO);
	void writeString(std::shared_ptr<Font> font, const std::string&, Vec2Int pos, Pixel color, Rotation rotation = Rotation::ZERO);
	void writeStringInArea(std::shared_ptr<Font> font, const std::string& text, Vec2Int pos, Vec2Int areaSize, Pixel color, Rotation rotation = Rotation::ZERO, unsigned int startingFontSize = 0);

private:
	unsigned int getBufferPos(Vec2Int pos) { return pos.x + pos.y * imgSize.x; }

	std::vector<Pixel> img;
	Vec2Int imgSize;
};

#endif /* cpuImageEditor */
