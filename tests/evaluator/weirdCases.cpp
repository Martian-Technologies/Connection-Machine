#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "computerAPI/directoryManager.h"

class WeirdCasesEvaluatorTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment { false };
	EvalLogicSimulator* simulator = nullptr;
	Circuit* circuit = nullptr;
	BlockType loadCircuit(const std::filesystem::path& path);
	const BlockData* getBlockData(BlockType type);
};

BlockType WeirdCasesEvaluatorTest::loadCircuit(const std::filesystem::path& path) {
	CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
	circuit_id_t circuitId = circuitFileManager.loadFromFile(path.string()).at(0);
	Circuit* circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	return circuit->getBlockType();
}

const BlockData* WeirdCasesEvaluatorTest::getBlockData(BlockType type) {
	const BlockData* blockData = environment.getBackend().getBlockDataManager().getBlockData(type);
	return blockData;
}

void WeirdCasesEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	simulator = environment.getBackend().getSimulator(simulatorId);
}

void WeirdCasesEvaluatorTest::TearDown() {
	circuit = nullptr;
	simulator = nullptr;
}

TEST_F(WeirdCasesEvaluatorTest, ConstJunctionAnd) {
	Position constPosition(0, 0);
	Position junctionPosition(1, 0);
	Position andPosition(2, 0);

	ASSERT_TRUE(circuit->tryInsertBlock(constPosition, Rotation::ZERO, BlockType::CONSTANT_ON));
	ASSERT_TRUE(circuit->tryInsertBlock(junctionPosition, Rotation::ZERO, BlockType::JUNCTION));
	ASSERT_TRUE(circuit->tryInsertBlock(andPosition, Rotation::ZERO, BlockType::AND));
	ASSERT_TRUE(circuit->tryCreateConnection(constPosition, junctionPosition));
	ASSERT_TRUE(circuit->tryCreateConnection(junctionPosition, andPosition));
	ASSERT_TRUE(circuit->tryCreateConnection(constPosition, andPosition));

	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(constPosition), logic_state_t::HIGH);
	EXPECT_EQ(simulator->getState(junctionPosition), logic_state_t::HIGH);
	EXPECT_EQ(simulator->getState(andPosition), logic_state_t::HIGH);

	ASSERT_TRUE(circuit->tryRemoveBlock(junctionPosition));

	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(constPosition), logic_state_t::HIGH);
	EXPECT_EQ(simulator->getState(andPosition), logic_state_t::HIGH);

	ASSERT_TRUE(circuit->tryRemoveBlock(constPosition));

	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(andPosition), logic_state_t::LOW);
}

TEST_F(WeirdCasesEvaluatorTest, ButtonBuffer) {
	Position buttonPos(0, 0);
	Position bufferPos(1, 0);

	ASSERT_TRUE(circuit->tryInsertBlock(buttonPos, Rotation::ZERO, BlockType::BUTTON));
	ASSERT_TRUE(circuit->tryInsertBlock(bufferPos, Rotation::ZERO, BlockType::BUFFER));

	simulator->tickStep();
	EXPECT_EQ(simulator->getState(buttonPos), logic_state_t::LOW);
	EXPECT_EQ(simulator->getState(bufferPos), logic_state_t::UNDEFINED);

	ASSERT_TRUE(circuit->tryCreateConnection(buttonPos, bufferPos));
	simulator->tickStep();
	EXPECT_EQ(simulator->getState(buttonPos), logic_state_t::LOW);
	EXPECT_EQ(simulator->getState(bufferPos), logic_state_t::LOW);

	simulator->setState(buttonPos, logic_state_t::HIGH);
	simulator->tickStep();
	EXPECT_EQ(simulator->getState(buttonPos), logic_state_t::HIGH);
	EXPECT_EQ(simulator->getState(bufferPos), logic_state_t::HIGH);

	simulator->setState(buttonPos, logic_state_t::LOW);
	simulator->tickStep();
	EXPECT_EQ(simulator->getState(buttonPos), logic_state_t::LOW);
	EXPECT_EQ(simulator->getState(bufferPos), logic_state_t::LOW);

	ASSERT_TRUE(circuit->tryRemoveConnection(buttonPos, bufferPos));
	simulator->tickStep();
	EXPECT_EQ(simulator->getState(buttonPos), logic_state_t::LOW);
	EXPECT_EQ(simulator->getState(bufferPos), logic_state_t::UNDEFINED);
}

