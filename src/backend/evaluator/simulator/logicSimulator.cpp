#include "logicSimulator.h"

// Fail at build time if the editor changes the numeric gate/state ABI.
static_assert(static_cast<std::uint16_t>(BlockType::AND) == 1);
static_assert(static_cast<std::uint16_t>(BlockType::OR) == 2);
static_assert(static_cast<std::uint16_t>(BlockType::XOR) == 3);
static_assert(static_cast<std::uint16_t>(BlockType::NAND) == 4);
static_assert(static_cast<std::uint16_t>(BlockType::NOR) == 5);
static_assert(static_cast<std::uint16_t>(BlockType::XNOR) == 6);
static_assert(static_cast<std::uint16_t>(BlockType::BUFFER) == 7);
static_assert(static_cast<std::uint16_t>(BlockType::NOT) == 8);
static_assert(static_cast<std::uint16_t>(BlockType::JUNCTION) == 9);
static_assert(static_cast<std::uint16_t>(BlockType::JUNCTION_L) == 10);
static_assert(static_cast<std::uint16_t>(BlockType::JUNCTION_H) == 11);
static_assert(static_cast<std::uint16_t>(BlockType::JUNCTION_X) == 12);
static_assert(static_cast<std::uint16_t>(BlockType::TRISTATE_BUFFER) == 13);
static_assert(static_cast<std::uint16_t>(BlockType::BUTTON) == 14);
static_assert(static_cast<std::uint16_t>(BlockType::TICK_BUTTON) == 15);
static_assert(static_cast<std::uint16_t>(BlockType::SWITCH) == 16);
static_assert(static_cast<std::uint16_t>(BlockType::CONSTANT_OFF) == 17);
static_assert(static_cast<std::uint16_t>(BlockType::CONSTANT_ON) == 18);
static_assert(static_cast<std::uint16_t>(BlockType::CONSTANT_Z) == 19);
static_assert(static_cast<std::uint16_t>(BlockType::CONSTANT_X) == 20);
static_assert(static_cast<std::uint8_t>(logic_state_t::LOW) == 0);
static_assert(static_cast<std::uint8_t>(logic_state_t::HIGH) == 1);
static_assert(static_cast<std::uint8_t>(logic_state_t::FLOATING) == 2);
static_assert(static_cast<std::uint8_t>(logic_state_t::UNDEFINED) == 3);

LogicSimulator::LogicSimulator(simulator_id_t id, std::vector<simulator_gate_id_t>& dirty, DataUpdateEventManager& events) :
	simulatorConfig(id, events), engine(cm_sim_new()), dirtySimulatorIds(dirty) {
	simulatorConfig.subscribe([this]() {
		{
			std::lock_guard lock(engineMutex);
			cm_sim_set_realistic(engine, simulatorConfig.isRealistic());
			cm_sim_set_threads(engine, std::max(1, simulatorConfig.getMaxThreadCount()));
			if (simulatorConfig.isRunning()) cm_sim_return_to_live(engine);
		}
		{
			std::lock_guard lock(controlMutex);
			controlRevision.fetch_add(1);
		}
		controlCv.notify_all();
	});
	cm_sim_set_threads(engine, std::max(1, simulatorConfig.getMaxThreadCount()));
	simulationThread = std::thread(&LogicSimulator::simulationLoop, this);
}

LogicSimulator::~LogicSimulator() {
	simulatorConfig.unsubscribe();
	{
		std::lock_guard lock(controlMutex);
		threadRunning.store(false);
	}
	controlCv.notify_all();
	if (simulationThread.joinable()) simulationThread.join();
	cm_sim_free(engine);
}

