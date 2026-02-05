#ifndef logicGroupRunner_h
#define logicGroupRunner_h

#include "../evalDefs.h"
#include "logicState.h"

class CompiledGateGroup;
class gate_group_id_t;

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

	logic_state_t getState(simulator_state_index_t simulatorStateIndex) const;
	void setState(simulator_state_index_t simulatorStateIndex, logic_state_t state);

	void moveStates(const std::unordered_map<simulator_state_index_t, simulator_state_index_t>& remapping);
	void setSimGroups(const std::unordered_map<gate_group_id_t, CompiledGateGroup>& simGroups);

private:
	mutable std::shared_mutex mainMutex;

};

#endif /* logicGroupRunner_h */