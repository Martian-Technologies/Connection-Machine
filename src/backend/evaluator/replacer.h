#ifndef replacer_h
#define replacer_h

#include "busInterfacePassthrough.h"
#include "replacement.h"

struct SimulatorStateAndPinSimId {
	std::variant<simulator_id_t, std::vector<simulator_id_t>> portSimIds;
	std::variant<simulator_id_t, std::vector<simulator_id_t>> pinSimIds;
};

class Replacer {
	friend class Replacement;
public:
	Replacer(
		EvalConfig& evalConfig,
		IdProvider<middle_id_t>& middleIdProvider,
		std::vector<simulator_id_t>& dirtySimulatorIds,
		std::vector<middle_id_t>& dirtyMiddleIds,
		BlockDataManager& blockDataManager) :
		busInterfacePassthrough(evalConfig, middleIdProvider, dirtySimulatorIds, blockDataManager),
		evalConfig(evalConfig),
		dirtyMiddleIds(dirtyMiddleIds),
		middleIdProvider(middleIdProvider),
		blockDataManager(blockDataManager) {}

	void addGate(SimPauseGuard& pauseGuard, const BlockType blockType, const middle_id_t gateId) {
		busInterfacePassthrough.addGate(pauseGuard, blockType, gateId);
	}

	void removeGate(SimPauseGuard& pauseGuard, const middle_id_t gateId) {
		pingId(pauseGuard, gateId, 0);
		busInterfacePassthrough.removeGate(pauseGuard, gateId);
	}

	inline SimPauseGuard beginEdit() {
		return busInterfacePassthrough.beginEdit();
	}

	void endEdit(SimPauseGuard& pauseGuard) {
		cleanReplacements();
		mergeBuses(pauseGuard, 0, 2);
		mergeJunctions(pauseGuard, 1);

		busInterfacePassthrough.endEdit(pauseGuard);
	}

	inline std::optional<simulator_id_t> getSimIdFromMiddleId(middle_id_t middleId) const {
		return busInterfacePassthrough.getSimIdFromMiddleId(middleId);
	}

	inline logic_state_t getState(EvalConnectionPoint point) const {
		return busInterfacePassthrough.getState(getReplacementConnectionPoint(point));
	}

	inline std::vector<logic_state_t> getStates(const std::vector<EvalConnectionPoint>& points) const {
		return busInterfacePassthrough.getStates(getReplacementConnectionPoints(points));
	}

	inline std::vector<logic_state_t> getPinStates(const std::vector<EvalConnectionPoint>& points) const {
		return busInterfacePassthrough.getPinStates(getReplacementConnectionPoints(points));
	}

	inline std::vector<logic_state_t> getStatesFromSimulatorIds(const std::vector<simulator_id_t>& simulatorIds) const {
		return busInterfacePassthrough.getStatesFromSimulatorIds(simulatorIds);
	}

	inline std::vector<simulator_id_t> getBlockSimulatorIds(const std::vector<std::optional<EvalConnectionPoint>>& points) const {
		std::vector<simulator_id_t> result;
		for (const auto& pointOpt : points) {
			if (!pointOpt.has_value()) {
				result.emplace_back(0);
				continue;
			}
			BlockType originalBlockType = busInterfacePassthrough.getBlockType(pointOpt->gateId);
			if (originalBlockType != BlockType::NONE) {
				const BlockData* blockData = blockDataManager.getBlockData(originalBlockType);
				if (!blockData->hasBlockState()) {
					result.emplace_back(0);
					continue;
				}
			}
			EvalConnectionPoint replacedPoint = getReplacementConnectionPoint(*pointOpt);
			BlockType blockType = busInterfacePassthrough.getBlockType(replacedPoint.gateId);
			if (blockType == BlockType::NONE) {
				result.emplace_back(0);
				continue;
			}
			simulator_id_t simId = busInterfacePassthrough.getBlockSimulatorId(replacedPoint);
			result.emplace_back(simId);
		}
		return result;
	}

	std::vector<std::variant<simulator_id_t, std::vector<simulator_id_t>>> getPinSimulatorIds(const std::vector<std::optional<EvalConnectionPoint>>& points) const;

	inline void setState(EvalConnectionPoint id, logic_state_t state) {
		busInterfacePassthrough.setState(getReplacementConnectionPoint(id), state);
	}

	void makeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		pingId(pauseGuard, connection.source.gateId, 0);
		pingId(pauseGuard, connection.destination.gateId, 0);
		busInterfacePassthrough.makeConnection(pauseGuard, connection);
	}

	void removeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		pingId(pauseGuard, connection.source.gateId, 0);
		pingId(pauseGuard, connection.destination.gateId, 0);
		busInterfacePassthrough.removeConnection(pauseGuard, connection);
		busInterfacePassthrough.removeConnection(pauseGuard, connection.reverse());
	}

	inline double getAverageTickrate() const {
		return busInterfacePassthrough.getAverageTickrate();
	}

