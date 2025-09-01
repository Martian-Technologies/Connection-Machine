#ifndef mainRenderer_h
#define mainRenderer_h

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Vertex.h>
#include <glm/ext/vector_float2.hpp>

#include "backend/evaluator/evaluator.h"
#include "gui/sdl/sdlWindow.h"

#include "renderer/windowRenderer.h"

#include "mainRendererDefs.h"
#include "blockRenderDataManager.h"
#include "vulkanInstance.h"


class MainRenderer {
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
	void resizeWindow(WindowId windowId, glm::vec2 size);
	void deregisterWindow(WindowId windowId);

	// RmlUI ====================================================================================================================================
	// Prepare for Rml
	void prepareForRmlRender(WindowId windowId);
	void endRmlRender(WindowId windowId);

	// Geometry resources
	Rml::CompiledGeometryHandle compileGeometry(WindowId windowId, Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices);
	void releaseGeometry(WindowId windowId, Rml::CompiledGeometryHandle geometry);

	// Texture resources
	Rml::TextureHandle loadTexture(WindowId windowId, Rml::Vector2i& texture_dimensions, const Rml::String& source);
	Rml::TextureHandle generateTexture(WindowId windowId, Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions);
	void releaseTexture(WindowId windowId, Rml::TextureHandle texture_handle);

	// Rendering (per window)
	void renderGeometry(WindowId windowId, Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture);
	void enableScissorRegion(WindowId windowId, bool enable);
	void setScissorRegion(WindowId windowId, Rml::Rectanglei region);

	// Block Render Data ===============================================================================================================================
	BlockRenderDataId registerBlockRenderData();
	void deregisterBlockRenderData(BlockRenderDataId blockRenderDataId);
	void setBlockName(BlockRenderDataId blockRenderDataId, const std::string& blockName);
	void setBlockSize(BlockRenderDataId blockRenderDataId, Size size);
	void setBlockTextureIndex(BlockRenderDataId blockRenderDataId, unsigned int textureIndex);
	BlockPortRenderDataId addBlockPort(BlockRenderDataId blockRenderDataId, bool isInput, FVector positionOnBlock);
	void removeBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId);
	void moveBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, FVector newPositionOnBlock);
	void setBlockPortName(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, const std::string& newPortName);

	// Viewports ================================================================================================================================
	ViewportId registerViewport(WindowId windowId, glm::vec2 origin, glm::vec2 size, Rml::Element* element); // tmp element
	void moveViewport(ViewportId viewportId, WindowId windowId, glm::vec2 origin, glm::vec2 size);
	void moveViewportView(ViewportId viewportId, FPosition topLeft, FPosition bottomRight);
	void setViewportEvaluator(ViewportId viewportId, Evaluator* evaluator, Address address); // tmp circuit
	void deregisterViewport(ViewportId viewportId);

	// block and wires
	void startMakingEdits(ViewportId viewportId);
	void stopMakingEdits(ViewportId viewportId);
	void addBlock(ViewportId viewportId, BlockRenderDataId blockRenderDataId, Position position, Orientation orientation, Position statePosition);
	void removeBlock(ViewportId viewportId, Position position);
	void moveBlock(ViewportId viewportId, Position curPos, Position newPos, Orientation newOrientation);
	void addWire(ViewportId viewportId, std::pair<Position, Position> points, std::pair<FVector, FVector> socketOffsets);
	void removeWire(ViewportId viewportId, std::pair<Position, Position> points);
	void resetCircuit(ViewportId viewportId);
	void updateSimulatorIds(ViewportId viewportId, const std::vector<SimulatorMappingUpdate>& simulatorMappingUpdates);

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

private:
	inline WindowId getNewWindowId() { return ++lastWindowId; }
	inline ViewportId getNewViewportId() { return ++lastViewportId; }

	VulkanInstance vulkanInstance;

	WindowId lastWindowId = 0;
	ViewportId lastViewportId = 0;
	std::map<WindowId, WindowRenderer> windowRenderers;
	std::map<ViewportId, ViewportRenderData> viewportRenderers;
	BlockRenderDataManager blockRenderDataManager;
};


#endif /* mainRenderer_h */