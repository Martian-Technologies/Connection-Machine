#include "logicSimulator.h"
#include "gateType.h"

LogicSimulator::LogicSimulator(
	EvalConfig& evalConfig,
	std::vector<simulator_id_t>& dirtySimulatorIds) :
	evalConfig(evalConfig),
	dirtySimulatorIds(dirtySimulatorIds) {
	// Subscribe to EvalConfig changes to update the simulator accordingly
	evalConfig.subscribe([this]() {
		// Notify the simulation thread about config changes
		std::lock_guard<std::mutex> lk(cvMutex);
		cv.notify_all();
	});

	simulationThread = std::thread(&LogicSimulator::simulationLoop, this);
	statesA.resize(1, logic_state_t::UNDEFINED);
	statesB.resize(1, logic_state_t::UNDEFINED);
	simulatorIdProvider.getNewId(); // reserve the 0th id to be used as an invalid id
}

LogicSimulator::~LogicSimulator() {
	{
		std::lock_guard<std::mutex> lk(cvMutex);
		running = false;
		cv.notify_all();
	}
	if (simulationThread.joinable()) {
		simulationThread.join();
	}
}

void LogicSimulator::clearState() { }

double LogicSimulator::getAverageTickrate() const {
	return averageTickrate.load(std::memory_order_acquire);
}

void LogicSimulator::simulationLoop() {
	using clock = std::chrono::steady_clock;
	auto nextTick = clock::now();
	auto lastTickTime = clock::now();
	bool isFirstTick = true;

	while (running) {
		if (pauseRequest.load(std::memory_order_acquire)) {
			averageTickrate.store(0.0, std::memory_order_release);
			std::unique_lock<std::mutex> lk(cvMutex);
			isPaused.store(true, std::memory_order_release);
			cv.notify_all();
			cv.wait(lk, [&] { return !pauseRequest || !running; });
			isPaused.store(false, std::memory_order_release);
			if (!running) break;
			// reset nextTick after resuming from pause
			nextTick = clock::now();
			lastTickTime = clock::now();
			isFirstTick = true;
		}

		processPendingStateChanges();
		if (evalConfig.isRunning()) {
			auto currentTime = clock::now();

			if (evalConfig.isRealistic()) {
				realisticTickOnce();
			} else {
				tickOnce();
			}

			// Calculate EMA for tickrate after each tick
			if (!isFirstTick) {
				auto deltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - lastTickTime);
				if (deltaTime.count() > 0) {
					// Calculate current tickrate in Hz (ticks per second)
					double currentTickrate = 1.0e9 / static_cast<double>(deltaTime.count());
					double dtSeconds = std::chrono::duration<double>(deltaTime).count();
					double alpha = 1.0 - std::exp(-dtSeconds * std::log(2.0) / tickrateHalflife);

					// Apply EMA: EMA_new = alpha * current + (1 - alpha) * EMA_old
					double currentEMA = averageTickrate.load(std::memory_order_acquire);
					double newEMA = alpha * currentTickrate + (1.0 - alpha) * currentEMA;
					averageTickrate.store(newEMA, std::memory_order_release);
				}
			} else {
				isFirstTick = false;
			}
			lastTickTime = currentTime;

			// handle timing after ticking
			if (evalConfig.isTickrateLimiterEnabled()) {
				long long targetTickrate = evalConfig.getTargetTickrate();
				if (targetTickrate > 0) {
					auto period = std::chrono::round<std::chrono::nanoseconds>(std::chrono::minutes { 1 }) / targetTickrate;
					nextTick += period;
					std::unique_lock lk(cvMutex);
					cv.wait_until(lk, nextTick, [&] { return pauseRequest || !running || !evalConfig.isRunning(); });
				}
			}
		} else {
			// wait for state change or resume signal
			averageTickrate.store(0.0, std::memory_order_release);
			std::unique_lock lk(cvMutex);
			cv.wait(lk, [&] {
				// Check for pending state changes under lock to avoid race conditions
				std::lock_guard<std::mutex> stateLock(stateChangeQueueMutex);
				return pauseRequest || !running || evalConfig.isRunning() || !pendingStateChanges.empty();
			});
			// reset nextTick when resuming simulation
			nextTick = clock::now();
			lastTickTime = clock::now();
			isFirstTick = true;
		}
	}
}

