#include "evaluatorICTest.h"

#include "backend/evaluator/evaluator.h"

void EvaluatorICTest::SetUp() {
    circuit_id_t circuitId = environment.getBackend().createCircuit();
    parentCircuit = environment.getBackend().getCircuit(circuitId);
    auto evalId = environment.getBackend().createEvaluator(circuitId);
    ASSERT_TRUE(evalId.has_value());
    evaluator = environment.getBackend().getEvaluator(evalId.value());
    idx = 0;
}

void EvaluatorICTest::TearDown() {
    parentCircuit.reset();
    evaluator.reset();
}

circuit_id_t EvaluatorICTest::createPassThroughIC(const std::string& name) {
    circuit_id_t childId = environment.getBackend().createCircuit(name);
    SharedCircuit child = environment.getBackend().getCircuit(childId);

    child->tryInsertBlock(Position(0, 0), Rotation::ZERO, BlockType::JUNCTION);

    CircuitManager& cm = environment.getBackend().getCircuitManager();
    BlockType icType = cm.setupBlockData(childId);

    BlockData* bd = cm.getBlockDataManager().getBlockData(icType);
    bd->setDefaultData(false);
    bd->setPrimitive(false);
    bd->setPath("Custom");
    bd->setSize(Size(1, 1));

    bd->setConnectionInput(Vector(0, 0), connection_end_id_t(0));
    bd->setConnectionOutput(Vector(0, 0), connection_end_id_t(1));

    CircuitBlockData* cbd = cm.getCircuitBlockDataManager().getCircuitBlockData(childId);

    cbd->setConnectionIdPosition(connection_end_id_t(0), Position(0, 0));
    cbd->setConnectionIdPosition(connection_end_id_t(1), Position(0, 0));

    return childId;
}

TEST_F(EvaluatorICTest, SingleIC_PropagatesSignal) {
    const circuit_id_t icId = createPassThroughIC("PassThrough");
    const BlockType icBlockType = getICBlockType(icId);

    const Position pSwitch(idx, idx); ++idx;
    const Position pIC(idx, idx); ++idx;
    const Position pLight(idx, idx); ++idx;

    ASSERT_TRUE(parentCircuit->tryInsertBlock(pSwitch, Rotation::ZERO, BlockType::SWITCH));
    ASSERT_TRUE(parentCircuit->tryInsertBlock(pIC, Rotation::ZERO, icBlockType));
    ASSERT_TRUE(parentCircuit->tryInsertBlock(pLight, Rotation::ZERO, BlockType::LIGHT));

    ASSERT_TRUE(parentCircuit->tryCreateConnection(pSwitch, pIC));
    ASSERT_TRUE(parentCircuit->tryCreateConnection(pIC, pLight));

    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::LOW);

    evaluator->setState(Address(pSwitch), logic_state_t::HIGH);
    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::HIGH);

    evaluator->setState(Address(pSwitch), logic_state_t::LOW);
    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::LOW);
}

TEST_F(EvaluatorICTest, NestedICs_PropagateThroughLevels) {
    const circuit_id_t innerICId = createPassThroughIC("InnerPassThrough");
    const BlockType innerICType = getICBlockType(innerICId);

    const circuit_id_t outerICId = environment.getBackend().createCircuit("OuterPassThrough");
    SharedCircuit outerCircuit = environment.getBackend().getCircuit(outerICId);
    ASSERT_TRUE(outerCircuit);

    ASSERT_TRUE(outerCircuit->tryInsertBlock(Position(0, 0), Rotation::ZERO, innerICType));

    CircuitManager& cm = environment.getBackend().getCircuitManager();
    BlockType outerICType = cm.setupBlockData(outerICId);
    ASSERT_NE(outerICType, BlockType::NONE);
    BlockData* bd = cm.getBlockDataManager().getBlockData(outerICType);
    ASSERT_TRUE(bd);
    bd->setDefaultData(false);
    bd->setPrimitive(false);
    bd->setPath("Custom");
    bd->setSize(Size(1, 1));
    bd->setConnectionInput(Vector(0, 0), connection_end_id_t(0));
    bd->setConnectionOutput(Vector(0, 0), connection_end_id_t(1));

    CircuitBlockData* cbd = cm.getCircuitBlockDataManager().getCircuitBlockData(outerICId);
    ASSERT_TRUE(cbd);
    cbd->setConnectionIdPosition(connection_end_id_t(0), Position(0, 0));
    cbd->setConnectionIdPosition(connection_end_id_t(1), Position(0, 0));

    const Position pSwitch(idx, idx); ++idx;
    const Position pIC(idx, idx); ++idx;
    const Position pLight(idx, idx); ++idx;
    ASSERT_TRUE(parentCircuit->tryInsertBlock(pSwitch, Rotation::ZERO, BlockType::SWITCH));
    ASSERT_TRUE(parentCircuit->tryInsertBlock(pIC, Rotation::ZERO, outerICType));
    ASSERT_TRUE(parentCircuit->tryInsertBlock(pLight, Rotation::ZERO, BlockType::LIGHT));

    ASSERT_TRUE(parentCircuit->tryCreateConnection(pSwitch, pIC));
    ASSERT_TRUE(parentCircuit->tryCreateConnection(pIC, pLight));

    evaluator->setState(Address(pSwitch), logic_state_t::HIGH);
    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::HIGH);

    evaluator->setState(Address(pSwitch), logic_state_t::LOW);
    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::LOW);
}

