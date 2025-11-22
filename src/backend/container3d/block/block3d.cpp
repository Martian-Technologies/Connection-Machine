#include "block3d.h"

nlohmann::json Block3d::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json stateJson;
	stateJson["id"] = blockId;
	stateJson["type"] = blocktype_to_string(blockType);
	stateJson["pos"] = position.toString();
	stateJson["orientation"] = orientation.toString();
	stateJson["connections"] = connections.dumpState();
	return stateJson;
}
