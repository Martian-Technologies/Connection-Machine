#ifndef logicGroupRunner_h
#define logicGroupRunner_h

#include "simulatorDefs.h"
#include "../evalDefs.h"
#include "logicState.h"
#include "threadPool.h"

struct LinkedGateGroup;
class LogicGroupRunner;

class ProcessingTable {
public:
	inline static logic_state_t buffer(logic_state_t input) {
		const static std::vector<logic_state_t> bufferTable = {
			logic_state_t::LOW, logic_state_t::HIGH, logic_state_t::UNDEFINED, logic_state_t::UNDEFINED
		};
		return bufferTable[static_cast<unsigned int>(input)];
	}
	inline static logic_state_t not_gate(logic_state_t input) {
		const static std::vector<logic_state_t> notTable = {
			logic_state_t::HIGH, logic_state_t::LOW, logic_state_t::UNDEFINED, logic_state_t::UNDEFINED
		};
		return notTable[static_cast<unsigned int>(input)];
	}
	inline static bool and_gate(logic_state_t& acc, logic_state_t input) {
		if (input == logic_state_t::LOW) {
			acc = logic_state_t::LOW;
			return false;
		}
		if (static_cast<unsigned int>(input) & 0b10) { // UNDEFINED or FLOATING
			acc = logic_state_t::UNDEFINED;
		}
		return true;
	}
	inline static bool or_gate(logic_state_t& acc, logic_state_t input) {
		if (input == logic_state_t::HIGH) {
			acc = logic_state_t::HIGH;
			return false;
		}
		if (static_cast<unsigned int>(input) & 0b10) { // UNDEFINED or FLOATING
			acc = logic_state_t::UNDEFINED;
		}
		return true;
	}
	inline static bool xor_gate(logic_state_t& acc, logic_state_t input) {
		if (static_cast<unsigned int>(input) & 0b10) { // UNDEFINED or FLOATING
			acc = logic_state_t::UNDEFINED;
			return false;
		}
		if (input == logic_state_t::HIGH) {
			acc = static_cast<logic_state_t>(static_cast<unsigned int>(acc) ^ 0b01);
		}
		return true;
	}
	inline static void invert_no_float_safe(logic_state_t& acc) {
		assert(acc != logic_state_t::FLOATING);
		unsigned int accVal = static_cast<unsigned int>(acc);
		unsigned int proc = (accVal ^ 0b01) | (accVal >> 1);
		acc = static_cast<logic_state_t>(proc);
	}
	inline static logic_state_t tristate_buffer(logic_state_t input, logic_state_t control) {
		if (control == logic_state_t::HIGH) {
			return buffer(input);
		} else if (control == logic_state_t::LOW) {
			return logic_state_t::FLOATING;
		} else {
			return logic_state_t::UNDEFINED;
		}
	}
	inline static bool junction(logic_state_t& acc, logic_state_t input) {
		if (input == logic_state_t::UNDEFINED) {
			acc = logic_state_t::UNDEFINED;
			return false;
		} else if (input == logic_state_t::HIGH) {
			if (acc == logic_state_t::LOW) {
				acc = logic_state_t::UNDEFINED;
				return false;
			}
			acc = logic_state_t::HIGH;
		} else if (input == logic_state_t::LOW) {
			if (acc == logic_state_t::HIGH) {
				acc = logic_state_t::UNDEFINED;
				return false;
			}
			acc = logic_state_t::LOW;
		}
		return true;
	}
	inline static logic_state_t pull_up(logic_state_t input) {
		if (input == logic_state_t::FLOATING) {
			return logic_state_t::HIGH;
		}
		return input;
	}
	inline static logic_state_t pull_down(logic_state_t input) {
		if (input == logic_state_t::FLOATING) {
			return logic_state_t::LOW;
		}
		return input;
	}
	inline static logic_state_t pull_x(logic_state_t input) {
		if (input == logic_state_t::FLOATING) {
			return logic_state_t::UNDEFINED;
		}
		return input;
	}
	inline static logic_state_t tristate(logic_state_t data, logic_state_t control) {
		if (control == logic_state_t::HIGH) {
			return buffer(data);
		}
		if (control == logic_state_t::LOW) {
			return logic_state_t::FLOATING;
		}
		return logic_state_t::UNDEFINED;
	}
};

enum class InstructionType : unsigned int {
	COPY,
	BUFFER,
	NOT,
	AND,
	NAND,
	OR,
	NOR,
	XOR,
	XNOR,
	SET_L,
	SET_H,
	SET_Z,
	SET_X,
	JUNCTION_PULL_H,
	JUNCTION_PULL_L,
	JUNCTION_PULL_X,
	JUNCTION_PULL_Z,
	TRISTATE,
};