TEST_F(EvaluatorICTest, SingleIC_MoveIOAndPropagatesSignal) {
    const circuit_id_t icId = createPassThroughIC("PassThrough");
    CircuitManager& cm = environment.getBackend().getCircuitManager();
    CircuitBlockData* cbd = cm.getCircuitBlockDataManager().getCircuitBlockData(icId);
    cbd->setConnectionIdPosition(connection_end_id_t(0), Position(1, 1));
    cbd->setConnectionIdPosition(connection_end_id_t(1), Position(1, 1));

    const BlockType icBlockType = getICBlockType(icId);

    const Position pSwitch(idx, idx); ++idx;
    const Position pIC(idx, idx); ++idx;
    const Position pLight(idx, idx); ++idx;

    ASSERT_TRUE(parentCircuit->tryInsertBlock(pSwitch, Rotation::ZERO, BlockType::SWITCH));
    ASSERT_TRUE(parentCircuit->tryInsertBlock(pIC, Rotation::ZERO, icBlockType));
    ASSERT_TRUE(parentCircuit->tryInsertBlock(pLight, Rotation::ZERO, BlockType::LIGHT));

    ASSERT_TRUE(parentCircuit->tryCreateConnection(pSwitch, pIC));
    ASSERT_TRUE(parentCircuit->tryCreateConnection(pIC, pLight));

    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::FLOATING);

    evaluator->setState(Address(pSwitch), logic_state_t::HIGH);
    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::FLOATING);

    evaluator->setState(Address(pSwitch), logic_state_t::LOW);
    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::FLOATING);

	cbd->setConnectionIdPosition(connection_end_id_t(0), Position(0, 0));
	cbd->setConnectionIdPosition(connection_end_id_t(1), Position(0, 0));

	evaluator->setState(Address(pSwitch), logic_state_t::HIGH);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::HIGH);

	evaluator->setState(Address(pSwitch), logic_state_t::LOW);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::LOW);
}

TEST_F(EvaluatorICTest, SingleIC_MoveBlockMaintainsSignal) {
	const circuit_id_t icId = createPassThroughIC("MovableIC");
	const BlockType icBlockType = getICBlockType(icId);

	const Position pSwitch(idx, idx); ++idx;
	Position pIC(idx, idx); ++idx;
	const Position pLight(idx, idx); ++idx;

	ASSERT_TRUE(parentCircuit->tryInsertBlock(pSwitch, Rotation::ZERO, BlockType::SWITCH));
	ASSERT_TRUE(parentCircuit->tryInsertBlock(pIC, Rotation::ZERO, icBlockType));
	ASSERT_TRUE(parentCircuit->tryInsertBlock(pLight, Rotation::ZERO, BlockType::LIGHT));

	ASSERT_TRUE(parentCircuit->tryCreateConnection(pSwitch, pIC));
	ASSERT_TRUE(parentCircuit->tryCreateConnection(pIC, pLight));

	evaluator->setState(Address(pSwitch), logic_state_t::HIGH);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::HIGH);

	const Position movedIC = pIC + Vector(3, 0);
	ASSERT_TRUE(parentCircuit->tryMoveBlock(pIC, movedIC, Orientation()));
	pIC = movedIC;

	evaluator->setState(Address(pSwitch), logic_state_t::LOW);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::LOW);

	evaluator->setState(Address(pSwitch), logic_state_t::HIGH);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::HIGH);
}

