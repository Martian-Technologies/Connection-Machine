#ifndef simBlockData_h
#define simBlockData_h

#include "../evalDefs.h"

namespace SimBlockData {
	enum class InputOutput {
		INPUT,
		OUTPUT
	};

	enum class PortDirection {
		INPUT,
		OUTPUT,
		BIDIRECTIONAL
	};

	enum class ConnectionDirection {
		AtoB,
		BtoA
	};

	struct PortInfo {
		PortDirection direction;
		bool limitedToOneConnection;
	};

	PortInfo getPortInfo(
		BlockType blockType,
		connection_end_id_t connectionEndId
	);
	inline PortDirection getPortDirection(
		BlockType blockType,
		connection_end_id_t connectionEndId
	) {
		return getPortInfo(blockType, connectionEndId).direction;
	}
	inline bool isPortLimitedToOneConnection(
		BlockType blockType,
		connection_end_id_t connectionEndId
	) {
		return getPortInfo(blockType, connectionEndId).limitedToOneConnection;
	}
	ConnectionDirection getConnectionDirection(
		BlockType blockTypeA,
		connection_end_id_t connectionEndIdA,
		BlockType blockTypeB,
		connection_end_id_t connectionEndIdB
	);
	const std::vector<connection_end_id_t>& getOutputPorts(BlockType blockType);
};

#endif
