#ifndef simulatorGates_h
#define simulatorGates_h

#include "logicState.h"
#include "backend/evaluator/evalDefs.h"
#include "backend/container/block/connectionEnd.h"

class SimulatorGate {
public:
	virtual ~SimulatorGate() = default;

	SimulatorGate(simulator_id_t id) : id(id) {}

	virtual void addInput(simulator_id_t inputId, connection_end_id_t portId) = 0;
	virtual void removeInput(simulator_id_t inputId, connection_end_id_t portId) = 0;
	virtual void removeIdRefs(simulator_id_t otherId) = 0;
	virtual simulator_id_t getIdOfOutputPort(connection_end_id_t portId) const = 0;
	virtual void resetState(bool realistic, IdVector<simulator_id_t, logic_state_t>& states) = 0;
	virtual std::vector<simulator_id_t> getOutputSimIds() const = 0;

	simulator_id_t getId() const { return id; }

protected:
	simulator_id_t id;

	inline void applyRealisticTick(logic_state_t targetState, const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		logic_state_t currentState = statesA[id];
		if (currentState == logic_state_t::UNDEFINED) {
			statesB[id] = targetState;
		} else if (targetState != currentState) {
			statesB[id] = logic_state_t::UNDEFINED;
		} else {
			statesB[id] = currentState;
		}
	}
};

class LogicGate : public SimulatorGate {
public:
	LogicGate(simulator_id_t id) : SimulatorGate(id) {}

	void resetState(bool realistic, IdVector<simulator_id_t, logic_state_t>& states) override {
		if (realistic) {
			states[id] = logic_state_t::UNDEFINED;
		} else {
			states[id] = logic_state_t::LOW;
		}
	}

	simulator_id_t getIdOfOutputPort(connection_end_id_t portId) const override {
		return id;
	}

	std::vector<simulator_id_t> getOutputSimIds() const override {
		return {id};
	}
};

class MultiInputGate : public LogicGate {
public:
	MultiInputGate(simulator_id_t id) : LogicGate(id) {}

	void addInput(simulator_id_t inputId, connection_end_id_t portId) override {
		inputs.push_back(inputId);
	}

	void removeInput(simulator_id_t inputId, connection_end_id_t portId) override {
		for (size_t i = 0; i < inputs.size(); ++i) {
			if (inputs[i] == inputId) {
				inputs[i] = inputs.back();
				inputs.pop_back();
				break;
			}
		}
	}

	void removeIdRefs(simulator_id_t otherId) override {
		for (size_t i = 0; i < inputs.size(); ++i) {
			if (inputs[i] == otherId) {
				inputs[i] = inputs.back();
				inputs.pop_back();
				--i;
			}
		}
	}

protected:
	std::vector<simulator_id_t> inputs;
};

class SingleInputGate : public LogicGate {
public:
	SingleInputGate(simulator_id_t id) : LogicGate(id) {}

	void addInput(simulator_id_t inputId, connection_end_id_t portId) override {
		if (input.has_value()) {
			logError("SingleInputGate already has an input", "SingleInputGate::addInput");
			return;
		}
		input = inputId;
	}

	void removeInput(simulator_id_t inputId, connection_end_id_t portId) override {
		if (input == inputId) {
			input.reset();
		} else {
			logError("SingleInputGate does not have the specified input", "SingleInputGate::removeInput");
		}
	}

	void removeIdRefs(simulator_id_t otherId) override {
		if (input.has_value() && input.value() == otherId) {
			input.reset();
		}
	}

protected:
	std::optional<simulator_id_t> input;
};

struct ANDLikeGate : public MultiInputGate {
	// By default, behaves like an AND gate
	// Can be used for AND, OR, NAND, and NOR
	bool inputsInverted;
	bool outputInverted;
	unsigned int lastInputToEarlyExit;

	ANDLikeGate(simulator_id_t id, bool inputsInverted = false, bool outputInverted = false)
		: MultiInputGate(id), inputsInverted(inputsInverted), outputInverted(outputInverted), lastInputToEarlyExit(0) {}

	inline logic_state_t calculate(const IdVector<simulator_id_t, logic_state_t>& statesA) noexcept {
		if (inputs.empty()) [[unlikely]] {
			return logic_state_t::LOW;
		}
		const logic_state_t desiredState = (logic_state_t)inputsInverted;
		if (statesA[inputs[lastInputToEarlyExit]] == desiredState) {
			return (logic_state_t)outputInverted;
		}
		bool foundGoofyState = false;
		for (unsigned int i = 0; i < inputs.size(); ++i) {
			const logic_state_t state = statesA[inputs[i]];
			// Early-out if the decisive (desired) state is present
			if (state == desiredState) {
				lastInputToEarlyExit = i; // remember the input that caused early exit so we can check it first next time
				return (logic_state_t)outputInverted;
			}
			// Track any unknown/floating driver; only matters if no decisive state was found
			foundGoofyState |= (state >= logic_state_t::FLOATING); // FLOATING or UNDEFINED
		}
		return foundGoofyState ? logic_state_t::UNDEFINED : (logic_state_t)!outputInverted;
	}