void LogicSimulator::simulationLoop() {
	using Clock = std::chrono::steady_clock;
	auto nextTick = Clock::now();
	auto lastTick = nextTick;
	bool firstTick = true;
	std::uint64_t revision = controlRevision.load();
	while (threadRunning.load()) {
		bool sprint = simulatorConfig.canConsumeSprintTick();
		if (!sprint && !simulatorConfig.isRunning()) {
			averageTickrate.store(0);
			std::unique_lock lock(controlMutex);
			controlCv.wait(lock, [this]() { return !threadRunning.load() || simulatorConfig.isRunning() || simulatorConfig.canConsumeSprintTick(); });
			nextTick = lastTick = Clock::now();
			firstTick = true;
			continue;
		}
		if (revision != controlRevision.load()) {
			revision = controlRevision.load();
			nextTick = Clock::now();
		}
		auto now = Clock::now();
		{
			std::lock_guard lock(engineMutex);
			if (!threadRunning.load()) break;
			sprint = simulatorConfig.canConsumeSprintTick();
			if (!simulatorConfig.isRunning() && !sprint) continue;
			cm_sim_tick(engine, 1);
		}
		if (sprint) {
			bool finished;
			{
				std::lock_guard lock(controlMutex);
				simulatorConfig.consumeSprintTick();
				finished = !simulatorConfig.canConsumeSprintTick();
			}
			if (finished) controlCv.notify_all();
		}
		if (!firstTick) {
			double seconds = std::chrono::duration<double>(now - lastTick).count();
			if (seconds > 0) {
				double alpha = 1.0 - std::exp(-seconds * std::log(2.0) / 0.3);
				averageTickrate.store(alpha / seconds + (1.0 - alpha) * averageTickrate.load());
			}
		}
		firstTick = false;
		lastTick = now;
		if (!sprint && simulatorConfig.isTickrateLimiterEnabled() && simulatorConfig.getTargetTickrate() > 0) {
			nextTick += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / simulatorConfig.getTargetTickrate()));
			std::unique_lock lock(controlMutex);
			controlCv.wait_until(lock, nextTick, [this, revision]() { return !threadRunning.load() || controlRevision.load() != revision; });
		} else {
			nextTick = Clock::now();
			std::this_thread::yield();
		}
	}
}

void LogicSimulator::waitForSprintComplete() {
	std::unique_lock lock(controlMutex);
	controlCv.wait(lock, [this]() { return !threadRunning.load() || !simulatorConfig.canConsumeSprintTick(); });
}

double LogicSimulator::getAverageTickrate() const {
	if (!simulatorConfig.isRunning()) return 0;
	double rate = averageTickrate.load();
	double target = simulatorConfig.getTargetTickrate();
	if (simulatorConfig.isTickrateLimiterEnabled() && target > 0 && std::abs(rate - target) < target * 0.01) return target;
	return rate;
}

void LogicSimulator::setState(simulator_gate_id_t id, logic_state_t state) {
	std::lock_guard lock(engineMutex);
	cm_sim_set_state(engine, id.get(), static_cast<std::uint8_t>(state));
}
void LogicSimulator::resetStates() { std::lock_guard lock(engineMutex); cm_sim_reset(engine); }
logic_state_t LogicSimulator::getState(simulator_gate_id_t id) const {
	std::lock_guard lock(engineMutex);
	return static_cast<logic_state_t>(cm_sim_state(engine, id.get()));
}

std::vector<logic_state_t> LogicSimulator::getStates(const std::vector<simulator_gate_id_t>& ids) const {
	std::vector<std::uint32_t> rawIds;
	rawIds.reserve(ids.size());
	for (auto id : ids) rawIds.push_back(id.get());
	std::vector<std::uint8_t> rawStates(ids.size());
	{
		std::lock_guard lock(engineMutex);
		cm_sim_states(engine, rawIds.data(), rawStates.data(), rawIds.size());
	}
	std::vector<logic_state_t> states;
	states.reserve(ids.size());
	for (auto state : rawStates) states.push_back(static_cast<logic_state_t>(state));
	return states;
}

simulator_gate_id_t LogicSimulator::addGate(BlockType type) {
	std::lock_guard lock(engineMutex);
	simulator_gate_id_t id(cm_sim_add_gate(engine, static_cast<std::uint16_t>(type)));
	if (id != 3) gateTypes[id] = type;
	else logError("Cannot add gate of type {}", "LogicSimulator", type);
	return id;
}
void LogicSimulator::removeGate(simulator_gate_id_t id) {
	std::lock_guard lock(engineMutex);
	if (cm_sim_remove_gate(engine, id.get())) { gateTypes.erase(id); dirtySimulatorIds.push_back(id); }
}