private:
	BusInterfacePassthrough busInterfacePassthrough;
	EvalConfig& evalConfig;
	IdProvider<middle_id_t>& middleIdProvider;
	std::vector<Replacement> replacements;
	std::unordered_map<middle_id_t, std::unordered_map<connection_port_id_t, EvalConnectionPoint>> replacedConnectionPoints;
	std::unordered_map<middle_id_t, middle_id_t> replacedIds;
	std::unordered_map<middle_id_t, int> replacementIdLayers;

	struct JunctionFloodFillResult {
		std::vector<EvalConnectionPoint> outputsGoingIntoJunctions;
		// connections that are going directly into the junctions
		// A -> JUNCTION
		std::vector<EvalConnection> inputsPullingFromJunctions;
		// connections that are pulling from the junctions
		// JUNCTION -> C
		std::vector<middle_id_t> junctionIds;
		std::vector<EvalConnection> connectionsToReroute;
		// connections that are connected to the junctions through the outputs of other gates
		// A -> JUNCTION, A -> B, A -> B is a connection to reroute because B should actually pull from the junction
		logic_state_t defaultState { logic_state_t::FLOATING };
	};

	struct BusFloodFillResult {
		std::vector<middle_id_t> busIds;
		std::vector<middle_id_t> junctionIds;
		std::vector<EvalConnection> connectionsBetweenBusesAndJunctions;
		std::vector<EvalConnection> connectionsIntoBuses;
		std::vector<EvalConnection> connectionsOutOfBuses;
	};

	Replacement& makeReplacement(int layer);
	void cleanReplacements();
	void pingId(SimPauseGuard& pauseGuard, middle_id_t id, int minLayer);
	EvalConnectionPoint getReplacementConnectionPoint(EvalConnectionPoint point) const;
	std::vector<EvalConnectionPoint> getReplacementConnectionPoints(const std::vector<EvalConnectionPoint>& points) const;
	std::vector<std::optional<EvalConnectionPoint>> getReplacementConnectionPoints(const std::vector<std::optional<EvalConnectionPoint>>& points) const;
	struct BusInternalJunctionArray {
		std::vector<middle_id_t> junctionIds {};
		int numDefined {0};
	};
	std::unordered_map<middle_id_t, BusInternalJunctionArray> busInternalJunctions;
	std::optional<middle_id_t> getJunctionInsideBus(middle_id_t busId, unsigned int laneId) {
		if (!busInternalJunctions.contains(busId)) {
			return std::nullopt;
		}
		const BusInternalJunctionArray& busInternalJunctionArray = busInternalJunctions.at(busId);
		if (busInternalJunctionArray.junctionIds.size() <= laneId) {
			return std::nullopt;
		}
		middle_id_t junctionId = busInternalJunctionArray.junctionIds[laneId];
		if (junctionId == 0) {
			return std::nullopt;
		}
		return junctionId;
	};
	void defineJunctionInsideBus(middle_id_t busId, unsigned int laneId, middle_id_t junctionId) {
		dirtyMiddleIds.push_back(busId);
		if (!busInternalJunctions.contains(busId)) {
			busInternalJunctions[busId] = { std::vector<middle_id_t>{}, 0 };
		}
		BusInternalJunctionArray& busInternalJunctionArray = busInternalJunctions.at(busId);
		if (busInternalJunctionArray.junctionIds.size() <= laneId) {
			busInternalJunctionArray.junctionIds.resize(laneId + 1, 0);
		}
		busInternalJunctionArray.junctionIds[laneId] = junctionId;
		busInternalJunctionArray.numDefined += 1;
	};
	void defineJunctionInsideBus(middle_id_t busId, unsigned int laneId, middle_id_t junctionId, Replacement& replacement) {
		defineJunctionInsideBus(busId, laneId, junctionId);
		replacement.addRevertAction([this, busId, laneId]() {
			undefineJunctionInsideBus(busId, laneId);
		});
	};
	void undefineJunctionInsideBus(middle_id_t busId, unsigned int laneId) {
		dirtyMiddleIds.push_back(busId);
		BusInternalJunctionArray& busInternalJunctionArray = busInternalJunctions.at(busId);
		busInternalJunctionArray.numDefined -= 1;
		busInternalJunctionArray.junctionIds[laneId] = 0;
		if (busInternalJunctionArray.numDefined == 0) {
			busInternalJunctions.erase(busId);
			return;
		}
	};

	struct BlockLane {
		middle_id_t blockId;
		unsigned int laneId;
		bool operator==(const BlockLane& other) const noexcept {
			return blockId == other.blockId && laneId == other.laneId;
		}
		bool operator!=(const BlockLane& other) const noexcept {
			return !(*this == other);
		}
		bool operator<(const BlockLane& other) const noexcept {
			return std::tie(blockId, laneId) < std::tie(other.blockId, other.laneId);
		}
		struct Hash {
			size_t operator()(const BlockLane& value) const noexcept {
				size_t seed = static_cast<size_t>(value.blockId);
				seed ^= static_cast<size_t>(value.laneId) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
				return seed;
			}
		};
	};

	void mergeBuses(SimPauseGuard& pauseGuard, int layer, int junctionOverpowerLayer);
	void mergeBusLane(SimPauseGuard& pauseGuard, int layer, int junctionOverpowerLayer, middle_id_t id, unsigned int laneId);
	void mergeJunctions(SimPauseGuard& pauseGuard, int layer);
	JunctionFloodFillResult junctionFloodFill(middle_id_t junctionId);
	BlockDataManager& blockDataManager;
	std::vector<middle_id_t>& dirtyMiddleIds;
	bool isJunctionType(BlockType blockType) const {
		return blockType == BlockType::JUNCTION || blockType == BlockType::JUNCTION_L || blockType == BlockType::JUNCTION_H || blockType == BlockType::JUNCTION_X;
	}
};

#endif /* replacer_h */
