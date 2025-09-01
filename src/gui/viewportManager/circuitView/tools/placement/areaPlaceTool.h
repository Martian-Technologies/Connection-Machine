#ifndef areaPlaceTool_h
#define areaPlaceTool_h

#include "baseBlockPlacementTool.h"

class AreaPlaceTool : public BaseBlockPlacementTool {
public:
	inline void reset() override final { click = 'n'; }

	void activate() override final;
	void updateElements() override final;

	bool startPlaceBlock(const Event* event);
	bool startDeleteBlocks(const Event* event);

protected:


private:
	Position clickPosition;
	char click = 'n';
};

#endif /* areaPlaceTool_h */
