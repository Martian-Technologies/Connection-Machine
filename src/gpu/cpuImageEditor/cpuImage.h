#ifndef cpuImageEditor_h
#define cpuImageEditor_h

#include "gpu/freetype/font.h"
#include "util/vec2.h"

class CpuImage {
public:
	struct Pixel {
		Pixel(std::uint8_t r=0, std::uint8_t g=0, std::uint8_t b=0, std::uint8_t a=255) : r(r), g(g), b(b), a(a) {}
		std::uint8_t r;
		std::uint8_t g;
		std::uint8_t b;
		std::uint8_t a;
	};

	CpuImage(Vec2Int size, Pixel color = Pixel()) : img(size.x * size.y, color), imgSize(size) {}
	const std::uint8_t* getData() const { return (std::uint8_t*)img.data(); }

	Vec2Int getSize() const { return imgSize; }

	void fill(Pixel color);
	Pixel getPixel(Vec2Int pos) const { return img[getBufferPos(pos)]; }
	void setPixel(Vec2Int pos, Pixel color) { img[getBufferPos(pos)] = color; }
	void addRect(Vec2Int pos, Vec2Int size, Pixel color, bool mix = true);
	void addOutlinedRect(Vec2Int pos, Vec2Int size, unsigned int outlineThickness, Pixel color, bool mix = true);
	void addLine(Vec2Int startPos, Vec2Int endPos, unsigned int radius, Pixel color, bool mix = true);
	void addCircle(Vec2Int pos, double radius, Pixel color, bool antiAlias = true, bool mix = true);
	void blitAlphaTexture(const std::uint8_t* texture, Vec2Int bufferSize, Vec2Int bufferTexturePos, Vec2Int size, Vec2Int pos, Pixel color, Rotation rotation = Rotation::ZERO);
	Vec2Int blitChar(std::shared_ptr<Font> font, char c, Vec2Int pos, Pixel color, Rotation rotation = Rotation::ZERO);
	void writeString(std::shared_ptr<Font> font, const std::string&, Vec2Int pos, Pixel color, Rotation rotation = Rotation::ZERO);
	std::pair<Vec2Int, Vec2Int> writeStringInArea(std::shared_ptr<Font> font, const std::string& text, Vec2Int pos, Vec2Int areaSize, Pixel color, Rotation rotation = Rotation::ZERO, bool centerV = false, bool centerH= false, unsigned int startingFontSize = 0);

private:
	unsigned int getBufferPos(Vec2Int pos) const { return pos.x + pos.y * imgSize.x; }
	static float pointLineDistanceSquared(const Vec2Int& point, const Vec2Int& lineStart, const Vec2Int& lineEnd);

	std::vector<Pixel> img;
	Vec2Int imgSize;
};

#endif /* cpuImageEditor */
