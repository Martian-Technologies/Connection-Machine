#ifndef areaMoveTool_h
#define areaMoveTool_h

#include "selectionHelperTool.h"

class AreaCreationTool : public SelectionHelperTool {
public:
	void reset() override final;
	void activate() override final;
	void updateElements() override final;

	bool click(const Event* event);
	bool unclick(const Event* event);

private:
	Position originPosition;
	bool hasOrigin = false;;
};

#endif /* tensorMoveTool_h */
