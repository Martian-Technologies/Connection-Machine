#ifndef replacement_h
#define replacement_h

#include "backend/container/block/blockDefs.h"
#include "evalConnection.h"

class SimPauseGuard;
class Replacer;

struct ReplacementGate {
	middle_id_t id;
	BlockType type;
};

class Replacement {
public:
	Replacement(
		Replacer* replacer,
		int layer
		) :
		replacer(replacer),
		layer(layer) {}

	void removeGate(SimPauseGuard& pauseGuard, middle_id_t gateId, std::unordered_map<connection_port_id_t, EvalConnectionPoint> replacementConnectionPoints);

	void removeGate(SimPauseGuard& pauseGuard, middle_id_t gateId, middle_id_t replacementId);

	void addGate(SimPauseGuard& pauseGuard, BlockType blockType, middle_id_t gateId);

	void removeConnection(SimPauseGuard& pauseGuard, EvalConnection connection);

	void makeConnection(SimPauseGuard& pauseGuard, EvalConnection connection);

	void overrideConnectionPoint(EvalConnectionPoint originalPoint, EvalConnectionPoint replacementPoint);

	void markIdAsReplaced(middle_id_t originalId, int overpowerLayer);

	void revert(SimPauseGuard& pauseGuard);

	void addRevertAction(std::function<void(SimPauseGuard&)> callback) {
		isEmpty = false;
		revertCallbacksWithPauseGuard.push_back(std::move(callback));
	}
	void addRevertAction(std::function<void()> callback) {
		isEmpty = false;
		revertCallbacks.push_back(std::move(callback));
	}

	void pingId(SimPauseGuard& pauseGuard, middle_id_t id) {
		if (isReverting) {
			return;
		}
		if (idsToTrack.contains(id)) {
			revert(pauseGuard);
		}
	}

	bool getIsEmpty() const {
		return isEmpty;
	}

	middle_id_t getNewId();

	void trackId(middle_id_t id) {
		idsToTrack.insert(id);
	}
	void trackConnection(EvalConnection conn) {
		trackId(conn.source.gateId);
		trackId(conn.destination.gateId);
	}
	void trackGate(middle_id_t id) {
		trackId(id);
	}

	int getLayer() const {
		return layer;
	}

private:
	struct ReplacementLayerEntry {
		middle_id_t id;
	};
	struct ReplacementConnectionPointOverride {
		middle_id_t gateId;
		connection_port_id_t portId;
		std::optional<EvalConnectionPoint> previousPoint;
	};

	void trackReplacementLayer(middle_id_t id, int layer);

	Replacer* replacer;
	std::vector<ReplacementGate> addedGates;
	std::vector<ReplacementGate> deletedGates;
	std::vector<EvalConnection> addedConnections;
	std::vector<EvalConnection> deletedConnections;
	std::vector<middle_id_t> reservedIds;
	std::vector<ReplacementLayerEntry> replacementLayerEntries;
	std::vector<ReplacementConnectionPointOverride> overriddenConnectionPoints;
	std::set<middle_id_t> idsToTrack;
	std::vector<std::function<void(SimPauseGuard&)>> revertCallbacksWithPauseGuard;
	std::vector<std::function<void()>> revertCallbacks;
	bool isEmpty { true };
	bool isReverting { false };
	int layer { 0 };
};

#endif /* replacement_h */
