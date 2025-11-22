#ifndef minimalDifference3d_h
#define minimalDifference3d_h

#include "../position/position3d.h"
#include "backend/container/block/blockDefs.h"
#include "difference3d.h"

class MinimalDifference {
	friend class BlockContainer;
public:
	MinimalDifference(Difference3dSharedPtr difference) {
		Difference3d::block_modification_t blockModification;
		Difference3d::connection_modification_t connectionModification;
		Difference3d::move_modification_t moveModification;
		for (const auto& modification : difference->getModifications()) {
			switch (modification.first) {
			case Difference3d::PLACE_BLOCK:
				blockModification = std::get<Difference3d::block_modification_t>(modification.second);
				addPlacedBlock(std::get<0>(blockModification), std::get<1>(blockModification), std::get<2>(blockModification));
				break;
			case Difference3d::REMOVED_BLOCK:
				blockModification = std::get<Difference3d::block_modification_t>(modification.second);
				addRemovedBlock(std::get<0>(blockModification), std::get<1>(blockModification), std::get<2>(blockModification));
				break;
			case Difference3d::CREATED_CONNECTION:
				connectionModification = std::get<Difference3d::connection_modification_t>(modification.second);
				addCreatedConnection(std::get<1>(connectionModification), std::get<3>(connectionModification));
				break;
			case Difference3d::REMOVED_CONNECTION:
				connectionModification = std::get<Difference3d::connection_modification_t>(modification.second);
				addRemovedConnection(std::get<1>(connectionModification), std::get<3>(connectionModification));
				break;
			case Difference3d::MOVE_BLOCK:
				moveModification = std::get<Difference3d::move_modification_t>(modification.second);
				addMovedBlock(
					std::get<0>(moveModification),
					std::get<1>(moveModification),
					std::get<2>(moveModification),
					std::get<3>(moveModification),
					std::get<4>(moveModification)
				);
				break;
			}
		}
	}

	enum ModificationType {
		REMOVED_BLOCK,
		PLACE_BLOCK,
		MOVE_BLOCK,
		REMOVED_CONNECTION,
		CREATED_CONNECTION,
	};
	typedef std::tuple<Position3, Orientation3d, BlockType> block_modification_t;
	typedef std::tuple<Position3, Orientation3d, Position3, Orientation3d, MoveType> move_modification_t;
	typedef std::pair<Position3, Position3> connection_modification_t;

	typedef std::pair<ModificationType, std::variant<block_modification_t, move_modification_t, connection_modification_t>> Modification;

	inline bool empty() const { return modifications.empty(); }
	inline const std::vector<Modification>& getModifications() const { return modifications; }

	// static nlohmann::json dumpModification(const Modification& modification) /* GCOVR_EXCL_FUNCTION */ {
	// 	nlohmann::json stateJson;
	// 	stateJson["type"] = modification.first;
	// 	switch (modification.first) {
	// 	case ModificationType::REMOVED_BLOCK:
	// 	case ModificationType::PLACE_BLOCK: {
	// 		auto blockModification = std::get<block_modification_t>(modification.second);
	// 		stateJson["position"] = std::get<0>(blockModification).toString();
	// 		stateJson["orientation"] = { { "rotation", std::get<1>(blockModification).rotation }, { "flipped", std::get<1>(blockModification).flipped } };
	// 		stateJson["blockType"] = blocktype_to_string(std::get<2>(blockModification));
	// 		break;
	// 	}
	// 	case ModificationType::MOVE_BLOCK: {
	// 		auto moveModification = std::get<move_modification_t>(modification.second);
	// 		stateJson["currentPosition"] = std::get<0>(moveModification).toString();
	// 		stateJson["currentOrientation"] = { { "rotation", std::get<1>(moveModification).rotation }, { "flipped", std::get<1>(moveModification).flipped } };
	// 		stateJson["newPosition"] = std::get<2>(moveModification).toString();
	// 		stateJson["newOrientation"] = { { "rotation", std::get<3>(moveModification).rotation }, { "flipped", std::get<3>(moveModification).flipped } };
	// 		stateJson["moveType"] = static_cast<int>(std::get<4>(moveModification));
	// 		break;
	// 	}
	// 	case ModificationType::REMOVED_CONNECTION:
	// 	case ModificationType::CREATED_CONNECTION: {
	// 		auto connectionModification = std::get<connection_modification_t>(modification.second);
	// 		stateJson["outputPosition"] = connectionModification.first.toString();
	// 		stateJson["inputPosition"] = connectionModification.second.toString();
	// 		break;
	// 	}
	// 	}
	// 	return stateJson;
	// }

	// nlohmann::json dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	// 	nlohmann::json stateJson;
	// 	stateJson["modifications"] = nlohmann::json::array();
	// 	for (const Modification& modification : modifications) {
	// 		stateJson["modifications"].push_back(MinimalDifference::dumpModification(modification));
	// 	}
	// 	return stateJson;
	// }

private:
	void addRemovedBlock(Position3 position, Orientation3d orientation, BlockType type) {
		modifications.push_back({ ModificationType::REMOVED_BLOCK, std::make_tuple(position, orientation, type) });
	}
	void addPlacedBlock(Position3 position, Orientation3d orientation, BlockType type) {
		modifications.push_back({ ModificationType::PLACE_BLOCK, std::make_tuple(position, orientation, type) });
	}
	void addMovedBlock(Position3 curPosition, Orientation3d curOrientation, Position3 newPosition, Orientation3d newOrientation, MoveType moveType = MoveType::SINGLE) {
		modifications.push_back({ ModificationType::MOVE_BLOCK, std::make_tuple(curPosition, curOrientation, newPosition, newOrientation, moveType) });
	}
	void addRemovedConnection(Position3 outputPosition, Position3 inputPosition) {
		modifications.push_back({ ModificationType::REMOVED_CONNECTION, std::make_pair(outputPosition, inputPosition) });
	}
	void addCreatedConnection(Position3 outputPosition, Position3 inputPosition) {
		modifications.push_back({ ModificationType::CREATED_CONNECTION, std::make_pair(outputPosition, inputPosition) });
	}

	std::vector<Modification> modifications;
};

#endif /* minimalDifference3d_h */
