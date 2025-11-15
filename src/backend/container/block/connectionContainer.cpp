#include "connectionContainer.h"

bool ConnectionContainer::tryMakeConnection(connection_end_id_t thisEndId, ConnectionEnd otherConnectionEnd) {
	auto pair = connections[thisEndId].emplace(otherConnectionEnd);
	return pair.second;
}

bool ConnectionContainer::tryRemoveConnection(connection_end_id_t thisEndId, ConnectionEnd otherConnectionEnd) {
	auto iter = connections.find(thisEndId);
	if (iter == connections.end()) return false;
	// get the connections set corresponding with the end id
	std::unordered_set<ConnectionEnd>& connectionsSet = iter->second;
	auto iter2 = connectionsSet.find(otherConnectionEnd);
	if (iter2 == connectionsSet.end()) {
		return false;
	}
	// if there this is the last connection remove the id from the connection container
	if (connectionsSet.size() == 1) {
		connections.erase(iter);
		return true;
	}
	connectionsSet.erase(iter2);
	return true;
}

bool ConnectionContainer::hasConnection(connection_end_id_t thisEndId, ConnectionEnd otherConnectionEnd) const {
	auto iter = connections.find(thisEndId);
	return iter != connections.end() && iter->second.contains(otherConnectionEnd);
}

nlohmann::json ConnectionContainer::dumpState() const {
	nlohmann::json stateJson;
	for (const auto& [endId, connectionSet] : connections) {
		nlohmann::json connectionArray = nlohmann::json::array();
		for (const ConnectionEnd& connectionEnd : connectionSet) {
			connectionArray.push_back(connectionEnd.dumpState());
		}
		stateJson[std::to_string(endId)] = connectionArray;
	}
	return stateJson;
}
