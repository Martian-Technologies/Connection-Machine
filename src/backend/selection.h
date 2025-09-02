#ifndef selection_h
#define selection_h

#include "backend/position/position.h"

typedef unsigned int dimensional_selection_size_t;

template<class OutputSelectionType, class InputSelectionType>
inline std::shared_ptr<const OutputSelectionType> selectionCast(const std::shared_ptr<const InputSelectionType>& selection) {
	return std::dynamic_pointer_cast<const OutputSelectionType>(selection);
}
template<class OutputSelectionType, class InputSelectionType>
inline const OutputSelectionType* selectionCast(const InputSelectionType* selection) {
	return dynamic_cast<const OutputSelectionType*>(selection);
}

// ---------------- Base Selection ----------------
class Selection;
typedef std::shared_ptr<const Selection> SharedSelection;
class Selection {
public:
	virtual ~Selection() = default;
};

// ---------------- Dimensional Selection ----------------
class DimensionalSelection : public Selection {
public:
	virtual SharedSelection getSelection(dimensional_selection_size_t index) const = 0;
	virtual dimensional_selection_size_t size() const = 0;

};
typedef std::shared_ptr<const DimensionalSelection> SharedDimensionalSelection;

// ---------------- Cell Selection ----------------
class CellSelection : public Selection {
public:
	inline CellSelection(Position position) : position(position) { }

	inline Position getPosition() const { return position; }

private:
	Position position;
};
typedef std::shared_ptr<const CellSelection> SharedCellSelection;

// ---------------- Space Selection ----------------
class SpaceSelection : public Selection {
public:
	virtual Position getCellPosition(unsigned int index) const = 0;
	virtual unsigned int cellCount() const = 0;
};
typedef std::shared_ptr<const SpaceSelection> SharedSpaceSelection;

// ---------------- Rect Selection ----------------
class RectSelection : public SpaceSelection {
public:
	RectSelection(Position position, Size size) : position(position), size(size) {}
	RectSelection(Position topLeftPosition, Position bottomRightPosition) : position(topLeftPosition), size(topLeftPosition, topLeftPosition) {}
	Position getPosition() const { return position; }
	Size getSize() const { return size; }
	Position getCellPosition(unsigned int index) const override final {
		return position + Vector(index % size.w, index / size.h);
	}
	unsigned int cellCount() const override final { return size.area();}
private:
	Position position;
	Size size;
};
typedef std::shared_ptr<const RectSelection> SharedRectSelection;

// ---------------- Rect Selection ----------------
class MultiSpaceSelection : public SpaceSelection {
public:
	const std::vector<SharedSpaceSelection>& getSpaceSelections() const { return spaceSelections; }
	Position getCellPosition(unsigned int index) const override final {
		for (unsigned int i = 0; i < spaceSelections.size(); ++i) {
			unsigned int cellCount = spaceSelections[i]->cellCount();
			if (cellCount > index) {
				return spaceSelections[i]->getCellPosition(index);
			} else {
				index -= cellCount;
			}
		}
		logError("Failed to getCellPosition because index {} was too big.", "MultiSpaceSelection", index);
		return Position();
	}
	unsigned int cellCount() const override final {
		unsigned int sum = 0;
		for (unsigned int i = 0; i < spaceSelections.size(); ++i) {
			sum += spaceSelections[i]->cellCount();
		}
		return sum;
	}
private:
	std::vector<SharedSpaceSelection> spaceSelections;
};
typedef std::shared_ptr<const MultiSpaceSelection> SharedMultiSpaceSelection;

// ---------------- Shift Selection ----------------
SharedSelection shiftSelection(SharedSelection selection, Vector shift);
class ShiftSelection : public DimensionalSelection {
	friend SharedSelection shiftSelection(SharedSelection selection, Vector shift);
public:
	SharedSelection getSelection(dimensional_selection_size_t index) const override {
		return shiftSelection(dimensionalSelection->getSelection(index), shift);
	}
	dimensional_selection_size_t size() const override { return dimensionalSelection->size(); }

private:
	ShiftSelection(SharedDimensionalSelection dimensionalSelection, Vector shift) : dimensionalSelection(std::move(dimensionalSelection)), shift(shift) { }