class RunnableGateGroup {
public:
	RunnableGateGroup() = default;
	RunnableGateGroup(
		const LinkedGateGroup& linkedGateGroup,
		gate_group_id_t groupId,
		LogicGroupRunner& logicGroupRunner
	);
	logic_state_t getState(unsigned int pullIndex) const {
		return dataField[publishedStateDataFieldIndices[pullIndex]];
	}
	logic_state_t getStaticState(EvalConnectionPoint connectionPoint) const;
	void setState(const EvalConnectionPoint& connectionPoint, logic_state_t state);
	bool isEmpty() const { return empty; }
	void runPull(const LogicGroupRunner& runner) const;
	void runTick();
	void calculateAllGateStates(const LogicGroupRunner& runner, std::vector<logic_state_t>& outputVector) const;
	const std::vector<logic_state_t>& getDataField() const { return dataField; }
	void setDataField(const std::vector<logic_state_t>& newDataField) { dataField = newDataField; }
private:
	bool empty = true;
	mutable std::vector<logic_state_t> dataField;
	std::vector<unsigned int> publishedStateDataFieldIndices;
	std::vector<unsigned int> pullDataBytecode;
	std::vector<unsigned int> calculateGatesBytecode;
	std::vector<unsigned int> calculateAllGateStatesBytecode;
	std::unordered_map<EvalConnectionPoint, std::pair<InstructionType, unsigned int>> getStateStaticInstructions;
	std::unordered_map<EvalConnectionPoint, unsigned int> dataFieldIndexForSetState;
	gate_group_id_t groupId;
};

class LogicGroupRunner {
	friend class EditingGuard;
	friend class ReadingGuard;
public:
	LogicGroupRunner();
	~LogicGroupRunner();

	class EditingGuard {
	public:
		EditingGuard(LogicGroupRunner& runner) : lock(runner.mainMutex) {}
		~EditingGuard() = default;

		// Movable but not copyable
		EditingGuard(EditingGuard&&) = default;
		EditingGuard& operator=(EditingGuard&&) = default;
		EditingGuard(const EditingGuard&) = delete;
		EditingGuard& operator=(const EditingGuard&) = delete;

	private:
		std::unique_lock<std::shared_mutex> lock;
	};

	class ReadingGuard {
	public:
		ReadingGuard(const LogicGroupRunner& runner) : lock(runner.mainMutex) {}
		~ReadingGuard() = default;

		// Movable but not copyable
		ReadingGuard(ReadingGuard&&) = default;
		ReadingGuard& operator=(ReadingGuard&&) = default;
		ReadingGuard(const ReadingGuard&) = delete;
		ReadingGuard& operator=(const ReadingGuard&) = delete;

	private:
		std::shared_lock<std::shared_mutex> lock;
	};

	class StateReadingGuard {
	public:
		StateReadingGuard(const LogicGroupRunner& runner) : lock(runner.statesOutputVectorMutex) {}
		~StateReadingGuard() = default;

		StateReadingGuard(StateReadingGuard&&) = default;
		StateReadingGuard& operator=(StateReadingGuard&&) = default;
		StateReadingGuard(const StateReadingGuard&) = delete;
		StateReadingGuard& operator=(const StateReadingGuard&) = delete;
	private:
		std::shared_lock<std::shared_mutex> lock;
	};

	EditingGuard getEditingGuard() { return EditingGuard(*this); }
	ReadingGuard getReadingGuard() const { return ReadingGuard(*this); }
	StateReadingGuard getStateReadingGuard() const { return StateReadingGuard(*this); }

	logic_state_t getState(simulator_state_reference simulatorStateIndex) const;
	logic_state_t getStaticState(EvalConnectionPoint connectionPoint) const;
	void setState(simulator_state_reference simulatorStateIndex, logic_state_t state);
	simulator_state_reference getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const;
	simulator_state_reference getSimulatorStateIndex_mut(EvalConnectionPoint evalConnectionPoint);

	const RunnableGateGroup& getGroup(gate_group_id_t groupId) const {
		return runnableGroups[groupId.get()];
	}

	void requestNewStatesOutputVector() const {
		updateStatesOutputVectorNextUpdate.store(true, std::memory_order_release);
	}

	void setGroups(const std::unordered_map<gate_group_id_t, LinkedGateGroup>& simGroups, const std::unordered_set<eval_gate_id>& deletedGates);

	void setRunning(bool running);
	void setRealistic(bool realistic);
	void setUseTickrateLimiter(bool useTickrateLimiter);
	void setTargetTickrate(double tickrate);
	void addSprint(unsigned int nTicks);
	void waitForSprintComplete();

	bool isRunning() const;
	bool isRealistic() const;
	bool getUseTickrateLimiter() const;
	double getTargetTickrate() const;
	double getAverageTickrate() const;
	unsigned int getSprintCount() const;

