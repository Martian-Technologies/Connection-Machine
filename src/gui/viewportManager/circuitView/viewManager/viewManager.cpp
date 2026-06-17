#include "viewManager.h"

#include "../events/eventRegister.h"
#include "../events/customEvents.h"
#include "backend/backend.h"

void ViewManager::setUpEvents(EventRegister& eventRegister) {
	this->eventRegister = &eventRegister;
	eventRegister.registerFunction("view zoom", std::bind(&ViewManager::zoom, this, std::placeholders::_1));
	eventRegister.registerFunction("view pan", std::bind(&ViewManager::pan, this, std::placeholders::_1));
	eventRegister.registerFunction("View Attach Anchor", std::bind(&ViewManager::attachAnchor, this, std::placeholders::_1));
	eventRegister.registerFunction("View Dettach Anchor", std::bind(&ViewManager::dettachAnchor, this, std::placeholders::_1));
	eventRegister.registerFunction("Pointer Move On View", std::bind(&ViewManager::pointerMove, this, std::placeholders::_1));
	eventRegister.registerFunction("pointer enter view", std::bind(&ViewManager::pointerEnterView, this, std::placeholders::_1));
	eventRegister.registerFunction("pointer exit view", std::bind(&ViewManager::pointerExitView, this, std::placeholders::_1));
}

void ViewManager::setCircuit(circuit_id_t circuitId) {
	const Circuit* circuit = backend.getCircuitManager().getCircuit(circuitId);
	if (!circuit) circuitId = 0;
	if (circuitId == currentCircuitId) return;

	if (backend.getCircuitManager().getCircuit(currentCircuitId) != nullptr) {
		perCircuitViewData.insert_or_assign(currentCircuitId, ViewPositioningData(viewCenter, viewScale));
	}

	FPosition oldCenter = viewCenter;
	float oldScale = viewScale;

	if (circuitId != 0) {
		currentCircuitId = circuitId;
		auto iter = perCircuitViewData.find(circuitId);
		if (iter == perCircuitViewData.end()) {
			focus();
		} else {
			viewCenter = iter->second.viewCenter;
			viewScale = iter->second.viewScale;
		}
	} else {
	    currentCircuitId = 0;
	}

	applyLimits();
	if (oldCenter.x != viewCenter.x || oldCenter.y != viewCenter.y || oldScale != viewScale) {
		viewChanged();
	}
}

bool ViewManager::zoom(const Event* event) {
	const DeltaEvent* deltaEvent = event->cast<DeltaEvent>();
	if (!deltaEvent) return false;

	// adjust zoom level
	viewScale *= std::pow(2.f, -deltaEvent->getDelta());
	applyLimits();

	// keep pointer position the same
	FPosition newPointerPosition = viewToGrid(pointerViewPosition);
	FVector pointerChange = newPointerPosition - pointerPosition;
	viewCenter -= pointerChange;
	applyLimits();

	viewChanged();
	return true;
}

bool ViewManager::pan(const Event* event) {
	const DeltaXYEvent* deltaXYEvent = event->cast<DeltaXYEvent>();
	if (!deltaXYEvent) return false;
	viewCenter.x -= deltaXYEvent->getDeltaX();
	viewCenter.y -= deltaXYEvent->getDeltaY();
	applyLimits();
	viewChanged();
	return true;
}

bool ViewManager::attachAnchor(const Event* event) {
	if (anchored) return false;
	anchored = true;
	return true;
}

bool ViewManager::dettachAnchor(const Event* event) {
	if (!anchored) return false;
	anchored = false;
	return true;
}

bool ViewManager::pointerMove(const Event* event) {
	if (!pointerActive) return false;
	const ViewPositionEvent* viewPositionEvent = event->cast<ViewPositionEvent>();
	if (!viewPositionEvent) return false;

	FPosition gridPos = viewToGrid(viewPositionEvent->getPosition());

	pointerViewPosition = viewPositionEvent->getPosition();

	if (anchored) {
		FVector delta = pointerPosition - gridPos;
		if (delta.length() < 0.001f) return false; // no change in pointer pos
		viewCenter += delta;
		applyLimits();
		viewChanged();
		eventRegister->doEvent(PositionEvent("Pointer Move", pointerPosition));
		return false;
	}

	pointerPosition = gridPos;
	eventRegister->doEvent(PositionEvent("Pointer Move", gridPos));

	return false;
}

bool ViewManager::pointerEnterView(const Event* event) {
	pointerActive = true;
	pointerMove(event);
	return false;
}

bool ViewManager::pointerExitView(const Event* event) {
	pointerActive = false;
	pointerMove(event);
	return false;
}

void ViewManager::focus() {
	const Circuit* circuit = backend.getCircuitManager().getCircuit(currentCircuitId);
	if (circuit == nullptr) {
		viewCenter = FPosition();
		viewScale = 8.0f;
	} else {
		bool first = true;
		Position topLeft;
		Size size;
		for (auto& pair : circuit->getBlockContainer()) {
			if (!circuit->isOnStack(pair.second.getPosition())) {
				if (first) {
					first = false;
					topLeft = pair.second.getPosition();
					size = pair.second.size();
				} else {
					if (pair.second.getPosition().x < topLeft.x) {
						size.w += topLeft.x - pair.second.getPosition().x;
						topLeft.x = pair.second.getPosition().x;
					}
					if (pair.second.getPosition().y < topLeft.y) {
						size.h += topLeft.y - pair.second.getPosition().y;
						topLeft.y = pair.second.getPosition().y;
					}
					size.extentToFitTartgetCell(pair.second.getLargestPosition() - topLeft);
				}
			}
		}
		if (first) {
			viewCenter = FPosition();
			viewScale = 8.0f;
		} else {
			viewScale = std::max(std::max((float)size.w, (float)size.h) + 0.5f, 8.f);
			viewCenter = topLeft.free() + size.getLargestVectorInArea().free()/2 + FVector(0.5f);
		}
	}
	applyLimits();
	viewChanged();
}

void ViewManager::applyLimits() {
	if (viewScale > 1200.0f) viewScale = 1200.0f;
	if (viewScale < 0.5f) viewScale = 0.5f;
	if (viewCenter.x > 5000000) viewCenter.x = 5000000;
	if (viewCenter.x < -5000000) viewCenter.x = -5000000;
	if (viewCenter.y > 5000000) viewCenter.y = 5000000;
	if (viewCenter.y < -5000000) viewCenter.y = -5000000;
}

Vec2 ViewManager::gridToView(FPosition position) const {
	return Vec2(
		(position.x - viewCenter.x) / getViewWidth() + 0.5f,
		(position.y - viewCenter.y) / getViewHeight() + 0.5f
	);
}

Vec2 ViewManager::gridToView(FVector vector) const {
	return Vec2(
		(vector.dx) / getViewWidth(),
		(vector.dy) / getViewHeight()
	);
}
