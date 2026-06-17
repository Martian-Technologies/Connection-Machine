#include "mainRenderer.h"
#include "renderer/imgui/imGuiRenderer.h"

std::optional<MainRenderer> mainRendererSingleton;

MainRenderer& MainRenderer::get() {
	if (!mainRendererSingleton) {
		logInfo("Creating MainRenderer", "MainRenderer");
		mainRendererSingleton.emplace();
		logInfo("MainRenderer created", "MainRenderer");
	};
	return *mainRendererSingleton;
}

void MainRenderer::kill() {
	logInfo("Killing MainRenderer", "MainRenderer");
	mainRendererSingleton.reset();
}

WindowId MainRenderer::registerWindow(SdlWindow* sdlWindow) {
	WindowId windowId = getNewWindowId();
	windowRenderers.try_emplace(windowId, windowId, sdlWindow);
	return windowId;
}

void MainRenderer::resizeWindow(WindowId windowId, std::pair<uint32_t, uint32_t> size) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call resizeWindow on non existent window {}.", "MainRenderer", windowId);
		return;
	}
	iter->second.resize(size);
}

void MainRenderer::setCurrentlyRenderedWindow(WindowId windowId) {
	assert(currentlyRenderedWindow == 0);
	currentlyRenderedWindow = windowId;
}

void MainRenderer::clearCurrentlyRenderedWindow() {
	currentlyRenderedWindow = 0;
}

void MainRenderer::deregisterWindow(WindowId windowId) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call deregisterWindow on non existent window {}.", "MainRenderer", windowId);
		return;
	}
	imageManager.clearWindow(iter->first);
	windowRenderers.erase(iter);
}

BlockRenderDataId MainRenderer::registerBlockRenderData() {
	return blockRenderDataManager.addBlockRenderData();
}

void MainRenderer::deregisterBlockRenderData(BlockRenderDataId blockRenderDataId) {
	blockRenderDataManager.removeBlockRenderData(blockRenderDataId);
}

void MainRenderer::setBlockName(BlockRenderDataId blockRenderDataId, const std::string& blockName) {
	blockRenderDataManager.setBlockName(blockRenderDataId, blockName);
}

void MainRenderer::setBlockSize(BlockRenderDataId blockRenderDataId, Size size) {
	blockRenderDataManager.setBlockSize(blockRenderDataId, size);
}

BlockTextureId MainRenderer::addBlockTexture(const std::string& path) {
	return vulkanInstance.getDevice().getBlockTextureManager().addTexture(path);
}

BlockTextureId MainRenderer::addBlockTexture(const std::filesystem::path& path) {
	return vulkanInstance.getDevice().getBlockTextureManager().addTexture(path.string());
}

void MainRenderer::refreshBlockTexture(const std::string& path) {
	vulkanInstance.getDevice().getBlockTextureManager().refreshBlockTexture(path);
}

BlockTextureId MainRenderer::addBlockTexture(const unsigned char* pixels, int textureWidth, int textureHeight) {
	return vulkanInstance.getDevice().getBlockTextureManager().addTexture(pixels, textureWidth, textureHeight);
}

void MainRenderer::updateBlockTexture(const unsigned char* pixels, BlockTextureId blockTextureId) {
	vulkanInstance.getDevice().getBlockTextureManager().updateBlockTexture(pixels, blockTextureId);
}

void MainRenderer::removeBlockTexture(const std::string& path) {
	vulkanInstance.getDevice().getBlockTextureManager().removeBlockTexture(path);
}

void MainRenderer::removeBlockTexture(BlockTextureId blockTextureId) {
	vulkanInstance.getDevice().getBlockTextureManager().removeBlockTexture(blockTextureId);
}

void MainRenderer::setBlockTexture(BlockRenderDataId blockRenderDataId, BlockTextureId blockTextureId) {
	blockRenderDataManager.setBlockTexture(blockRenderDataId, blockTextureId);
}

void MainRenderer::setBlockTexture(BlockRenderDataId blockRenderDataId, BlockTextureId blockTextureId, Vec2Int smallestTextureCord, Vec2Int textureSize) {
	blockRenderDataManager.setBlockTexture(blockRenderDataId, blockTextureId, smallestTextureCord, textureSize);
}

void MainRenderer::setBlockTexture(BlockRenderDataId blockRenderDataId, BlockTextureId blockTextureId, Vec2Int smallestTextureCord, Vec2Int textureSize, Vec2Int textureStepSize) {
	blockRenderDataManager.setBlockTexture(blockRenderDataId, blockTextureId, smallestTextureCord, textureSize, textureStepSize);
}

BlockPortRenderDataId MainRenderer::addBlockPort(BlockRenderDataId blockRenderDataId, bool isInput, FVector positionOnBlock) {
	return blockRenderDataManager.addBlockPort(blockRenderDataId, isInput, positionOnBlock);
}

