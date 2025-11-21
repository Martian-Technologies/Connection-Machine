#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"
#include "backend/blockData/blockDataManager.h"
#include "computerAPI/directoryManager.h"

class BusesSimpleEvaluatorTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment { false };
	SharedEvaluator evaluator = nullptr;
	SharedCircuit circuit = nullptr;
	logic_state_t L = logic_state_t::LOW;
	logic_state_t H = logic_state_t::HIGH;
	logic_state_t Z = logic_state_t::FLOATING;
	logic_state_t X = logic_state_t::UNDEFINED;
	SharedCircuit busTristate1Circuit = nullptr;
	SharedCircuit busTristate2Circuit = nullptr;
	BlockType BT1;
	BlockType BT2;
	BlockType BUS2;
};

void BusesSimpleEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuit(circuitId);
	evaluator_id_t evalId = environment.getBackend().createEvaluator(circuitId).value();
	evaluator = environment.getBackend().getEvaluator(evalId);
	CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
	circuit_id_t busTristate1 = circuitFileManager.loadFromFile((DirectoryManager::getResourceDirectory() / "circuits" / "evaluator" / "bus_tristate_1.cir").string()).at(0);
	busTristate1Circuit = environment.getBackend().getCircuitManager().getCircuit(busTristate1);
	BT1 = busTristate1Circuit->getBlockType();
	circuit_id_t busTristate2 = circuitFileManager.loadFromFile((DirectoryManager::getResourceDirectory() / "circuits" / "evaluator" / "bus_tristate_2.cir").string()).at(0);
	busTristate2Circuit = environment.getBackend().getCircuitManager().getCircuit(busTristate2);
	BT2 = busTristate2Circuit->getBlockType();
	BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
	BUS2 = blockDataManager.getBusBlock(2);
}

void BusesSimpleEvaluatorTest::TearDown() {
	circuit.reset();
	evaluator.reset();
}

TEST_F(BusesSimpleEvaluatorTest, SimpleBus2) {
	BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
	const BlockData* bus2Data = blockDataManager.getBlockData(BUS2);
	ASSERT_NE(bus2Data, nullptr);
	Position switch1Pos(0, 0);
	Position switch2Pos(0, 1);
	ASSERT_TRUE(circuit->tryInsertBlock(switch1Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(switch2Pos, 0, BlockType::SWITCH));

	Position busPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(busPos, 0, BUS2));

	ASSERT_TRUE(circuit->tryCreateConnection(switch1Pos, busPos + *bus2Data->getConnectionVector(0)));
	ASSERT_TRUE(circuit->tryCreateConnection(switch2Pos, busPos + *bus2Data->getConnectionVector(1)));

	Position busOutputPos = busPos + *(bus2Data->getConnectionVector(2));

	std::variant<logic_state_t, std::vector<logic_state_t>> outputState = evaluator->getPinState(busOutputPos);
	ASSERT_TRUE(std::holds_alternative<std::vector<logic_state_t>>(outputState));
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(outputState), std::vector<logic_state_t>({ L, L }));

	evaluator->setState(switch1Pos, H);

	outputState = evaluator->getPinState(busOutputPos);
	ASSERT_TRUE(std::holds_alternative<std::vector<logic_state_t>>(outputState));
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(outputState), std::vector<logic_state_t>({ H, L }));

	evaluator->setState(switch2Pos, H);

	outputState = evaluator->getPinState(busOutputPos);
	ASSERT_TRUE(std::holds_alternative<std::vector<logic_state_t>>(outputState));
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(outputState), std::vector<logic_state_t>({ H, H }));

	evaluator->setState(switch1Pos, L);

	outputState = evaluator->getPinState(busOutputPos);
	ASSERT_TRUE(std::holds_alternative<std::vector<logic_state_t>>(outputState));
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(outputState), std::vector<logic_state_t>({ L, H }));
}

