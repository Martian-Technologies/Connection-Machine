#include "viewportRenderer.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

void ViewportRenderer::init(VulkanDevice* device, VkRenderPass renderPass) {
	gridRenderer.init(device, renderPass);
	chunkRenderer.init(device, renderPass);
	elementRenderer.init(device, renderPass);
}

void ViewportRenderer::cleanup() {
	elementRenderer.cleanup();
	chunkRenderer.cleanup();
	gridRenderer.cleanup();
}

void ViewportRenderer::render(Frame& frame, ViewportRenderData* viewport) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	// get view data
	ViewportViewData viewData = viewport->getViewData();

	// set dynamic state
	vkCmdSetViewport(frame.mainCommandBuffer, 0, 1, &viewData.viewport);
	VkRect2D scissor{};
	scissor.offset = { static_cast<int32_t>(viewData.viewport.x), static_cast<int32_t>(viewData.viewport.y) };
	scissor.extent = { static_cast<uint32_t>(viewData.viewport.width), static_cast<uint32_t>(viewData.viewport.height) };
	vkCmdSetScissor(frame.mainCommandBuffer, 0, 1, &scissor);

	// render subrenderers
	gridRenderer.render(frame, viewData.viewportViewMat, viewData.viewScale, true);
	chunkRenderer.render(frame, viewData.viewportViewMat, viewport->getEvaluator(), viewport->getAddress(), viewport->getChunker().getAllocations(viewData.viewBounds.first.snap(), viewData.viewBounds.second.snap()));
	elementRenderer.renderBlockPreviews(frame, viewData.viewportViewMat, viewport->getBlockPreviews());
	elementRenderer.renderConnectionPreviews(frame, viewData.viewportViewMat, viewport->getConnectionPreviews());
	elementRenderer.renderBoxSelections(frame, viewData.viewportViewMat, viewport->getBoxSelections());
	elementRenderer.renderArrows(frame, viewData.viewportViewMat, viewport->getArrows());
}
