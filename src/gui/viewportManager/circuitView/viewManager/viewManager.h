#ifndef viewManager_h
#define viewManager_h

#include "backend/circuit/circuit.h"
#include "backend/position/position.h"
#include "util/vec2.h"

class EventRegister;
class Event;

class ViewManager {
private:
	struct ViewPositioningData {
		ViewPositioningData(FPosition viewCenter, float viewScale) : viewCenter(viewCenter), viewScale(viewScale) {}
		FPosition viewCenter;
		float viewScale;
	};
public:
	// initialization
	ViewManager() : viewCenter(), viewScale(8.0f), aspectRatio(16.0f / 9.0f), pointerViewPosition(0.5f, 0.5f) {}
	void setUpEvents(EventRegister& eventRegister);

	// event output
	inline void connectViewChanged(const std::function<void()>& func) { viewChangedListener = func; }

	// setters
	void setCircuit(Circuit* circuit);
	inline void setAspectRatio(float value) { if (value > 10000.f || value < 0.0001f) return; aspectRatio = value; viewChanged(); }
	inline void setViewCenter(FPosition value) { viewCenter = value; viewChanged(); }

	// getters
	inline float getViewScale() const { return viewScale; }
	inline FPosition getViewCenter() const { return viewCenter; }
	inline const FPosition& getPointerPosition() const { return pointerPosition; }
	inline float getAspectRatio() const { return aspectRatio; }

	// auxiliary getters
	inline float getViewHeight() const { return viewScale / (aspectRatio <= 1.0f ? aspectRatio : 1.0f); }
	inline float getViewWidth() const { return viewScale * (aspectRatio > 1.0f ? aspectRatio : 1.0f); }
	inline FPosition getTopLeft() const { return viewCenter - FVector(getViewWidth() / 2.0f, getViewHeight() / 2.0f); }
	inline FPosition getBottomRight() const { return viewCenter + FVector(getViewWidth() / 2.0f, getViewHeight() / 2.0f); }

	// coordinate system conversion
	inline FPosition viewToGrid(Vec2 view) const { return getTopLeft() + FVector(getViewWidth() * view.x, getViewHeight() * view.y); }
	Vec2 gridToView(FPosition position) const;
	Vec2 gridToView(FVector vector) const;

private:
	// helpers
	void applyLimits();
	inline void viewChanged() {
		if (pointerActive) { // only recompute pointer grid position when pointer is actually over the view
			pointerPosition = viewToGrid(pointerViewPosition);
		}
		if (viewChangedListener) viewChangedListener();
	}

	// input events (called by listeners)
	bool zoom(const Event* event);
	bool pan(const Event* event);
	bool attachAnchor(const Event* event);
	bool dettachAnchor(const Event* event);
	bool pointerMove(const Event* event);
	bool pointerEnterView(const Event* event);
	bool pointerExitView(const Event* event);
private:

	// pointer
	bool anchored = false;
	bool pointerActive = false;
	FPosition pointerPosition;
	Vec2 pointerViewPosition;

	// view data per circuit
	std::map<circuit_id_t, ViewPositioningData> perCircuitViewData;
	circuit_id_t currentCircuitId = 0;
	
	// view
	FPosition viewCenter;
	float viewScale;
	
	float aspectRatio;

	EventRegister* eventRegister = nullptr;

	// event
	std::function<void()> viewChangedListener;
};

#endif /* viewManager_h */
