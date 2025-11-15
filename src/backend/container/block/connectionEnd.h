#ifndef connectionEnd_h
#define connectionEnd_h

#include "blockDefs.h"
#include "util/id.h"
class ConnectionContainer;

DECLARE_ID_TYPE(connection_end_id_t, unsigned int);

class ConnectionEnd {
	friend ConnectionContainer;
public:
	ConnectionEnd(block_id_t blockId, connection_end_id_t connectionId) : blockId(blockId), connectionId(connectionId) { }

	block_id_t getBlockId() const { return blockId; }
	connection_end_id_t getConnectionId() const { return connectionId; }

	bool operator==(ConnectionEnd other) const { return other.connectionId == connectionId && other.blockId == blockId; }

	std::string toString() const { return "ConnectionEnd(blockId=" + std::to_string(blockId) + ", connectionId=" + std::to_string(connectionId) + ")"; }

	auto operator<=>(const ConnectionEnd&) const = default;

	nlohmann::json dumpState() const {
		nlohmann::json stateJson;
		stateJson["blockId"] = blockId;
		stateJson["connectId"] = connectionId.get();
		return stateJson;
	}
private:
	void setBlockId(block_id_t id) { blockId = id; }
	void setConnectionId(connection_end_id_t id) { connectionId = id; }

	block_id_t blockId;
	connection_end_id_t connectionId;
};

template <>
struct std::hash<ConnectionEnd> {
	inline std::size_t operator()(ConnectionEnd connectionEnd) const noexcept {
		std::size_t a = std::hash<connection_end_id_t>{}(connectionEnd.getConnectionId());
		std::size_t b = std::hash<block_id_t>{}(connectionEnd.getBlockId());
		return a + 0x9e3779b9 + (b << 6) + (b >> 2);
	}
};

#endif /* connectionEnd_h */