TEST_F(WeirdCasesEvaluatorTest, InitializationBehaviorWithICs) {
	BlockType passthrough = loadCircuit((DirectoryManager::getResourceDirectory() / "circuits" / "evaluator" / "passthrough.cir").string());
	Position xnorPos(0, 0);
	Position icPos(1, 0);
	Position andPos(0, 1);
	ASSERT_TRUE(circuit->tryInsertBlock(xnorPos, Rotation::ZERO, BlockType::XNOR));
	ASSERT_TRUE(circuit->tryInsertBlock(icPos, Rotation::ZERO, passthrough));
	ASSERT_TRUE(circuit->tryInsertBlock(andPos, Rotation::ZERO, BlockType::AND));
	ASSERT_TRUE(circuit->tryCreateConnection(icPos, xnorPos));
	ASSERT_TRUE(circuit->tryCreateConnection(andPos, andPos));
	ASSERT_TRUE(circuit->tryCreateConnection(andPos, icPos));

	EXPECT_EQ(simulator->getState(andPos), logic_state_t::LOW);
	simulator->tickStep(1);
	EXPECT_EQ(simulator->getState(xnorPos), logic_state_t::HIGH);

	simulator->setState(andPos, logic_state_t::HIGH);
	EXPECT_EQ(simulator->getState(andPos), logic_state_t::HIGH);
	simulator->tickStep(1);
	EXPECT_EQ(simulator->getState(xnorPos), logic_state_t::LOW);

	simulator_id_t simulatorId2 = environment.getBackend().createSimulator(circuit->getCircuitId()).value();
	EvalLogicSimulator* simulator2 = environment.getBackend().getSimulator(simulatorId2);
	EXPECT_EQ(simulator2->getState(andPos), logic_state_t::LOW);
	simulator2->tickStep(1);
	EXPECT_EQ(simulator2->getState(xnorPos), logic_state_t::HIGH);

	simulator2->setState(andPos, logic_state_t::HIGH);
	EXPECT_EQ(simulator2->getState(andPos), logic_state_t::HIGH);
	simulator2->tickStep(1);
	EXPECT_EQ(simulator2->getState(xnorPos), logic_state_t::LOW);
}

TEST_F(WeirdCasesEvaluatorTest, RemovingSharedDriverLeavesInputOnBusPort) {
	// Regression for a crash in the (pre-bus-replacement) JunctionMergeEvalLayer,
	// minimized from a fuzzer-found case. A NOT gate's input ("notA") is driven by
	// two things: a bus port, and the output of a second NOT gate ("notB"). notB
	// also drives a pull-down junction. Removing notB makes the merge layer rescan
	// notA's input, which now forms a junction-free group whose only neighbour is
	// the bus port. A bus port is not classified as a "single pin" output, so the
	// layer found no driving output and tried to wire the group from a null
	// connection point, tripping an assert in EvalLayerState::addConnection.
	BlockType busType = environment.getBackend().getBlockDataManager().getBusBlock(4, 1, 1, 4);
	ASSERT_NE(busType, BlockType::NONE);

	Position busPos(0, 0);
	Position notAPos(6, 0);
	Position notBPos(6, 4);
	Position pullDownPos(6, 8);

	ASSERT_TRUE(circuit->tryInsertBlock(busPos, Rotation::ZERO, busType));
	ASSERT_TRUE(circuit->tryInsertBlock(notAPos, Rotation::ZERO, BlockType::NOT));
	ASSERT_TRUE(circuit->tryInsertBlock(notBPos, Rotation::ZERO, BlockType::NOT));
	ASSERT_TRUE(circuit->tryInsertBlock(pullDownPos, Rotation::ZERO, BlockType::JUNCTION_L));

	const Block* bus = circuit->getBlockContainer().getBlock(busPos);
	const Block* notA = circuit->getBlockContainer().getBlock(notAPos);
	const Block* notB = circuit->getBlockContainer().getBlock(notBPos);
	const Block* pullDown = circuit->getBlockContainer().getBlock(pullDownPos);
	ASSERT_NE(bus, nullptr);
	ASSERT_NE(notA, nullptr);
	ASSERT_NE(notB, nullptr);
	ASSERT_NE(pullDown, nullptr);

	// notA input (end 0) is driven by a bus port (end 2) and by notB's output (end 1)
	ASSERT_TRUE(circuit->tryCreateConnection({ bus->id(), connection_end_id_t(2) }, { notA->id(), connection_end_id_t(0) }));
	ASSERT_TRUE(circuit->tryCreateConnection({ notB->id(), connection_end_id_t(1) }, { notA->id(), connection_end_id_t(0) }));
	// notB also drives a pull-down junction
	ASSERT_TRUE(circuit->tryCreateConnection({ notB->id(), connection_end_id_t(1) }, { pullDown->id(), connection_end_id_t(0) }));

	simulator->tickStep(2);

	// removing notB leaves notA's input wired only to the (undriven) bus port
	ASSERT_TRUE(circuit->tryRemoveBlock(notBPos));
	simulator->tickStep(2);
	EXPECT_EQ(simulator->getState(notAPos), logic_state_t::UNDEFINED);
}