	SharedDimensionalSelection dimensionalSelection;
	Vector shift;
};
typedef std::shared_ptr<const ShiftSelection> SharedShiftSelection;

// used to safely shift a selection
inline SharedSelection shiftSelection(SharedSelection selection, Vector shift) {
	if (shift.dx == 0 && shift.dy == 0) return std::move(selection);
	SharedDimensionalSelection dimensionalSelection = selectionCast<DimensionalSelection>(selection);
	if (dimensionalSelection) {
		SharedShiftSelection shiftSelection_ = selectionCast<ShiftSelection>(dimensionalSelection);
		if (shiftSelection_) {
			return shiftSelection(shiftSelection_->dimensionalSelection, shiftSelection_->shift + shift);
		}
		return std::make_shared<const ShiftSelection>(ShiftSelection(std::move(dimensionalSelection), shift));
	}
	SharedCellSelection cellSelection = selectionCast<CellSelection>(std::move(selection));
	if (cellSelection) {
		return std::make_shared<const CellSelection>(cellSelection->getPosition() + shift);
	}
	return nullptr;
}

// ---------------- Projection Selection ----------------
class ProjectionSelection : public DimensionalSelection {
public:
	inline ProjectionSelection(Position position, Vector step, dimensional_selection_size_t count) :
		selection(std::make_shared<CellSelection>(position)), step(step), count(count) { }
	inline ProjectionSelection(SharedSelection selection, Vector step, dimensional_selection_size_t count) :
		selection(std::move(selection)), step(step), count(count) { }

	inline SharedSelection getSelection(dimensional_selection_size_t index) const override {
		return shiftSelection(selection, step * index);
	};

	inline dimensional_selection_size_t size() const override { return count; }

	inline Vector getStep() const { return step; }

private:
	SharedSelection selection;
	Vector step;
	dimensional_selection_size_t count;
};
typedef std::shared_ptr<const ProjectionSelection> SharedProjectionSelection;

// ---------------- helpers ----------------
inline bool sameSelectionShape(const SharedSelection& selectionA, const SharedSelection& selectionB) {
	// check if both cell selections
	const SharedCellSelection& cellSelectionA = selectionCast<CellSelection>(selectionA);
	const SharedCellSelection& cellSelectionB = selectionCast<CellSelection>(selectionB);
	if (cellSelectionA || cellSelectionB) return cellSelectionA && cellSelectionB;

	const SharedDimensionalSelection& dimensionalSelectionA = selectionCast<DimensionalSelection>(selectionA);
	const SharedDimensionalSelection& dimensionalSelectionB = selectionCast<DimensionalSelection>(selectionB);
	if (dimensionalSelectionA && dimensionalSelectionB) {
		if (
			(dimensionalSelectionA->size() == 1 || dimensionalSelectionB->size() == 1) ||
			(dimensionalSelectionA->size() == dimensionalSelectionB->size())
			) {
			return sameSelectionShape(dimensionalSelectionA->getSelection(0), dimensionalSelectionB->getSelection(0));
		}
	}
	return false;
}
inline Position getSelectionOrigin(const SharedSelection& selection) {
	const SharedDimensionalSelection& dimensionalSelection = selectionCast<DimensionalSelection>(selection);
	if (dimensionalSelection) {
		return getSelectionOrigin(dimensionalSelection->getSelection(0));
	}

	const SharedCellSelection& cellSelection = selectionCast<CellSelection>(selection);
	if (cellSelection) {
		return cellSelection->getPosition();
	}

	logError("Could not find origin of selection. Selection not Cell or Dimensional Selection.", "Selection");
	return Position();
}
inline void flattenSelection(const SharedSelection& selection, std::unordered_set<Position>& positions) {
	// Cell Selection
	SharedCellSelection cellSelection = selectionCast<CellSelection>(selection);
	if (cellSelection) {
		positions.insert(cellSelection->getPosition());
		return;
	}

	// Dimensional Selection
	SharedDimensionalSelection dimensionalSelection = selectionCast<DimensionalSelection>(selection);
	if (dimensionalSelection) {
		for (dimensional_selection_size_t i = dimensionalSelection->size(); i > 0; i--) {
			flattenSelection(dimensionalSelection->getSelection(i - 1), positions);
		}
	}
}

#endif /* selection_h */