BlockData::ConnectionData::PortType LogicSimulator::getPortType(simulator_gate_id_t id, connection_end_id_t port) const {
	std::lock_guard lock(engineMutex);
	using Port = BlockData::ConnectionData::PortType;
	auto it = gateTypes.find(id);
	if (it == gateTypes.end()) return Port::NONE;
	BlockType type = it->second;
	if (type >= BlockType::JUNCTION && type <= BlockType::JUNCTION_X) return Port::BIDIRECTIONAL;
	if (type >= BlockType::BUTTON && type <= BlockType::CONSTANT_X) return Port::OUTPUT;
	if (type == BlockType::TRISTATE_BUFFER) return port == 2 ? Port::OUTPUT : Port::INPUT;
	return port == 1 ? Port::OUTPUT : Port::INPUT;
}
std::optional<simulator_gate_id_t> LogicSimulator::getOutputPortId(simulator_gate_id_t id, connection_end_id_t port) const {
	std::lock_guard lock(engineMutex);
	if (!gateTypes.contains(id)) return std::nullopt;
	return id;
}

void LogicSimulator::changeConnection(simulator_gate_id_t source, connection_end_id_t sourcePort, simulator_gate_id_t destination, connection_end_id_t destinationPort, int delta) {
	std::lock_guard lock(engineMutex);
	using Port = BlockData::ConnectionData::PortType;
	if (getPortType(source, sourcePort) == Port::INPUT || getPortType(destination, destinationPort) == Port::OUTPUT) {
		std::swap(source, destination); std::swap(sourcePort, destinationPort);
	}
	if (!cm_sim_connect(engine, source.get(), destination.get(), destinationPort.get(), delta)) logError("Invalid simulator connection {} -> {}", "LogicSimulator", source, destination);
	dirtySimulatorIds.push_back(source);
}
void LogicSimulator::makeConnection(simulator_gate_id_t source, connection_end_id_t sourcePort, simulator_gate_id_t destination, connection_end_id_t destinationPort) { changeConnection(source, sourcePort, destination, destinationPort, 1); }
void LogicSimulator::removeConnection(simulator_gate_id_t source, connection_end_id_t sourcePort, simulator_gate_id_t destination, connection_end_id_t destinationPort) { changeConnection(source, sourcePort, destination, destinationPort, -1); }
void LogicSimulator::endEdit() { std::lock_guard lock(engineMutex); cm_sim_compile(engine); }
bool LogicSimulator::stepBack() { std::lock_guard lock(engineMutex); return cm_sim_step_back(engine); }
bool LogicSimulator::stepForward() { std::lock_guard lock(engineMutex); return cm_sim_step_forward(engine); }
bool LogicSimulator::skipBack() { std::lock_guard lock(engineMutex); return cm_sim_skip_back(engine); }
bool LogicSimulator::skipForward() { std::lock_guard lock(engineMutex); return cm_sim_skip_forward(engine); }
bool LogicSimulator::isViewingReplay() const { std::lock_guard lock(engineMutex); return cm_sim_is_replay(engine); }

nlohmann::json LogicSimulator::dumpState() const {
	std::lock_guard lock(engineMutex);
	std::uint64_t diagnostics[4];
	cm_sim_diagnostics(engine, diagnostics);
	nlohmann::json json = { { "engine", "rust" }, { "tick", cm_sim_tick_index(engine) }, { "replayBytes", cm_sim_replay_bytes(engine) },
		{ "viewingReplay", cm_sim_is_replay(engine) }, { "simulatorConfig", simulatorConfig.dumpState() } };
	json["evaluations"] = diagnostics[0];
	json["jitGroups"] = diagnostics[1];
	json["jitCodeBytes"] = diagnostics[2];
	json["jitCalls"] = diagnostics[3];
	json["gates"] = nlohmann::json::array();
	for (const auto& [id, type] : gateTypes) json["gates"].push_back({ { "id", id.get() }, { "type", blocktype_to_string(type) }, { "state", logicstate_to_string(static_cast<logic_state_t>(cm_sim_state(engine, id.get()))) } });
	return json;
}