inline void LogicSimulator::tickOnce() {
	std::unique_lock lkNext(statesBMutex);
	for (auto& gate : andGates) gate.tick(statesA, statesB);
	for (auto& gate : xorGates) gate.tick(statesA, statesB);
	for (auto& gate : constantResetGates) gate.tick(statesB);
	for (auto& gate : copySelfOutputGates) gate.tick(statesA, statesB);
	for (auto& gate : tristateBuffers) gate.tick(statesA, statesB);

	for (auto& gate : junctions) gate.tick(statesB);
	std::unique_lock lkCurEx(statesAMutex);
	std::swap(statesA, statesB);
}

inline void LogicSimulator::realisticTickOnce() {
	std::unique_lock lkNext(statesBMutex);
	for (auto& gate : andGates) gate.realisticTick(statesA, statesB);
	for (auto& gate : xorGates) gate.realisticTick(statesA, statesB);
	for (auto& gate : constantResetGates) gate.tick(statesB);
	for (auto& gate : copySelfOutputGates) gate.tick(statesA, statesB);
	for (auto& gate : tristateBuffers) gate.realisticTick(statesA, statesB);

	for (auto& gate : junctions) gate.tick(statesB);
	std::unique_lock lkCurEx(statesAMutex);
	std::swap(statesA, statesB);
}

void LogicSimulator::processPendingStateChanges() {
	// Process pending state changes without blocking the calling thread
	std::queue<StateChange> localQueue;
	{
		std::lock_guard<std::mutex> lock(stateChangeQueueMutex);
		std::swap(localQueue, pendingStateChanges);
	}

	// Apply all pending state changes
	if (!localQueue.empty()) {
		std::scoped_lock lk(statesBMutex, statesAMutex);
		while (!localQueue.empty()) {
			const StateChange& change = localQueue.front();

			// Ensure the states vectors are large enough
			if (statesA.size() <= change.id) {
				statesA.resize(change.id + 1, logic_state_t::UNDEFINED);
				statesB.resize(change.id + 1, logic_state_t::UNDEFINED);
			}

			statesA[change.id] = change.state;
			statesB[change.id] = change.state;
			localQueue.pop();
		}
		for (auto& gate : junctions) gate.doubleTick(statesA, statesB);
	}
}

void LogicSimulator::setState(simulator_id_t id, logic_state_t st) {
	// Try to acquire locks non-blockingly first
	std::unique_lock lkB(statesBMutex, std::try_to_lock);
	std::unique_lock lkA(statesAMutex, std::try_to_lock);

	if (lkB.owns_lock() && lkA.owns_lock()) {
		// Successfully acquired both locks, apply immediately
		if (statesA.size() <= id) {
			statesA.resize(id + 1, logic_state_t::UNDEFINED);
			statesB.resize(id + 1, logic_state_t::UNDEFINED);
		}
		statesA[id] = st;
		statesB[id] = st;
		for (auto& gate : junctions) gate.doubleTick(statesA, statesB);
	} else {
		// Couldn't acquire locks, fall back to queuing
		std::lock_guard<std::mutex> lock(stateChangeQueueMutex);
		pendingStateChanges.push({ id, st });
		// Wake up the simulation thread to process state changes
		cv.notify_one();
	}
}

void LogicSimulator::setStates(const std::vector<simulator_id_t>& ids, const std::vector<logic_state_t>& states) {
	if (ids.size() != states.size()) {
		throw std::invalid_argument("ids and states must have the same size");
	}

	// Try to acquire locks non-blockingly first
	std::unique_lock lkB(statesBMutex, std::try_to_lock);
	std::unique_lock lkA(statesAMutex, std::try_to_lock);

	if (lkB.owns_lock() && lkA.owns_lock()) {
		// Successfully acquired both locks, apply immediately
		for (size_t i = 0; i < ids.size(); ++i) {
			// Ensure the states vectors are large enough
			if (statesA.size() <= ids[i]) {
				statesA.resize(ids[i] + 1, logic_state_t::UNDEFINED);
				statesB.resize(ids[i] + 1, logic_state_t::UNDEFINED);
			}
			statesA[ids[i]] = states[i];
			statesB[ids[i]] = states[i];
		}
		for (auto& gate : junctions) gate.doubleTick(statesA, statesB);
	} else {
		// Couldn't acquire locks, fall back to queuing
		std::lock_guard<std::mutex> lock(stateChangeQueueMutex);
		for (size_t i = 0; i < ids.size(); ++i) {
			pendingStateChanges.push({ ids[i], states[i] });
		}
		// Wake up the simulation thread to process state changes
		cv.notify_one();
	}
}

