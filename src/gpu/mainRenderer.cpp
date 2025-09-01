#include "mainRenderer.h"

std::optional<MainRenderer> singleton;

MainRenderer& MainRenderer::get() {
	if (!singleton) singleton.emplace();
	return *singleton;
}

void MainRenderer::kill() {
	singleton.reset();
}

WindowId MainRenderer::registerWindow(SdlWindow* sdlWindow) {
	auto pair = windowRenderers.try_emplace(getNewWindowId(), sdlWindow);
	return lastWindowId;
}

void MainRenderer::resizeWindow(WindowId windowId, glm::vec2 size) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call resizeWindow on non existent window {}.", "MainRenderer", windowId);
		return;
	}
	iter->second.resize({size.x, size.y});
}

void MainRenderer::deregisterWindow(WindowId windowId) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call deregisterWindow on non existent window {}.", "MainRenderer", windowId);
		return;
	}
	windowRenderers.erase(iter);
}

void MainRenderer::prepareForRmlRender(WindowId windowId) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call prepareForRmlRender on non existent window {}.", "MainRenderer", windowId);
		return;
	}
	iter->second.getRmlRenderer().prepareForRmlRender();
}

void MainRenderer::endRmlRender(WindowId windowId) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call endRmlRender on non existent window {}", "MainRenderer", windowId);
		return;
	}
	iter->second.getRmlRenderer().endRmlRender();
}

Rml::CompiledGeometryHandle MainRenderer::compileGeometry(WindowId windowId, Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call CompileGeometry on non existent window {}", "MainRenderer", windowId);
		return (Rml::CompiledGeometryHandle)0;
	}
	return iter->second.getRmlRenderer().compileGeometry(vertices, indices);
}

void MainRenderer::releaseGeometry(WindowId windowId, Rml::CompiledGeometryHandle geometry) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call ReleaseGeometry on non existent window {}", "MainRenderer", windowId);
		return;
	}
	iter->second.getRmlRenderer().releaseGeometry(geometry);
}

Rml::TextureHandle MainRenderer::loadTexture(WindowId windowId, Rml::Vector2i& texture_dimensions, const Rml::String& source) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call loadTexture on non existent window {}", "MainRenderer", windowId);
		return (Rml::TextureHandle)0;
	}
	return iter->second.getRmlRenderer().loadTexture(texture_dimensions, source);
}

Rml::TextureHandle MainRenderer::generateTexture(WindowId windowId, Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call generateTexture on non existent window {}", "MainRenderer", windowId);
		return (Rml::TextureHandle)0;
	}
	return iter->second.getRmlRenderer().generateTexture(source, source_dimensions);
}

void MainRenderer::releaseTexture(WindowId windowId, Rml::TextureHandle texture_handle) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call releaseTexture on non existent window {}", "MainRenderer", windowId);
		return;
	}
	iter->second.getRmlRenderer().releaseTexture(texture_handle);
}

void MainRenderer::renderGeometry(WindowId windowId, Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call renderGeometry on non existent window {}", "MainRenderer", windowId);
		return;
	}
	iter->second.getRmlRenderer().renderGeometry(handle, translation, texture);
}

void MainRenderer::enableScissorRegion(WindowId windowId, bool enable) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call enableScissorRegion on non existent window {}", "MainRenderer", windowId);
		return;
	}
	iter->second.getRmlRenderer().enableScissorRegion(enable);
}

void MainRenderer::setScissorRegion(WindowId windowId, Rml::Rectanglei region) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call setScissorRegion on non existent window {}", "MainRenderer", windowId);
		return;
	}
	iter->second.getRmlRenderer().setScissorRegion(region);
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

void MainRenderer::setBlockTextureIndex(BlockRenderDataId blockRenderDataId, unsigned int textureIndex) {
	blockRenderDataManager.setBlockTextureIndex(blockRenderDataId, textureIndex);
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

ViewportId MainRenderer::registerViewport(WindowId windowId, glm::vec2 origin, glm::vec2 size, Rml::Element* element) {
	auto iter = windowRenderers.find(windowId);
	if (iter == windowRenderers.end()) {
		logError("Failed to call registerViewport on non existent window {}", "MainRenderer", windowId);
		return 0;
	}
	auto pair = viewportRenderers.try_emplace(getNewViewportId(), iter->second.getDevice(), element);
	iter->second.registerViewportRenderData(&(pair.first->second));
	return lastViewportId;
}

void MainRenderer::moveViewport(ViewportId viewportId, WindowId windowId, glm::vec2 origin, glm::vec2 size) {
	auto viewportIter = viewportRenderers.find(viewportId);
	if (viewportIter == viewportRenderers.end()) {
		logError("Failed to call moveViewport on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	auto windowIter = windowRenderers.find(windowId);
	if (windowIter == windowRenderers.end()) {
		logError("Failed to call moveViewport on non existent window {}", "MainRenderer", windowId);
		return;
	}
	if (! windowIter->second.hasViewportRenderData(&(viewportIter->second))) {
		logError("moving viewport to other window not supported yet");
		return;
	}
	viewportIter->second.updateViewFrame(origin, size);
}

void MainRenderer::moveViewportView(ViewportId viewportId, FPosition topLeft, FPosition bottomRight) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call moveViewportView on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.updateView(topLeft, bottomRight);
}

void MainRenderer::setViewportEvaluator(ViewportId viewportId, Evaluator* evaluator, Address address) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call setViewportEvaluator on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.setEvaluator(evaluator, address);
}

void MainRenderer::deregisterViewport(ViewportId viewport) { }

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

void MainRenderer::addBlock(ViewportId viewportId, BlockType type, Position position, Size size, Orientation orientation, Position statePosition) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call startMakingEdits on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().addBlock(type, position, size, orientation, statePosition);
}

void MainRenderer::removeBlock(ViewportId viewportId, Position position) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call removeBlock on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().removeBlock(position);
}

void MainRenderer::moveBlock(ViewportId viewportId, Position curPos, Position newPos, Orientation newOrientation, Size newSize) {
	auto iter = viewportRenderers.find(viewportId);
	if (iter == viewportRenderers.end()) {
		logError("Failed to call moveBlock on non existent viewport {}", "MainRenderer", viewportId);
		return;
	}
	iter->second.getChunker().moveBlock(curPos, newPos, newOrientation, newSize);
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
