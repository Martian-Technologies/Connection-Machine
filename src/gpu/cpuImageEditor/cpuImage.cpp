#include "cpuImage.h"

void CpuImage::fill(Pixel color) {
	for (Pixel& pixel : img) {
		pixel = color;
	}
}

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

void CpuImage::addOutlinedRect(Vec2Int pos, Vec2Int size, unsigned int outlineThickness, Pixel color, bool mix) {
	if (outlineThickness == 0) {
		return;
	}
	addRect(pos, Vec2Int(size.x, outlineThickness), color, mix);
	addRect(pos + Vec2Int(0, size.y - outlineThickness), Vec2Int(size.x, outlineThickness), color, mix);
	addRect(pos + Vec2Int(0, outlineThickness), Vec2Int(outlineThickness, size.y - 2 * outlineThickness), color, mix);
	addRect(pos + Vec2Int(size.x - outlineThickness, outlineThickness), Vec2Int(outlineThickness, size.y - 2 * outlineThickness), color, mix);
}

float CpuImage::pointLineDistanceSquared(const Vec2Int& point, const Vec2Int& lineStart, const Vec2Int& lineEnd) {
	int l2 = lineStart.distanceToSquared(lineEnd);
	if (l2 == 0) return point.distanceToSquared(lineStart);
	float t = std::clamp(static_cast<float>((point - lineStart).dot(lineEnd - lineStart)) / l2, 0.0f, 1.0f);
	Vec2 projection = Vec2(lineEnd - lineStart) * t + lineStart;
	return projection.distanceToSquared(point);
}

void CpuImage::addLine(Vec2Int startPos, Vec2Int endPos, unsigned int radius, Pixel color, bool mix) {
	int minX = std::max(0, std::min(startPos.x, endPos.x) - static_cast<int>(radius));
	int maxX = std::min(imgSize.x - 1, std::max(startPos.x, endPos.x) + static_cast<int>(radius));
	int minY = std::max(0, std::min(startPos.y, endPos.y) - static_cast<int>(radius));
	int maxY = std::min(imgSize.y - 1, std::max(startPos.y, endPos.y) + static_cast<int>(radius));
	for (int x = minX; x <= maxX; x++) {
		for (int y = minY; y <= maxY; y++) {
			Vec2Int point = Vec2Int(x, y);
			float distSq = pointLineDistanceSquared(point, startPos, endPos);
			if (distSq > (radius + 1) * (radius + 1)) continue;
			float alpha = 1.0f;
			double dist = sqrt(distSq);
			alpha = static_cast<float>(std::clamp(radius - dist, 0.0, 1.0));
			Pixel drawColor = color;
			drawColor.a = static_cast<std::uint8_t>(drawColor.a * alpha);
			if (point.x < 0 || point.y < 0 || point.x >= imgSize.x || point.y >= imgSize.y) continue;
			if (mix) {
				Pixel& imgColor = img[getBufferPos(point)];

				float srcA = drawColor.a / 255.0f;
				float dstA = imgColor.a / 255.0f;
				float outA = srcA + dstA * (1.0f - srcA);

				imgColor.r = ((drawColor.r * srcA) + (imgColor.r * dstA * (1.0f - srcA))) / outA;
				imgColor.g = ((drawColor.g * srcA) + (imgColor.g * dstA * (1.0f - srcA))) / outA;
				imgColor.b = ((drawColor.b * srcA) + (imgColor.b * dstA * (1.0f - srcA))) / outA;
				imgColor.a = outA * 255.0f;
			} else {
				img[getBufferPos(point)] = drawColor;
			}
		}
	}
}