void MainRenderer::removeBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId) {
	blockRenderDataManager.removeBlockPort(blockRenderDataId, blockPortRenderDataId);
}

void MainRenderer::moveBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, FVector newPositionOnBlock) {
	blockRenderDataManager.moveBlockPort(blockRenderDataId, blockPortRenderDataId, newPositionOnBlock);
}

void MainRenderer::setBlockPortName(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, const std::string& newPortName) {
	blockRenderDataManager.setBlockPortName(blockRenderDataId, blockPortRenderDataId, newPortName);
}

void MainRenderer::setTextureVirtualConnection(BlockRenderDataId blockRenderDataId, std::optional<virtual_connection_id_t> textureVirtualConnection) {
	blockRenderDataManager.setTextureVirtualConnection(blockRenderDataId, textureVirtualConnection);
}

void MainRenderer::regenerateAllChunksWithBlock(BlockRenderDataId blockRenderDataId) {
	for (std::pair<const unsigned int, ViewportRenderer>& pair : viewportRenderers) {
		pair.second.getChunker().regenerateAllChunksWithBlock(blockRenderDataId);
	}
}

// window id is needed because we need to have it work with a particularly imgui context
ViewportId MainRenderer::registerViewport(WindowId windowId, glm::vec2 size) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call registerViewport on non existent window {}.", "MainRenderer", windowId);
		return 0;
	}
	viewportRenderers.try_emplace(getNewViewportId(), getVulkanInstance().getDevice(), iter->second.getImGuiRenderer());
	return lastViewportId;
}

void MainRenderer::resizeViewport(ViewportId viewportId, glm::vec2 size) {
	auto viewportIter = viewportRenderers.find(viewportId);
	if (viewportIter == viewportRenderers.end()) {
		logError("Failed to call moveViewport on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	// auto windowIter = windowRenderers.find(windowId);
	// if (windowIter == windowRenderers.end()) {
	// 	logError("Failed to call moveViewport on non existent window {}", "MainRenderer", windowId);
	// 	return;
	// }
	// if (! windowIter->second.hasViewportRenderer(&(viewportIter->second))) {
	// 	logError("moving viewport to other window not supported yet");
	// 	return;
	// }
	viewportIter->second.updateViewFrame({std::max(size.x, 1.f), std::max(size.y, 1.f)});
}

void MainRenderer::moveViewportView(ViewportId viewportId, FPosition topLeft, FPosition bottomRight) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call moveViewportView on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.updateView(topLeft, bottomRight);
}

void MainRenderer::setViewportSimulator(ViewportId viewportId, const EvalLogicSimulator* simulator, Address address) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call setViewportSimulator on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.setSimulator(simulator, address);
}

vk::DescriptorSet MainRenderer::startViewportRendering(ViewportId viewportId) {
	if (currentlyRenderedWindow == 0) {
		logError("Can't start viewport rendering when no window is rendering.");
		return {};
	}
	auto windowsIter = windowRenderers.find(currentlyRenderedWindow);
	assert(windowsIter != windowRenderers.end());
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call get_viewport_latest_image on non existent viewport {}", "MainRenderer", viewportId);
		return {};
	}
	auto [vkDescriptorSet, vkSemaphore, lifetimeObjects] = iter->second.startImageRender();
	windowsIter->second.addSemaphore(vkSemaphore, lifetimeObjects);
	return vkDescriptorSet;
}

// float MainRenderer::getFps(ViewportId viewportId) const {
// 	auto iter = viewportRenderers.find(viewportId);
// 	if (iter == viewportRenderers.end()) {
// 		logError("Failed to call getFps on non existent viewport {}", "MainRenderer", viewportId);
// 		return 0;
// 	}
// 	return iter->second.getFps();
// }

void MainRenderer::resetViewport(ViewportId viewportId) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call setViewportSimulator on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().reset();
	iter->second.setSimulator(nullptr, Address());
}

void MainRenderer::deregisterViewport(ViewportId viewport) {
	viewportRenderers.erase(viewport);
}

void MainRenderer::startMakingEdits(ViewportId viewportId) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call startMakingEdits on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().startMakingEdits();
}

void MainRenderer::stopMakingEdits(ViewportId viewportId) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call startMakingEdits on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().stopMakingEdits();
}

void MainRenderer::addBlock(ViewportId viewportId, BlockRenderDataId blockRenderDataId, Position position, Orientation orientation) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call startMakingEdits on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().addBlock(blockRenderDataId, position, orientation);
}

void MainRenderer::removeBlock(ViewportId viewportId, Position position) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call removeBlock on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().removeBlock(position);
}

void MainRenderer::moveBlock(ViewportId viewportId, Position curPos, Position newPos, Orientation newOrientation) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call moveBlock on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().moveBlock(curPos, newPos, newOrientation);
}

