#include "areaCreationTool.h"

void AreaCreationTool::reset() {
	SelectionHelperTool::reset();
	hasOrigin = false;
	updateElements();
}

void AreaCreationTool::activate() {
	SelectionHelperTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&AreaCreationTool::click, this, std::placeholders::_1));
	registerFunction("Tool Secondary Activate", std::bind(&AreaCreationTool::unclick, this, std::placeholders::_1));
}

// void AreaCreationTool::undoFinished() {
// 	SelectionHelperTool::undoFinished();
// 	unclick();
// }

bool AreaCreationTool::click(const Event* event) {
	if (!circuit) return false;
	if (hasOrigin) {
		finished(
			std::make_shared<ProjectionSelection>(
				std::make_shared<ProjectionSelection>(
					originPosition,
					Vector(signum(lastPointerPosition.x - originPosition.x), 0),
					Abs(lastPointerPosition.x - originPosition.x) + 1
				),
				Vector(0, signum(lastPointerPosition.y - originPosition.y)),
				Abs(lastPointerPosition.y - originPosition.y) + 1
			)
		);
	} else {
		hasOrigin = true;
		originPosition = lastPointerPosition;
		updateElements();
	}

	return true;
}

bool AreaCreationTool::unclick(const Event* event) {
	if (hasOrigin) {
		hasOrigin = false;
		updateElements();
	} else {
		finished(nullptr);
	}
	return true;
}

void AreaCreationTool::updateElements() {
	if (!isActivate || !elementCreator.isSetup()) return;
	elementCreator.clear();
	if (hasOrigin) {
		setStatusBar("Left click to select second corner. Right click to cancel.");
	} else {
		setStatusBar("Left click to select the origin.");
	}
	if (!pointerInView) {
		if (hasOrigin) {
			elementCreator.addSelectionElement(SelectionElement(originPosition));
		}
		return;
	};
	if (hasOrigin) {
		elementCreator.addSelectionElement(SelectionElement(originPosition, lastPointerPosition));
	} else {
		elementCreator.addSelectionElement(SelectionElement(lastPointerPosition));
	}
}