void CpuImage::addCircle(Vec2Int pos, double radius, Pixel color, bool antiAlias, bool mix) {
	int xStart = static_cast<int>(-radius - 1);
	int xEnd = static_cast<int>(radius + 1);
	int yStart = static_cast<int>(-radius - 1);
	int yEnd = static_cast<int>(radius + 1);
	for (int x = xStart; x <= xEnd; x++) {
		for (int y = yStart; y <= yEnd; y++) {
			double distSq = static_cast<double>(x * x + y * y);
			if (distSq > (radius + 1) * (radius + 1)) continue;
			float alpha = 1.0f;
			if (antiAlias) {
				double dist = sqrt(distSq);
				alpha = static_cast<float>(std::clamp(radius - dist, 0.0, 1.0));
			} else {
				if (distSq > radius * radius) continue;
			}
			Pixel drawColor = color;
			drawColor.a = static_cast<std::uint8_t>(drawColor.a * alpha);
			Vec2Int drawPos = Vec2Int(pos.x + x, pos.y + y);
			if (drawPos.x < 0 || drawPos.y < 0 || drawPos.x >= imgSize.x || drawPos.y >= imgSize.y) continue;
			if (mix) {
				Pixel& imgColor = img[getBufferPos(drawPos)];

				float srcA = drawColor.a / 255.0f;
				float dstA = imgColor.a / 255.0f;
				float outA = srcA + dstA * (1.0f - srcA);

				imgColor.r = ((drawColor.r * srcA) + (imgColor.r * dstA * (1.0f - srcA))) / outA;
				imgColor.g = ((drawColor.g * srcA) + (imgColor.g * dstA * (1.0f - srcA))) / outA;
				imgColor.b = ((drawColor.b * srcA) + (imgColor.b * dstA * (1.0f - srcA))) / outA;
				imgColor.a = outA * 255.0f;
			} else {
				img[getBufferPos(drawPos)] = drawColor;
			}
		}
	}
}

