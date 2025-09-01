#ifndef singlePlaceTool_h
#define singlePlaceTool_h

#include "baseBlockPlacementTool.h"

class SinglePlaceTool : public BaseBlockPlacementTool {
public:
	inline void reset() override final { memset(clicks, 'n', 2); updateElements(); }

	void activate() override final;
	void updateElements()  override final;

	bool startPlaceBlock(const Event* event);
	bool stopPlaceBlock(const Event* event);
	bool startDeleteBlocks(const Event* event);
	bool stopDeleteBlocks(const Event* event);
	bool pointerMove(const Event* event);

private:
	char clicks[2] = { 'n', 'n' };
};

#endif /* SinglePlaceTool_h */