void LogicSimulator::setStateImmediate(simulator_id_t id, logic_state_t st) {
	std::scoped_lock lk(statesBMutex, statesAMutex);
	// Ensure the states vectors are large enough
	if (statesA.size() <= id) {
		statesA.resize(id + 1, logic_state_t::UNDEFINED);
		statesB.resize(id + 1, logic_state_t::UNDEFINED);
	}
	statesA[id] = st;
	statesB[id] = st;
}

void LogicSimulator::setStatesImmediate(const std::vector<simulator_id_t>& ids, const std::vector<logic_state_t>& states) {
	if (ids.size() != states.size()) {
		throw std::invalid_argument("ids and states must have the same size");
	}
	std::scoped_lock lk(statesBMutex, statesAMutex);
	for (size_t i = 0; i < ids.size(); ++i) {
		// Ensure the states vectors are large enough
		if (statesA.size() <= ids[i]) {
			statesA.resize(ids[i] + 1, logic_state_t::UNDEFINED);
			statesB.resize(ids[i] + 1, logic_state_t::UNDEFINED);
		}
		statesA[ids[i]] = states[i];
		statesB[ids[i]] = states[i];
	}
}

logic_state_t LogicSimulator::getState(simulator_id_t id) const {
	std::shared_lock lk(statesAMutex);
	return statesA[id];
}

std::vector<logic_state_t> LogicSimulator::getStates(const std::vector<simulator_id_t>& ids) const {
	std::vector<logic_state_t> result(ids.size());
	std::shared_lock lk(statesAMutex);
	for (size_t i = 0; i < ids.size(); ++i) {
		const size_t id = ids[i];
		if (id < statesA.size()) {
			result[i] = statesA[id];
		} else {
			result[i] = logic_state_t::UNDEFINED;
		}
	}
	return result;
}

