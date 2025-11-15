#include "copiedBlocks.h"

#include "blockContainer.h"

CopiedBlocks::CopiedBlocks(const BlockContainer& blockContainer, SharedSelection selection) {
	std::unordered_set<Position> positions;
	std::unordered_set<const Block*> blocksSet;
	bool foundPos = false;
	flattenSelection(selection, positions);
	for (Position position : positions) {
		const Block* block = blockContainer.getBlock(position);
		if (!block) continue;
		if (foundPos) {
			if (minPosition.x > position.x) minPosition.x = position.x;
			else if (maxPosition.x > position.x) maxPosition.x = position.x;
			if (minPosition.y > position.y) minPosition.y = position.y;
			else if (maxPosition.y > position.y) maxPosition.y = position.y;
		} else {
			minPosition = maxPosition = position;
		}
		if (blocksSet.contains(block)) continue;
		blocksSet.insert(block);
		blocks.emplace_back(
			block->type(),
			block->getPosition(),
			block->getOrientation()
		);
		const BlockData* blockData = blockContainer.getBlockDataManager().getBlockData(block->type());
		for (auto& iter : block->getConnectionContainer().getConnections()) {
			std::optional<Vector> connectionVector = blockData->getConnectionVector(iter.first, block->getOrientation());
			if (!connectionVector) continue;
			Position connectionPosition = block->getPosition() + connectionVector.value();
			BlockData::ConnectionData::PortType portType = blockData->getConnectionPortType(iter.first);
			auto otherConnections = block->getConnectionContainer().getConnections(iter.first);
			if (!otherConnections) continue;
			for (ConnectionEnd connectionEnd : *otherConnections) {
				const Block* otherBlock = blockContainer.getBlock(connectionEnd.getBlockId());
				if (!otherBlock) continue;
				bool skipConnection = true;
				for (Position::Iterator iter = otherBlock->getPosition().iterTo(otherBlock->getLargestPosition()); iter; iter++) {
					if (positions.contains(*iter)) { skipConnection = false; break; }
				}
				if (skipConnection) continue;
				const BlockData* otherBlockData = blockContainer.getBlockDataManager().getBlockData(otherBlock->type());
				std::optional<Vector> otherConnectionVector = otherBlockData->getConnectionVector(
					connectionEnd.getConnectionId(), otherBlock->getOrientation()
				);
				if (!otherConnectionVector) continue;
				Position otherConnectionPosition = otherBlock->getPosition() + otherConnectionVector.value();
				if (portType == BlockData::ConnectionData::PortType::INPUT) connections.emplace_back(connectionPosition, otherConnectionPosition);
				else if (portType == BlockData::ConnectionData::PortType::BIDIRECTIONAL) {
					if (otherBlockData->isConnectionOutputOrBidirectional(connectionEnd.getConnectionId())) {
						connections.emplace_back(connectionPosition, otherConnectionPosition);
					}
				}
				// else connections.emplace_back(otherConnectionPosition, connectionPosition);
			}
		}
	}
	logInfo("Copied {} blocks", "CopiedBlocks", blocks.size());
}

nlohmann::json CopiedBlocks::dumpState() const {
	nlohmann::json stateJson;
	stateJson["minPosition"] = minPosition.toString();
	stateJson["maxPosition"] = maxPosition.toString();
	stateJson["blocks"] = nlohmann::json::array();
	for (const CopiedBlockData& blockData : blocks) {
		stateJson["blocks"].push_back(blockData.dumpState());
	}
	stateJson["connections"] = nlohmann::json::array();
	for (const auto& [fromPosition, toPosition] : connections) {
		nlohmann::json connectionJson;
		connectionJson["from"] = fromPosition.toString();
		connectionJson["to"] = toPosition.toString();
		stateJson["connections"].push_back(connectionJson);
	}
	return stateJson;
}
