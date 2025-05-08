#include "circuitTests.h"
#include "util/uuid.h"

void CircuitTest::SetUp() {
	circuit_id_t circuitId = circuitManager.createNewCircuit("Circuit", generate_uuid_v4());
	circuit = circuitManager.getCircuit(circuitId);
	i = 0;
}

void CircuitTest::TearDown() {
	circuit.reset();
}

TEST_F(CircuitTest, BlockContainerBasicOperations) {
	Position pos(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	bool success = circuit->tryInsertBlock(pos, rot, BlockType::AND);
	ASSERT_TRUE(success);

	const BlockContainer* container = circuit->getBlockContainer();

	const Block* block = container->getBlock(pos);
	ASSERT_TRUE(block != nullptr);
	ASSERT_EQ(block->type(), BlockType::AND);

	// Test block by id access
	const Block* blockById = container->getBlock(block->id());
	ASSERT_TRUE(blockById != nullptr);
	ASSERT_EQ(blockById, block);
}

TEST_F(CircuitTest, BlockPlacementCollision) {
	Position pos(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	bool success = circuit->tryInsertBlock(pos, rot, BlockType::AND);
	bool failure = circuit->tryInsertBlock(pos, rot, BlockType::OR);
	ASSERT_TRUE(success);
	ASSERT_FALSE(failure);

	const Block* block = circuit->getBlockContainer()->getBlock(pos);
	ASSERT_TRUE(block != nullptr);
	ASSERT_EQ(block->type(), BlockType::AND);
}

TEST_F(CircuitTest, ConnectionCreation) {
	Position pos1(i, i); ++i;
	Position pos2(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	bool insert_one = circuit->tryInsertBlock(pos1, rot, BlockType::AND);
	bool insert_two = circuit->tryInsertBlock(pos2, rot, BlockType::OR);
	ASSERT_TRUE(insert_one && insert_two);

	bool connected = circuit->tryCreateConnection(pos1, pos2);
	ASSERT_TRUE(connected);

	const BlockContainer* container = circuit->getBlockContainer();

	//const Block* block_one = container->getBlock(pos1);
	//auto [outputConnectionId, outputSuccess] = block_one->getOutputConnectionId(pos1);
	//ASSERT_TRUE(outputSuccess);

	//const Block* block_two = container->getBlock(pos2);
	//auto [inputConnectionId, inputSuccess] = block_two->getInputConnectionId(pos2);
	//ASSERT_TRUE(inputSuccess);

	//ASSERT_TRUE(block_two->getConnectionContainer().hasConnection(inputConnectionId, ConnectionEnd(block_one->id(), outputConnectionId)));

	bool valid_connection = container->connectionExists(pos1, pos2);
	bool invalid_connection = container->connectionExists(pos2, pos1); // transitive
	ASSERT_TRUE(valid_connection);
	ASSERT_FALSE(invalid_connection);
}

TEST_F(CircuitTest, ConnectionCreationConnectionEnd) {
	Position pos1(i, i); ++i;
	Position pos2(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	bool insert_one = circuit->tryInsertBlock(pos1, rot, BlockType::AND);
	bool insert_two = circuit->tryInsertBlock(pos2, rot, BlockType::OR);
	ASSERT_TRUE(insert_one && insert_two);

	const BlockContainer* container = circuit->getBlockContainer();

	bool connected = circuit->tryCreateConnection(
		ConnectionEnd(container->getBlock(pos1)->id(), container->getBlock(pos1)->getOutputConnectionId(pos1).first),
		ConnectionEnd(container->getBlock(pos2)->id(), container->getBlock(pos2)->getInputConnectionId(pos2).first)
	);
	ASSERT_TRUE(connected);

	bool valid_connection = container->connectionExists(pos1, pos2);
	bool invalid_connection = container->connectionExists(pos2, pos1); // transitive
	ASSERT_TRUE(valid_connection);
	ASSERT_FALSE(invalid_connection);
}

TEST_F(CircuitTest, InvalidConnections) {
	Position pos1(i, i); ++i;
	Position pos2(i, i); ++i;
	Position nonExistent(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	circuit->tryInsertBlock(pos1, rot, BlockType::AND);
	circuit->tryInsertBlock(pos2, rot, BlockType::AND);

	// test self-connection
	bool selfConnect = circuit->tryCreateConnection(pos1, pos1);
	ASSERT_TRUE(selfConnect);

	// test connection to a not yet inserted block on the circuit
	bool nonExistentConnect = circuit->tryCreateConnection(pos1, nonExistent);
	ASSERT_FALSE(nonExistentConnect);

	const BlockContainer* container = circuit->getBlockContainer();
	ASSERT_TRUE(container->connectionExists(pos1, pos1));
	ASSERT_FALSE(container->connectionExists(pos1, nonExistent));
}

TEST_F(CircuitTest, BlockRemoval) {
	Position pos(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	circuit->tryInsertBlock(pos, rot, BlockType::AND);
	const Block* block = circuit->getBlockContainer()->getBlock(pos);
	ASSERT_TRUE(block != nullptr);

	circuit->tryRemoveBlock(pos);

	const Block* removedBlock = circuit->getBlockContainer()->getBlock(pos);
	ASSERT_TRUE(removedBlock == nullptr);
}

TEST_F(CircuitTest, ConnectionRemoval) {
	Position pos1(i, i); ++i;
	Position pos2(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	circuit->tryInsertBlock(pos1, rot, BlockType::AND);
	circuit->tryInsertBlock(pos2, rot, BlockType::AND);
	circuit->tryCreateConnection(pos1, pos2);

	bool removed = circuit->tryRemoveConnection(pos1, pos2);
	ASSERT_TRUE(removed);

	const BlockContainer* container = circuit->getBlockContainer();
	ASSERT_FALSE(container->connectionExists(pos1, pos2));
}

TEST_F(CircuitTest, BlockTypePlacement) {
	Position pos(i, i); ++i;
	Rotation rot = Rotation::ZERO;
	BlockType type = (BlockType)(i % 100);

	bool success = circuit->tryInsertBlock(pos, rot, type);
	if (!(circuitManager.getBlockDataManager()->blockExists(type))) {
		ASSERT_FALSE(success);
		const Block* block = circuit->getBlockContainer()->getBlock(pos);
		ASSERT_EQ(block, nullptr);
	} else {
		ASSERT_TRUE(success);
		const Block* block = circuit->getBlockContainer()->getBlock(pos);
		ASSERT_NE(block, nullptr);
		ASSERT_EQ(block->type(), type);
	}
}

TEST_F(CircuitTest, ConnectionRemovalConnectionEnd) {
	Position pos1(i, i); ++i;
	Position pos2(i, i); ++i;
	Rotation rot = Rotation::ZERO;

	circuit->tryInsertBlock(pos1, rot, BlockType::AND);
	circuit->tryInsertBlock(pos2, rot, BlockType::AND);
	circuit->tryCreateConnection(pos1, pos2);

	const BlockContainer* container = circuit->getBlockContainer();

	bool removed = circuit->tryRemoveConnection(
		ConnectionEnd(container->getBlock(pos1)->id(), container->getBlock(pos1)->getOutputConnectionId(pos1).first),
		ConnectionEnd(container->getBlock(pos2)->id(), container->getBlock(pos2)->getInputConnectionId(pos2).first)
	);
	ASSERT_TRUE(removed);

	ASSERT_FALSE(container->connectionExists(pos1, pos2));
}

TEST_F(CircuitTest, CircuitPlacement) {


	circuit_id_t circuitId = circuitManager.createNewCircuit(generate_uuid_v4(), "Circuit");
	SharedCircuit circuit2 = circuitManager.getCircuit(circuitId);
	const BlockType blockType = circuitManager.setupBlockData(circuitId);


	ASSERT_NE(blockType, NONE);
	ASSERT_TRUE(circuitManager.getBlockDataManager()->blockExists(blockType));
	circuitManager.getBlockDataManager()->getBlockData(blockType)->setSize(Vector(2, 2));
	ASSERT_TRUE(circuit->tryInsertBlock(Position(), Rotation::ZERO, blockType));


	const Block* block1 = circuit->getBlockContainer()->getBlock(Position());
	const Block* block2 = circuit->getBlockContainer()->getBlock(Position(0, 1));
	const Block* block3 = circuit->getBlockContainer()->getBlock(Position(1, 0));
	const Block* block4 = circuit->getBlockContainer()->getBlock(Position(1, 1));
	ASSERT_NE(block1, nullptr);
	ASSERT_EQ(block1->type(), blockType);
	ASSERT_TRUE(block1 == block2 || block1 == block3 || block1 == block4);
}