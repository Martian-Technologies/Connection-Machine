#include "block.h"

nlohmann::json Block::dumpState() const {
	nlohmann::json stateJson;
	stateJson["id"] = blockId;
	stateJson["type"] = blocktype_to_string(blockType);
	stateJson["pos"] = position.toString();
	stateJson["orientation"] = orientation.toString();
	stateJson["connections"] = connections.dumpState();
	return stateJson;
}
