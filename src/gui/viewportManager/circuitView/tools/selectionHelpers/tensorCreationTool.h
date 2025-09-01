#ifndef tensorMoveTool_h
#define tensorMoveTool_h

#include "backend/selection.h"
#include "selectionHelperTool.h"

class TensorCreationTool : public SelectionHelperTool {
public:
	void reset() override final;
	void activate() override final;
	void updateElements() override final;

	void undoFinished() override final;

	void setSelectionToFollow(SharedSelection selectionToFollow) override final;

	bool click(const Event* event);
	bool unclick(const Event* event = nullptr);
	bool confirm(const Event* event);

private:
	SharedSelection selection;
	Position originPosition;
	Vector step;
	int tensorStage = -1;
	bool followingSelection = false;
	std::vector<dimensional_selection_size_t> selectionToFollow;
};

#endif /* tensorMoveTool_h */