	inline void tick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	inline void realisticTick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		logic_state_t targetState = calculate(statesA);
		applyRealisticTick(targetState, statesA, statesB);
	}

	void removeInput(simulator_id_t inputId, connection_end_id_t portId) override {
		lastInputToEarlyExit = 0;
		MultiInputGate::removeInput(inputId, portId);
	}

	void removeIdRefs(simulator_id_t otherId) override {
		lastInputToEarlyExit = 0;
		MultiInputGate::removeIdRefs(otherId);
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		stateJson["inputsInverted"] = inputsInverted;
		stateJson["outputInverted"] = outputInverted;
		stateJson["lastInputToEarlyExit"] = lastInputToEarlyExit;
		stateJson["inputs"] = nlohmann::json::array();
		for (const simulator_id_t inputId : inputs) {
			stateJson["inputs"].push_back(inputId.get());
		}
		return stateJson;
	}
};

struct XORLikeGate : public MultiInputGate {
	// Behaves like an XOR gate
	// Can be used for XOR and XNOR
	bool outputInverted;

	XORLikeGate(simulator_id_t id, bool outputInverted = false)
		: MultiInputGate(id), outputInverted(outputInverted) {}

	inline logic_state_t calculate(const IdVector<simulator_id_t, logic_state_t>& statesA) const noexcept {
		if (inputs.empty()) {
			return logic_state_t::LOW;
		}
		// parity == true means current result would be HIGH
		bool parity = outputInverted;
		for (const simulator_id_t inputId : inputs) {
			const logic_state_t state = statesA[inputId];
			if (state >= logic_state_t::FLOATING) { // FLOATING or UNDEFINED
				return logic_state_t::UNDEFINED;
			}
			parity = parity ^ (state == logic_state_t::HIGH);
		}
		return (logic_state_t)parity;
	}

	inline void tick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	inline void realisticTick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		logic_state_t targetState = calculate(statesA);
		applyRealisticTick(targetState, statesA, statesB);
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		stateJson["outputInverted"] = outputInverted;
		stateJson["inputs"] = nlohmann::json::array();
		for (const simulator_id_t inputId : inputs) {
			stateJson["inputs"].push_back(inputId.get());
		}
		return stateJson;
	}
};

struct JunctionGate : public SimulatorGate {
	std::vector<simulator_id_t> inputs;
	logic_state_t defaultState;

	JunctionGate(simulator_id_t id, logic_state_t defaultState) : SimulatorGate(id), defaultState(defaultState) {}

	inline logic_state_t calculate(const IdVector<simulator_id_t, logic_state_t>& states) const noexcept {
		logic_state_t outputState = logic_state_t::FLOATING;
		for (const simulator_id_t inputId : inputs) {
			const logic_state_t state = states[inputId];
			if (state == logic_state_t::FLOATING) {
				continue;
			}
			if (state == logic_state_t::UNDEFINED) {
				return logic_state_t::UNDEFINED;
			}
			if (outputState == logic_state_t::FLOATING) {
				outputState = state;
			} else if (outputState != state) {
				return logic_state_t::UNDEFINED;
			}
		}
		if (outputState == logic_state_t::FLOATING) {
			return defaultState;
		}
		return outputState;
	}

	inline void tick(IdVector<simulator_id_t, logic_state_t>& states) noexcept {
		states[id] = calculate(states);
	}

	inline void doubleTick(IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		logic_state_t state = calculate(statesB);
		statesA[id] = state;
		statesB[id] = state;
	}

	void addInput(simulator_id_t inputId, connection_end_id_t portId) override {
		inputs.push_back(inputId);
	}

	void removeInput(simulator_id_t inputId, connection_end_id_t portId) override {
		auto it = std::find(inputs.begin(), inputs.end(), inputId);
		if (it != inputs.end()) {
			inputs.erase(it);
		}
	}

	void removeIdRefs(simulator_id_t otherId) override {
		inputs.erase(std::remove(inputs.begin(), inputs.end(), otherId), inputs.end());
	}

	void resetState(bool realistic, IdVector<simulator_id_t, logic_state_t>& states) override {
		states[id] = logic_state_t::FLOATING;
	}

	simulator_id_t getIdOfOutputPort(connection_end_id_t portId) const override {
		return id;
	}

