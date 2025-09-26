#ifndef replacer_h
#define replacer_h

#include "simulatorOptimizer.h"
#include "evalConfig.h"
#include "evalConnection.h"
#include "evalTypedef.h"
#include "idProvider.h"
#include "gateType.h"
#include "logicSimulator.h"
#include "replacement.h"

class Replacement;

class Replacer {
public:
	friend class Replacement;
	Replacer(
		EvalConfig& evalConfig,
		IdProvider<middle_id_t>& middleIdProvider,
		std::vector<simulator_id_t>& dirtySimulatorIds) :
		simulatorOptimizer(evalConfig, middleIdProvider, dirtySimulatorIds),
		evalConfig(evalConfig),
		middleIdProvider(middleIdProvider) {}

	inline void addGate(SimPauseGuard& pauseGuard, const GateType gateType, const middle_id_t gateId) {
		simulatorOptimizer.addGate(pauseGuard, gateType, gateId);
	}

	void removeGate(SimPauseGuard& pauseGuard, const middle_id_t gateId) {
		pingOutputs(pauseGuard, gateId);
		pingInputs(pauseGuard, gateId);
		simulatorOptimizer.removeGate(pauseGuard, gateId);
	}

	inline SimPauseGuard beginEdit() {
		return simulatorOptimizer.beginEdit();
	}

	void endEdit(SimPauseGuard& pauseGuard) {
		cleanReplacements();
		mergeJunctions(pauseGuard);

		simulatorOptimizer.endEdit(pauseGuard);
	}

	inline std::optional<simulator_id_t> getSimIdFromMiddleId(middle_id_t middleId) const {
		return simulatorOptimizer.getSimIdFromMiddleId(middleId);
	}

	inline std::optional<simulator_id_t> getSimIdFromConnectionPoint(const EvalConnectionPoint& point) const {
		return simulatorOptimizer.getSimIdFromConnectionPoint(point);
	}

	inline logic_state_t getState(EvalConnectionPoint point) const {
		return simulatorOptimizer.getState(getReplacementConnectionPoint(point));
	}

	inline std::vector<logic_state_t> getStates(const std::vector<EvalConnectionPoint>& points) const {
		return simulatorOptimizer.getStates(getReplacementConnectionPoints(points));
	}

	inline std::vector<logic_state_t> getPinStates(const std::vector<EvalConnectionPoint>& points) const {
		return simulatorOptimizer.getPinStates(getReplacementConnectionPoints(points));
	}

	inline std::vector<logic_state_t> getStatesFromSimulatorIds(const std::vector<simulator_id_t>& simulatorIds) const {
		return simulatorOptimizer.getStatesFromSimulatorIds(simulatorIds);
	}

	inline std::vector<SimulatorStateAndPinSimId> getSimulatorIds(const std::vector<EvalConnectionPoint>& points) const {
		return simulatorOptimizer.getSimulatorIds(getReplacementConnectionPoints(points));
	}

	inline std::vector<simulator_id_t> getBlockSimulatorIds(const std::vector<std::optional<EvalConnectionPoint>>& points) const {
		return simulatorOptimizer.getBlockSimulatorIds(getReplacementConnectionPoints(points));
	}

	inline std::vector<simulator_id_t> getPinSimulatorIds(const std::vector<std::optional<EvalConnectionPoint>>& points) const {
		return simulatorOptimizer.getPinSimulatorIds(getReplacementConnectionPoints(points));
	}

	inline void setState(EvalConnectionPoint id, logic_state_t state) {
		simulatorOptimizer.setState(getReplacementConnectionPoint(id), state);
	}

	void makeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		pingOutputs(pauseGuard, connection.source.gateId);
		pingInputs(pauseGuard, connection.destination.gateId);
		simulatorOptimizer.makeConnection(pauseGuard, connection);
	}

	void removeConnection(SimPauseGuard& pauseGuard, EvalConnection connection) {
		pingOutputs(pauseGuard, connection.source.gateId);
		pingInputs(pauseGuard, connection.destination.gateId);
		simulatorOptimizer.removeConnection(pauseGuard, connection);
	}

	inline double getAverageTickrate() const {
		return simulatorOptimizer.getAverageTickrate();
	}

private:
	SimulatorOptimizer simulatorOptimizer;
	EvalConfig& evalConfig;
	IdProvider<middle_id_t>& middleIdProvider;
	std::vector<Replacement> replacements;
	std::unordered_map<middle_id_t, std::unordered_map<connection_port_id_t, EvalConnectionPoint>> replacedConnectionPoints;
	std::unordered_map<middle_id_t, middle_id_t> replacedIds;
	std::unordered_set<middle_id_t> replacementIds;

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
	};

	Replacement& makeReplacement();
	void cleanReplacements();
	void pingOutputs(SimPauseGuard& pauseGuard, middle_id_t id);
	void pingInputs(SimPauseGuard& pauseGuard, middle_id_t id);
	EvalConnectionPoint getReplacementConnectionPoint(EvalConnectionPoint point) const;
	std::vector<EvalConnectionPoint> getReplacementConnectionPoints(const std::vector<EvalConnectionPoint>& points) const;
	std::vector<std::optional<EvalConnectionPoint>> getReplacementConnectionPoints(const std::vector<std::optional<EvalConnectionPoint>>& points) const;
	void mergeJunctions(SimPauseGuard& pauseGuard);
	JunctionFloodFillResult junctionFloodFill(middle_id_t junctionId);
};

#endif /* replacer_h */
