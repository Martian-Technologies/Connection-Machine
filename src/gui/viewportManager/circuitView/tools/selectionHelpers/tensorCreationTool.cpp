#include "tensorCreationTool.h"

void TensorCreationTool::reset() {
	SelectionHelperTool::reset();
	selection = nullptr;
	selectionToFollow.clear();
	followingSelection = false;
	tensorStage = -1;
	updateElements();
}

void TensorCreationTool::activate() {
	SelectionHelperTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&TensorCreationTool::click, this, std::placeholders::_1));
	registerFunction("Tool Secondary Activate", std::bind(&TensorCreationTool::unclick, this, std::placeholders::_1));
	registerFunction("Confirm", std::bind(&TensorCreationTool::confirm, this, std::placeholders::_1));
}

void TensorCreationTool::undoFinished() {
	SelectionHelperTool::undoFinished();
	if (followingSelection) unclick();
}

bool TensorCreationTool::click(const Event* event) {
	if (!circuit) return false;

	if (tensorStage == -1) {
		originPosition = lastPointerPosition;
		selection = std::make_shared<CellSelection>(originPosition);
		tensorStage = 0;
	} else if (tensorStage % 2 == 0) { // step
		step = lastPointerPosition - originPosition;
		if (step.manhattenLength() == 0) {
			selection = std::make_shared<ProjectionSelection>(selection, Vector(), 1);
			tensorStage += 2;
		} else if (followingSelection) {
			dimensional_selection_size_t size = selectionToFollow[selectionToFollow.size() - tensorStage / 2 - 1];
			if (size == 1) {
				tensorStage++;
			} else {
				selection = std::make_shared<ProjectionSelection>(selection, step, size);
				tensorStage += 2;
			}
		} else {
			tensorStage++;
		}
	} else { // count
		float dis = step.length();
		float length = lastPointerFPosition.lengthAlongProjectToVec(originPosition.free() + FVector(0.5f, 0.5f), step.free());
		int count = Abs(round(length / dis)) + 1;
		selection = std::make_shared<ProjectionSelection>(selection, (length > 0) ? step : step * -1, count);
		tensorStage++;
	}
	if (followingSelection && tensorStage == selectionToFollow.size() * 2) {
		finished(selection);
		return true;
	}
	updateElements();
	return true;
}

bool TensorCreationTool::unclick(const Event* event) {
	if (tensorStage == -1) {
		finished(nullptr);
		return true;
	}
	// undo orgin
	if (tensorStage == 0) {
		selection = nullptr;
		tensorStage = -1;
	} else if (tensorStage % 2 == 1) { // undo step
		tensorStage--;
	} else { // undo count
		SharedProjectionSelection projectionSelection = selectionCast<ProjectionSelection>(selection);
		if (projectionSelection->size() == 1 || (followingSelection && selectionToFollow[selectionToFollow.size() - tensorStage / 2] != 1)) {
			tensorStage -= 2;
			selection = projectionSelection->getSelection(0);
		} else {
			step = projectionSelection->getStep();
			selection = projectionSelection->getSelection(0);
			tensorStage--;
		}
	}
	updateElements();
	return true;
}

bool TensorCreationTool::confirm(const Event* event) {
	if (tensorStage == -1) return false;
	if (tensorStage % 2 == 1) tensorStage--;
	finished(selection);
	return false;
}

std::string formatOrdinal(int n) {
	std::string suffix = "th";
	if (n % 10 == 1 and n % 100 != 11) suffix = "st";
	else if (n % 10 == 2 and n % 100 != 12) suffix = "nd";
	else if (n % 10 == 3 and n % 100 != 13) suffix = "rd";
	return std::to_string(n) + suffix;
}

void TensorCreationTool::updateElements() {
	if (!elementCreator.isSetup()) return;
	elementCreator.clear();

	if (!pointerInView) {
		if (tensorStage == -1) {
			setStatusBar("Left click to select the origin of tensor.");
		} else if (tensorStage % 2 == 0) {
			if (followingSelection) setStatusBar(
				"Left click to specify the " + formatOrdinal(tensorStage / 2 + 1) + " dimension vector of the tensor (" +
				std::to_string(tensorStage / 2 + 1) + "/" + std::to_string(selectionToFollow.size()) + ")."
			);
			else setStatusBar("Left click to specify the " + formatOrdinal(tensorStage / 2 + 1) + " dimension vector of the tensor. Press E to complete tensor.");
			elementCreator.addSelectionElement(SelectionObjectElement(
				selection, SelectionObjectElement::RenderMode::ARROWS
			));
		} else {
			setStatusBar("Left click to specify the range of the " + formatOrdinal(tensorStage / 2 + 1) + " dimension vector of the tensor (2)");
			elementCreator.addSelectionElement(SelectionObjectElement(
				std::make_shared<ProjectionSelection>(selection, step, 2),
				SelectionObjectElement::RenderMode::ARROWS
			));
		}
		return;
	};

	SharedSelection displaySelection;
	if (tensorStage == -1) {
		setStatusBar("Left click to select the origin of tensor.");
		displaySelection = std::make_shared<CellSelection>(lastPointerPosition);
	} else if (tensorStage % 2 == 0) { // step
		if (followingSelection) setStatusBar(
			"Left click to specify the " + formatOrdinal(tensorStage / 2 + 1) + " dimension vector of the tensor (" +
			std::to_string(tensorStage / 2 + 1) + "/" + std::to_string(selectionToFollow.size()) + ")."
		);
		else setStatusBar("Left click to specify the " + formatOrdinal(tensorStage / 2 + 1) + " dimension vector of the tensor. Press E to complete tensor.");
		step = lastPointerPosition - originPosition;
		if (step.manhattenLength() == 0) {
			displaySelection = std::make_shared<ProjectionSelection>(selection, Vector(), 1);
		} else if (followingSelection) {
			dimensional_selection_size_t size = selectionToFollow[selectionToFollow.size() - tensorStage / 2 - 1];
			if (size == 1) {
				displaySelection = std::make_shared<ProjectionSelection>(selection, step, 2);
			} else {
				displaySelection = std::make_shared<ProjectionSelection>(selection, step, size);
			}
		} else {
			displaySelection = std::make_shared<ProjectionSelection>(selection, step, 2);
		}
	} else { // count
		float dis = step.length();
		float length = lastPointerFPosition.lengthAlongProjectToVec(originPosition.free() + FVector(0.5f, 0.5f), step.free());
		int count = Abs(round(length / dis)) + 1;
		setStatusBar("Specify the range of the " + formatOrdinal(tensorStage / 2 + 1) + " dimension vector of the tensor (" + std::to_string(count) + ")");
		displaySelection = std::make_shared<ProjectionSelection>(selection, (length > 0) ? step : step * -1, count);
	}
	elementCreator.addSelectionElement(SelectionObjectElement(displaySelection, SelectionObjectElement::RenderMode::ARROWS));
}

void TensorCreationTool::setSelectionToFollow(SharedSelection selectionToFollow) {
	this->selectionToFollow.clear();
	followingSelection = (bool)selectionToFollow;
	SharedDimensionalSelection selectionToFollowDimension = selectionCast<DimensionalSelection>(selectionToFollow);
	while (selectionToFollowDimension) {
		this->selectionToFollow.push_back(selectionToFollowDimension->size());
		selectionToFollowDimension = selectionCast<DimensionalSelection>(selectionToFollowDimension->getSelection(0));
	}
}
