#ifndef blockRenderDataFeeder_h
#define blockRenderDataFeeder_h

#include "gpu/blockRenderDataManager.h"
#include "backend/container/block/blockDefs.h"
#include "backend/container/block/connectionEnd.h"
#include "backend/dataUpdateEventManager.h"

class Backend;

class BlockRenderDataFeeder {
public:
	BlockRenderDataFeeder(Backend* backend);

	BlockRenderDataId getBlockRenderDataId(BlockType blockType) const;

	void newBlockTypeUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void postBlockSizeChangeUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void blockNameChangeUpdate(const DataUpdateEventManager::EventData* dataEvent);

	void blockDataSetConnectionUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void blockDataRemoveConnectionUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void blockDataConnectionNameSetUpdate(const DataUpdateEventManager::EventData* dataEvent);

private:
	struct RenderIdData {
		RenderIdData(BlockRenderDataId blockRenderDataId) : blockRenderDataId(blockRenderDataId) {}
		BlockRenderDataId blockRenderDataId;
		std::map<connection_end_id_t, BlockPortRenderDataId> blockPortRenderDataIds;
	};

	Backend* backend;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
	std::map<BlockType, RenderIdData> blockTypeToRenderIdData;
};

#endif /* blockRenderDataFeeder_h */
