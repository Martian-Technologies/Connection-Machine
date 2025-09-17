#ifndef minimalDifference_h
#define minimalDifference_h

#include "block/blockDefs.h"
#include "../position/position.h"
#include "difference.h"

class MinimalDifference {
	friend class BlockContainer;
public:
	MinimalDifference(DifferenceSharedPtr difference) {
		Difference::block_modification_t blockModification;
		Difference::connection_modification_t connectionModification;
		Difference::move_modification_t moveModification;
		for (const auto& modification : difference->getModifications()) {
			switch (modification.first) {
			case Difference::PLACE_BLOCK:
				blockModification = std::get<Difference::block_modification_t>(modification.second);
				addPlacedBlock(std::get<0>(blockModification), std::get<1>(blockModification), std::get<2>(blockModification));
				break;
			case Difference::REMOVED_BLOCK:
				blockModification = std::get<Difference::block_modification_t>(modification.second);
				addRemovedBlock(std::get<0>(blockModification), std::get<1>(blockModification), std::get<2>(blockModification));
				break;
			case Difference::CREATED_CONNECTION:
				connectionModification = std::get<Difference::connection_modification_t>(modification.second);
				addCreatedConnection(std::get<1>(connectionModification), std::get<3>(connectionModification));
				break;
			case Difference::REMOVED_CONNECTION:
				connectionModification = std::get<Difference::connection_modification_t>(modification.second);
				addRemovedConnection(std::get<1>(connectionModification), std::get<3>(connectionModification));
				break;
			case Difference::MOVE_BLOCK:
				moveModification = std::get<Difference::move_modification_t>(modification.second);
				addMovedBlock(std::get<0>(moveModification), std::get<1>(moveModification), std::get<2>(moveModification), std::get<3>(moveModification), std::get<4>(moveModification));
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
	typedef std::tuple<Position, Orientation, BlockType> block_modification_t;
	typedef std::tuple<Position, Orientation, Position, Orientation, MoveType> move_modification_t;
	typedef std::pair<Position, Position> connection_modification_t;

	typedef std::pair<ModificationType, std::variant<block_modification_t, move_modification_t, connection_modification_t>> Modification;

	inline bool empty() const { return modifications.empty(); }
	inline const std::vector<Modification>& getModifications() const { return modifications; }

private:
	void addRemovedBlock(Position position, Orientation orientation, BlockType type) { modifications.push_back({ ModificationType::REMOVED_BLOCK, std::make_tuple(position, orientation, type) }); }
	void addPlacedBlock(Position position, Orientation orientation, BlockType type) { modifications.push_back({ ModificationType::PLACE_BLOCK, std::make_tuple(position, orientation, type) }); }
	void addMovedBlock(Position curPosition, Orientation curOrientation, Position newPosition, Orientation newOrientation, MoveType moveType = MoveType::SINGLE) { modifications.push_back({ ModificationType::MOVE_BLOCK, std::make_tuple(curPosition, curOrientation, newPosition, newOrientation, moveType) }); }
	void addRemovedConnection(Position outputPosition, Position inputPosition) { modifications.push_back({ ModificationType::REMOVED_CONNECTION, std::make_pair(outputPosition, inputPosition) }); }
	void addCreatedConnection(Position outputPosition, Position inputPosition) { modifications.push_back({ ModificationType::CREATED_CONNECTION, std::make_pair(outputPosition, inputPosition) }); }

	std::vector<Modification> modifications;
};

#endif /* minimalDifference_h */
