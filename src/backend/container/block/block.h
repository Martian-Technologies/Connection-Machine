#ifndef block_h
#define block_h

#include "connectionContainer.h"
#include "backend/blockData/blockDataManager.h"
#include "blockHelpers.h"

class Block {
	friend class BlockContainer;
	friend Block getBlockClass(const BlockDataManager* blockDataManager, BlockType type);
public:
	inline Block(const BlockDataManager* blockDataManager) : Block(blockDataManager, BlockType::NONE) { }

	// getters
	block_id_t id() const { return blockId; }
	BlockType type() const { return blockType; }

	inline Position getPosition() const { return position; }
	inline Position getLargestPosition() const { return position + size().getLargestVectorInArea(); }
	inline Orientation getOrientation() const { return orientation; }

	inline Size size() const { return blockDataManager->getBlockSize(type(), getOrientation()); }
	inline Size sizeNoOrientation() const { return blockDataManager->getBlockSize(type()); }

	inline bool withinBlock(Position position) const { return position.withinArea(getPosition(), getLargestPosition()); }

	inline const ConnectionContainer& getConnectionContainer() const { return connections; }
	inline const std::unordered_set<ConnectionEnd>* getInputConnections(Position position) const {
		std::optional<connection_end_id_t> connectionId = getInputConnectionId(position);
		return connectionId ? getConnectionContainer().getConnections(connectionId.value()) : nullptr;
	}
	inline const std::unordered_set<ConnectionEnd>* getOutputConnections(Position position) const {
		std::optional<connection_end_id_t> connectionId = getOutputConnectionId(position);
		return connectionId ? getConnectionContainer().getConnections(connectionId.value()) : nullptr;
	}
	inline const std::unordered_set<ConnectionEnd>* getBidirectionalConnections(Position position) const {
		std::optional<connection_end_id_t> connectionId = getBidirectionalConnectionId(position);
		return connectionId ? getConnectionContainer().getConnections(connectionId.value()) : nullptr;
	}
	inline std::optional<connection_end_id_t> getInputConnectionId(Position position) const {
		if (!withinBlock(position)) return std::nullopt;
		return blockDataManager->getInputConnectionId(type(), getOrientation(), position - getPosition());
	}
	inline std::optional<connection_end_id_t> getOutputConnectionId(Position position) const {
		if (!withinBlock(position)) return std::nullopt;
		return blockDataManager->getOutputConnectionId(type(), getOrientation(), position - getPosition());
	}
	inline std::optional<connection_end_id_t> getBidirectionalConnectionId(Position position) const {
		if (!withinBlock(position)) return std::nullopt;
		return blockDataManager->getBidirectionalConnectionId(type(), getOrientation(), position - getPosition());
	}
	inline std::optional<connection_end_id_t> getInputOrBidirectionalConnectionId(Position position) const {
		if (!withinBlock(position)) return std::nullopt;
		return blockDataManager->getInputOrBidirectionalConnectionId(type(), getOrientation(), position - getPosition());
	}
	inline std::optional<connection_end_id_t> getOutputOrBidirectionalConnectionId(Position position) const {
		if (!withinBlock(position)) return std::nullopt;
		return blockDataManager->getOutputOrBidirectionalConnectionId(type(), getOrientation(), position - getPosition());
	}
	inline std::optional<Position> getConnectionPosition(connection_end_id_t connectionId) const {
		std::optional<Vector> output = blockDataManager->getConnectionVector(type(), getOrientation(), connectionId);
		if (!output) return std::nullopt;
		return getPosition() + output.value();
	}
	inline std::optional<Vector>  getConnectionVector(connection_end_id_t connectionId) const {
		return blockDataManager->getConnectionVector(type(), getOrientation(), connectionId);
	}
	inline bool connectionExists(connection_end_id_t connectionId) const { return blockDataManager->connectionExists(type(), connectionId); }
	inline bool isConnectionInput(connection_end_id_t connectionId) const { return blockDataManager->isConnectionInput(type(), connectionId); }
	inline bool isConnectionOutput(connection_end_id_t connectionId) const { return blockDataManager->isConnectionOutput(type(), connectionId); }
	inline bool isConnectionBidirectional(connection_end_id_t connectionId) const { return blockDataManager->isConnectionBidirectional(type(), connectionId); }
	inline bool isConnectionInputOrBidirectional(connection_end_id_t connectionId) const { return blockDataManager->isConnectionInputOrBidirectional(type(), connectionId); }
	inline bool isConnectionOutputOrBidirectional(connection_end_id_t connectionId) const { return blockDataManager->isConnectionOutputOrBidirectional(type(), connectionId); }

protected:
	inline void destroy() { }
	inline ConnectionContainer& getConnectionContainer() { return connections; }
	inline void setPosition(Position position) { this->position = position; }
	inline void setOrientation(Orientation orientation) { this->orientation = orientation; }
	inline void setId(block_id_t id) { blockId = id; }

	inline Block(const BlockDataManager* blockDataManager, BlockType blockType) : blockType(blockType), blockDataManager(blockDataManager) { }

	// const data
	BlockType blockType;
	block_id_t blockId = 0;

	// helpers
	ConnectionContainer connections;
	const BlockDataManager* blockDataManager;

	// changing data
	Position position;
	Orientation orientation = Orientation(Rotation::ZERO, false);
};

inline Block getBlockClass(const BlockDataManager* blockDataManager, BlockType type) { return Block(blockDataManager, type); }

#endif /* block_h */
