#ifndef undoSystem_h
#define undoSystem_h

#include "backend/container/minimalDifference.h"

class UndoSystem {
public:
	inline void addBlocker() {
		while (undoPosition < differences.size()) differences.pop_back();
		++undoPosition; differences.emplace_back(std::nullopt);
	}
	inline void addDifference(DifferenceSharedPtr difference) {
		while (undoPosition < differences.size()) differences.pop_back();
		++undoPosition; differences.emplace_back(difference);
	}
	inline const MinimalDifference* undoDifference() {
		if (undoPosition == 0) return nullptr;
		const std::optional<MinimalDifference>& dif = differences[--undoPosition];
		if (dif) return &(dif.value());
		undoPosition++;
		return nullptr;
	}
	inline const MinimalDifference* redoDifference() {
		if (undoPosition == differences.size()) return nullptr;
		const std::optional<MinimalDifference>& dif = differences[undoPosition++];
		if (dif) return &(dif.value());
		undoPosition--;
		return nullptr;
	}
	inline void clear() {
		differences.clear();
		undoPosition = 0;
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson;
		stateJson["undoPosition"] = undoPosition;
		stateJson["differences"] = nlohmann::json::array();
		for (const auto& diffOpt : differences) {
			if (diffOpt.has_value()) {
				stateJson["differences"].push_back(diffOpt->dumpState());
			} else {
				stateJson["differences"].push_back(nullptr);
			}
		}
		return stateJson;
	}

private:
	unsigned int undoPosition = 0;
	std::vector<std::optional<MinimalDifference>> differences;

};

#endif /* undoSystem_h */
