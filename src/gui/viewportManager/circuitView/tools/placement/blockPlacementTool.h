#ifndef blockPlacementTool_h
#define blockPlacementTool_h

#include "baseBlockPlacementTool.h"
#include "../circuitTool.h"

class BlockPlacementTool : public CircuitTool {
public:
	BlockPlacementTool(Environment& environment);

	void activate() override final;

	static inline std::vector<std::string> getModes_() { return { "Single", "Area" }; }
	static inline std::string getPath_() { return "placement"; }
	inline std::vector<std::string> getModes() const override final { return getModes_(); }
	inline std::string getPath() const override final { return getPath_(); }
	void setMode(const std::string& mode) override final;

	void selectBlock(BlockType selectedBlock) {
		this->selectedBlock = selectedBlock;
		if (activePlacementTool) activePlacementTool->selectBlock(selectedBlock);
	}

	void setOrientation(Orientation orientation) {
		this->orientation = orientation;
		if (activePlacementTool) activePlacementTool->setOrientation(orientation);
	}

	BlockType getSelectedBlock() const {
		return selectedBlock;
	}

private:
	std::string mode = "None";
	SharedBaseBlockPlacementTool activePlacementTool = nullptr;
	BlockType selectedBlock = BlockType::NONE;
	Orientation orientation = Orientation();
};

typedef std::shared_ptr<BlockPlacementTool> SharedBlockPlacementTool;

#endif /* blockPlacementTool_h */
