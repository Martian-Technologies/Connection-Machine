#ifndef mainRenderer_h
#define mainRenderer_h

#include <glm/ext/vector_float2.hpp>

#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "gpu/renderer/viewport/viewportRenderer.h"
#include "gui/sdl/sdlWindow.h"

#include "renderer/windowRenderer.h"

#include "mainRendererDefs.h"
#include "blockRenderDataManager.h"
#include "vulkanInstance.h"
#include "imageManager.h"

class MainRenderer {
	friend class ImageManager;
public:
	static MainRenderer& get();
	static void kill();

	// getters for internal objects
	const VulkanInstance& getVulkanInstance() const { return vulkanInstance; }
	VulkanInstance& getVulkanInstance() { return vulkanInstance; }
	const BlockRenderDataManager& getBlockRenderDataManager() const { return blockRenderDataManager; }
	BlockRenderDataManager& getBlockRenderDataManager() { return blockRenderDataManager; }

	// Windows ==================================================================================================================================
	WindowId registerWindow(SdlWindow* window);
	void resizeWindow(WindowId windowId, std::pair<uint32_t, uint32_t> size);
	void setCurrentlyRenderedWindow(WindowId windowId);
	void clearCurrentlyRenderedWindow();
	void deregisterWindow(WindowId windowId);

	// Block Render Data ===============================================================================================================================
	BlockRenderDataId registerBlockRenderData();
	void deregisterBlockRenderData(BlockRenderDataId blockRenderDataId);
	void setBlockName(BlockRenderDataId blockRenderDataId, const std::string& blockName);
	void setBlockSize(BlockRenderDataId blockRenderDataId, Size size);
	BlockTextureId addBlockTexture(const std::string& path);
	BlockTextureId addBlockTexture(const std::filesystem::path& path);
	void refreshBlockTexture(const std::string& path);
	BlockTextureId addBlockTexture(const unsigned char* pixels, int textureWidth, int textureHeight);
	void updateBlockTexture(const unsigned char* pixels, BlockTextureId blockTextureId);
	void removeBlockTexture(const std::string& path);
	void removeBlockTexture(BlockTextureId blockTextureId);
	void setBlockTexture(BlockRenderDataId blockRenderDataId, BlockTextureId blockTextureId);
	void setBlockTexture(BlockRenderDataId blockRenderDataId, BlockTextureId blockTextureId, Vec2Int smallestTextureCord, Vec2Int blockSize);
	void setBlockTexture(BlockRenderDataId blockRenderDataId, BlockTextureId blockTextureId, Vec2Int smallestTextureCord, Vec2Int blockSize, Vec2Int textureStepSize);
	BlockPortRenderDataId addBlockPort(BlockRenderDataId blockRenderDataId, bool isInput, FVector positionOnBlock);
	void removeBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId);
	void moveBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, FVector newPositionOnBlock);
	void setBlockPortName(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, const std::string& newPortName);
	void setTextureVirtualConnection(BlockRenderDataId blockRenderDataId, std::optional<virtual_connection_id_t> textureVirtualConnection);
	void regenerateAllChunksWithBlock(BlockRenderDataId blockRenderDataId);

	// Viewports ================================================================================================================================
	ViewportId registerViewport(WindowId windowId, glm::vec2 size);
	void resizeViewport(ViewportId viewportId, glm::vec2 size);
	void moveViewportView(ViewportId viewportId, FPosition topLeft, FPosition bottomRight);
	void setViewportSimulator(ViewportId viewportId, const EvalLogicSimulator* simulator, Address address); // tmp circuit
	vk::DescriptorSet startViewportRendering(ViewportId viewportId);
	// float getFps(ViewportId viewportId) const;
	void resetViewport(ViewportId viewportId);
	void deregisterViewport(ViewportId viewportId);

	// block and wires
	void startMakingEdits(ViewportId viewportId);
	void stopMakingEdits(ViewportId viewportId);
	void addBlock(ViewportId viewportId, BlockRenderDataId blockRenderDataId, Position position, Orientation orientation);
	void removeBlock(ViewportId viewportId, Position position);
	void moveBlock(ViewportId viewportId, Position curPos, Position newPos, Orientation newOrientation);
	void addWire(ViewportId viewportId, std::pair<Position, Position> points, std::pair<FVector, FVector> socketOffsets);
	void removeWire(ViewportId viewportId, std::pair<Position, Position> points);
	void resetCircuit(ViewportId viewportId);
	void updateSimulatorIds(ViewportId viewportId, const std::vector<SimulatorMappingUpdate>& simulatorMappingUpdates);

	vk::DescriptorSet getBlockTextureArrayLayer(WindowId windowId, unsigned int layerIndex);

	// elements
	ElementId addSelectionObjectElement(ViewportId viewportId, const SelectionObjectElement& selection);
	ElementId addSelectionElement(ViewportId viewportId, const SelectionElement& selection);
	void removeSelectionElement(ViewportId viewportId, ElementId id);

	ElementId addBlockPreview(ViewportId viewportId, BlockPreview&& blockPreview);
	void shiftBlockPreview(ViewportId viewportId, ElementId id, Vector shift);
	void removeBlockPreview(ViewportId viewportId, ElementId id);

	ElementId addConnectionPreview(ViewportId viewportId, const ConnectionPreview& connectionPreview);
	void removeConnectionPreview(ViewportId viewportId, ElementId id);

	ElementId addHalfConnectionPreview(ViewportId viewportId, const HalfConnectionPreview& halfConnectionPreview);
	void removeHalfConnectionPreview(ViewportId viewportId, ElementId id);

	ElementId addTextOnViewport(ViewportId viewportId, const TextRenderData& textRenderData);
	void removeTextOnViewport(ViewportId viewportId, ElementId id);
	const std::unordered_map<ElementId, TextRenderData>* getTextOnViewport(ViewportId viewportId) const;

	// images
	vk::DescriptorSet getImage(const std::string& path);
private:
	inline WindowId getNewWindowId() { return ++lastWindowId; }
	inline ViewportId getNewViewportId() { return ++lastViewportId; }

	VulkanInstance vulkanInstance;

	WindowId currentlyRenderedWindow = 0;
	WindowId lastWindowId = 0;
	ViewportId lastViewportId = 0;
	std::map<WindowId, WindowRenderer> windowRenderers;
	std::map<ViewportId, ViewportRenderer> viewportRenderers;
	BlockRenderDataManager blockRenderDataManager;
	ImageManager imageManager;
};


#endif /* mainRenderer_h */
