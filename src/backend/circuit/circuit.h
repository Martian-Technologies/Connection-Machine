#ifndef circuit_h
#define circuit_h

#include <assert.h>

#include "backend/container/blockContainer.h"
#include "backend/container/copiedBlocks.h"
#include "backend/selection.h"
#include "undoSystem.h"

class CircuitManager;
class GeneratedCircuit;
class ParsedCircuit;

typedef unsigned int circuit_id_t;
typedef unsigned int circuit_update_count;

typedef std::function<void(DifferenceSharedPtr, circuit_id_t)> CircuitDiffListenerFunction;

class Circuit {
	friend class CircuitManager;
public:
	Circuit(circuit_id_t circuitId, CircuitManager* circuitManager, BlockDataManager* blockDataManager, DataUpdateEventManager* dataUpdateEventManager, const std::string& name, const std::string& uuid);

	void clear(bool clearUndoTree = false);

	inline BlockType getBlockType() const { return blockContainer.getBlockType(); }
	inline const std::string& getUUID() const { return circuitUUID; }
	inline circuit_id_t getCircuitId() const { return circuitId; }
	inline std::string getCircuitNameNumber() const { return circuitName + " : " + std::to_string(circuitId); }
	inline const std::string& getCircuitName() const { return circuitName; }
	void setCircuitName(const std::string& name);

	inline unsigned long long getEditCount() const { return editCount; }
	void addEdit() { editCount++; }

	bool isEditable() { return editable; }
	void setEditable(bool isEditable) { editable = isEditable; }

	/* ----------- listener ----------- */
	// subject to change
	void connectListener(void* object, CircuitDiffListenerFunction func, unsigned int priority = 100);
	// subject to change
	void disconnectListener(void* object);

	// allows accese to BlockContainer getters (never null)
	inline const BlockContainer* getBlockContainer() const { return &blockContainer; }

	/* ----------- blocks ----------- */
	// Trys to insert a block. Returns if successful.
	bool tryInsertBlock(Position position, Orientation transformAmount, BlockType blockType);
	// Trys to remove a block. Returns if successful.
	bool tryRemoveBlock(Position position);
	// Trys to move a block. Returns if successful.
	bool tryMoveBlock(Position positionOfBlock, Position position);
	// Trys to move blocks. Wont move any if one cant move. Returns if successful.
	bool tryMoveBlocks(const SharedSelection& selection, Vector movement, Orientation transformAmount);
	// Sets the type of blocks. Will set as many of the blocks as possible.
	void setType(const SharedSelection& selection, BlockType type);

	void tryInsertOverArea(Position cellA, Position cellB, Orientation transformAmount, BlockType blockType);
	void tryRemoveOverArea(Position cellA, Position cellB);

	bool checkCollision(const SharedSelection& selection);

	// Trys to place a parsed circuit at a position
	bool tryInsertParsedCircuit(const ParsedCircuit& parsedCircuit, Position position);
	bool tryInsertGeneratedCircuit(const GeneratedCircuit& generatedCircuit, Position position);
	bool tryInsertCopiedBlocks(const SharedCopiedBlocks& copiedBlocks, Position position, Orientation transformAmount);

	/* ----------- connections ----------- */
	// Trys to creates a connection. Returns if successful.
	bool tryCreateConnection(Position outputPosition, Position inputPosition);
	// Trys to remove a connection. Returns if successful.
	bool tryRemoveConnection(Position outputPosition, Position inputPosition);
	// Trys to creates a connection. Returns if successful.
	bool tryCreateConnection(ConnectionEnd outputConnectionEnd, ConnectionEnd inputConnectionEnd);
	// Trys to remove a connection. Returns if successful.
	bool tryRemoveConnection(ConnectionEnd outputConnectionEnd, ConnectionEnd inputConnectionEnd);
	// Trys to creates a connection. Returns if successful.
	bool tryCreateConnection(const SharedSelection& outputSelection, const SharedSelection& inputSelection);
	// Trys to remove connections.
	bool tryRemoveConnection(const SharedSelection& outputSelection, const SharedSelection& inputSelection);

	/* ----------- undo ----------- */
	void undo();
	void redo();

private:
	void pushOntoStack(Position blockPosition, Difference * difference, MoveType moveType = MoveType::MULTI_BEGIN);
	void popOffStack(Position position, Orientation transformAmount, bool resetRotation, Difference * difference, MoveType moveType = MoveType::MULTI_FINAL);

	void setBlockType(BlockType blockType);
	void blockSizeChange(const DataUpdateEventManager::EventData* eventData);
	void addConnectionPort(const DataUpdateEventManager::EventData* eventData);
	void removeConnectionPort(const DataUpdateEventManager::EventData* eventData);

	// helpers
	void setType(const SharedSelection& selection, BlockType type, Difference* difference);

	void createConnection(const SharedSelection& outputSelection, const SharedSelection& inputSelection, Difference* difference);
	void removeConnection(const SharedSelection& outputSelection, const SharedSelection& inputSelection, Difference* difference);

	void startUndo() { midUndo = true; }
	void endUndo() { midUndo = false; }

	void sendDifference(DifferenceSharedPtr difference) {
		if (difference->empty()) return;
		editCount++;
		if (!midUndo) undoSystem.addDifference(difference);
		for (const CircuitDiffListenerData& circuitDiffListenerData : listenerFunctions) circuitDiffListenerData.circuitDiffListenerFunction(difference, circuitId);
	}

	const Position stackBottom = Position(10000000, -10000000);
	Position stackTop = stackBottom;
	std::string circuitName;
	std::string circuitUUID;
	circuit_id_t circuitId;
	BlockContainer blockContainer;
	DataUpdateEventManager* dataUpdateEventManager;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;

	struct CircuitDiffListenerData {
		void* obj;
		unsigned int priority;
		CircuitDiffListenerFunction circuitDiffListenerFunction;
	};

	std::vector<CircuitDiffListenerData> listenerFunctions;

	UndoSystem undoSystem;
	bool midUndo = false;
	bool editable = true;

	unsigned long long editCount = 0;
};

typedef std::shared_ptr<Circuit> SharedCircuit;

#endif /* circuit_h */
