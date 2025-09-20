#include "evaluatorICTest.h"

void EvaluatorICTest::SetUp() {
    circuit_id_t circuitId = backend.createCircuit();
    parentCircuit = backend.getCircuit(circuitId);
    auto evalId = backend.createEvaluator(circuitId);
    ASSERT_TRUE(evalId.has_value());
    evaluator = backend.getEvaluator(evalId.value());
    idx = 0;
}

void EvaluatorICTest::TearDown() {
    parentCircuit.reset();
    evaluator.reset();
}

circuit_id_t EvaluatorICTest::createPassThroughIC(const std::string& name) {
    circuit_id_t childId = backend.createCircuit(name);
    SharedCircuit child = backend.getCircuit(childId);

    child->tryInsertBlock(Position(0, 0), Rotation::ZERO, BlockType::JUNCTION);

    CircuitManager& cm = backend.getCircuitManager();
    BlockType icType = cm.setupBlockData(childId);

    BlockData* bd = cm.getBlockDataManager()->getBlockData(icType);
    bd->setDefaultData(false);
    bd->setPrimitive(false);
    bd->setPath("Custom");
    bd->setSize(Size(1, 1));

    bd->setConnectionInput(Vector(0, 0), 0);
    bd->setConnectionOutput(Vector(0, 0), 1);

    CircuitBlockDataManager* cbdm = cm.getCircuitBlockDataManager();
    CircuitBlockData* cbd = cbdm->getCircuitBlockData(childId);

    cbd->setConnectionIdPosition(0, Position(0, 0));
    cbd->setConnectionIdPosition(1, Position(0, 0));

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

    const circuit_id_t outerICId = backend.createCircuit("OuterPassThrough");
    SharedCircuit outerCircuit = backend.getCircuit(outerICId);
    ASSERT_TRUE(outerCircuit);

    ASSERT_TRUE(outerCircuit->tryInsertBlock(Position(0, 0), Rotation::ZERO, innerICType));

    CircuitManager& cm = backend.getCircuitManager();
    BlockType outerICType = cm.setupBlockData(outerICId);
    ASSERT_NE(outerICType, BlockType::NONE);
    BlockData* bd = cm.getBlockDataManager()->getBlockData(outerICType);
    ASSERT_TRUE(bd);
    bd->setDefaultData(false);
    bd->setPrimitive(false);
    bd->setPath("Custom");
    bd->setSize(Size(1, 1));
    bd->setConnectionInput(Vector(0, 0), 0);
    bd->setConnectionOutput(Vector(0, 0), 1);

    CircuitBlockData* cbd = cm.getCircuitBlockDataManager()->getCircuitBlockData(outerICId);
    ASSERT_TRUE(cbd);
    cbd->setConnectionIdPosition(0, Position(0, 0));
    cbd->setConnectionIdPosition(1, Position(0, 0));

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
    CircuitManager& cm = backend.getCircuitManager();
    CircuitBlockData* cbd = cm.getCircuitBlockDataManager()->getCircuitBlockData(icId);
    cbd->setConnectionIdPosition(0, Position(1, 1));
    cbd->setConnectionIdPosition(1, Position(1, 1));

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

    cbd->setConnectionIdPosition(0, Position(0, 0));
    cbd->setConnectionIdPosition(1, Position(0, 0));

    evaluator->setState(Address(pSwitch), logic_state_t::HIGH);
    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::HIGH);

    evaluator->setState(Address(pSwitch), logic_state_t::LOW);
    EXPECT_EQ(evaluator->getState(Address(pLight)), logic_state_t::LOW);
}
