#ifndef connectionContainer_h
#define connectionContainer_h

#include "connectionEnd.h"

class ConnectionContainer {
	friend class BlockContainer;
public:
	inline const std::unordered_map<connection_end_id_t, std::unordered_set<ConnectionEnd>>& getConnections() const { return connections; }

	// returns null if no connection made to that port (even if the port exist)
	inline const std::unordered_set<ConnectionEnd>* getConnections(connection_end_id_t thisEndId) const {
		auto iter = connections.find(thisEndId);
		if (iter == connections.end()) return nullptr;
		return &(iter->second);
	}

	bool hasConnection(connection_end_id_t thisEndId, ConnectionEnd otherConnectionEnd) const;

	nlohmann::json dumpState() const;

private:
	bool tryMakeConnection(connection_end_id_t thisEndId, ConnectionEnd otherConnectionEnd);
	bool tryRemoveConnection(connection_end_id_t thisEndId, ConnectionEnd otherConnectionEnd);

	std::unordered_map<connection_end_id_t, std::unordered_set<ConnectionEnd>> connections;
};

#endif /* connectionContainer_h */