TEST_F(EvaluatorICTest, SingleIC_RemapInputToConstantSource) {
	const circuit_id_t icId = createPassThroughIC("RemapIC");
	const BlockType icBlockType = getICBlockType(icId);

	const Position pSwitch(idx, idx); ++idx;
	const Position pIC(idx, idx); ++idx;
	const Position pLight(idx, idx); ++idx;

	ASSERT_TRUE(parentCircuit->tryInsertBlock(pSwitch, Rotation::ZERO, BlockType::SWITCH));
	ASSERT_TRUE(parentCircuit->tryInsertBlock(pIC, Rotation::ZERO, icBlockType));
	ASSERT_TRUE(parentCircuit->tryInsertBlock(pLight, Rotation::ZERO, BlockType::LIGHT));

	ASSERT_TRUE(parentCircuit->tryCreateConnection(pSwitch, pIC));
	ASSERT_TRUE(parentCircuit->tryCreateConnection(pIC, pLight));

	evaluator->setState(Address(pSwitch), logic_state_t::HIGH);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::HIGH);
	evaluator->setState(Address(pSwitch), logic_state_t::LOW);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::LOW);

	SharedCircuit child = environment.getBackend().getCircuit(icId);
	ASSERT_TRUE(child);
	ASSERT_TRUE(child->tryInsertBlock(Position(1, 0), Rotation::ZERO, BlockType::JUNCTION));
	ASSERT_TRUE(child->tryInsertBlock(Position(2, 0), Rotation::ZERO, BlockType::CONSTANT_ON));
	ASSERT_TRUE(child->tryCreateConnection(Position(2, 0), Position(1, 0)));

	CircuitManager& cm = environment.getBackend().getCircuitManager();
	CircuitBlockData* cbd = cm.getCircuitBlockDataManager().getCircuitBlockData(icId);
	ASSERT_TRUE(cbd);

	cbd->setConnectionIdPosition(connection_end_id_t(0), Position(1, 0));
	cbd->setConnectionIdPosition(connection_end_id_t(1), Position(1, 0));

	evaluator->setState(Address(pSwitch), logic_state_t::LOW);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::UNDEFINED); // contention with constant HIGH

	evaluator->setState(Address(pSwitch), logic_state_t::HIGH);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::HIGH);

	cbd->setConnectionIdPosition(connection_end_id_t(0), Position(0, 0));
	cbd->setConnectionIdPosition(connection_end_id_t(1), Position(0, 0));

	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::HIGH);

	evaluator->setState(Address(pSwitch), logic_state_t::LOW);
	EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::LOW);
}

TEST_F(EvaluatorICTest, MultipleICInstances_RemappingPropagatesToAll) {
	const circuit_id_t icId = createPassThroughIC("MultiInstanceIC");
	const BlockType icBlockType = getICBlockType(icId);

	const Position pSwitchA(idx, idx); ++idx;
	Position pICA(idx, idx); ++idx;
	const Position pLightA(idx, idx); ++idx;

	const Position pSwitchB(idx, idx); ++idx;
	Position pICB(idx, idx); ++idx;
	const Position pLightB(idx, idx); ++idx;

	ASSERT_TRUE(parentCircuit->tryInsertBlock(pSwitchA, Rotation::ZERO, BlockType::SWITCH));
	ASSERT_TRUE(parentCircuit->tryInsertBlock(pICA, Rotation::ZERO, icBlockType));
	ASSERT_TRUE(parentCircuit->tryInsertBlock(pLightA, Rotation::ZERO, BlockType::LIGHT));

	ASSERT_TRUE(parentCircuit->tryInsertBlock(pSwitchB, Rotation::ZERO, BlockType::SWITCH));
	ASSERT_TRUE(parentCircuit->tryInsertBlock(pICB, Rotation::ZERO, icBlockType));
	ASSERT_TRUE(parentCircuit->tryInsertBlock(pLightB, Rotation::ZERO, BlockType::LIGHT));

	ASSERT_TRUE(parentCircuit->tryCreateConnection(pSwitchA, pICA));
	ASSERT_TRUE(parentCircuit->tryCreateConnection(pICA, pLightA));

	ASSERT_TRUE(parentCircuit->tryCreateConnection(pSwitchB, pICB));
	ASSERT_TRUE(parentCircuit->tryCreateConnection(pICB, pLightB));

	evaluator->setState(Address(pSwitchA), logic_state_t::HIGH);
	evaluator->setState(Address(pSwitchB), logic_state_t::LOW);
	EXPECT_EQ(evaluator->getState(Address(pLightA)), logic_state_t::HIGH);
	EXPECT_EQ(evaluator->getState(Address(pLightB)), logic_state_t::LOW);

	SharedCircuit child = environment.getBackend().getCircuit(icId);
	ASSERT_TRUE(child);
	ASSERT_TRUE(child->tryInsertBlock(Position(1, 0), Rotation::ZERO, BlockType::JUNCTION));
	ASSERT_TRUE(child->tryInsertBlock(Position(2, 0), Rotation::ZERO, BlockType::CONSTANT_ON));
	ASSERT_TRUE(child->tryCreateConnection(Position(2, 0), Position(1, 0)));

	CircuitManager& cm = environment.getBackend().getCircuitManager();
	CircuitBlockData* cbd = cm.getCircuitBlockDataManager().getCircuitBlockData(icId);
	ASSERT_TRUE(cbd);

	cbd->setConnectionIdPosition(connection_end_id_t(0), Position(1, 0));
	cbd->setConnectionIdPosition(connection_end_id_t(1), Position(1, 0));

	EXPECT_EQ(evaluator->getState(Address(pLightA)), logic_state_t::HIGH);
	EXPECT_EQ(evaluator->getState(Address(pLightB)), logic_state_t::UNDEFINED); // contention with switch B LOW and constant HIGH

	const Position movedICB = pICB + Vector(0, 3);
	ASSERT_TRUE(parentCircuit->tryMoveBlock(pICB, movedICB, Orientation())); // nothing should change
	pICB = movedICB;

	EXPECT_EQ(evaluator->getState(Address(pLightA)), logic_state_t::HIGH);
	EXPECT_EQ(evaluator->getState(Address(pLightB)), logic_state_t::UNDEFINED);
}
