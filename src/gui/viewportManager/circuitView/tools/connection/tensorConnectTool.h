#ifndef tensorConnectTool_h
#define tensorConnectTool_h

#include "../selectionHelpers/selectionHelperTool.h"

class TensorConnectTool : public CircuitToolHelper {
public:
	void reset() override final;

	void activate() override final;

	void updateElements() override final;

	// bool click(const Event* event);
	bool unclick(const Event* event);
	bool confirm(const Event* event);
	bool invertMode(const Event* event);

private:
	bool doingDisconnect = false;
	bool placingOutout = true;
	SharedSelectionHelperTool activeOutputSelectionHelper;
	SharedSelectionHelperTool activeInputSelectionHelper;
};

#endif /* tensorConnectTool_h */
