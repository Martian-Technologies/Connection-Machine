#include <QApplication>

#include "viewManager.h"
#include "gui/circuitView/events/customEvents.h"

bool ViewManager::zoom(const Event* event) {
	const DeltaEvent* deltaEvent = event->cast<DeltaEvent>();
	if (!deltaEvent) return false;
	viewHeight *= std::pow(2.f, -deltaEvent->getDelta());
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
	if (doPointerMovement) return false;
	doPointerMovement = true;
	return true;
}

bool ViewManager::dettachAnchor(const Event* event) {
	if (!doPointerMovement) return false;
	doPointerMovement = false;
	return true;
}

bool ViewManager::pointerMove(const Event* event) {
	if (!pointerActive) return false;
	const PositionEvent* positionEvent = event->cast<PositionEvent>();
	if (!positionEvent) return false;

	pointerViewPosition = gridToView(positionEvent->getFPosition());

	if (doPointerMovement) {
		FVector delta = pointerPosition - positionEvent->getFPosition();
		if (delta.manhattenlength() < 0.001f) return false; // no change in pointer pos
		viewCenter += delta;
		applyLimits();
		viewChanged();
		return true;
	}

	pointerPosition = positionEvent->getFPosition();
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

void ViewManager::applyLimits() {
	if (viewHeight > 150.0f) viewHeight = 150.0f;
	if (viewHeight < 0.5f) viewHeight = 0.5f;
	if (viewCenter.x > 10000000) viewCenter.x = 10000000;
	if (viewCenter.x < -10000000) viewCenter.x = -10000000;
	if (viewCenter.y > 10000000) viewCenter.y = 10000000;
	if (viewCenter.y < -10000000) viewCenter.y = -10000000;
}

Vec2 ViewManager::gridToView(FPosition position) const {
	return Vec2(
		(position.x - viewCenter.x) / getViewWidth() + 0.5f,
		(position.y - viewCenter.y) / getViewHeight() + 0.5f
	);
}
