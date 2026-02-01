#ifndef evalLogicSimulator_h
#define evalLogicSimulator_h

#include "backend/circuit/circuitDefs.h"
#include "backend/address.h"
#include "logicSimulator.h"
#include "simulatorDefs.h"
#include "../evalDefs.h"

class EvaluatorInternal;
class BlockDataManager;
class CircuitManager;
class Address;

class EvalLogicSimulator {
public:
	static constexpr double MIN_TICKRATE_DECREASABLE = 0.1;

	EvalLogicSimulator(simulator_id_t simulatorId, const CircuitManager& circuitManager, circuit_id_t circuitId, DataUpdateEventManager& dataUpdateEventManager);
	~EvalLogicSimulator();

	std::string getSimulatorName() const;
	simulator_id_t getSimulatorId() const { return simulatorId; }
	circuit_id_t getCircuitId() const { return circuitId; }
	circuit_id_t getCircuitId(const Address& address) const;

	// --------------- Controls ---------------

	void resetStates() { logicSimulator.resetStates(); }

	// Simulator Id State
	void setState(simulator_state_index_t id, logic_state_t state) { logicSimulator.setState(id, state); }
	logic_state_t getState(simulator_state_index_t id) const { return logicSimulator.getState(id); }
	std::vector<logic_state_t> getStates(const std::vector<simulator_state_index_t>& ids) const { return logicSimulator.getStates(ids); }

	// Address State
	void setState(const Address& address, logic_state_t state);
	logic_state_t getState(const Address& address) const;
	std::variant<logic_state_t, std::vector<logic_state_t>> getPinState(const Address& address);

	// Speed/Ticking
	void setPause(bool pause) { logicSimulator.setRunning(!pause); }
	bool isPause() const { return !logicSimulator.isRunning(); }
	void addSprint(unsigned int nTicks) { logicSimulator.addSprint(nTicks); }
	bool isSprinting() const { return logicSimulator.getSprintCount() > 0; }
	void waitForSprintComplete();
	void tickStep(unsigned int nTicks);
	void tickStep() { tickStep(1); }
	bool stepBack();
	void stepForward();
	bool skipBack();
	bool skipForward();
	inline bool isViewingReplay() const { return logicSimulator.isViewingReplay(); }
	void setRealistic(bool realistic) { logicSimulator.setRealistic(realistic); }
	bool isRealistic() const { return logicSimulator.isRealistic(); }
	void setTickrate(double tickrate) { logicSimulator.setTargetTickrate(tickrate); }
	double getTickrate() const { return logicSimulator.getTargetTickrate(); }
	void increaseTickrateSeq();
	void decreaseTickrateSeq();
	void setUseTickrate(bool useTickrate) { logicSimulator.setTickrateLimiter(useTickrate); }
	bool getUseTickrate() const { return logicSimulator.isTickrateLimiterEnabled(); }
	double getRealTickrate() const { return logicSimulator.getAverageTickrate(); }

	// --------------- Other ---------------

	std::optional<simulator_state_index_t> getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const;

	SimulatorStateIndexVecVariant getVirtualConnectionSimulatorId(const Address& address, virtual_connection_id_t virtualConnectionId) const;
	SimulatorStateIndexVecVariant getPinSimulatorId(const Address& address) const;
	std::pair<SimulatorStateIndexVecVariant, SimulatorStateIndexVecVariant> getPinAndNotPinSimulatorId(std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPoints) const;
	std::vector<SimulatorStateIndexVecVariant> getVirtualConnectionSimulatorIds(const Address& addressOrigin, const std::vector<std::pair<Position, virtual_connection_id_t>>& virtualConnections) const;
	std::vector<SimulatorStateIndexVecVariant> getPinSimulatorIds(const Address& addressOrigin, const std::vector<Position>& positions) const;

	void processEdits();

	void connectListener(void* object, const Address& address, SimulatorMappingUpdateListenerFunction func) const;
	void disconnectListener(void* object) const;

	nlohmann::json dumpState() const { return logicSimulator.dumpState(); }

private:
	SimulatorStateIndexVecVariant getVirtualConnectionSimulatorId_noMux(const Address& address, virtual_connection_id_t virtualConnectionId) const;
	SimulatorStateIndexVecVariant getPinSimulatorId_noMux(const Address& address) const;
	std::pair<SimulatorStateIndexVecVariant, SimulatorStateIndexVecVariant> getPinAndNotPinSimulatorId_noMux(std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPoints) const;
	std::pair<simulator_state_index_t, simulator_state_index_t> getPinAndNotPinSimulatorId_noMux(EvalConnectionPoint connectionPoint) const;
	std::optional<simulator_state_index_t> getSimulatorStateIndex_noMux(EvalConnectionPoint evalConnectionPoint) const;
	std::optional<simulator_state_index_t> getSimulatorStateIndexConsideringOutput_noMux(EvalConnectionPoint evalConnectionPoint) const;

	mutable std::mutex mux;

	std::vector<simulator_state_index_t> dirtySimulatorIds;
	LogicSimulator logicSimulator;
	const CircuitManager& circuitManager;
	const EvaluatorInternal& evaluatorInternal;
	simulator_id_t simulatorId;
	circuit_id_t circuitId;

	mutable std::map<void*, SimulatorMappingUpdateListener> simulatorMappingUpdateListeners;
};

#endif /* evalLogicSimulator_h */
