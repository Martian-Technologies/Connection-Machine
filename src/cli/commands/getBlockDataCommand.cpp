#include "getBlockDataCommand.h"

#include "environment/environment.h"
#include "../commandManager.h"
#include "util/runAtStartup.h"

runAtStartup(CommandManager::get().registerCommand(std::make_unique<GetBlockDataCommand>());)

void GetBlockDataCommand::run(const std::vector<std::string>& args, Environment& environment) {
    if (args.size() != 4) {
  		logError("Wrong number of arguments passed to get_block_data. Proper usage is 'get_block_data {ID} {x} {y}'.", "GetBlockDataCommand");
	  	return;
    }

    int xPos;
    int yPos;
    circuit_id_t cirID;
    try {
        xPos = std::stoi(args[2]);
        yPos = std::stoi(args[3]);
        cirID = std::stoi(args[1]);
    }
    catch (...) {
        logError("Exception occured. Check your arguments, they should be reasonably-sized integers.", "GetBlockDataCommand");
        return;
    }

    SharedCircuit cir = environment.getBackend().getCircuitManager().getCircuit(cirID);
    if (cir == nullptr) {
        logError("Unrecognized circuit ID. Available circuits can be found with the 'list_circuits' command.", "GetBlockDataCommand");
        return;
    }

    const BlockContainer* blockContainer = cir->getBlockContainer();
    Position pos = Position(xPos, yPos);
    const Block* block = blockContainer->getBlock(pos);
    if (block == nullptr) {
        logInfo("No block at position {}.", "GetBlockDataCommand", pos);
		return;
    }

    const BlockDataManager* blockDataManager = environment.getBackend().getBlockDataManager();
    const BlockData* blockData = blockDataManager->getBlockData(block->type());
    logInfo("Info for block at position {}", "GetBlockDataCommand", pos);
    logInfo("Type: {}, ID: {}", "GetBlockDataCommand", blockDataManager->getName(block->type()), block->id());
    logInfo("Connections:", "GetBlockDataCommand");
    const auto connections = block->getConnectionContainer().getConnections();
    for (auto portIter = connections.begin(); portIter != connections.end(); portIter++) {
        logInfo("{} (ID: {})>:", "GetBlockDataCommand", blockData->getConnectionIdToName(portIter->first).value_or("Port"), portIter->first);
        for (auto connsIter = portIter->second.begin(); connsIter != portIter->second.end(); connsIter++) {
            const Block* connBlock = blockContainer->getBlock(connsIter->getBlockId());
            logInfo("\t>{}:{} (ID: {}) @ {}", "GetBlockDataCommand", blockData->getConnectionIdToName(connsIter->getConnectionId()).value_or("Port"), blockDataManager->getName(connBlock->type()), connsIter->getBlockId(), connBlock->getPosition());
        }

    }

}