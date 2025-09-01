#ifndef simulatorGates_h
#define simulatorGates_h

#include "evalTypedef.h"
#include "logicState.h"
#include "idProvider.h"

class SimulatorGate {
public:
	virtual ~SimulatorGate() = default;

	SimulatorGate(simulator_id_t id) : id(id) {}

	virtual void addInput(simulator_id_t inputId, connection_port_id_t portId) = 0;
	virtual void removeInput(simulator_id_t inputId, connection_port_id_t portId) = 0;
	virtual void removeIdRefs(simulator_id_t otherId) = 0;
	virtual simulator_id_t getIdOfOutputPort(connection_port_id_t portId) const = 0;
	virtual void resetState(bool realistic, std::vector<logic_state_t>& states) = 0;
	virtual std::vector<simulator_id_t> getOutputSimIds() const = 0;

	simulator_id_t getId() const { return id; }

protected:
	simulator_id_t id;

	inline void applyRealisticTick(logic_state_t targetState, const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
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

	void resetState(bool realistic, std::vector<logic_state_t>& states) override {
		if (realistic) {
			states[id] = logic_state_t::UNDEFINED;
		} else {
			states[id] = logic_state_t::LOW;
		}
	}

	simulator_id_t getIdOfOutputPort(connection_port_id_t portId) const override {
		return id;
	}

	std::vector<simulator_id_t> getOutputSimIds() const override {
		return {id};
	}
};

class MultiInputGate : public LogicGate {
public:
	MultiInputGate(simulator_id_t id) : LogicGate(id) {}

	void addInput(simulator_id_t inputId, connection_port_id_t portId) override {
		inputs.push_back(inputId);
	}

	void removeInput(simulator_id_t inputId, connection_port_id_t portId) override {
		auto it = std::find(inputs.begin(), inputs.end(), inputId);
		if (it != inputs.end()) {
			inputs.erase(it);
		}
	}

	void removeIdRefs(simulator_id_t otherId) override {
		inputs.erase(std::remove(inputs.begin(), inputs.end(), otherId), inputs.end());
	}

protected:
	std::vector<simulator_id_t> inputs;
};

class SingleInputGate : public LogicGate {
public:
	SingleInputGate(simulator_id_t id) : LogicGate(id) {}

	void addInput(simulator_id_t inputId, connection_port_id_t portId) override {
		if (input.has_value()) {
			logError("SingleInputGate already has an input", "SingleInputGate::addInput");
			return;
		}
		input = inputId;
	}

	void removeInput(simulator_id_t inputId, connection_port_id_t portId) override {
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

	ANDLikeGate(simulator_id_t id, bool inputsInverted = false, bool outputInverted = false)
		: MultiInputGate(id), inputsInverted(inputsInverted), outputInverted(outputInverted) {}

	inline logic_state_t calculate(const std::vector<logic_state_t>& statesA) const noexcept {
		if (inputs.empty()) [[unlikely]] {
			return logic_state_t::LOW;
		}
		bool foundGoofyState = false;
		const logic_state_t desiredState = (logic_state_t)inputsInverted;
		for (const simulator_id_t inputId : inputs) {
			const logic_state_t state = statesA[inputId];
			// Early-out if the decisive (desired) state is present
			if (state == desiredState) {
				return (logic_state_t)outputInverted;
			}
			// Track any unknown/floating driver; only matters if no decisive state was found
			foundGoofyState |= (state >= logic_state_t::UNDEFINED); // FLOATING or UNDEFINED
		}
		return foundGoofyState ? logic_state_t::UNDEFINED : (logic_state_t)!outputInverted;
	}

	inline void tick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	inline void realisticTick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		logic_state_t targetState = calculate(statesA);
		applyRealisticTick(targetState, statesA, statesB);
	}
};

struct XORLikeGate : public MultiInputGate {
	// Behaves like an XOR gate
	// Can be used for XOR and XNOR
	bool outputInverted;

	XORLikeGate(simulator_id_t id, bool outputInverted = false)
		: MultiInputGate(id), outputInverted(outputInverted) {}

