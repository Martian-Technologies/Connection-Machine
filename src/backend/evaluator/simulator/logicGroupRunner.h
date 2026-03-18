#ifndef logicGroupRunner_h
#define logicGroupRunner_h

#include "simulatorDefs.h"
#include "../evalDefs.h"
#include "logicState.h"

class LinkedGateGroup;
class LogicGroupRunner;

class RunnableGateGroup {
public:
	RunnableGateGroup() = default;
	RunnableGateGroup(const LinkedGateGroup& linkedGateGroup, gate_group_id_t groupId);
	logic_state_t getState(unsigned int pullIndex) const {
		return dataField[publishedStateDataFieldIndices[pullIndex]];
	}
	logic_state_t getState(EvalConnectionPoint connectionPoint) const;
	bool isEmpty() const { return empty; }
	void runPull(const LogicGroupRunner& runner) const;
	void runTick() const;
private:
	bool empty = true;
	mutable std::vector<logic_state_t> dataField;
	std::vector<unsigned int> publishedStateDataFieldIndices;
	std::vector<unsigned int> pullDataBytecode;
	std::vector<unsigned int> calculateGatesBytecode;
	std::unordered_map<EvalConnectionPoint, std::vector<unsigned int>> fetchInstructionsForConnectionPoint;
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
	void setState(simulator_state_reference simulatorStateIndex, logic_state_t state);
	simulator_state_reference getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const;

	void setGroups(const std::unordered_map<gate_group_id_t, LinkedGateGroup>& simGroups);
	void setGroup(gate_group_id_t groupId, const LinkedGateGroup& simGroup);

private:
	mutable std::shared_mutex mainMutex;
	std::vector<LinkedGateGroup> groupsCache;
	std::vector<RunnableGateGroup> runnableGroups;
	std::unordered_map<eval_gate_id, gate_group_id_t> gateIdToGroupId;
	std::unordered_map<simulator_state_reference, EvalConnectionPoint> simulatorStateIndexToConnectionPoint;
	std::unordered_map<EvalConnectionPoint, simulator_state_reference> connectionPointToSimulatorStateIndex;

	simulator_state_reference getSimulatorStateIndex_mut(EvalConnectionPoint evalConnectionPoint);
	LinearIdProvider<simulator_state_reference> stateIndexProvider;
};

#endif /* logicGroupRunner_h */