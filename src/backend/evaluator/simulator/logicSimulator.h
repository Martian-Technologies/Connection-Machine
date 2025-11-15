#ifndef logicSimulator_h
#define logicSimulator_h

#include "backend/container/block/blockDefs.h"
#include "backend/container/block/connectionEnd.h"
#include "backend/evaluator/util/evalConfig.h"
#include "backend/evaluator/evalDefs.h"
#include "simulatorGates.h"
#include "logicState.h"
#include "threadPool.h"

enum class SimGateType : int {
	AND = 0,
	XOR = 1,
	JUNCTION = 2,
	BUFFER = 3,
	SINGLE_BUFFER = 4,
	TRISTATE_BUFFER = 5,
	CONSTANT = 6,
	CONSTANT_RESET = 7,
	COPY_SELF_OUTPUT = 8,
	PORTS_TO_INT = 9
};

inline std::string simgatetype_to_string(SimGateType type) {
	switch (type) {
		case SimGateType::AND:
			return "AND";
		case SimGateType::XOR:
			return "XOR";
		case SimGateType::JUNCTION:
			return "JUNCTION";
		case SimGateType::BUFFER:
			return "BUFFER";
		case SimGateType::SINGLE_BUFFER:
			return "SINGLE_BUFFER";
		case SimGateType::TRISTATE_BUFFER:
			return "TRISTATE_BUFFER";
		case SimGateType::CONSTANT:
			return "CONSTANT";
		case SimGateType::CONSTANT_RESET:
			return "CONSTANT_RESET";
		case SimGateType::COPY_SELF_OUTPUT:
			return "COPY_SELF_OUTPUT";
		case SimGateType::PORTS_TO_INT:
			return "PORTS_TO_INT";
		default:
			return "UNKNOWN_GATE_TYPE";
	}
}

class LogicSimulator {
friend class SimulatorOptimizer;
friend class SimPauseGuard;
public:
	LogicSimulator(
		EvalConfig& evalConfig,
		std::vector<simulator_id_t>& dirtySimulatorIds);
	~LogicSimulator();
	void clearState();
	double getAverageTickrate() const;
	void setState(simulator_id_t id, logic_state_t state);

	logic_state_t getState(simulator_id_t id) const;
	std::vector<logic_state_t> getStates(const std::vector<simulator_id_t>& ids) const;
	std::optional<simulator_id_t> getOutputPortId(simulator_id_t simId, connection_end_id_t portId) const;

	simulator_id_t addGate(const BlockType blockType);
	void removeGate(simulator_id_t gateId);
	void makeConnection(simulator_id_t sourceId, connection_end_id_t sourcePort, simulator_id_t destinationId, connection_end_id_t destinationPort);
	void removeConnection(simulator_id_t sourceId, connection_end_id_t sourcePort, simulator_id_t destinationId, connection_end_id_t destinationPort);
	void endEdit();

	const std::vector<simulator_id_t> getOutputs(simulator_id_t simId);

	bool stepBack();
	bool stepForward();
	bool skipBack();
	bool skipForward();
	inline bool isViewingReplay() const {
		return viewingReplay;
	}

	nlohmann::json dumpState() const;

private:
	EvalConfig& evalConfig;
	std::thread simulationThread;
	std::atomic<bool> running { true };

	std::atomic<bool> pauseRequest { false };
	std::mutex cvMutex;
	std::condition_variable cv;

	IdVector<simulator_id_t, logic_state_t> statesA;
	IdVector<simulator_id_t, logic_state_t> statesB;
	std::array<IdVector<simulator_id_t, logic_state_t>, 1024> statesReplay;
	std::array<bool, 1024> setStateUsed = { false };
	int replayHead = 0;
	int replayTail = 0;
	bool viewingReplay = false;
	int stateView = 0;
	inline void advanceReplayHead() {
		replayHead = (replayHead + 1) % statesReplay.size();
		if (replayHead == replayTail) {
			replayTail = (replayTail + 1) % statesReplay.size();
		}
		viewingReplay = false;
	}
	inline void invalidateReplay() {
		replayTail = replayHead;
		viewingReplay = false;
	}