	inline logic_state_t calculate(const std::vector<logic_state_t>& statesA) const noexcept {
		if (inputs.empty()) {
			return logic_state_t::LOW;
		}
		// parity == true means current result would be HIGH
		bool parity = outputInverted;
		for (const simulator_id_t inputId : inputs) {
			const logic_state_t state = statesA[inputId];
			if (state >= logic_state_t::UNDEFINED) { // FLOATING or UNDEFINED
				return logic_state_t::UNDEFINED;
			}
			parity = parity ^ (state == logic_state_t::HIGH);
		}
		return (logic_state_t)parity;
	}

	inline void tick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	inline void realisticTick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		logic_state_t targetState = calculate(statesA);
		applyRealisticTick(targetState, statesA, statesB);
	}
};

struct JunctionGate : public SimulatorGate {
	std::vector<simulator_id_t> inputs;

	JunctionGate(simulator_id_t id) : SimulatorGate(id) {}

	inline logic_state_t calculate(const std::vector<logic_state_t>& states) const noexcept {
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
		return outputState;
	}

	inline void tick(std::vector<logic_state_t>& states) noexcept {
		states[id] = calculate(states);
	}

	inline void doubleTick(std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		logic_state_t state = calculate(statesB);
		statesA[id] = state;
		statesB[id] = state;
	}

	void addInput(simulator_id_t inputId, connection_port_id_t portId) override {
		inputs.push_back(inputId);
	}

	void removeInput(simulator_id_t inputId, connection_port_id_t portId) override {
		auto it = std::find(inputs.begin(), inputs.end(), inputId);
		if (it != inputs.end()) {
			inputs.erase(it);
		}
	}

	void removeIdRefs(simulator_id_t otherId) override {
		inputs.erase(std::remove(inputs.begin(), inputs.end(), otherId), inputs.end());
	}

	void resetState(bool realistic, std::vector<logic_state_t>& states) override {
		states[id] = logic_state_t::FLOATING;
	}

	simulator_id_t getIdOfOutputPort(connection_port_id_t portId) const override {
		return id;
	}

	std::vector<simulator_id_t> getOutputSimIds() const override {
		return {id};
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

	inline void tick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {}
};

struct SingleBufferGate : public BufferGateBase {
	SingleBufferGate(simulator_id_t id, bool outputInverted = false)
		: BufferGateBase(id, outputInverted) {}

	inline logic_state_t calculate(const std::vector<logic_state_t>& statesA) const noexcept {
		return input.has_value() ? statesA[input.value()] : logic_state_t::LOW;
	}

	inline void tick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	inline void realisticTick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		logic_state_t targetState = calculate(statesA);
		applyRealisticTick(targetState, statesA, statesB);
	}
};

struct TristateBufferGate : public SimulatorGate {
	bool enableInverted;
	std::vector<simulator_id_t> inputs;
	std::vector<simulator_id_t> enableInputs;

	TristateBufferGate(simulator_id_t id, bool enableInverted = false)
		: SimulatorGate(id), enableInverted(enableInverted) {}

	void addInput(simulator_id_t inputId, connection_port_id_t portId) override {
		if (portId == 0) {
			inputs.push_back(inputId);
		} else {
			enableInputs.push_back(inputId);
		}
	}

	void removeInput(simulator_id_t inputId, connection_port_id_t portId) override {
		if (portId == 0) {
			auto it = std::find(inputs.begin(), inputs.end(), inputId);
			if (it != inputs.end()) {
				inputs.erase(it);
			}
		} else {
			auto it = std::find(enableInputs.begin(), enableInputs.end(), inputId);
			if (it != enableInputs.end()) {
				enableInputs.erase(it);
			}
		}
	}

	void removeIdRefs(simulator_id_t otherId) override {
		inputs.erase(std::remove(inputs.begin(), inputs.end(), otherId), inputs.end());
		enableInputs.erase(std::remove(enableInputs.begin(), enableInputs.end(), otherId), enableInputs.end());
	}

	void resetState(bool realistic, std::vector<logic_state_t>& states) override {
		if (realistic) {
			states[id] = logic_state_t::UNDEFINED;
		} else {
			states[id] = logic_state_t::FLOATING;
		}
	}

