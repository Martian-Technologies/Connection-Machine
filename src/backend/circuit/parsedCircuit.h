#ifndef parsedCircuit_h
#define parsedCircuit_h

#include "backend/blockData/blockDataManager.h"
#include "backend/container/block/connectionEnd.h"
#include "backend/position/position.h"

class CircuitManager;

class ParsedCircuit {
	friend class CircuitValidator;
public:
	struct BlockData {
		BlockData(FPosition position, Orientation orientation, BlockType type) : position(position), orientation(orientation), type(type) { }
		BlockData(BlockType type) : type(type) { }

		FPosition position = FPosition::getInvalid(); // will be validated into integer values
		Orientation orientation = Orientation(Rotation::ZERO, false);
		BlockType type = BlockType::NONE;
	};

	struct ConnectionData {
		ConnectionData(block_id_t outputBlockId, connection_end_id_t outputEndId, block_id_t inputBlockId, connection_end_id_t inputEndId) :
			outputBlockId(outputBlockId), outputEndId(outputEndId), inputBlockId(inputBlockId), inputEndId(inputEndId) { }
		block_id_t outputBlockId;
		connection_end_id_t outputEndId;
		block_id_t inputBlockId;
		connection_end_id_t inputEndId;

		bool operator==(const ConnectionData& other) const {
			return outputEndId == other.outputEndId && inputEndId == other.inputEndId &&
				outputBlockId == other.outputBlockId && inputBlockId == other.inputBlockId;
		}
	};

	struct ConnectionPort {
		ConnectionPort(
			bool isInput,
			connection_end_id_t connectionEndId,
			Vector positionOnBlock,
			block_id_t internalBlockId,
			connection_end_id_t internalBlockConnectionEndId,
			const std::string& portName = ""
		) : isInput(isInput), connectionEndId(connectionEndId), positionOnBlock(positionOnBlock),
			internalBlockId(internalBlockId), internalBlockConnectionEndId(internalBlockConnectionEndId), portName(portName) { }
		ConnectionPort(
			bool isInput,
			connection_end_id_t connectionEndId,
			std::optional<Position> positionOfBlock,
			Vector positionOnBlock,
			FVector portOffset,
			const std::string& portName = "",
			unsigned int bitWidth = 1
		) : isInput(isInput), connectionEndId(connectionEndId), positionOfBlock(positionOfBlock), positionOnBlock(positionOnBlock), portOffset(portOffset), portName(portName), bitWidth(bitWidth) { }
		// ConnectionPort(
		// 	bool isInput,
		// 	connection_end_id_t connectionEndId,
		// 	Vector positionOnBlock,
		// 	const std::string& portName = ""
		// ) : isInput(isInput), connectionEndId(connectionEndId), positionOnBlock(positionOnBlock), portName(portName) { }
		bool isInput;
		connection_end_id_t connectionEndId;
		Vector positionOnBlock;
		FVector portOffset = FVector(0.5f, 0.5f);
		block_id_t internalBlockId = 0;
		std::optional<Position> positionOfBlock = std::nullopt;
		connection_end_id_t internalBlockConnectionEndId = connection_end_id_t(0);
		std::string portName = "";
		unsigned int bitWidth = 1;
	};

	void addConnectionPort(
		bool isInput,
		connection_end_id_t connectionEndId,
		Vector positionOnBlock,
		block_id_t internalBlockId,
		connection_end_id_t internalBlockConnectionEndId,
		const std::string& portName = ""
	);
	void addConnectionPort(
		bool isInput,
		connection_end_id_t connectionEndId,
		std::optional<Position> positionOfBlock,
		Vector positionOnBlock,
		FVector portOffset,
		const std::string& portName = "",
		unsigned int bitWidth = 1
	);
	// void addConnectionPort(
	// 	bool isInput,
	// 	connection_end_id_t connectionEndId,
	// 	Vector positionOnBlock,
	// 	const std::string& portName = ""
	// );
	const std::vector<ConnectionPort>& getConnectionPorts() const { return ports; }

	void addBlock(block_id_t id, FPosition pos, Orientation orientation, BlockType type);
	void addBlock(block_id_t id, BlockType type);
	void addConnection(block_id_t outputBlockId, connection_end_id_t outputEndId, block_id_t inputBlockId, connection_end_id_t inputEndId);

