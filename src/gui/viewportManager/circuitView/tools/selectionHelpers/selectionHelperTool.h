#ifndef selectionHelperTool_h
#define selectionHelperTool_h

#include "backend/selection.h"
#include "../circuitToolHelper.h"

class SelectionHelperTool : public CircuitToolHelper {
public:
	inline bool canMakeEdits() const override final { return false; }

	virtual void undoFinished() { createdSelection = nullptr; };
	inline bool isFinished() const { return (bool)createdSelection; }
	inline const SharedSelection getSelection() const { return createdSelection; }
	virtual void setSelectionToFollow(SharedSelection selectionToFollow) { logError("Following a selection is not implemented for this tool", "SelectionHelperTool"); }

protected:
	void reset() override { CircuitToolHelper::reset(); createdSelection = nullptr; }
	void activate() override { CircuitToolHelper::activate(); }
	void finished(SharedSelection createdSelection) {
		this->createdSelection = createdSelection;
		toolStackInterface->popTool();
	}
private:
	SharedSelection createdSelection;
};

typedef std::shared_ptr<SelectionHelperTool> SharedSelectionHelperTool;

#endif /* selectionHelperTool_h */