	logic_state_t getStateUnlocked(simulator_id_t id) const;

	mutable std::shared_mutex statesAMutex;
	mutable std::mutex mainDataMutex;

	struct StateChange {
		simulator_id_t id;
		logic_state_t state;
		nlohmann::json dumpState() const;
	};
	std::queue<StateChange> pendingStateChanges;
	std::mutex stateChangeQueueMutex;

	std::vector<ANDLikeGate> andGates;
	std::vector<XORLikeGate> xorGates;
	std::vector<JunctionGate> junctions;
	std::vector<BufferGate> buffers;
	std::vector<SingleBufferGate> singleBuffers;
	std::vector<TristateBufferGate> tristateBuffers;
	std::vector<ConstantGate> constantGates;
	std::vector<ConstantResetGate> constantResetGates;
	std::vector<CopySelfOutputGate> copySelfOutputGates;
	std::vector<PortsToIntGate> portsToIntGates;

	struct JobInstruction {
		LogicSimulator* self;
		size_t start;
		size_t end;
	};

	// Static job executors (free-function compatible) ---------------------------------
	static void execAND(void* jobInstruction);
	static void execANDRealistic(void* jobInstruction);
	static void execXOR(void* jobInstruction);
	static void execXORRealistic(void* jobInstruction);
	static void execTristate(void* jobInstruction);
	static void execTristateRealistic(void* jobInstruction);
	static void execSingleBuffer(void* jobInstruction);
	static void execSingleBufferRealistic(void* jobInstruction);
	static void execConstantReset(void* jobInstruction);
	static void execCopySelfOutput(void* jobInstruction);
	static void execPortsToInt(void* jobInstruction);
	static void saveReplayStates(void* jobInstruction);

	void tickANDGates(void* jobInstruction) {
		auto* ji = static_cast<JobInstruction*>(jobInstruction);
		for (size_t i = ji->start; i < ji->end; ++i) {
			andGates[i].tick(statesA, statesB);
		}
	};
	void tickXORGates(void* jobInstruction) {
		auto* ji = static_cast<JobInstruction*>(jobInstruction);
		for (size_t i = ji->start; i < ji->end; ++i) {
			xorGates[i].tick(statesA, statesB);
		}
	};
	void tickTristateBuffers(void* jobInstruction) {
		auto* ji = static_cast<JobInstruction*>(jobInstruction);
		for (size_t i = ji->start; i < ji->end; ++i) {
			tristateBuffers[i].tick(statesA, statesB);
		}
	};
	void tickConstantResetGates(void* jobInstruction) {
		auto* ji = static_cast<JobInstruction*>(jobInstruction);
		for (size_t i = ji->start; i < ji->end; ++i) {
			constantResetGates[i].tick(statesB);
		}
	}
	void tickCopySelfOutputGates(void* jobInstruction) {
		auto* ji = static_cast<JobInstruction*>(jobInstruction);
		for (size_t i = ji->start; i < ji->end; ++i) {
			copySelfOutputGates[i].tick(statesA, statesB);
		}
	}
	void tickPortsToIntGates(void* jobInstruction) {
		auto* ji = static_cast<JobInstruction*>(jobInstruction);
		for (size_t i = ji->start; i < ji->end; ++i) {
			portsToIntGates[i].tick(statesA, statesB);
		}
	}

	void realisticTickANDGates(void* jobInstruction) {
		auto* ji = static_cast<JobInstruction*>(jobInstruction);
		for (size_t i = ji->start; i < ji->end; ++i) {
			andGates[i].realisticTick(statesA, statesB);
		}
	};
	void realisticTickXORGates(void* jobInstruction) {
		auto* ji = static_cast<JobInstruction*>(jobInstruction);
		for (size_t i = ji->start; i < ji->end; ++i) {
			xorGates[i].realisticTick(statesA, statesB);
		}
	};
	void realisticTickTristateBuffers(void* jobInstruction) {
		auto* ji = static_cast<JobInstruction*>(jobInstruction);
		for (size_t i = ji->start; i < ji->end; ++i) {
			tristateBuffers[i].realisticTick(statesA, statesB);
		}
	};

