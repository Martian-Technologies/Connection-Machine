#include "logicSimulator.h"
#include "gateType.h"

LogicSimulator::LogicSimulator(
	EvalConfig& evalConfig,
	std::vector<simulator_id_t>& dirtySimulatorIds) :
	evalConfig(evalConfig),
	dirtySimulatorIds(dirtySimulatorIds) {
	evalConfig.subscribe([this]() {
		{
			SimPauseGuard pauseGuard(*this);
			this->regenerateJobs();
		}
		bool shouldSprint = (this->evalConfig.isRunning() && !this->evalConfig.isTickrateLimiterEnabled());
		this->threadPool.setSprinting(shouldSprint);

		std::lock_guard<std::mutex> lk(cvMutex);
		cv.notify_all();
	});

	simulationThread = std::thread(&LogicSimulator::simulationLoop, this);
	extendDataVectors(simulatorIdProvider.getNewId()); // reserve the 0th id to be used as an invalid id
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
	if (!evalConfig.isRunning()) {
		return false;
	}
	double tickspeed = averageTickrate.load(std::memory_order_acquire);
	// if tickspeed close enough to target tickspeed, return target tickspeed
	double targetTickrate = evalConfig.getTargetTickrate();
	double percentageError = (tickspeed - targetTickrate) / targetTickrate;
	if (std::abs(percentageError) < 0.01) {
		return targetTickrate;
	}
	return tickspeed;
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
			nextTick = clock::now();
			lastTickTime = clock::now();
			isFirstTick = true;
		}

		processPendingStateChanges();

		bool didSprint = false;
		while (running && !pauseRequest.load(std::memory_order_acquire) && evalConfig.consumeSprintTick()) {
			didSprint = true;
			auto currentTime = clock::now();
			tickOnce();
			updateEmaTickrate(currentTime, lastTickTime, isFirstTick);
			if (pauseRequest.load(std::memory_order_acquire)) break;
		}

		if (!didSprint && evalConfig.isRunning()) {
			auto currentTime = clock::now();

			tickOnce();

			updateEmaTickrate(currentTime, lastTickTime, isFirstTick);

			if (evalConfig.isTickrateLimiterEnabled()) {
				double targetTickrate = evalConfig.getTargetTickrate();
				if (targetTickrate > 0) {
					auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(1.0 / targetTickrate));
					nextTick += period;
					std::unique_lock lk(cvMutex);
					cv.wait_until(lk, nextTick, [&] { return pauseRequest || !running || !evalConfig.isRunning(); });
				}
			}
		} else if (!didSprint) {
			averageTickrate.store(0.0, std::memory_order_release);
			std::unique_lock lk(cvMutex);
			cv.wait(lk, [&] {
				std::lock_guard<std::mutex> stateLock(stateChangeQueueMutex);
				return pauseRequest || !running || evalConfig.isRunning() || evalConfig.getSprintCount() > 0 || !pendingStateChanges.empty();
			});
			nextTick = clock::now();
			lastTickTime = clock::now();
			isFirstTick = true;
		}
	}
}

inline void LogicSimulator::updateEmaTickrate(
	const std::chrono::steady_clock::time_point& currentTime,
	std::chrono::steady_clock::time_point& lastTickTime,
	bool& isFirstTick) {
	if (!isFirstTick) {
		auto deltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - lastTickTime);
		if (deltaTime.count() > 0) {
			double currentTickrate = 1.0e9 / static_cast<double>(deltaTime.count());
			double dtSeconds = std::chrono::duration<double>(deltaTime).count();
			double alpha = 1.0 - std::exp(-dtSeconds * std::log(2.0) / tickrateHalflife);

			double currentEMA = averageTickrate.load(std::memory_order_acquire);
			double newEMA = alpha * currentTickrate + (1.0 - alpha) * currentEMA;
			averageTickrate.store(newEMA, std::memory_order_release);
		}
	} else {
		isFirstTick = false;
	}
	lastTickTime = currentTime;
}

inline void LogicSimulator::tickOnce() {
	std::unique_lock lkNext(statesBMutex);

	threadPool.resetAndLoad(jobs);
	threadPool.waitForCompletion();

	for (auto& gate : junctions) gate.tick(statesB);
	std::unique_lock lkCurEx(statesAMutex);
	std::swap(statesA, statesB);
}