TEST_F(BusesSimpleEvaluatorTest, BusTristate) {
	BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
	const BlockData* bus2Data = blockDataManager.getBlockData(BUS2);
	ASSERT_NE(bus2Data, nullptr);

	Position switch1Pos(0, 0);
	Position switch2Pos(0, 1);
	Position toggleSwitchPos(0, 2);

	ASSERT_TRUE(circuit->tryInsertBlock(switch1Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(switch2Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(toggleSwitchPos, 0, BlockType::SWITCH));

	Position busPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(busPos, 0, BUS2));
	ASSERT_TRUE(circuit->tryCreateConnection(switch1Pos, busPos + *bus2Data->getConnectionVector(0)));
	ASSERT_TRUE(circuit->tryCreateConnection(switch2Pos, busPos + *bus2Data->getConnectionVector(1)));
	Position busOutputPos = busPos + *(bus2Data->getConnectionVector(2));

	Position tristatePos(3, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(tristatePos, 0, BT1));
	ASSERT_TRUE(circuit->tryRemoveBlock(tristatePos));
	ASSERT_TRUE(circuit->tryInsertBlock(tristatePos, 0, BT2));
	ASSERT_TRUE(circuit->tryCreateConnection(busOutputPos, tristatePos));
	ASSERT_TRUE(circuit->tryCreateConnection(toggleSwitchPos, tristatePos + Vector(0, 1)));

	EXPECT_EQ(std::get<std::vector<logic_state_t>>(evaluator->getPinState(tristatePos)), std::vector<logic_state_t>({ Z, Z }));
	evaluator->setState(switch1Pos, H);
	evaluator->tickStep();
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(evaluator->getPinState(tristatePos)), std::vector<logic_state_t>({ Z, Z }));
	evaluator->setState(switch2Pos, H);
	evaluator->tickStep();
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(evaluator->getPinState(tristatePos)), std::vector<logic_state_t>({ Z, Z }));
	evaluator->setState(toggleSwitchPos, H);
	evaluator->tickStep();
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(evaluator->getPinState(tristatePos)), std::vector<logic_state_t>({ H, H }));
	evaluator->setState(switch1Pos, L);
	evaluator->tickStep();
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(evaluator->getPinState(tristatePos)), std::vector<logic_state_t>({ L, H }));
	evaluator->setState(switch2Pos, L);
	evaluator->tickStep();
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(evaluator->getPinState(tristatePos)), std::vector<logic_state_t>({ L, L }));
	evaluator->setState(toggleSwitchPos, L);
	evaluator->tickStep();
	EXPECT_EQ(std::get<std::vector<logic_state_t>>(evaluator->getPinState(tristatePos)), std::vector<logic_state_t>({ Z, Z }));
}

TEST_F(BusesSimpleEvaluatorTest, BusEndsHaveSameSimIds) {
	BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
	const BlockData* bus2Data = blockDataManager.getBlockData(BUS2);
	ASSERT_NE(bus2Data, nullptr);

	Position bus1Pos(0, 0);
	Position bus2Pos(2, 0);
	Position bus3Pos(4, 0);
	Position switch1Pos(0, 2);
	Position switch2Pos(1, 2);
	ASSERT_TRUE(circuit->tryInsertBlock(bus1Pos, 0, BUS2));
	ASSERT_TRUE(circuit->tryInsertBlock(bus2Pos, 0, BUS2));
	ASSERT_TRUE(circuit->tryInsertBlock(bus3Pos, 0, BUS2));
	ASSERT_TRUE(circuit->tryInsertBlock(switch1Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(switch2Pos, 0, BlockType::SWITCH));

	ASSERT_TRUE(circuit->tryCreateConnection(switch1Pos, bus1Pos + *bus2Data->getConnectionVector(0)));
	ASSERT_TRUE(circuit->tryCreateConnection(switch2Pos, bus3Pos + *bus2Data->getConnectionVector(1)));
	ASSERT_TRUE(circuit->tryCreateConnection(bus1Pos + *bus2Data->getConnectionVector(2), bus2Pos + *bus2Data->getConnectionVector(2)));
	ASSERT_TRUE(circuit->tryCreateConnection(bus2Pos + *bus2Data->getConnectionVector(2), bus3Pos + *bus2Data->getConnectionVector(2)));

	std::variant<simulator_id_t, std::vector<simulator_id_t>> bus1OutputSimId = evaluator->getPinSimulatorId(bus1Pos + *(bus2Data->getConnectionVector(2)));
	std::variant<simulator_id_t, std::vector<simulator_id_t>> bus2OutputSimId = evaluator->getPinSimulatorId(bus2Pos + *(bus2Data->getConnectionVector(2)));
	std::variant<simulator_id_t, std::vector<simulator_id_t>> bus3OutputSimId = evaluator->getPinSimulatorId(bus3Pos + *(bus2Data->getConnectionVector(2)));

	ASSERT_TRUE(std::holds_alternative<std::vector<simulator_id_t>>(bus1OutputSimId));
	ASSERT_TRUE(std::holds_alternative<std::vector<simulator_id_t>>(bus2OutputSimId));
	ASSERT_TRUE(std::holds_alternative<std::vector<simulator_id_t>>(bus3OutputSimId));
	EXPECT_EQ(std::get<std::vector<simulator_id_t>>(bus1OutputSimId), std::get<std::vector<simulator_id_t>>(bus2OutputSimId));
	EXPECT_EQ(std::get<std::vector<simulator_id_t>>(bus2OutputSimId), std::get<std::vector<simulator_id_t>>(bus3OutputSimId));
}