	std::vector<simulator_id_t> getOutputSimIds() const override {
		return {id};
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		stateJson["defaultState"] = logicstate_to_string(defaultState);
		stateJson["inputs"] = nlohmann::json::array();
		for (const simulator_id_t inputId : inputs) {
			stateJson["inputs"].push_back(inputId.get());
		}
		return stateJson;
	}
};


class BufferGateBase : public SingleInputGate {
public:
	bool outputInverted;

	BufferGateBase(simulator_id_t id, bool outputInverted = false)
		: SingleInputGate(id), outputInverted(outputInverted) {}
};

struct BufferGate : public BufferGateBase {
	unsigned int extraDelayTicks;

	BufferGate(simulator_id_t id, bool outputInverted = false, unsigned int extraDelayTicks = 0)
		: BufferGateBase(id, outputInverted), extraDelayTicks(extraDelayTicks) {}

	inline void tick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		stateJson["outputInverted"] = outputInverted;
		stateJson["extraDelayTicks"] = extraDelayTicks;
		if (input.has_value()) {
			stateJson["input"] = input.value().get();
		} else {
			stateJson["input"] = nullptr;
		}
		return stateJson;
	}
};

struct SingleBufferGate : public BufferGateBase {
	SingleBufferGate(simulator_id_t id, bool outputInverted = false)
		: BufferGateBase(id, outputInverted) {}

	inline logic_state_t calculate(const IdVector<simulator_id_t, logic_state_t>& statesA) const noexcept {
		if (!input.has_value()) [[unlikely]] {
			return logic_state_t::UNDEFINED;
		}
		logic_state_t state = statesA[input.value()];
		if (state >= logic_state_t::FLOATING) { // FLOATING or UNDEFINED
			return logic_state_t::UNDEFINED;
		}
		return outputInverted ? (state == logic_state_t::HIGH ? logic_state_t::LOW : logic_state_t::HIGH) : state;
	}

	inline void tick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	inline void realisticTick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		logic_state_t targetState = calculate(statesA);
		applyRealisticTick(targetState, statesA, statesB);
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		stateJson["outputInverted"] = outputInverted;
		if (input.has_value()) {
			stateJson["input"] = input.value().get();
		} else {
			stateJson["input"] = nullptr;
		}
		return stateJson;
	}
};

struct TristateBufferGate : public SimulatorGate {
	bool enableInverted;
	simulator_id_t dataInput;
	simulator_id_t enableInput;

	TristateBufferGate(simulator_id_t id, bool enableInverted = false)
		: SimulatorGate(id), enableInverted(enableInverted) {}

	void addInput(simulator_id_t inputId, connection_end_id_t portId) override {
		if (portId == connection_end_id_t(0)) {
			dataInput = inputId;
		} else {
			enableInput = inputId;
		}
	}

	void removeInput(simulator_id_t inputId, connection_end_id_t portId) override {
		if (portId == connection_end_id_t(0)) {
			dataInput = simulator_id_t(0);
		} else {
			enableInput = simulator_id_t(0);
		}
	}

	void removeIdRefs(simulator_id_t otherId) override {
		if (dataInput == otherId) {
			dataInput = simulator_id_t(0);
		}
		if (enableInput == otherId) {
			enableInput = simulator_id_t(0);
		}
	}

	void resetState(bool realistic, IdVector<simulator_id_t, logic_state_t>& states) override {
		if (realistic) {
			states[id] = logic_state_t::UNDEFINED;
		} else {
			states[id] = logic_state_t::FLOATING;
		}
	}

	inline logic_state_t calculate(const IdVector<simulator_id_t, logic_state_t>& statesA) const noexcept {
		logic_state_t enableState = statesA[enableInput];
		if (enableState >= logic_state_t::FLOATING) { // FLOATING or UNDEFINED
			return logic_state_t::UNDEFINED;
		}
		if (enableState == (logic_state_t)enableInverted) {
			return logic_state_t::FLOATING;
		}
		// Enabled
		logic_state_t state = statesA[dataInput];
		if (state == logic_state_t::FLOATING) {
			return logic_state_t::UNDEFINED;
		}
		return state;
	}

	inline void tick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	inline void realisticTick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		logic_state_t targetState = calculate(statesA);
		applyRealisticTick(targetState, statesA, statesB);
	}

	simulator_id_t getIdOfOutputPort(connection_end_id_t portId) const override {
		return id;
	}

	std::vector<simulator_id_t> getOutputSimIds() const override {
		return {id};
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		stateJson["enableInverted"] = enableInverted;
		stateJson["dataInput"] = dataInput.get();
		stateJson["enableInput"] = enableInput.get();
		return stateJson;
	}
};

class ConstantGateBase : public SimulatorGate {
public:
	logic_state_t outputState;

	ConstantGateBase(simulator_id_t id, logic_state_t outputState)
		: SimulatorGate(id), outputState(outputState) {}