void LogicSimulator::processPendingStateChanges() {
	std::queue<StateChange> localQueue;
	{
		std::lock_guard<std::mutex> lock(stateChangeQueueMutex);
		std::swap(localQueue, pendingStateChanges);
	}

	if (!localQueue.empty()) {
		std::scoped_lock lk(statesBMutex, statesAMutex);
		while (!localQueue.empty()) {
			const StateChange& change = localQueue.front();

			extendDataVectors(change.id);

			statesA[change.id] = change.state;
			statesB[change.id] = change.state;
			localQueue.pop();
		}
		for (auto& gate : junctions) gate.doubleTick(statesA, statesB);
	}
}

void LogicSimulator::setState(simulator_id_t id, logic_state_t st) {
	// we don't want to freeze up if the mutexes are locked, so we'll only set the state if we can successfully lock. otherwise, we'll wait until the next tick to set the states.
	std::unique_lock lkB(statesBMutex, std::try_to_lock);
	std::unique_lock lkA(statesAMutex, std::try_to_lock);

	if (lkB.owns_lock() && lkA.owns_lock()) {
		if (statesA.size() <= id) {
			statesA.resize(id + 1, logic_state_t::UNDEFINED);
			statesB.resize(id + 1, logic_state_t::UNDEFINED);
		}
		statesA[id] = st;
		statesB[id] = st;
		for (auto& gate : junctions) gate.doubleTick(statesA, statesB);
	} else {
		std::lock_guard<std::mutex> lock(stateChangeQueueMutex);
		pendingStateChanges.push({ id, st });
		cv.notify_one();
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
	simulator_id_t simulatorId;

	switch (gateType) {
	case GateType::AND:
		simulatorId = andGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(andGates.back().getId());
		extendDataVectors(simulatorId);
		andGates.push_back({ simulatorId, false, false });
		updateGateLocation(simulatorId, SimGateType::AND, andGates.size() - 1);
		andGates.back().resetState(evalConfig.isRealistic(), statesA);
		andGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::OR:
		simulatorId = andGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(andGates.back().getId());
		extendDataVectors(simulatorId);
		andGates.push_back({ simulatorId, true, true });
		updateGateLocation(simulatorId, SimGateType::AND, andGates.size() - 1);
		andGates.back().resetState(evalConfig.isRealistic(), statesA);
		andGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::NAND:
		simulatorId = andGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(andGates.back().getId());
		extendDataVectors(simulatorId);
		andGates.push_back({ simulatorId, false, true });
		updateGateLocation(simulatorId, SimGateType::AND, andGates.size() - 1);
		andGates.back().resetState(evalConfig.isRealistic(), statesA);
		andGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::NOR:
		simulatorId = andGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(andGates.back().getId());
		extendDataVectors(simulatorId);
		andGates.push_back({ simulatorId, true, false });
		updateGateLocation(simulatorId, SimGateType::AND, andGates.size() - 1);
		andGates.back().resetState(evalConfig.isRealistic(), statesA);
		andGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::XOR:
		simulatorId = xorGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(xorGates.back().getId());
		extendDataVectors(simulatorId);
		xorGates.push_back({ simulatorId, false });
		updateGateLocation(simulatorId, SimGateType::XOR, xorGates.size() - 1);
		xorGates.back().resetState(evalConfig.isRealistic(), statesA);
		xorGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::XNOR:
		simulatorId = xorGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(xorGates.back().getId());
		extendDataVectors(simulatorId);
		xorGates.push_back({ simulatorId, true });
		updateGateLocation(simulatorId, SimGateType::XOR, xorGates.size() - 1);
		xorGates.back().resetState(evalConfig.isRealistic(), statesA);
		xorGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::JUNCTION:
		simulatorId = junctions.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(junctions.back().getId());
		extendDataVectors(simulatorId);
		junctions.push_back({ simulatorId });
		updateGateLocation(simulatorId, SimGateType::JUNCTION, junctions.size() - 1);
		junctions.back().resetState(evalConfig.isRealistic(), statesA);
		junctions.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::TRISTATE_BUFFER:
		simulatorId = tristateBuffers.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(tristateBuffers.back().getId());
		extendDataVectors(simulatorId);
		tristateBuffers.push_back({ simulatorId, false });
		updateGateLocation(simulatorId, SimGateType::TRISTATE_BUFFER, tristateBuffers.size() - 1);
		tristateBuffers.back().resetState(evalConfig.isRealistic(), statesA);
		tristateBuffers.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::TRISTATE_BUFFER_INVERTED:
		simulatorId = tristateBuffers.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(tristateBuffers.back().getId());
		extendDataVectors(simulatorId);
		tristateBuffers.push_back({ simulatorId, true });
		updateGateLocation(simulatorId, SimGateType::TRISTATE_BUFFER, tristateBuffers.size() - 1);
		tristateBuffers.back().resetState(evalConfig.isRealistic(), statesA);
		tristateBuffers.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::CONSTANT_OFF:
		simulatorId = constantGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(constantGates.back().getId());
		extendDataVectors(simulatorId);
		constantGates.push_back({ simulatorId, logic_state_t::LOW });
		updateGateLocation(simulatorId, SimGateType::CONSTANT, constantGates.size() - 1);
		constantGates.back().resetState(evalConfig.isRealistic(), statesA);
		constantGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::CONSTANT_ON:
		simulatorId = constantGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(constantGates.back().getId());
		extendDataVectors(simulatorId);
		constantGates.push_back({ simulatorId, logic_state_t::HIGH });
		updateGateLocation(simulatorId, SimGateType::CONSTANT, constantGates.size() - 1);
		constantGates.back().resetState(evalConfig.isRealistic(), statesA);
		constantGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::DUMMY_INPUT:
		simulatorId = copySelfOutputGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(copySelfOutputGates.back().getId());
		extendDataVectors(simulatorId);
		copySelfOutputGates.push_back({ simulatorId });
		updateGateLocation(simulatorId, SimGateType::COPY_SELF_OUTPUT, copySelfOutputGates.size() - 1);
		copySelfOutputGates.back().resetState(evalConfig.isRealistic(), statesA);
		copySelfOutputGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::THROUGH:
		simulatorId = singleBuffers.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(singleBuffers.back().getId());
		extendDataVectors(simulatorId);
		singleBuffers.push_back({ simulatorId, false });
		updateGateLocation(simulatorId, SimGateType::SINGLE_BUFFER, singleBuffers.size() - 1);
		singleBuffers.back().resetState(evalConfig.isRealistic(), statesA);
		singleBuffers.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::TICK_INPUT:
		simulatorId = constantResetGates.size() == 0 ? simulatorIdProvider.getNewId() : simulatorIdProvider.getNewId(constantResetGates.back().getId());
		extendDataVectors(simulatorId);
		constantResetGates.push_back({ simulatorId, logic_state_t::LOW });
		updateGateLocation(simulatorId, SimGateType::CONSTANT_RESET, constantResetGates.size() - 1);
		constantResetGates.back().resetState(evalConfig.isRealistic(), statesA);
		constantResetGates.back().resetState(evalConfig.isRealistic(), statesB);
		break;
	case GateType::NONE:
		logError("Cannot add gate of type NONE", "LogicSimulator::addGate");
		return 0;
	}
	return simulatorId;
}

void LogicSimulator::removeGate(simulator_id_t simulatorId) {
	auto locationIt = gateLocations.find(simulatorId);
	if (locationIt == gateLocations.end()) {
		logError("Cannot remove gate: not found " + std::to_string(simulatorId), "LogicSimulator::removeGate");
		return;
	}

	std::optional<std::vector<simulator_id_t>> outputIdsOpt = getOutputSimIdsFromGate(simulatorId);
	if (!outputIdsOpt.has_value()) {
		logError("Cannot remove gate: no output IDs found for simulator_id_t " + std::to_string(simulatorId), "LogicSimulator::removeGate");
		return;
	}
	const auto& outputIds = outputIdsOpt.value();

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

	SimGateType gateType = locationIt->second.gateType;
	size_t gateIndex = locationIt->second.gateIndex;

	auto fixMovedIndex = [&](auto& vec) {
		const size_t last = vec.size() - 1;
		if (gateIndex != last) {
			std::swap(vec[gateIndex], vec[last]);
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
	regenerateJobs();
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

void LogicSimulator::regenerateJobs() {
	threadPool.waitForCompletion();
	jobs.clear();
	jobInstructionStorage.clear();
	bool isRealistic = evalConfig.isRealistic();

	auto makeJI = [&](size_t start, size_t end) -> JobInstruction* {
		jobInstructionStorage.emplace_back(std::make_unique<JobInstruction>(JobInstruction{ this, start, end }));
		return jobInstructionStorage.back().get();
	};

	constexpr size_t batch = 512;

	for (size_t i = 0; i < andGates.size(); i += batch) {
		JobInstruction* ji = makeJI(i, std::min(i + batch, andGates.size()));
		jobs.push_back(ThreadPool::Job{ isRealistic ? &LogicSimulator::execANDRealistic : &LogicSimulator::execAND, ji });
	}
	for (size_t i = 0; i < xorGates.size(); i += batch) {
		JobInstruction* ji = makeJI(i, std::min(i + batch, xorGates.size()));
		jobs.push_back(ThreadPool::Job{ isRealistic ? &LogicSimulator::execXORRealistic : &LogicSimulator::execXOR, ji });
	}
	for (size_t i = 0; i < tristateBuffers.size(); i += batch) {
		JobInstruction* ji = makeJI(i, std::min(i + batch, tristateBuffers.size()));
		jobs.push_back(ThreadPool::Job{ isRealistic ? &LogicSimulator::execTristateRealistic : &LogicSimulator::execTristate, ji });
	}
	for (size_t i = 0; i < constantResetGates.size(); i += batch) {
		JobInstruction* ji = makeJI(i, std::min(i + batch, constantResetGates.size()));
		jobs.push_back(ThreadPool::Job{ &LogicSimulator::execConstantReset, ji });
	}
	for (size_t i = 0; i < copySelfOutputGates.size(); i += batch) {
		JobInstruction* ji = makeJI(i, std::min(i + batch, copySelfOutputGates.size()));
		jobs.push_back(ThreadPool::Job{ &LogicSimulator::execCopySelfOutput, ji });
	}
	if (jobs.size() < std::thread::hardware_concurrency() / 2) {
		updateThreadCount(jobs.size());
	} else {
		updateThreadCount(std::thread::hardware_concurrency() / 2);
	}
	// logInfo("{} jobs created for the current round", "LogicSimulator::regenerateJobs", jobs.size());
}

void LogicSimulator::execAND(void* jobInstruction) {
	auto* ji = static_cast<JobInstruction*>(jobInstruction);
	for (size_t i = ji->start; i < ji->end; ++i) ji->self->andGates[i].tick(ji->self->statesA, ji->self->statesB);
}
void LogicSimulator::execANDRealistic(void* jobInstruction) {
	auto* ji = static_cast<JobInstruction*>(jobInstruction);
	for (size_t i = ji->start; i < ji->end; ++i) ji->self->andGates[i].realisticTick(ji->self->statesA, ji->self->statesB);
}
void LogicSimulator::execXOR(void* jobInstruction) {
	auto* ji = static_cast<JobInstruction*>(jobInstruction);
	for (size_t i = ji->start; i < ji->end; ++i) ji->self->xorGates[i].tick(ji->self->statesA, ji->self->statesB);
}
void LogicSimulator::execXORRealistic(void* jobInstruction) {
	auto* ji = static_cast<JobInstruction*>(jobInstruction);
	for (size_t i = ji->start; i < ji->end; ++i) ji->self->xorGates[i].realisticTick(ji->self->statesA, ji->self->statesB);
}
void LogicSimulator::execTristate(void* jobInstruction) {
	auto* ji = static_cast<JobInstruction*>(jobInstruction);
	for (size_t i = ji->start; i < ji->end; ++i) ji->self->tristateBuffers[i].tick(ji->self->statesA, ji->self->statesB);
}
void LogicSimulator::execTristateRealistic(void* jobInstruction) {
	auto* ji = static_cast<JobInstruction*>(jobInstruction);
	for (size_t i = ji->start; i < ji->end; ++i) ji->self->tristateBuffers[i].realisticTick(ji->self->statesA, ji->self->statesB);
}
void LogicSimulator::execConstantReset(void* jobInstruction) {
	auto* ji = static_cast<JobInstruction*>(jobInstruction);
	for (size_t i = ji->start; i < ji->end; ++i) ji->self->constantResetGates[i].tick(ji->self->statesB);
}
void LogicSimulator::execCopySelfOutput(void* jobInstruction) {
	auto* ji = static_cast<JobInstruction*>(jobInstruction);
	for (size_t i = ji->start; i < ji->end; ++i) ji->self->copySelfOutputGates[i].tick(ji->self->statesA, ji->self->statesB);
}