simulator_id_t LogicSimulator::addGate(const GateType gateType) {
	simulator_id_t simulatorId = simulatorIdProvider.getNewId();

	// extend the states if necessary
	if (statesA.size() <= simulatorId) {
		statesA.resize(simulatorId + 1, logic_state_t::UNDEFINED);
		statesB.resize(simulatorId + 1, logic_state_t::UNDEFINED);
	}

	switch (gateType) {
	case GateType::AND:
		andGates.emplace_back( simulatorId, evaluatorLocalityVectorGenerator.getVector(andGates.size()), false, false );
		updateGateLocation(simulatorId, SimGateType::AND, andGates.size() - 1);
		andGates.back().resetState(evalConfig.isRealistic(), statesA);
		andGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::OR:
		andGates.emplace_back( simulatorId, evaluatorLocalityVectorGenerator.getVector(andGates.size()), true, true );
		updateGateLocation(simulatorId, SimGateType::AND, andGates.size() - 1);
		andGates.back().resetState(evalConfig.isRealistic(), statesA);
		andGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::NAND:
		andGates.emplace_back( simulatorId, evaluatorLocalityVectorGenerator.getVector(andGates.size()), false, true );
		updateGateLocation(simulatorId, SimGateType::AND, andGates.size() - 1);
		andGates.back().resetState(evalConfig.isRealistic(), statesA);
		andGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::NOR:
		andGates.emplace_back( simulatorId, evaluatorLocalityVectorGenerator.getVector(andGates.size()), true, false );
		updateGateLocation(simulatorId, SimGateType::AND, andGates.size() - 1);
		andGates.back().resetState(evalConfig.isRealistic(), statesA);
		andGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::XOR:
		xorGates.emplace_back( simulatorId, evaluatorLocalityVectorGenerator.getVector(xorGates.size()), false );
		updateGateLocation(simulatorId, SimGateType::XOR, xorGates.size() - 1);
		xorGates.back().resetState(evalConfig.isRealistic(), statesA);
		xorGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::XNOR:
		xorGates.emplace_back( simulatorId, evaluatorLocalityVectorGenerator.getVector(xorGates.size()), true );
		updateGateLocation(simulatorId, SimGateType::XOR, xorGates.size() - 1);
		xorGates.back().resetState(evalConfig.isRealistic(), statesA);
		xorGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::JUNCTION:
		junctions.push_back({ simulatorId });
		updateGateLocation(simulatorId, SimGateType::JUNCTION, junctions.size() - 1);
		junctions.back().resetState(evalConfig.isRealistic(), statesA);
		junctions.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::TRISTATE_BUFFER:
		tristateBuffers.push_back({ simulatorId, false });
		updateGateLocation(simulatorId, SimGateType::TRISTATE_BUFFER, tristateBuffers.size() - 1);
		tristateBuffers.back().resetState(evalConfig.isRealistic(), statesA);
		tristateBuffers.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::TRISTATE_BUFFER_INVERTED:
		tristateBuffers.push_back({ simulatorId, true });
		updateGateLocation(simulatorId, SimGateType::TRISTATE_BUFFER, tristateBuffers.size() - 1);
		tristateBuffers.back().resetState(evalConfig.isRealistic(), statesA);
		tristateBuffers.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::CONSTANT_OFF:
		constantGates.push_back({ simulatorId, logic_state_t::LOW });
		updateGateLocation(simulatorId, SimGateType::CONSTANT, constantGates.size() - 1);
		constantGates.back().resetState(evalConfig.isRealistic(), statesA);
		constantGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::CONSTANT_ON:
		constantGates.push_back({ simulatorId, logic_state_t::HIGH });
		updateGateLocation(simulatorId, SimGateType::CONSTANT, constantGates.size() - 1);
		constantGates.back().resetState(evalConfig.isRealistic(), statesA);
		constantGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::DUMMY_INPUT:
		copySelfOutputGates.push_back({ simulatorId });
		updateGateLocation(simulatorId, SimGateType::COPY_SELF_OUTPUT, copySelfOutputGates.size() - 1);
		copySelfOutputGates.back().resetState(evalConfig.isRealistic(), statesA);
		copySelfOutputGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::THROUGH:
		singleBuffers.push_back({ simulatorId, false });
		updateGateLocation(simulatorId, SimGateType::SINGLE_BUFFER, singleBuffers.size() - 1);
		singleBuffers.back().resetState(evalConfig.isRealistic(), statesA);
		singleBuffers.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::TICK_INPUT:
		constantResetGates.push_back({ simulatorId, logic_state_t::LOW });
		updateGateLocation(simulatorId, SimGateType::CONSTANT_RESET, constantResetGates.size() - 1);
		constantResetGates.back().resetState(evalConfig.isRealistic(), statesA);
		constantResetGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::NONE:
		logError("Cannot add gate of type NONE", "LogicSimulator::addGate");
		break;
	}
	return simulatorId;
}

