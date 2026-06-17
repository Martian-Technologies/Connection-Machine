#ifndef blockRenderDataFeeder_h
#define blockRenderDataFeeder_h

#include "gpu/cpuImageEditor/cpuImage.h"
#include "backend/container/block/blockDefs.h"
#include "backend/container/block/connectionEnd.h"
#include "backend/dataUpdateEventManager.h"
#include "blockTextureGenerator.h"

typedef uint32_t BlockRenderDataId;
typedef uint32_t BlockPortRenderDataId;
typedef uint32_t BlockTextureId;

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
	void newBlockType_event(const DataUpdateEventManager::EventData* event);
	void postBlockSizeChange_event(const DataUpdateEventManager::EventData* event);
	void blockNameChange_event(const DataUpdateEventManager::EventData* event);

	void blockDataSetConnection_event(const DataUpdateEventManager::EventData* event);
	void blockDataPortBitConfigurationSet_event(const DataUpdateEventManager::EventData* event);
	void blockDataRemoveConnection_event(const DataUpdateEventManager::EventData* event);
	void blockDataConnectionNameSet_event(const DataUpdateEventManager::EventData* event);

	void blockDataTexturePathChange_event(const DataUpdateEventManager::EventData* event);
	void blockDataTextureUseFullTextureChange_event(const DataUpdateEventManager::EventData* event);
	void blockDataTextureTopLeftChange_event(const DataUpdateEventManager::EventData* event);
	void blockDataTextureSizeChange_event(const DataUpdateEventManager::EventData* event);
	void blockDataTextureRenderStateChange_event(const DataUpdateEventManager::EventData* event);
	void blockDataTextureVirtualConnectionChange_event(const DataUpdateEventManager::EventData* event);
	void blockDataTextureStateOffsetChange_event(const DataUpdateEventManager::EventData* event);

	struct RenderData {
		RenderData(BlockRenderDataId blockRenderDataId) : blockRenderDataId(blockRenderDataId) {}
		BlockTextureId blockTextureId = 0;
		BlockRenderDataId blockRenderDataId = 0;
		std::map<connection_end_id_t, BlockPortRenderDataId> blockPortRenderDataIds;
	};

	std::set<BlockType> blockTexturesToUpdate;
	std::shared_ptr<Font> font;
	std::unique_ptr<BlockTextureGenerator> blockTextureGenerator;

	BlockTextureId getBlockTextureId(const BlockData& blockData, RenderData& renderData);

	std::unordered_map<BlockTextureId, unsigned int> blockTextureIdUsage;

	Backend& backend;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
	std::map<BlockType, RenderData> blockTypeToRenderData;

	std::pair<int, int> calculatePadding(int width, int height);
	CpuImage padTexture(const CpuImage& img);
};

#endif /* blockRenderDataFeeder_h */
