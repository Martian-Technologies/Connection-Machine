#ifndef logicGroupRunner_h
#define logicGroupRunner_h

#include "simulatorDefs.h"
#include "../evalDefs.h"
#include "logicState.h"

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
	RunnableGateGroup(const LinkedGateGroup& linkedGateGroup, gate_group_id_t groupId);
	logic_state_t getState(unsigned int pullIndex) const {
		return dataField[publishedStateDataFieldIndices[pullIndex]];
	}
	logic_state_t getState(const LogicGroupRunner& runner, EvalConnectionPoint connectionPoint) const;
	logic_state_t getStaticState(EvalConnectionPoint connectionPoint) const;
	void setState(const EvalConnectionPoint& connectionPoint, logic_state_t state);
	bool isEmpty() const { return empty; }
	void runPull(const LogicGroupRunner& runner) const;
	void runTick();
private:
	bool empty = true;
	mutable std::vector<logic_state_t> dataField;
	std::vector<unsigned int> publishedStateDataFieldIndices;
	std::vector<unsigned int> pullDataBytecode;
	std::vector<unsigned int> calculateGatesBytecode;
	std::unordered_map<EvalConnectionPoint, std::vector<unsigned int>> fetchInstructionsForConnectionPoint;
	std::unordered_map<EvalConnectionPoint, unsigned int> dataFieldIndexForSetState;
	gate_group_id_t groupId;
};

class LogicGroupRunner {
	friend class EditingGuard;
	friend class ReadingGuard;
public:
	LogicGroupRunner() {}

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

	EditingGuard getEditingGuard() { return EditingGuard(*this); }
	ReadingGuard getReadingGuard() const { return ReadingGuard(*this); }

	logic_state_t getState(simulator_state_reference simulatorStateIndex) const;
	logic_state_t getStaticState(EvalConnectionPoint connectionPoint) const;
	void setState(simulator_state_reference simulatorStateIndex, logic_state_t state);
	simulator_state_reference getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const;

	void setGroups(const std::unordered_map<gate_group_id_t, LinkedGateGroup>& simGroups, const std::unordered_set<eval_gate_id>& deletedGates);
	void preserveStates(const std::unordered_map<gate_group_id_t, LinkedGateGroup>& simGroups, std::unordered_map<EvalConnectionPoint, logic_state_t>& statesToPreserve, const std::unordered_set<eval_gate_id>& deletedGates);
	void setGroup(gate_group_id_t groupId, const LinkedGateGroup& simGroup);

	const RunnableGateGroup& getGroup(gate_group_id_t groupId) const {
		return runnableGroups[groupId.get()];
	}

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

	bool stepBack() const;
	bool stepForward() const;
	bool skipBack() const;
	bool skipForward() const;
	bool isViewingReplay() const;

	void tick();

private:
	mutable std::shared_mutex mainMutex;
	std::vector<LinkedGateGroup> groupsCache;
	std::vector<RunnableGateGroup> runnableGroups;
	std::unordered_map<eval_gate_id, gate_group_id_t> gateIdToGroupId;
	std::unordered_map<simulator_state_reference, EvalConnectionPoint> simulatorStateIndexToConnectionPoint;
	std::unordered_map<EvalConnectionPoint, simulator_state_reference> connectionPointToSimulatorStateIndex;

	simulator_state_reference getSimulatorStateIndex_mut(EvalConnectionPoint evalConnectionPoint);
	LinearIdProvider<simulator_state_reference> stateIndexProvider { 4 };
	mutable std::vector<std::uint8_t> groupsPulled;
	mutable bool groupsPulledValid = false;
};

#endif /* logicGroupRunner_h */