void LogicSimulator::removeGate(simulator_id_t simulatorId) {
	auto locationIt = gateLocations.find(simulatorId);
	if (locationIt == gateLocations.end()) {
		logError("Cannot remove gate: not found " + std::to_string(simulatorId), "LogicSimulator::removeGate");
		return;
	}

	// Fetch outputs directly via location mapping (O(1))
	std::optional<std::vector<simulator_id_t>> outputIdsOpt = getOutputSimIdsFromGate(simulatorId);
	if (!outputIdsOpt.has_value()) {
		logError("Cannot remove gate: no output IDs found for simulator_id_t " + std::to_string(simulatorId), "LogicSimulator::removeGate");
		return;
	}
	const auto& outputIds = outputIdsOpt.value();

	// Remove references to this gate's outputs from dependent gates and purge dependency entries
	for (const auto& outId : outputIds) {
		auto depIt = outputDependencies.find(outId);
		if (depIt != outputDependencies.end()) {
			for (const auto& dependency : depIt->second) {
				auto depLocIt = gateLocations.find(dependency.gateId);
				if (depLocIt == gateLocations.end()) continue;

				const auto depType = depLocIt->second.gateType;
				const auto depIdx = depLocIt->second.gateIndex;
				switch (depType) {
				case SimGateType::AND:             if (depIdx < andGates.size())             andGates[depIdx].removeIdRefs(outId); break;
				case SimGateType::XOR:             if (depIdx < xorGates.size())             xorGates[depIdx].removeIdRefs(outId); break;
				case SimGateType::JUNCTION:        if (depIdx < junctions.size())            junctions[depIdx].removeIdRefs(outId); break;
				case SimGateType::BUFFER:          if (depIdx < buffers.size())              buffers[depIdx].removeIdRefs(outId); break;
				case SimGateType::SINGLE_BUFFER:   if (depIdx < singleBuffers.size())        singleBuffers[depIdx].removeIdRefs(outId); break;
				case SimGateType::TRISTATE_BUFFER: if (depIdx < tristateBuffers.size())      tristateBuffers[depIdx].removeIdRefs(outId); break;
				case SimGateType::CONSTANT:        if (depIdx < constantGates.size())        constantGates[depIdx].removeIdRefs(outId); break;
				case SimGateType::CONSTANT_RESET:  if (depIdx < constantResetGates.size())   constantResetGates[depIdx].removeIdRefs(outId); break;
				case SimGateType::COPY_SELF_OUTPUT:if (depIdx < copySelfOutputGates.size())  copySelfOutputGates[depIdx].removeIdRefs(outId); break;
				}
			}
			outputDependencies.erase(depIt);
		}
		simulatorIdProvider.releaseId(outId);
		dirtySimulatorIds.push_back(outId);
	}

	// Now erase the gate itself using swap-and-pop; update only the moved gate's index
	SimGateType gateType = locationIt->second.gateType;
	size_t gateIndex = locationIt->second.gateIndex;

	auto fixMovedIndex = [&](auto& vec) {
		const size_t last = vec.size() - 1;
		if (gateIndex != last) {
			vec[gateIndex] = std::move(vec[last]);
			simulator_id_t movedId = vec[gateIndex].getId();
			gateLocations[movedId].gateIndex = gateIndex;
		}
		vec.pop_back();
	};

	switch (gateType) {
	case SimGateType::AND:             if (!andGates.empty())             fixMovedIndex(andGates); break;
	case SimGateType::XOR:             if (!xorGates.empty())             fixMovedIndex(xorGates); break;
	case SimGateType::JUNCTION:        if (!junctions.empty())            fixMovedIndex(junctions); break;
	case SimGateType::BUFFER:          if (!buffers.empty())              fixMovedIndex(buffers); break;
	case SimGateType::SINGLE_BUFFER:   if (!singleBuffers.empty())        fixMovedIndex(singleBuffers); break;
	case SimGateType::TRISTATE_BUFFER: if (!tristateBuffers.empty())      fixMovedIndex(tristateBuffers); break;
	case SimGateType::CONSTANT:        if (!constantGates.empty())        fixMovedIndex(constantGates); break;
	case SimGateType::CONSTANT_RESET:  if (!constantResetGates.empty())   fixMovedIndex(constantResetGates); break;
	case SimGateType::COPY_SELF_OUTPUT:if (!copySelfOutputGates.empty())  fixMovedIndex(copySelfOutputGates); break;
	}

	removeGateLocation(simulatorId);
}

void LogicSimulator::makeConnection(simulator_id_t sourceId, connection_port_id_t sourcePort, simulator_id_t destinationId, connection_port_id_t destinationPort) {
	std::optional<simulator_id_t> actualSourceId = getOutputPortId(sourceId, sourcePort);

	if (!actualSourceId.has_value()) {
		logError("Cannot resolve actual source ID for connection", "LogicSimulator::makeConnection");
		return;
	}

	addInputToGate(destinationId, actualSourceId.value(), destinationPort);
}

void LogicSimulator::removeConnection(simulator_id_t sourceId, connection_port_id_t sourcePort, simulator_id_t destinationId, connection_port_id_t destinationPort) {
	std::optional<simulator_id_t> actualSourceId = getOutputPortId(sourceId, sourcePort);
	if (!actualSourceId.has_value()) {
		logError("Cannot resolve actual source ID for disconnection", "LogicSimulator::removeConnection");
		return;
	}
	removeInputFromGate(destinationId, actualSourceId.value(), destinationPort);
}

void LogicSimulator::endEdit() {
	for (auto& gate : junctions) gate.doubleTick(statesA, statesB);
}

