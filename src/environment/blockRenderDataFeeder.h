#ifndef blockRenderDataFeeder_h
#define blockRenderDataFeeder_h

#include "gpu/blockRenderDataManager.h"
#include "gpu/cpuImageEditor/cpuImage.h"
#include "backend/container/block/blockDefs.h"
#include "backend/container/block/connectionEnd.h"
#include "backend/dataUpdateEventManager.h"
#include "blockTextureGenerator.h"

class Font;
class Backend;
class BlockData;

class BlockRenderDataFeeder {
public:
	BlockRenderDataFeeder(Backend& backend);

	BlockRenderDataId getBlockRenderDataId(BlockType blockType) const;

	void refreshBlockTexture(BlockType blockType);

	void doBlockTextureUpdates();

private:
	void newBlockTypeUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void postBlockSizeChangeUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void blockNameChangeUpdate(const DataUpdateEventManager::EventData* dataEvent);

	void blockDataSetConnectionUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void blockDataRemoveConnectionUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void blockDataConnectionNameSetUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void blockDataTextureChangeUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void blockDataUsesTileMapTextureChangeUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void blockDataTextureTileChangeUpdate(const DataUpdateEventManager::EventData* dataEvent);
	void updateImageIfNotSpecified(const DataUpdateEventManager::EventData* dataEvent);

	struct RenderData {
		RenderData(BlockRenderDataId blockRenderDataId) : blockRenderDataId(blockRenderDataId) {}
		BlockTextureId blockTextureId = 0;
		BlockRenderDataId blockRenderDataId = 0;
		std::map<connection_end_id_t, BlockPortRenderDataId> blockPortRenderDataIds;
	};

	std::set<BlockType> blockTexturesToUpdate;
	std::shared_ptr<Font> font;
	std::unique_ptr<BlockTextureGenerator> blockTextureGenerator;

	BlockTextureId getBlockTextureId(const BlockData* blockData, RenderData* renderData);

	std::unordered_map<BlockTextureId, unsigned int> blockTextureIdUsage;

	Backend& backend;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
	std::map<BlockType, RenderData> blockTypeToRenderData;

	std::pair<int, int> calculatePadding(int width, int height);
	CpuImage padTexture(const CpuImage& img);
};

#endif /* blockRenderDataFeeder_h */
