#ifndef difference3d_h
#define difference3d_h

#include "backend/container/block/blockDefs.h"
#include "backend/position/position3d.h"
#include "backend/container/difference.h"

class Difference3d {
	friend class BlockContainer3d;
public:
	enum ModificationType {
		REMOVED_BLOCK,
		PLACE_BLOCK,
		MOVE_BLOCK,
		REMOVED_CONNECTION,
		CREATED_CONNECTION
	};
	typedef std::tuple<Position3, Orientation3d, BlockType> block_modification_t;
	typedef std::tuple<Position3, Orientation3d, Position3, Orientation3d, MoveType> move_modification_t;
	typedef std::tuple<Position3, Position3, Position3, Position3> connection_modification_t;

	typedef std::pair<ModificationType, std::variant<block_modification_t, move_modification_t, connection_modification_t>> Modification;

	inline bool empty() const { return modifications.empty(); }
	inline const std::vector<Modification>& getModifications() const { return modifications; }
	inline bool clearsAll() const { return isClear; }

private:
	void addRemovedBlock(Position3 Position3, Orientation3d Orientation3d, BlockType type) { modifications.push_back({ ModificationType::REMOVED_BLOCK, std::make_tuple(Position3, Orientation3d, type) }); }
	void addPlacedBlock(Position3 Position3, Orientation3d Orientation3d, BlockType type) { modifications.push_back({ ModificationType::PLACE_BLOCK, std::make_tuple(Position3, Orientation3d, type) }); }
	void addMovedBlock(Position3 curPosition3, Orientation3d curOrientation3d, Position3 newPosition3, Orientation3d newOrientation3d, MoveType moveType = MoveType::SINGLE) { modifications.push_back({ ModificationType::MOVE_BLOCK, std::make_tuple(curPosition3, curOrientation3d, newPosition3, newOrientation3d, moveType) }); }
	void addRemovedConnection(Position3 outputBlockPosition3, Position3 outputPosition3, Position3 inputBlockPosition3, Position3 inputPosition3) { modifications.push_back({ ModificationType::REMOVED_CONNECTION, std::make_tuple(outputBlockPosition3, outputPosition3, inputBlockPosition3, inputPosition3) }); }
	void addCreatedConnection(Position3 outputBlockPosition3, Position3 outputPosition3, Position3 inputBlockPosition3, Position3 inputPosition3) { modifications.push_back({ ModificationType::CREATED_CONNECTION, std::make_tuple(outputBlockPosition3, outputPosition3, inputBlockPosition3, inputPosition3) }); }
	void setIsClear() { isClear = true; }

	bool isClear = false;
	std::vector<Modification> modifications;
};
typedef std::shared_ptr<Difference3d> Difference3dSharedPtr;

#endif /* difference3d_h */