std::optional<simulator_id_t> LogicSimulator::getOutputPortId(simulator_id_t simId, connection_port_id_t portId) const {
	auto locationIt = gateLocations.find(simId);
	if (locationIt != gateLocations.end()) {
		SimGateType gateType = locationIt->second.gateType;
		size_t gateIndex = locationIt->second.gateIndex;

		switch (gateType) {
		case SimGateType::AND:
			if (gateIndex < andGates.size()) {
				return andGates[gateIndex].getIdOfOutputPort(portId);
			}
			break;
		case SimGateType::XOR:
			if (gateIndex < xorGates.size()) {
				return xorGates[gateIndex].getIdOfOutputPort(portId);
			}
			break;
		case SimGateType::JUNCTION:
			if (gateIndex < junctions.size()) {
				return junctions[gateIndex].getIdOfOutputPort(portId);
			}
			break;
		case SimGateType::BUFFER:
			if (gateIndex < buffers.size()) {
				return buffers[gateIndex].getIdOfOutputPort(portId);
			}
			break;
		case SimGateType::SINGLE_BUFFER:
			if (gateIndex < singleBuffers.size()) {
				return singleBuffers[gateIndex].getIdOfOutputPort(portId);
			}
			break;
		case SimGateType::TRISTATE_BUFFER:
			if (gateIndex < tristateBuffers.size()) {
				return tristateBuffers[gateIndex].getIdOfOutputPort(portId);
			}
			break;
		case SimGateType::CONSTANT:
			if (gateIndex < constantGates.size()) {
				return constantGates[gateIndex].getIdOfOutputPort(portId);
			}
			break;
		case SimGateType::CONSTANT_RESET:
			if (gateIndex < constantResetGates.size()) {
				return constantResetGates[gateIndex].getIdOfOutputPort(portId);
			}
			break;
		case SimGateType::COPY_SELF_OUTPUT:
			if (gateIndex < copySelfOutputGates.size()) {
				return copySelfOutputGates[gateIndex].getIdOfOutputPort(portId);
			}
			break;
		}
	}

	return std::nullopt;
}