void MainRenderer::addWire(ViewportId viewportId, std::pair<Position, Position> points, std::pair<FVector, FVector> socketOffsets) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call addWire on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().addWire(points, socketOffsets);
}

void MainRenderer::removeWire(ViewportId viewportId, std::pair<Position, Position> points) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call removeWire on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().removeWire(points);
}

void MainRenderer::resetCircuit(ViewportId viewportId) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call reset on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().reset();
}

void MainRenderer::updateSimulatorIds(ViewportId viewportId, const std::vector<SimulatorMappingUpdate>& simulatorMappingUpdates) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call updateSimulatorIds on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().updateSimulatorIds(simulatorMappingUpdates);
}

vk::DescriptorSet MainRenderer::getBlockTextureArrayLayer(WindowId windowId, unsigned int layerIndex) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call getBlockTextureArrayLayer on non existent window {}.", "MainRenderer", windowId);
		return {};
	}
	auto [descriptorSet, lifetimeObjects] = iter->second.getBlockTextureArrayLayer_ImGui(layerIndex);
	iter->second.addLifetimeObjects(lifetimeObjects);
	return descriptorSet;
}


ElementId MainRenderer::addSelectionObjectElement(ViewportId viewportId, const SelectionObjectElement& selection) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call addSelectionObjectElement on non existent viewport {}", "MainRenderer", viewportId);
		return 0;
	}
	return iter->second.addSelectionObjectElement(selection);
}

ElementId MainRenderer::addSelectionElement(ViewportId viewportId, const SelectionElement& selection) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call addSelectionElement on non existent viewport {}", "MainRenderer", viewportId);
		return 0;
	}
	return iter->second.addSelectionElement(selection);
}

void MainRenderer::removeSelectionElement(ViewportId viewportId, ElementId id) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call removeSelectionElement on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.removeSelectionElement(id);
}

ElementId MainRenderer::addBlockPreview(ViewportId viewportId, BlockPreview&& blockPreview) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call addBlockPreview on non existent viewport {}", "MainRenderer", viewportId);
		return 0;
	}
	return iter->second.addBlockPreview(std::move(blockPreview));
}

void MainRenderer::shiftBlockPreview(ViewportId viewportId, ElementId id, Vector shift) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call shiftBlockPreview on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.shiftBlockPreview(id, shift);
}

void MainRenderer::removeBlockPreview(ViewportId viewportId, ElementId id) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call removeBlockPreview on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.removeBlockPreview(id);
}

ElementId MainRenderer::addConnectionPreview(ViewportId viewportId, const ConnectionPreview& connectionPreview) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call addConnectionPreview on non existent viewport {}", "MainRenderer", viewportId);
		return 0;
	}
	return iter->second.addConnectionPreview(connectionPreview);
}

void MainRenderer::removeConnectionPreview(ViewportId viewportId, ElementId id) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call removeConnectionPreview on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.removeConnectionPreview(id);
}

ElementId MainRenderer::addHalfConnectionPreview(ViewportId viewportId, const HalfConnectionPreview& halfConnectionPreview) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call addHalfConnectionPreview on non existent viewport {}", "MainRenderer", viewportId);
		return 0;
	}
	return iter->second.addHalfConnectionPreview(halfConnectionPreview);
}

void MainRenderer::removeHalfConnectionPreview(ViewportId viewportId, ElementId id) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call removeHalfConnectionPreview on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.removeHalfConnectionPreview(id);
}

ElementId MainRenderer::addTextOnViewport(ViewportId viewportId, const TextRenderData& textRenderData) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call addTextOnViewport on non existent viewport {}", "MainRenderer", viewportId);
		return 0;
	}
	return iter->second.addText(textRenderData);
}

void MainRenderer::removeTextOnViewport(ViewportId viewportId, ElementId id) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call removeTextOnViewport on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.removeText(id);
}

const std::unordered_map<ElementId, TextRenderData>* MainRenderer::getTextOnViewport(ViewportId viewportId) const {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call getTextOnViewport on non existent viewport {}", "MainRenderer", viewportId);
		return nullptr;
	}
	return &iter->second.getTextOnViewport();
}

vk::DescriptorSet MainRenderer::getImage(const std::string& path) {
	if (currentlyRenderedWindow == 0) {
		logError("Can't start viewport rendering when no window is rendering.");
		return {};
	}
	auto windowsIter = windowRenderers.find(currentlyRenderedWindow);
	assert(windowsIter != windowRenderers.end());
	auto [descriptorSet, lifetimeObjects] = imageManager.getImage(path, currentlyRenderedWindow);
	if (!descriptorSet) return {};
	windowsIter->second.addLifetimeObjects(lifetimeObjects);
	return descriptorSet;
}
