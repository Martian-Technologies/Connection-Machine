#ifndef tileSet_h
#define tileSet_h

#include "util/vec2.h"
#include "backend/evaluator/logicState.h"

class TileSetInfo {
public:
	TileSetInfo(int cellSize, int numBlocks, int numStates)
		: cellPixelSize(cellSize, cellSize), numBlocks(numBlocks), cellUVSize(1.0f / numBlocks, 1.0f / numStates) { }

	inline Vec2Int getCellPixelSize() const { return cellPixelSize; }
	inline Vec2Int getTopLeftPixel(int index, logic_state_t state) const { return Vec2Int(index * cellPixelSize.x, static_cast<unsigned char>(state) * cellPixelSize.y); }

	inline Vec2 getCellUVSize() const { return cellUVSize; }
	inline Vec2 getTopLeftUV(int index, int state) const { return Vec2(index * cellUVSize.x, static_cast<unsigned int>(state) * cellUVSize.y); }

private:
	Vec2Int cellPixelSize;
	Vec2 cellUVSize;
	int numBlocks;
};


#endif /* tileSet_h */