	IdProvider<simulator_id_t> simulatorIdProvider;

	struct GateDependency {
		simulator_id_t gateId;

		GateDependency() : gateId(0) {}
		explicit GateDependency(simulator_id_t id) : gateId(id) {}

		bool operator==(const GateDependency& other) const {
			return gateId == other.gateId;
		}
		nlohmann::json dumpState() const;
	};

	struct GateLocation {
		SimGateType gateType;
		size_t gateIndex;

		GateLocation() : gateType(SimGateType::AND), gateIndex(0) {}
		GateLocation(SimGateType type, size_t index) : gateType(type), gateIndex(index) {}
		nlohmann::json dumpState() const;
	};

	std::unordered_map<simulator_id_t, std::vector<GateDependency>> outputDependencies;
	std::unordered_map<simulator_id_t, GateLocation> gateLocations;

	void simulationLoop();
	inline void tickOnce();
	void processPendingStateChanges();

	inline void updateEmaTickrate(
		const std::chrono::steady_clock::time_point& currentTime,
		std::chrono::steady_clock::time_point& lastTickTime,
		bool& isFirstTick);

	void addInputToGate(simulator_id_t simId, simulator_id_t inputId, connection_end_id_t portId);
	void removeInputFromGate(simulator_id_t simId, simulator_id_t inputId, connection_end_id_t portId);
	std::optional<std::vector<simulator_id_t>> getOutputSimIdsFromGate(simulator_id_t simId) const;

	void updateGateLocation(simulator_id_t gateId, SimGateType gateType, size_t gateIndex);
	void removeGateLocation(simulator_id_t gateId);
	void addOutputDependency(simulator_id_t outputId, simulator_id_t dependentGateId);
	void removeOutputDependency(simulator_id_t outputId, simulator_id_t dependentGateId);

	std::atomic<double> averageTickrate { 0.0 };
	double tickrateHalflife { 0.3 };

	std::vector<simulator_id_t>& dirtySimulatorIds;

	ThreadPool threadPool;
	std::vector<std::vector<ThreadPool::Job>> jobs;
	std::vector<std::unique_ptr<JobInstruction>> jobInstructionStorage;

	void regenerateJobs();

	void extendDataVectors(simulator_id_t id) {
		if (statesA.size() <= id) {
			statesA.resizeWithOffset(id, 1, logic_state_t::UNDEFINED);
			statesB.resizeWithOffset(id, 1, logic_state_t::UNDEFINED);
			for (IdVector<simulator_id_t, logic_state_t>& replayStates : statesReplay) {
				replayStates.resizeWithOffset(id, 1, logic_state_t::UNDEFINED);
			}
		}
	}

	void updateThreadCount(size_t threadCount) {
		threadCount = std::max(threadCount, size_t(1));
		threadPool.resizeThreads(threadCount - 1);
	}

	std::chrono::steady_clock::time_point nextTick;
	std::chrono::steady_clock::time_point lastTickTime;
};

class SimPauseGuard {
public:
	explicit SimPauseGuard(LogicSimulator& s) : sim(s), mainLock(sim.mainDataMutex) {
		sim.pauseRequest.store(true, std::memory_order_release);
	}
	~SimPauseGuard() {
		sim.pauseRequest.store(false, std::memory_order_release);
		// sim.nextTick = std::chrono::steady_clock::now();
		// sim.lastTickTime = std::chrono::steady_clock::now();
	}

private:
	LogicSimulator& sim;
	std::unique_lock<std::mutex> mainLock;
};

#endif /* logicSimulator_h */