	bool stepBack();
	bool stepForward();
	bool skipBack();
	bool skipForward();
	bool isViewingReplay() const;

	void tick(bool recordReplay = true);
	void simulationLoop();
	void updateEmaTickrate(
		const std::chrono::steady_clock::time_point& currentTime,
		std::chrono::steady_clock::time_point& lastTickTime,
		bool& isFirstTick
	);
	void setMaxReplayKeyframes(unsigned int n) { maxReplayKeyframes.store(n, std::memory_order_release); }
	unsigned int getMaxReplayKeyframes() const { return maxReplayKeyframes.load(std::memory_order_acquire); }

private:
	void preserveStates(std::unordered_map<EvalConnectionPoint, logic_state_t>& statesToPreserve, const std::vector<gate_group_id_t>& groupIdsToPreserve, const std::unordered_set<eval_gate_id>& deletedGates);
	bool setGroup(gate_group_id_t groupId, const LinkedGateGroup& simGroup);

	void calculateAllGateStates();
	void setState_noCalculate(simulator_state_reference simulatorStateIndex, logic_state_t state);

	struct GroupJobInstruction {
		LogicGroupRunner* runner;
		size_t groupIndex;
	};
	static void execRunPull(void* jobInstruction);
	static void execRunTick(void* jobInstruction);
	void rebuildTickJobs();
	void runThreadPoolJobs(const std::vector<std::vector<ThreadPool::Job>>& jobs);
	void updateThreadCount(size_t threadCount);
	static size_t getDefaultMaxThreadCount();

	void resetReplay();
	void saveReplayKeyframe();
	enum class ReplayEventType {
		SetState,
	};
	struct ReplayEvent {
		unsigned long long tickIndex;
		ReplayEventType type;
	};
	void recordReplayEvent(ReplayEventType type);
	void trimReplayEvents(unsigned long long earliestSavedTickIndex);
	std::optional<unsigned long long> previousReplayEventTick(unsigned long long tickIndex, ReplayEventType type) const;
	std::optional<unsigned long long> nextReplayEventTick(unsigned long long tickIndex, ReplayEventType type) const;
	std::optional<unsigned int> whichReplayKeyframe(unsigned long long tickIndex) const;
	void normalizeReplayState();
	void replayTickIndex(unsigned long long targetTickIndex);
	void replayTickIndex(unsigned int keyframeIndex, unsigned long long targetTickIndex);

	mutable std::shared_mutex mainMutex;
	std::vector<LinkedGateGroup> groupsCache;
	std::vector<RunnableGateGroup> runnableGroups;
	std::unordered_map<eval_gate_id, gate_group_id_t> gateIdToGroupId;
	std::unordered_map<simulator_state_reference, EvalConnectionPoint> simulatorStateIndexToConnectionPoint;
	std::unordered_map<EvalConnectionPoint, simulator_state_reference> connectionPointToSimulatorStateIndex;

	LinearIdProvider<simulator_state_reference> stateIndexProvider { 4 };
	mutable std::vector<std::uint8_t> groupsPulled;

	std::thread simulationThread;
	std::atomic<bool> simulationThreadRunning { true };
	std::atomic<bool> running { false };
	std::atomic<bool> realistic { false };
	std::atomic<bool> useTickrateLimiter { true };
	std::atomic<double> targetTickrate { 0.0 };
	std::atomic<double> averageTickrate { 0.0 };
	std::atomic<unsigned int> sprintCounter { 0 };
	std::atomic<unsigned long long> simulationTickIndex { 0 };

	std::atomic<unsigned long long> viewingTickIndex { 0 };
	std::atomic<bool> viewingReplay { false };

	double tickrateHalflife { 0.3 };
	mutable std::mutex controlMutex;
	std::condition_variable controlCv;

	mutable std::atomic<bool> updateStatesOutputVectorNextUpdate { false };

	ThreadPool threadPool;
	std::vector<std::vector<ThreadPool::Job>> pullJobs;
	std::vector<std::vector<ThreadPool::Job>> tickJobs;
	std::vector<std::unique_ptr<GroupJobInstruction>> groupJobInstructionStorage;

	mutable std::shared_mutex statesOutputVectorMutex;
	mutable std::shared_mutex replayKeyframesMutex;
	mutable std::shared_mutex replayEventsMutex;

	std::vector<logic_state_t> statesOutputVector;

	struct ReplayKeyframe {
		unsigned long long tickIndex;
		std::vector<std::vector<logic_state_t>> groupsDataFields;
	};

	std::deque<ReplayKeyframe> replayKeyframes;
	std::deque<ReplayEvent> replayEvents;
	std::atomic<unsigned int> maxReplayKeyframes = 4096;
};

#endif /* logicGroupRunner_h */