void CpuImage::blitAlphaTexture(
	const std::uint8_t* texture,
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
			std::uint8_t textureAlpha = texture[bufferPos.x + bufferPos.y * bufferSize.x];

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

std::pair<Vec2Int, Vec2Int> CpuImage::writeStringInArea(std::shared_ptr<Font> font, const std::string& text, Vec2Int pos, Vec2Int areaSize, Pixel color, Rotation rotation, bool centerV, bool centerH, unsigned int startingFontSize) {
	if (!areaSize.withinArea({0, 0}, imgSize)) {
		logError("Cant have writeStringInArea areaSize {}, be larger than img size {}", "CpuImage", areaSize, imgSize);
		return { {0, 0}, {0, 0} };
	}

	if (startingFontSize == 0) startingFontSize = font->getSize();

	Vec2Int rotatedAreaSize = areaSize;
	if (rotation & 1) rotatedAreaSize = Vec2Int(rotatedAreaSize.y, rotatedAreaSize.x);

	int startingYOffsetToCenterText = 0;

	while (true) {
		std::optional<Font::AtlasInfo> atlasInfo = font->getAtlasInfoDifferentSizeText(startingFontSize);
		if (!atlasInfo) {
			logError("Failed to get AtlasInfo", "CpuImage");
			return { {0, 0}, {0, 0} };
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
		if (!failed) {
			if ((rotation & 1 && centerH) || (!(rotation & 1) && centerV)) startingYOffsetToCenterText = (rotatedAreaSize.y - lowestY)/2;
			break;
		}
		if (startingFontSize <= 1) {
			logError("Could not fix text {} in area {}", "CpuImage", text, areaSize);
			return { {0, 0}, {0, 0} };
		}
		startingFontSize--;
	}

	uint32_t oldFontSize = font->getSize();
	font->setSize(startingFontSize);
	const Font::AtlasInfo& atlasInfo = font->getAtlasInfo();

	const bool centerLine = (rotation & 1 && centerV) || (!(rotation & 1) && centerH);

	Vec2Int nextOriginPos;
	std::string line;
	int lineLength = 0;
	Vec2Int lineMin = rotatedAreaSize;
	Vec2Int lineMax = Vec2Int(0, 0);
	bool lineHasGlyph = false;
	Vec2Int usedMin = rotatedAreaSize;
	Vec2Int usedMax = Vec2Int(0, 0);
	bool usedInitialized = false;

	for (char c : text) {
		if (atlasInfo.metrics.size() <= c - 32) {
			logInfo("Char {} out of font metrics range.", "CpuImage", c - 32);
			continue;
		}
		const Font::AtlasMetric& atlasMetric = atlasInfo.metrics[c - 32];

		Vec2Int textOrigin = nextOriginPos + atlasMetric.bearing;
		Vec2Int textOtherCorner = textOrigin + atlasMetric.size;

		if (textOtherCorner.x > rotatedAreaSize.x) {
			if (!line.empty()) {
				int lineStartX = centerLine ? (rotatedAreaSize.x - lineLength)/2 : 0;
				Vec2Int linePos = Vec2Int(lineStartX, nextOriginPos.y + startingYOffsetToCenterText);
				writeString(font, line, pos + rotateVectorWithArea(linePos, rotatedAreaSize, rotation), color, rotation);

				if (lineHasGlyph) {
					Vec2Int minShifted = Vec2Int(lineMin.x + lineStartX, lineMin.y + startingYOffsetToCenterText);
					Vec2Int maxShifted = Vec2Int(lineMax.x + lineStartX, lineMax.y + startingYOffsetToCenterText);
					if (!usedInitialized) {
						usedMin = minShifted;
						usedMax = maxShifted;
						usedInitialized = true;
					} else {
						usedMin.x = std::min(usedMin.x, minShifted.x);
						usedMin.y = std::min(usedMin.y, minShifted.y);
						usedMax.x = std::max(usedMax.x, maxShifted.x);
						usedMax.y = std::max(usedMax.y, maxShifted.y);
					}
				}
			}

			line.clear();
			lineLength = 0;
			lineMin = rotatedAreaSize;
			lineMax = Vec2Int(0, 0);
			lineHasGlyph = false;

			nextOriginPos.x = 0;
			nextOriginPos.y += atlasInfo.newlineHeight;

			textOrigin = nextOriginPos + atlasMetric.bearing;
			textOtherCorner = textOrigin + atlasMetric.size;
		}

		if (textOrigin.y < 0) {
			nextOriginPos.y -= textOrigin.y;
			textOtherCorner.y -= textOrigin.y;
			textOrigin.y = 0;
		}
		if (atlasMetric.size.x > 0 && atlasMetric.size.y > 0) {
			if (!lineHasGlyph) {
				lineMin = textOrigin;
				lineMax = textOtherCorner;
				lineHasGlyph = true;
			} else {
				lineMin.x = std::min(lineMin.x, textOrigin.x);
				lineMin.y = std::min(lineMin.y, textOrigin.y);
				lineMax.x = std::max(lineMax.x, textOtherCorner.x);
				lineMax.y = std::max(lineMax.y, textOtherCorner.y);
			}
		}
		nextOriginPos.x += atlasMetric.advance.x;
		line += c;
		lineLength = textOtherCorner.x;
	}
	if (!line.empty()) {
		int lineStartX = centerLine ? (rotatedAreaSize.x - lineLength)/2 : 0;
		Vec2Int linePos = Vec2Int(lineStartX, nextOriginPos.y + startingYOffsetToCenterText);
		writeString(font, line, pos + rotateVectorWithArea(linePos, rotatedAreaSize, rotation), color, rotation);

		if (lineHasGlyph) {
			Vec2Int minShifted = Vec2Int(lineMin.x + lineStartX, lineMin.y + startingYOffsetToCenterText);
			Vec2Int maxShifted = Vec2Int(lineMax.x + lineStartX, lineMax.y + startingYOffsetToCenterText);
			if (!usedInitialized) {
				usedMin = minShifted;
				usedMax = maxShifted;
				usedInitialized = true;
			} else {
				usedMin.x = std::min(usedMin.x, minShifted.x);
				usedMin.y = std::min(usedMin.y, minShifted.y);
				usedMax.x = std::max(usedMax.x, maxShifted.x);
				usedMax.y = std::max(usedMax.y, maxShifted.y);
			}
		}
	}

	font->setSize(oldFontSize); // make sure to not break the font size

	if (!usedInitialized) {
		return { {0, 0}, {0, 0} };
	}

	Vec2Int localMin = usedMin;
	Vec2Int localMax = usedMax;
	Vec2Int localSize = localMax - localMin;
	if (localSize.x <= 0 || localSize.y <= 0) {
		return { {0, 0}, {0, 0} };
	}

	Vec2Int corners[4] = {
		localMin,
		{ localMax.x - 1, localMin.y },
		{ localMin.x, localMax.y - 1 },
		{ localMax.x - 1, localMax.y - 1 }
	};

	Vec2Int worldMin = pos + rotateVectorWithArea(corners[0], rotatedAreaSize, rotation);
	Vec2Int worldMax = worldMin;
	for (int i = 1; i < 4; ++i) {
		Vec2Int worldPoint = pos + rotateVectorWithArea(corners[i], rotatedAreaSize, rotation);
		worldMin.x = std::min(worldMin.x, worldPoint.x);
		worldMin.y = std::min(worldMin.y, worldPoint.y);
		worldMax.x = std::max(worldMax.x, worldPoint.x);
		worldMax.y = std::max(worldMax.y, worldPoint.y);
	}

	Vec2Int worldSize = worldMax - worldMin + Vec2Int(1, 1);
	return { worldMin, worldSize };
}
