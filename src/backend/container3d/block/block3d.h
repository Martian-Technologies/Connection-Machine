#ifndef block3d_h
#define block3d_h

#include "backend/container/block/connectionContainer.h"
#include "backend/blockData/blockDataManager.h"
#include "backend/position/position3d.h"

class Block3d {
	friend class BlockContainer3d;
	friend Block3d getBlock3dClass(const BlockDataManager& blockDataManager, BlockType type);
public:
	inline Block3d(const BlockDataManager& blockDataManager) : Block3d(blockDataManager, BlockType::NONE) { }

	// getters
	block_id_t id() const { return blockId; }
	BlockType type() const { return blockType; }

	inline Position3 getPosition() const { return position; }
	inline Position3 getLargestPosition() const { return position + size().getLargestVectorInArea(); }
	inline Orientation3d getOrientation() const { return orientation; }

	inline Size3 size() const { Size size = blockDataManager.getBlockSize(type()); return getOrientation() * Size3(size.w, size.h, 0); }
	inline Size3 sizeNoOrientation() const { Size size = blockDataManager.getBlockSize(type()); return Size3(size.w, size.h, 0); }

	inline bool withinBlock(Position3 position) const { return position.withinArea(getPosition(), getLargestPosition()); }

	inline const ConnectionContainer& getConnectionContainer() const { return connections; }
	inline const std::unordered_set<ConnectionEnd>* getInputConnections(Position3 position) const {
		std::optional<connection_end_id_t> connectionId = getInputConnectionId(position);
		return connectionId ? getConnectionContainer().getConnections(connectionId.value()) : nullptr;
	}
	inline const std::unordered_set<ConnectionEnd>* getOutputConnections(Position3 position) const {
		std::optional<connection_end_id_t> connectionId = getOutputConnectionId(position);
		return connectionId ? getConnectionContainer().getConnections(connectionId.value()) : nullptr;
	}
	inline const std::unordered_set<ConnectionEnd>* getBidirectionalConnections(Position3 position) const {
		std::optional<connection_end_id_t> connectionId = getBidirectionalConnectionId(position);
		return connectionId ? getConnectionContainer().getConnections(connectionId.value()) : nullptr;
	}
	inline std::optional<connection_end_id_t> getInputConnectionId(Position3 position) const {
		if (!withinBlock(position)) return std::nullopt;
		Vector3 rotated = getOrientation().inverseTransformVectorWithArea(position - getPosition(), size());
		return blockDataManager.getInputConnectionId(type(), Vector(rotated.dx, rotated.dy));
	}
	inline std::optional<connection_end_id_t> getOutputConnectionId(Position3 position) const {
		if (!withinBlock(position)) return std::nullopt;
		Vector3 rotated = getOrientation().inverseTransformVectorWithArea(position - getPosition(), size());
		return blockDataManager.getOutputConnectionId(type(), Vector(rotated.dx, rotated.dy));
	}
	inline std::optional<connection_end_id_t> getBidirectionalConnectionId(Position3 position) const {
		if (!withinBlock(position)) return std::nullopt;
		Vector3 rotated = getOrientation().inverseTransformVectorWithArea(position - getPosition(), size());
		return blockDataManager.getBidirectionalConnectionId(type(), Vector(rotated.dx, rotated.dy));
	}
	inline std::optional<connection_end_id_t> getInputOrBidirectionalConnectionId(Position3 position) const {
		if (!withinBlock(position)) return std::nullopt;
		Vector3 rotated = getOrientation().inverseTransformVectorWithArea(position - getPosition(), size());
		return blockDataManager.getInputOrBidirectionalConnectionId(type(), Vector(rotated.dx, rotated.dy));
	}
	inline std::optional<connection_end_id_t> getOutputOrBidirectionalConnectionId(Position3 position) const {
		if (!withinBlock(position)) return std::nullopt;
		Vector3 rotated = getOrientation().inverseTransformVectorWithArea(position - getPosition(), size());
		return blockDataManager.getOutputOrBidirectionalConnectionId(type(), Vector(rotated.dx, rotated.dy));
	}
	inline std::optional<Position3> getConnectionPosition(connection_end_id_t connectionId) const {
		std::optional<Vector> output = blockDataManager.getConnectionVector(type(), connectionId);
		if (!output) return std::nullopt;
		Vector3 rotated = getOrientation().transformVectorWithArea(Vector3(output.value().dx, output.value().dy, 0), sizeNoOrientation());
		return getPosition() + rotated;
	}
	inline std::optional<Vector> getConnectionVector(connection_end_id_t connectionId) const {
		return blockDataManager.getConnectionVector(type(), connectionId);
	}
	inline bool connectionExists(connection_end_id_t connectionId) const { return blockDataManager.connectionExists(type(), connectionId); }
	inline bool isConnectionInput(connection_end_id_t connectionId) const { return blockDataManager.isConnectionInput(type(), connectionId); }
	inline bool isConnectionOutput(connection_end_id_t connectionId) const { return blockDataManager.isConnectionOutput(type(), connectionId); }
	inline bool isConnectionBidirectional(connection_end_id_t connectionId) const { return blockDataManager.isConnectionBidirectional(type(), connectionId); }
	inline bool isConnectionInputOrBidirectional(connection_end_id_t connectionId) const { return blockDataManager.isConnectionInputOrBidirectional(type(), connectionId); }
	inline bool isConnectionOutputOrBidirectional(connection_end_id_t connectionId) const { return blockDataManager.isConnectionOutputOrBidirectional(type(), connectionId); }
	nlohmann::json dumpState() const;

protected:
	inline void destroy() { }
	inline ConnectionContainer& getConnectionContainer() { return connections; }
	inline void setPosition(Position3 position) { this->position = position; }
	inline void setOrientation(Orientation3d orientation) { this->orientation = orientation; }
	inline void setId(block_id_t id) { blockId = id; }

	inline Block3d(const BlockDataManager& blockDataManager, BlockType blockType) : blockType(blockType), blockDataManager(blockDataManager) { }

	// const data
	BlockType blockType;
	block_id_t blockId = 0;

	// helpers
	ConnectionContainer connections;
	const BlockDataManager& blockDataManager;

	// changing data
	Position3 position;
	Orientation3d orientation;
};

inline Block3d getBlock3dClass(const BlockDataManager& blockDataManager, BlockType type) { return Block3d(blockDataManager, type); }

#endif /* block3d_h */