void LogicSimulator::addInputToGate(simulator_id_t simId, simulator_id_t inputId, connection_port_id_t portId) {
	dirtySimulatorIds.push_back(inputId);
	auto locationIt = gateLocations.find(simId);
	if (locationIt != gateLocations.end()) {
		SimGateType gateType = locationIt->second.gateType;
		size_t gateIndex = locationIt->second.gateIndex;

		switch (gateType) {
		case SimGateType::AND:
			if (gateIndex < andGates.size()) {
				andGates[gateIndex].addInput(inputId, portId);
				addOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::XOR:
			if (gateIndex < xorGates.size()) {
				xorGates[gateIndex].addInput(inputId, portId);
				addOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::JUNCTION:
			if (gateIndex < junctions.size()) {
				junctions[gateIndex].addInput(inputId, portId);
				addOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::BUFFER:
			if (gateIndex < buffers.size()) {
				buffers[gateIndex].addInput(inputId, portId);
				addOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::SINGLE_BUFFER:
			if (gateIndex < singleBuffers.size()) {
				singleBuffers[gateIndex].addInput(inputId, portId);
				addOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::TRISTATE_BUFFER:
			if (gateIndex < tristateBuffers.size()) {
				tristateBuffers[gateIndex].addInput(inputId, portId);
				addOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::CONSTANT:
			if (gateIndex < constantGates.size()) {
				constantGates[gateIndex].addInput(inputId, portId);
				addOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::CONSTANT_RESET:
			if (gateIndex < constantResetGates.size()) {
				constantResetGates[gateIndex].addInput(inputId, portId);
				addOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::COPY_SELF_OUTPUT:
			if (gateIndex < copySelfOutputGates.size()) {
				copySelfOutputGates[gateIndex].addInput(inputId, portId);
				addOutputDependency(inputId, simId);
			}
			break;
		}
		return;
	}

	logError("Gate not found for addInputToGate", "LogicSimulator::addInputToGate");
}

void LogicSimulator::removeInputFromGate(simulator_id_t simId, simulator_id_t inputId, connection_port_id_t portId) {
	dirtySimulatorIds.push_back(inputId);
	auto locationIt = gateLocations.find(simId);
	if (locationIt != gateLocations.end()) {
		SimGateType gateType = locationIt->second.gateType;
		size_t gateIndex = locationIt->second.gateIndex;

		switch (gateType) {
		case SimGateType::AND:
			if (gateIndex < andGates.size()) {
				andGates[gateIndex].removeInput(inputId, portId);
				removeOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::XOR:
			if (gateIndex < xorGates.size()) {
				xorGates[gateIndex].removeInput(inputId, portId);
				removeOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::JUNCTION:
			if (gateIndex < junctions.size()) {
				junctions[gateIndex].removeInput(inputId, portId);
				removeOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::BUFFER:
			if (gateIndex < buffers.size()) {
				buffers[gateIndex].removeInput(inputId, portId);
				removeOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::SINGLE_BUFFER:
			if (gateIndex < singleBuffers.size()) {
				singleBuffers[gateIndex].removeInput(inputId, portId);
				removeOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::TRISTATE_BUFFER:
			if (gateIndex < tristateBuffers.size()) {
				tristateBuffers[gateIndex].removeInput(inputId, portId);
				removeOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::CONSTANT:
			if (gateIndex < constantGates.size()) {
				constantGates[gateIndex].removeInput(inputId, portId);
				removeOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::CONSTANT_RESET:
			if (gateIndex < constantResetGates.size()) {
				constantResetGates[gateIndex].removeInput(inputId, portId);
				removeOutputDependency(inputId, simId);
			}
			break;
		case SimGateType::COPY_SELF_OUTPUT:
			if (gateIndex < copySelfOutputGates.size()) {
				copySelfOutputGates[gateIndex].removeInput(inputId, portId);
				removeOutputDependency(inputId, simId);
			}
			break;
		}
		return;
	}

	logError("Gate not found for removeInputFromGate", "LogicSimulator::removeInputFromGate");
}

std::optional<std::vector<simulator_id_t>> LogicSimulator::getOutputSimIdsFromGate(simulator_id_t simId) const {
	auto locationIt = gateLocations.find(simId);
	if (locationIt == gateLocations.end()) return std::nullopt;

	SimGateType gateType = locationIt->second.gateType;
	size_t gateIndex = locationIt->second.gateIndex;

	switch (gateType) {
	case SimGateType::AND:
		if (gateIndex < andGates.size()) return andGates[gateIndex].getOutputSimIds();
		break;
	case SimGateType::XOR:
		if (gateIndex < xorGates.size()) return xorGates[gateIndex].getOutputSimIds();
		break;
	case SimGateType::JUNCTION:
		if (gateIndex < junctions.size()) return junctions[gateIndex].getOutputSimIds();
		break;
	case SimGateType::BUFFER:
		if (gateIndex < buffers.size()) return buffers[gateIndex].getOutputSimIds();
		break;
	case SimGateType::SINGLE_BUFFER:
		if (gateIndex < singleBuffers.size()) return singleBuffers[gateIndex].getOutputSimIds();
		break;
	case SimGateType::TRISTATE_BUFFER:
		if (gateIndex < tristateBuffers.size()) return tristateBuffers[gateIndex].getOutputSimIds();
		break;
	case SimGateType::CONSTANT:
		if (gateIndex < constantGates.size()) return constantGates[gateIndex].getOutputSimIds();
		break;
	case SimGateType::CONSTANT_RESET:
		if (gateIndex < constantResetGates.size()) return constantResetGates[gateIndex].getOutputSimIds();
		break;
	case SimGateType::COPY_SELF_OUTPUT:
		if (gateIndex < copySelfOutputGates.size()) return copySelfOutputGates[gateIndex].getOutputSimIds();
		break;
	}
	return std::nullopt;
}

void LogicSimulator::updateGateLocation(simulator_id_t gateId, SimGateType gateType, size_t gateIndex) {
	gateLocations[gateId] = GateLocation(gateType, gateIndex);
}

void LogicSimulator::removeGateLocation(simulator_id_t gateId) {
	gateLocations.erase(gateId);
}

void LogicSimulator::addOutputDependency(simulator_id_t outputId, simulator_id_t dependentGateId) {
	outputDependencies[outputId].emplace_back(dependentGateId);
}

void LogicSimulator::removeOutputDependency(simulator_id_t outputId, simulator_id_t dependentGateId) {
	auto it = outputDependencies.find(outputId);
	if (it != outputDependencies.end()) {
		auto& deps = it->second;
		deps.erase(std::remove(deps.begin(), deps.end(), GateDependency(dependentGateId)), deps.end());
		if (deps.empty()) {
			outputDependencies.erase(it);
		}
	}
}
