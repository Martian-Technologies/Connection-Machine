#ifndef evalLogicSimulator_h
#define evalLogicSimulator_h

#include "backend/circuit/circuitDefs.h"
#include "backend/address.h"
#include "simulatorDefs.h"
#include "../evalDefs.h"
#include "logicState.h"

class LogicSimulator;
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

	void resetStates();

	// Simulator Id State
	void setState(simulator_state_reference id, logic_state_t state);
	logic_state_t getState(simulator_state_reference id) const;
	std::vector<logic_state_t> getStates(const std::vector<simulator_state_reference>& ids) const;
	void requestNewStatesOutputVector() const;

	// Address State
	void setState(const Address& address, logic_state_t state);
	logic_state_t getState(const Address& address) const;
	std::variant<logic_state_t, std::vector<logic_state_t>> getPinState(const Address& address);

	// Speed/Ticking
	void setPause(bool pause);
	bool isPause() const;
	void addSprint(unsigned int nTicks);
	bool isSprinting() const;
	void waitForSprintComplete();
	void tickStep(unsigned int nTicks);
	void tickStep() { tickStep(1); }
	bool stepBack();
	void stepForward();
	bool skipBack();
	bool skipForward();
	bool isViewingReplay() const;
	void setRealistic(bool realistic);
	bool isRealistic() const;
	void setTickrate(double tickrate);
	double getTickrate() const;
	void increaseTickrateSeq();
	void decreaseTickrateSeq();
	void setUseTickrate(bool useTickrate);
	bool getUseTickrate() const;
	double getRealTickrate() const;

	// --------------- Other ---------------

	std::optional<simulator_state_reference> getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const;

	SimulatorStateIndexVecVariant getVirtualConnectionSimulatorId(const Address& address, virtual_connection_id_t virtualConnectionId) const;
	SimulatorStateIndexVecVariant getPinSimulatorId(const Address& address) const;
	std::pair<SimulatorStateIndexVecVariant, SimulatorStateIndexVecVariant> getPinAndNotPinSimulatorId(std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPoints) const;
	std::vector<SimulatorStateIndexVecVariant> getVirtualConnectionSimulatorIds(const Address& addressOrigin, const std::vector<std::pair<Position, virtual_connection_id_t>>& virtualConnections) const;
	std::vector<SimulatorStateIndexVecVariant> getPinSimulatorIds(const Address& addressOrigin, const std::vector<Position>& positions) const;

	void processEdits();

	void connectListener(void* object, const Address& address, SimulatorMappingUpdateListenerFunction func) const;
	void disconnectListener(void* object) const;

	nlohmann::json dumpState() const;

private:
	SimulatorStateIndexVecVariant getVirtualConnectionSimulatorId_noMux(const Address& address, virtual_connection_id_t virtualConnectionId) const;
	SimulatorStateIndexVecVariant getPinSimulatorId_noMux(const Address& address) const;
	std::pair<SimulatorStateIndexVecVariant, SimulatorStateIndexVecVariant> getPinAndNotPinSimulatorId_noMux(std::variant<EvalConnectionPoint, std::vector<EvalConnectionPoint>> connectionPoints) const;
	std::pair<simulator_state_reference, simulator_state_reference> getPinAndNotPinSimulatorId_noMux(EvalConnectionPoint connectionPoint) const;
	std::optional<simulator_state_reference> getSimulatorStateIndex_noMux(EvalConnectionPoint evalConnectionPoint) const;
	std::optional<simulator_state_reference> getSimulatorStateIndexConsideringOutput_noMux(EvalConnectionPoint evalConnectionPoint) const;

	mutable std::mutex mux;

	std::vector<simulator_state_reference> dirtySimulatorIds;
	std::unique_ptr<LogicSimulator> logicSimulator;
	const CircuitManager& circuitManager;
	const EvaluatorInternal& evaluatorInternal;
	simulator_id_t simulatorId;
	circuit_id_t circuitId;

	mutable std::map<void*, SimulatorMappingUpdateListener> simulatorMappingUpdateListeners;
};

#endif /* evalLogicSimulator_h */
