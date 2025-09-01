#ifndef baseBlockPlacementTool_h
#define baseBlockPlacementTool_h

#include "../circuitToolHelper.h"
#include "backend/position/position.h"

class BaseBlockPlacementTool : public CircuitToolHelper {
public:
	// This will also tell the tool to reset.
	inline void selectBlock(BlockType selectedBlock) { this->selectedBlock = selectedBlock; updateElements(); }
	inline void setRotation(Orientation orientation) { this->orientation = orientation; updateElements(); }

	void activate() override;

	bool rotateBlockCW(const Event* event);
	bool rotateBlockCCW(const Event* event);

protected:
	BlockType selectedBlock = BlockType::NONE;
	Orientation orientation = Orientation();
};

typedef std::shared_ptr<BaseBlockPlacementTool> SharedBaseBlockPlacementTool;

#endif /* baseBlockPlacementTool_h */