TEST_F(WeirdCasesEvaluatorTest, RemovingGateAdjacentToJunctionKeepsJunctionNet) {
	Position constZ(-14, -5);
	Position tristate(-7, 1);
	Position junctionX(20, 8);
	Position nand(8, -5);

	ASSERT_TRUE(circuit->tryInsertBlock(constZ, Orientation(3), BlockType::CONSTANT_Z));
	ASSERT_TRUE(circuit->tryInsertBlock(tristate, Orientation(3 | 4), BlockType::TRISTATE_BUFFER));
	ASSERT_TRUE(circuit->tryInsertBlock(junctionX, Orientation(2 | 4), BlockType::JUNCTION_X));
	ASSERT_TRUE(circuit->tryCreateConnection(constZ, Position(20, 10)));
	ASSERT_TRUE(circuit->tryInsertBlock(nand, Orientation(3), BlockType::NAND));
	ASSERT_TRUE(circuit->tryCreateConnection(constZ, Position(-6, 1)));
	ASSERT_TRUE(circuit->tryCreateConnection(nand, Position(-6, 1)));
	ASSERT_TRUE(circuit->tryRemoveBlock(nand));

	simulator->tickStep(1);

	simulator_id_t refId = environment.getBackend().createSimulator(circuit->getCircuitId()).value();
	EvalLogicSimulator* ref = environment.getBackend().getSimulator(refId);
	ref->tickStep(1);

	for (Position p : { constZ, tristate, junctionX, Position(-6, 1), Position(20, 10) }) {
		EXPECT_EQ(simulator->getState(p), ref->getState(p)) << "incremental vs fresh mismatch at " << p.toString();
	}
}

TEST_F(WeirdCasesEvaluatorTest, RemovingSharedDriverWithBusKeepsConsistentRemapping) {
	BlockType busType = environment.getBackend().getBlockDataManager().getBusBlock(4, 1, 1, 4);
	ASSERT_NE(busType, BlockType::NONE);

	Position notA(10, -3);
	Position buffer(-6, -1);
	Position constZ(2, -11);
	Position bus(-16, -8);
	Position notB(16, 16);

	ASSERT_TRUE(circuit->tryInsertBlock(notA, Orientation(2 | 4), BlockType::NOT));
	ASSERT_TRUE(circuit->tryInsertBlock(buffer, Orientation(2), BlockType::BUFFER));
	ASSERT_TRUE(circuit->tryInsertBlock(constZ, Orientation(3 | 4), BlockType::CONSTANT_Z));
	ASSERT_TRUE(circuit->tryCreateConnection(constZ, buffer));
	ASSERT_TRUE(circuit->tryInsertBlock(bus, Orientation(2 | 4), busType));
	circuit->tryCreateConnection(constZ, Position(-15, -7)); // CONSTANT_Z -> bus port
	ASSERT_TRUE(circuit->tryCreateConnection(notA, buffer));
	ASSERT_TRUE(circuit->tryInsertBlock(notB, Orientation(3), BlockType::NOT));
	ASSERT_TRUE(circuit->tryCreateConnection(notB, buffer));
	ASSERT_TRUE(circuit->tryRemoveBlock(notB));

	simulator->tickStep(2);

	simulator_id_t refId = environment.getBackend().createSimulator(circuit->getCircuitId()).value();
	EvalLogicSimulator* ref = environment.getBackend().getSimulator(refId);
	ref->tickStep(2);

	for (Position p : { notA, buffer, constZ }) {
		EXPECT_EQ(simulator->getState(p), ref->getState(p)) << "incremental vs fresh mismatch at " << p.toString();
	}
}

TEST_F(WeirdCasesEvaluatorTest, PullUpPullDownButWithDifferentConnectionMethod) {
	Position pullUpPos(0, 0);
	Position pullDownPos(1, 0);

	logInfo("Inserting pull-up and pull-down junctions", "WeirdCasesEvaluatorTest::PullUpPullDownButWithDifferentConnectionMethod");

	ASSERT_TRUE(circuit->tryInsertBlock(pullUpPos, Rotation::ZERO, BlockType::JUNCTION_H));
	ASSERT_TRUE(circuit->tryInsertBlock(pullDownPos, Rotation::ZERO, BlockType::JUNCTION_L));

	EXPECT_EQ(simulator->getState(pullUpPos), logic_state_t::HIGH);
	EXPECT_EQ(simulator->getState(pullDownPos), logic_state_t::LOW);

	const Block* pullUpBlock = circuit->getBlockContainer().getBlock(pullUpPos);
	const Block* pullDownBlock = circuit->getBlockContainer().getBlock(pullDownPos);
	ASSERT_NE(pullUpBlock, nullptr);
	ASSERT_NE(pullDownBlock, nullptr);

	logInfo("Creating connection between pull-up and pull-down junctions", "WeirdCasesEvaluatorTest::PullUpPullDownButWithDifferentConnectionMethod");

	ASSERT_TRUE(circuit->tryCreateConnection({ pullUpBlock->id(), connection_end_id_t(0) }, { pullDownBlock->id(), connection_end_id_t(0) }));
	EXPECT_EQ(simulator->getState(pullUpPos), logic_state_t::UNDEFINED);
	EXPECT_EQ(simulator->getState(pullDownPos), logic_state_t::UNDEFINED);

	logInfo("Removing connection between pull-up and pull-down junctions", "WeirdCasesEvaluatorTest::PullUpPullDownButWithDifferentConnectionMethod");

	ASSERT_TRUE(circuit->tryRemoveConnection({ pullDownBlock->id(), connection_end_id_t(0) }, { pullUpBlock->id(), connection_end_id_t(0) }));
	EXPECT_EQ(simulator->getState(pullUpPos), logic_state_t::HIGH);
	EXPECT_EQ(simulator->getState(pullDownPos), logic_state_t::LOW);
}
