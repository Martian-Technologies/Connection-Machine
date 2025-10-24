#include "cpuImage.h"

void CpuImage::addRect(Vec2Int pos, Vec2Int size, Pixel color, bool mix) {
	if (pos.x < 0 || pos.y < 0 || size.x < 0 || size.y < 0 || (pos.x + size.x) > imgSize.x || (pos.y + size.y) > imgSize.y) {
		logError("addRect try out of range fill.", "CpuImage");
		return;
	}

	for (int x = 0; x < size.x; x++) {
		for (int y = 0; y < size.y; y++) {
			if (mix) {
				Pixel& imgColor = img[getBufferPos(pos + Vec2Int(x, y))];

				float srcA = color.a / 255.0f;
				float dstA = imgColor.a / 255.0f;
				float outA = srcA + dstA * (1.0f - srcA);

				imgColor.r = ((color.r * srcA) + (imgColor.r * dstA * (1.0f - srcA))) / outA;
				imgColor.g = ((color.g * srcA) + (imgColor.g * dstA * (1.0f - srcA))) / outA;
				imgColor.b = ((color.b * srcA) + (imgColor.b * dstA * (1.0f - srcA))) / outA;
				imgColor.a = outA * 255.0f;
			} else {
				img[getBufferPos(pos + Vec2Int(x, y))] = color;
			}
		}
	}
}

void CpuImage::blitAlphaTexture(
	const unsigned char* texture,
	Vec2Int bufferSize,
	Vec2Int bufferTexturePos,
	Vec2Int size,
	Vec2Int pos,
	Pixel color,
	Rotation rotation
) {
	if (pos.x < 0 || pos.y < 0 || size.x < 0 || size.y < 0 || (pos.x + size.x) > imgSize.x || (pos.y + size.y) > imgSize.y) {
		logError("blitAlphaTexture try out of range blit.", "CpuImage");
		return;
	}

	for (int x = 0; x < size.x; x++) {
		for (int y = 0; y < size.y; y++) {
			Vec2Int bufferPos = Vec2Int(x, y) + bufferTexturePos;
			unsigned char textureAlpha = texture[bufferPos.x + bufferPos.y * bufferSize.x];

			Vec2Int imgPixelPos = pos + rotateVector(Vec2Int(x, y), rotation);

			Pixel& imgColor = img[getBufferPos(imgPixelPos)];

			float srcA = (color.a / 255.0f) * (textureAlpha / 255.0f);
			float dstA = imgColor.a / 255.0f;
			float outA = srcA + dstA * (1.0f - srcA);

			imgColor.r = ((color.r * srcA) + (imgColor.r * dstA * (1.0f - srcA))) / outA;
			imgColor.g = ((color.g * srcA) + (imgColor.g * dstA * (1.0f - srcA))) / outA;
			imgColor.b = ((color.b * srcA) + (imgColor.b * dstA * (1.0f - srcA))) / outA;
			imgColor.a = outA * 255.0f;
		}
	}
}

Vec2Int CpuImage::blitChar(std::shared_ptr<Font> font, char c, Vec2Int pos, Pixel color, Rotation rotation) {
	if (font->getAtlasInfo().metrics.size() <= c - 32) {
		logInfo("Char {} out of font metrics range.", "CpuImage", c - 32);
		return {0, 0};
	}
	const Font::AtlasMetric& atlasMetric = font->getAtlasInfo().metrics[c - 32];
	blitAlphaTexture(
		font->getTexture().data(),
		font->getAtlasInfo().textureSize,
		atlasMetric.offset,
		atlasMetric.size,
		pos + rotateVector(atlasMetric.bearing, rotation),
		color,
		rotation
	);
	return rotateVector(Vec2Int(atlasMetric.advance.x, 0), rotation);
}

void CpuImage::writeString(std::shared_ptr<Font> font, const std::string& text, Vec2Int pos, Pixel color, Rotation rotation) {
	Vec2Int offset;
	for (char c : text) {
		offset = offset + blitChar(font, c, pos + offset, color, rotation);
	}
}