	const BlockData* getBlock(block_id_t id) const {
		auto itr = blocks.find(id);
		if (itr != blocks.end()) return &itr->second;
		return nullptr;
	}
	const std::unordered_map<block_id_t, BlockData>& getBlocks() const { return blocks; }
	const std::vector<ConnectionData>& getConns() const { return connections; }

	void setAbsoluteFilePath(const std::string& fpath) { absoluteFilePath = fpath; }
	const std::string& getAbsoluteFilePath() const { return absoluteFilePath; }

	void setName(const std::string& name) { this->name = name; }
	const std::string& getName() const { return name; }

	void setUUID(const std::string& uuid) { this->uuid = uuid; }
	const std::string& getUUID() const { return uuid; }

	void setTexturePath(const std::string& texturePath) { this->texturePath = texturePath; }
	const std::string& getTexturePath() const { return texturePath; }

	void setUsesTileMapTexture(bool usesTileMapTexture) { this->usesTileMapTexture = usesTileMapTexture; }
	bool getUsesTileMapTexture() const { return usesTileMapTexture; }

	void setTextureTileSize(Vec2Int textureTileSize) { this->textureTileSize = textureTileSize; }
	Vec2Int getTextureTileSize() const { return textureTileSize; }

	void setTextureSmallestCordTile(Vec2Int textureSmallestCordTile) { this->textureSmallestCordTile = textureSmallestCordTile; }
	Vec2Int getTextureSmallestCordTile() const { return textureSmallestCordTile; }

	void setTextureBlockTileSize(Vec2Int textureBlockTileSize) { this->textureBlockTileSize = textureBlockTileSize; }
	Vec2Int getTextureBlockTileSize() const { return textureBlockTileSize; }

	Size getSize() const { return size; }
	void setSize(Size size) { this->size = size; valid = false; }

	bool isOldFileVersion() const { return oldFileVersion; }
	void setOldFileVersion(bool oldFileVersion) { this->oldFileVersion = oldFileVersion; }

	void markAsCustom() { isCustomBlock = true; }
	bool isCustom() const { return isCustomBlock; }
	bool isValid() const { return valid; }

private:
	std::string absoluteFilePath;
	std::string uuid;
	std::string name;
	std::string texturePath = "";
	bool usesTileMapTexture = false;
	Vec2Int textureTileSize = {0, 0}; // mean that the whole texture is 1 tile.
	Vec2Int textureSmallestCordTile = {0, 0};
	Vec2Int textureBlockTileSize = {1, 1};

	// If this represents a custom block:
	bool isCustomBlock;
	Size size;

	std::vector<ConnectionPort> ports; // connection id is the index in the vector

	std::unordered_map<block_id_t, BlockData> blocks;
	std::vector<ConnectionData> connections;

	bool oldFileVersion = false;

	bool valid = true;
};

typedef std::shared_ptr<ParsedCircuit> SharedParsedCircuit;

class CircuitValidator {
public:
	CircuitValidator(ParsedCircuit& parsedCircuit, BlockDataManager& blockDataManager) : parsedCircuit(parsedCircuit), blockDataManager(blockDataManager) { validate(); }
private:
	struct ConnectionHash {
		size_t operator()(const ParsedCircuit::ConnectionData& p) const {
			return std::hash<block_id_t>()(p.outputBlockId) ^
				std::hash<connection_end_id_t>()(p.outputEndId) ^
				std::hash<block_id_t>()(p.inputBlockId) ^
				std::hash<connection_end_id_t>()(p.inputEndId);
		}
	};

	void validate();
	bool validateBlockData();
	bool validateBlockTypes();
	bool setBlockPositionsInt();
	bool handleInvalidConnections();
	bool setOverlapsUnpositioned();

	bool handleUnpositionedBlocks();

	bool isIntegerPosition(const FPosition& pos) const {
		return pos.x == std::floor(pos.x) && pos.y == std::floor(pos.y);
	}
	block_id_t generateNewBlockId() const {
		block_id_t id = 0;
		// slow
		while (parsedCircuit.blocks.find(id) != parsedCircuit.blocks.end()) {
			++id;
		}
		return id;
	}

	BlockDataManager& blockDataManager;
	ParsedCircuit& parsedCircuit;
	std::unordered_set<Position> occupiedPositions;
};

#endif /* parsedCircuit_h */