	inline logic_state_t calculate(const std::vector<logic_state_t>& statesA) const noexcept {
		bool foundGoofyState = false;
		bool foundEnabled = false;
		bool foundDisabled = false;
		for (const simulator_id_t enableId : enableInputs) {
			logic_state_t enableState = statesA[enableId];
			if (enableState == logic_state_t::UNDEFINED) {
				foundGoofyState = true;
				break;
			}
			if (enableState == logic_state_t::HIGH) {
				foundEnabled = true;
			} else if (enableState == logic_state_t::LOW) {
				foundDisabled = true;
			}
			if (foundEnabled && foundDisabled) {
				break;
			}
		}
		if (foundEnabled == foundDisabled) {
			foundGoofyState = true;
		}
		if (foundGoofyState) {
			return logic_state_t::UNDEFINED;
		}
		if (foundEnabled == enableInverted) {
			return logic_state_t::FLOATING;
		}
		if (inputs.empty()) {
			return logic_state_t::UNDEFINED;
		}
		logic_state_t outputState = logic_state_t::FLOATING;
		for (const simulator_id_t inputId : inputs) {
			logic_state_t state = statesA[inputId];
			if (state == logic_state_t::UNDEFINED) {
				foundGoofyState = true;
				break;
			}
			if (state == logic_state_t::FLOATING) {
				continue;
			}
			if (outputState == logic_state_t::FLOATING) {
				outputState = state;
			} else if (outputState != state) {
				outputState = logic_state_t::UNDEFINED;
				break;
			}
		}
		if (foundGoofyState) {
			return logic_state_t::UNDEFINED;
		} else {
			return outputState;
		}
	}

	inline void tick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	inline void realisticTick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		logic_state_t targetState = calculate(statesA);
		applyRealisticTick(targetState, statesA, statesB);
	}

	simulator_id_t getIdOfOutputPort(connection_port_id_t portId) const override {
		return id;
	}

	std::vector<simulator_id_t> getOutputSimIds() const override {
		return {id};
	}
};

class ConstantGateBase : public SimulatorGate {
public:
	logic_state_t outputState;

	ConstantGateBase(simulator_id_t id, logic_state_t outputState)
		: SimulatorGate(id), outputState(outputState) {}

	void addInput(simulator_id_t inputId, connection_port_id_t portId) override {}

	void removeInput(simulator_id_t inputId, connection_port_id_t portId) override {}

	void resetState(bool realistic, std::vector<logic_state_t>& states) override {
		states[id] = outputState;
	}

	void removeIdRefs(simulator_id_t otherId) override {}

	simulator_id_t getIdOfOutputPort(connection_port_id_t portId) const override {
		return id;
	}

	std::vector<simulator_id_t> getOutputSimIds() const override {
		return {id};
	}
};

struct ConstantGate : public ConstantGateBase {
	ConstantGate(simulator_id_t id, logic_state_t outputState = logic_state_t::LOW)
		: ConstantGateBase(id, outputState) {}
};

struct ConstantResetGate : public ConstantGateBase {
	ConstantResetGate(simulator_id_t id, logic_state_t outputState = logic_state_t::LOW)
		: ConstantGateBase(id, outputState) {}

	inline logic_state_t calculate() const noexcept {
		return outputState;
	}

	inline void tick(std::vector<logic_state_t>& statesB) noexcept {
		statesB[id] = calculate();
	}
};

struct CopySelfOutputGate : public LogicGate {
	CopySelfOutputGate(simulator_id_t id) : LogicGate(id) {}

	void addInput(simulator_id_t inputId, connection_port_id_t portId) override {}

	void removeInput(simulator_id_t inputId, connection_port_id_t portId) override {}

	void removeIdRefs(simulator_id_t otherId) override {}

	inline logic_state_t calculate(const std::vector<logic_state_t>& statesA) const noexcept {
		return statesA[id];
	}

	inline void tick(const std::vector<logic_state_t>& statesA, std::vector<logic_state_t>& statesB) noexcept {
		statesB[id] = calculate(statesA);
	}

	simulator_id_t getIdOfOutputPort(connection_port_id_t portId) const override {
		return id;
	}

	void resetState(bool realistic, std::vector<logic_state_t>& states) override {
		states[id] = logic_state_t::LOW;
	}
};

#endif /* simulatorGates_h */