void CpuImage::writeStringInArea(std::shared_ptr<Font> font, const std::string& text, Vec2Int pos, Vec2Int areaSize, Pixel color, Rotation rotation, unsigned int startingFontSize) {
	if (!areaSize.withinArea({0, 0}, imgSize)) {
		logError("Cant have writeStringInArea areaSize {}, be larger than img size {}", "CpuImage", areaSize, imgSize);
		return;
	}

	if (startingFontSize == 0) startingFontSize = font->getSize();

	Vec2Int rotatedAreaSize = areaSize;
	if (rotation & 1) rotatedAreaSize = Vec2Int(rotatedAreaSize.y, rotatedAreaSize.x);

	while (true) {
		std::optional<Font::AtlasInfo> atlasInfo = font->getAtlasInfoDifferentSizeText(startingFontSize);
		if (!atlasInfo) {
			logError("Failed to get AtlasInfo", "CpuImage");
			return;
		}

		Vec2Int nextOriginPos;
		bool failed = false;
		int lowestY = 0;

		for (char c : text) {
			if (atlasInfo->metrics.size() <= c - 32) {
				logInfo("Char {} out of font metrics range.", "CpuImage", c - 32);
				continue;
			}
			const Font::AtlasMetric& atlasMetric = atlasInfo->metrics[c - 32];

			if (!atlasMetric.size.withinArea({0, 0}, rotatedAreaSize)) {
				failed = true;
				break;
			}

			Vec2Int textOrigin = nextOriginPos + atlasMetric.bearing;
			Vec2Int textOtherCorner = textOrigin + atlasMetric.size;

			if (textOtherCorner.x > rotatedAreaSize.x) {
				nextOriginPos.x = 0;
				nextOriginPos.y += atlasInfo->newlineHeight;

				textOrigin = nextOriginPos + atlasMetric.bearing;
				textOtherCorner = textOrigin + atlasMetric.size;
			}

			if (textOrigin.y < 0) {
				lowestY -= textOrigin.y;
				nextOriginPos.y -= textOrigin.y;
				textOtherCorner.y -= textOrigin.y;
				textOrigin.y = 0;
			}
			if (textOtherCorner.y > lowestY) lowestY = textOtherCorner.y;
			if (lowestY > rotatedAreaSize.y) {
				failed = true;
				break;
			}

			nextOriginPos.x += atlasMetric.advance.x;
		}
		if (!failed) break;
		if (startingFontSize <= 1) {
			logError("Could not fix text {} in area {}", "CpuImage", text, areaSize);
			return;
		}
		startingFontSize--;
	}

	uint32_t oldFontSize = font->getSize();
	font->setSize(startingFontSize);
	const Font::AtlasInfo& atlasInfo = font->getAtlasInfo();

	Vec2Int nextOriginPos;
	bool failed = false;
	int lowestY = 0;

	std::string line;

	for (char c : text) {
		if (atlasInfo.metrics.size() <= c - 32) {
			logInfo("Char {} out of font metrics range.", "CpuImage", c - 32);
			continue;
		}
		const Font::AtlasMetric& atlasMetric = atlasInfo.metrics[c - 32];

		if (!atlasMetric.size.withinArea({0, 0}, rotatedAreaSize)) {
			failed = true;
			break;
		}

		Vec2Int textOrigin = nextOriginPos + atlasMetric.bearing;
		Vec2Int textOtherCorner = textOrigin + atlasMetric.size;

		if (textOtherCorner.x > rotatedAreaSize.x) {
			writeString(font, line, pos + rotateVectorWithArea(Vec2Int(0, nextOriginPos.y), rotatedAreaSize, rotation), color, rotation);
			line = "";

			nextOriginPos.x = 0;
			nextOriginPos.y += atlasInfo.newlineHeight;

			textOrigin = nextOriginPos + atlasMetric.bearing;
			textOtherCorner = textOrigin + atlasMetric.size;
		}

		if (textOrigin.y < 0) {
			lowestY -= textOrigin.y;
			nextOriginPos.y -= textOrigin.y;
			textOtherCorner.y -= textOrigin.y;
			textOrigin.y = 0;
		}
		if (textOtherCorner.y > lowestY) lowestY = textOtherCorner.y;
		if (lowestY > rotatedAreaSize.y) {
			failed = true;
			break;
		}

		nextOriginPos.x += atlasMetric.advance.x;
		line += c;
	}
	if (failed) {
		logError("Could not not re fit text {} in area {} even tho fitting was already found.", "CpuImage", text, areaSize);
		return;
	}
	if (!line.empty()) {
		writeString(font, line, pos + rotateVectorWithArea(Vec2Int(0, nextOriginPos.y), rotatedAreaSize, rotation), color, rotation);
	}

	font->setSize(oldFontSize); // make sure to not break the font size
}