	void addInput(simulator_id_t inputId, connection_end_id_t portId) override {}

	void removeInput(simulator_id_t inputId, connection_end_id_t portId) override {}

	void resetState(bool realistic, IdVector<simulator_id_t, logic_state_t>& states) override {
		states[id] = outputState;
	}

	void removeIdRefs(simulator_id_t otherId) override {}

	simulator_id_t getIdOfOutputPort(connection_end_id_t portId) const override {
		return id;
	}

	std::vector<simulator_id_t> getOutputSimIds() const override {
		return {id};
	}
};

struct ConstantGate : public ConstantGateBase {
	ConstantGate(simulator_id_t id, logic_state_t outputState = logic_state_t::LOW)
		: ConstantGateBase(id, outputState) {}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		stateJson["outputState"] = logicstate_to_string(outputState);
		return stateJson;
	}
};

struct ConstantResetGate : public ConstantGateBase {
	ConstantResetGate(simulator_id_t id, logic_state_t outputState = logic_state_t::LOW)
		: ConstantGateBase(id, outputState) {}

	inline logic_state_t calculate() const noexcept {
		return outputState;
	}

	inline void tick(IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		statesB[id] = calculate();
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		stateJson["outputState"] = logicstate_to_string(outputState);
		return stateJson;
	}
};

struct CopySelfOutputGate : public LogicGate {
	CopySelfOutputGate(simulator_id_t id) : LogicGate(id) {}

	void addInput(simulator_id_t inputId, connection_end_id_t portId) override {}

	void removeInput(simulator_id_t inputId, connection_end_id_t portId) override {}

	void removeIdRefs(simulator_id_t otherId) override {}

	inline logic_state_t calculate(const IdVector<simulator_id_t, logic_state_t>& statesA) const noexcept {
		return statesA[id];
	}

	inline void tick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	simulator_id_t getIdOfOutputPort(connection_end_id_t portId) const override {
		return id;
	}

	void resetState(bool realistic, IdVector<simulator_id_t, logic_state_t>& states) override {
		states[id] = logic_state_t::LOW;
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		return stateJson;
	}
};

struct PortsToIntGate : public SimulatorGate {
	IdVector<connection_end_id_t, simulator_id_t> inputPorts;

	PortsToIntGate(simulator_id_t id, int numInputs) : SimulatorGate(id) {
		inputPorts.resize(connection_end_id_t(numInputs), simulator_id_t(0));
	}

	void addInput(simulator_id_t inputId, connection_end_id_t portId) override {
		if (portId >= inputPorts.size()) {
			logError("Port ID out of range in PortsToIntGate::addInput", "PortsToIntGate::addInput");
			return;
		}
		inputPorts[portId] = inputId;
	}

	void removeInput(simulator_id_t inputId, connection_end_id_t portId) override {
		if (portId >= inputPorts.size()) {
			logError("Port ID out of range in PortsToIntGate::removeInput", "PortsToIntGate::removeInput");
			return;
		}
		if (inputPorts[portId] == inputId) {
			inputPorts[portId] = simulator_id_t(0);
		}
	}

	void removeIdRefs(simulator_id_t otherId) override {
		for (simulator_id_t& inputId : inputPorts) {
			if (inputId == otherId) {
				inputId = simulator_id_t(0);
			}
		}
	}

	void resetState(bool realistic, IdVector<simulator_id_t, logic_state_t>& states) override {
		states[id] = static_cast<logic_state_t>(0);
	}

	inline logic_state_t calculate(const IdVector<simulator_id_t, logic_state_t>& statesA) const noexcept {
		unsigned int result = 0;
		for (connection_end_id_t i : inputPorts.ids()) {
			simulator_id_t inputId = inputPorts[i];
			if (inputId == simulator_id_t(0)) {
				continue;
			}
			logic_state_t state = statesA[inputId];
			if (state == logic_state_t::HIGH) {
				result |= (1 << i.get());
			}
		}
		return static_cast<logic_state_t>(result);
	}

	inline void tick(const IdVector<simulator_id_t, logic_state_t>& statesA, IdVector<simulator_id_t, logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	simulator_id_t getIdOfOutputPort(connection_end_id_t portId) const override {
		return id;
	}
	std::vector<simulator_id_t> getOutputSimIds() const override {
		return {id};
	}

	nlohmann::json dumpState() const {
		nlohmann::json stateJson = nlohmann::json::object();
		stateJson["id"] = id.get();
		stateJson["inputPorts"] = nlohmann::json::array();
		for (connection_end_id_t i : inputPorts.ids()) {
			stateJson["inputPorts"].push_back(inputPorts[i].get());
		}
		return stateJson;
	}
};

#endif /* simulatorGates_h */
