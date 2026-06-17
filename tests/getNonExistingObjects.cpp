#include "getNonExistingObjects.h"

#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "environment/environment.h"

TEST_F(GetNonExistingObjects, BlockContainer) {
	Environment environment(false);
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	Circuit* circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	ASSERT_TRUE(circuit);
	const BlockContainer& blockContainer = circuit->getBlockContainer();
	ASSERT_EQ(blockContainer.getBlock(Position(1, 2)), nullptr);
	ASSERT_EQ(blockContainer.getBlock(29), nullptr);
	ASSERT_EQ(blockContainer.getBitwidthOfJunction(Position(-129, 291)), 0);
	ASSERT_EQ(blockContainer.getBitwidthOfJunction(10), 0);
	ASSERT_EQ(blockContainer.getBitwidthOfJunctionIgnorePort(10, BlockType::AND, 2), 0);
	// ASSERT_EQ(blockContainer.getBlockType(), BlockType::NONE); // not yet true, will be
	ASSERT_EQ(blockContainer.begin(), blockContainer.end());
	ASSERT_EQ(blockContainer.getCell(Position(29, -2)), nullptr);
	ASSERT_EQ(blockContainer.getInputConnectionEnd(Position(12, 93)), std::nullopt);
	ASSERT_EQ(blockContainer.getOutputConnectionEnd(Position(-21, 219)), std::nullopt);
	ASSERT_EQ(blockContainer.getInputOrBidirectionalConnectionEnd(Position(291, 12)), std::nullopt);
	ASSERT_EQ(blockContainer.getOutputOrBidirectionalConnectionEnd(Position(-675, 0)), std::nullopt);
	ASSERT_EQ(blockContainer.getInputConnections(Position(0, 0)), nullptr);
	ASSERT_EQ(blockContainer.getOutputConnections(Position(0, 129)), nullptr);
	ASSERT_EQ(blockContainer.getCellCount(), 0);
}

TEST_F(GetNonExistingObjects, Circuit) {
	Environment environment(false);
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	Circuit* circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	ASSERT_NE(circuit, nullptr);
	// ASSERT_EQ(circuit->getBlockType(), BlockType::NONE);
	ASSERT_FALSE(circuit->tryRemoveBlock(Position(9, -5)));
	ASSERT_FALSE(circuit->tryRemoveConnection(Position(83, 0), Position(0, 17)));
	ASSERT_FALSE(circuit->tryRemoveConnection(
		std::make_shared<ProjectionSelection>(Position(74, 0), Vector(-2, 9), 5),
		std::make_shared<ProjectionSelection>(Position(-3, 19), Vector(28, 9), 8)
	));
	ASSERT_TRUE(
		circuit->tryMoveBlocks(std::make_shared<ProjectionSelection>(Position(47, -5), Vector(-5, 1), 2), Vector(58, 29), Orientation(Rotation::TWO_SEVENTY, true))
	); // moving no blocks mean nothing will block the move and so this is true
	ASSERT_FALSE(circuit->tryMoveBlock(Position(-758, 17), Position(-15, 421), Orientation(Rotation::TWO_SEVENTY, true)));
}

TEST_F(GetNonExistingObjects, Evaluator) {
	Environment environment(false);
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	Circuit* circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	ASSERT_NE(circuit, nullptr);
	std::optional<simulator_id_t> simulatorId = environment.getBackend().createSimulator(circuitId);
	ASSERT_NE(simulatorId, std::nullopt);
	EvalLogicSimulator* simulator = environment.getBackend().getSimulator(*simulatorId);
	ASSERT_NE(simulator, nullptr);
	ASSERT_EQ(simulator->getVirtualConnectionSimulatorId(Address(Position(10, 29)), 0), (SimulatorStateIndexVecVariant)3);
	std::vector<SimulatorStateIndexVecVariant> XstateVec = { 3 };
	ASSERT_EQ(
		simulator->getVirtualConnectionSimulatorIds(Address(Position(29, -86)), {{Position(-21, 18), 0}}),
		XstateVec
	);
	ASSERT_EQ(simulator->getState(Position(-1, 2)), X);
	ASSERT_EQ(simulator->getState(Position(0, 0)), X);
}

TEST_F(GetNonExistingObjects, Backend) {
	Environment environment(false);
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	std::optional<simulator_id_t> simulatorId = environment.getBackend().createSimulator(circuitId);
	ASSERT_EQ(environment.getBackend().getCircuitManager().getCircuit(0), nullptr);
	ASSERT_EQ(environment.getBackend().getCircuitManager().getCircuit(194 + (circuitId == 194)), nullptr);
	ASSERT_EQ(environment.getBackend().getSimulator(0), nullptr);
	ASSERT_EQ(environment.getBackend().getSimulator(412 + (simulatorId.value_or(0) == 412)), nullptr);
}
