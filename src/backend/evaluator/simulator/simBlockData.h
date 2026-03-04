#ifndef simBlockData_h
#define simBlockData_h

#include "../evalDefs.h"

namespace SimBlockData {
	enum class PortDirection {
		INPUT,
		OUTPUT,
		BIDIRECTIONAL
	};

	struct PortInfo {
		PortDirection direction;
		bool limitedToOneConnection;
	};

	static PortInfo getPortInfo(BlockType blockType, connection_end_id_t connectionEndId);
	static PortDirection getPortDirection(BlockType blockType, connection_end_id_t connectionEndId) { return getPortInfo(blockType, connectionEndId).direction; }
	static bool isPortLimitedToOneConnection(BlockType blockType, connection_end_id_t connectionEndId) { return getPortInfo(blockType, connectionEndId).limitedToOneConnection; }
};

#endif