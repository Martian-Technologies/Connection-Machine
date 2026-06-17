#ifndef viewportRenderer_h
#define viewportRenderer_h

#include <glm/ext/matrix_float4x4.hpp>

#include "gpu/abstractions/vulkanImageSwapchain.h"
#include "gpu/renderer/imgui/imGuiRenderer.h"
#include "logic/chunking/vulkanChunker.h"
#include "gpu/renderer/frameManager.h"
#include "gpu/mainRendererDefs.h"
#include "logic/chunking/chunkRenderer.h"
#include "elements/elementRenderer.h"
#include "grid/gridRenderer.h"

class ImGuiRenderer;

struct ViewportViewData {
	glm::mat4 viewportViewMat;
	std::pair<FPosition, FPosition> viewBounds;
	float viewScale;
	vk::Extent2D viewportSize;
};

class ViewportRenderer {
public:
	struct Sampler {
		Sampler(vk::UniqueSampler s) : sampler(std::move(s)) { }
		vk::UniqueSampler sampler;
	};
	ViewportRenderer(VulkanDevice& device, ImGuiRenderer& imGuiRenderer);
	~ViewportRenderer();

	ViewportViewData getViewData();
	inline VulkanChunker& getChunker() { return chunker; }
	inline const EvalLogicSimulator* getSimulator() {
		std::lock_guard<std::mutex> lock(simulatorMux);
		return simulator;
	}
	inline const Address& getAddress() {
		std::lock_guard<std::mutex> lock(addressMux);
		return address;
	}

	std::vector<BlockPreviewRenderData> getBlockPreviews();
	std::vector<BoxSelectionRenderData> getBoxSelections();
	std::vector<ConnectionPreviewRenderData> getConnectionPreviews();
	std::vector<ArrowRenderData> getArrows();

	void setSimulator(const EvalLogicSimulator* simulator, const Address& address);

	void updateViewFrame(glm::vec2 size);
	void updateView(FPosition topLeft, FPosition bottomRight);

	std::tuple<vk::DescriptorSet, vk::Semaphore, std::vector<std::shared_ptr<void>>> startImageRender();

	ElementId addSelectionObjectElement(const SelectionObjectElement& selection);
	ElementId addSelectionElement(const SelectionElement& selection);
	void removeSelectionElement(ElementId selection);

	ElementId addBlockPreview(BlockPreview&& blockPreview);
	void shiftBlockPreview(ElementId id, Vector shift);
	void removeBlockPreview(ElementId blockPreview);

	ElementId addConnectionPreview(const ConnectionPreview& connectionPreview);
	void removeConnectionPreview(ElementId connectionPreview);

	ElementId addHalfConnectionPreview(const HalfConnectionPreview& halfConnectionPreview);
	void removeHalfConnectionPreview(ElementId halfConnectionPreview);

	ElementId addText(const TextRenderData& textRenderData);
	void removeText(ElementId id);
	const std::unordered_map<ElementId, TextRenderData>& getTextOnViewport() const;

	std::shared_ptr<Frame> getCurrentFrame() { return frames.getCurrentFrame(); }

private:
	void destroyImages();
	void createImages();
	void createRenderPass();
	void render(Frame& frame);
	void renderToCommandBuffer(Frame& frame, uint32_t imageIndex);

	const EvalLogicSimulator* simulator = nullptr;
	std::mutex simulatorMux;
	Address address;
	std::mutex addressMux;

	VulkanChunker chunker;
	VulkanDevice& device;
	ImGuiRenderer& imGuiRenderer;

	ElementId currentElementId = 0;
	std::unordered_multimap<ElementId, BlockPreviewRenderData> blockPreviews;
	std::unordered_map<ElementId, std::vector<BoxSelectionRenderData>> boxSelections;
	std::unordered_map<ElementId, ConnectionPreviewRenderData> connectionPreviews;
	std::unordered_map<ElementId, std::vector<ArrowRenderData>> arrows;
	std::unordered_map<ElementId, TextRenderData> renderText;
	std::mutex elementsMux;

	ViewportViewData newViewData{};
	ViewportViewData viewData{};
	std::mutex viewMux;

	vk::UniqueRenderPass renderPass;
	GridRenderer gridRenderer;
	ChunkRenderer chunkRenderer;
	ElementRenderer elementRenderer;

	std::shared_ptr<Sampler> sampler;

	std::mutex framesMutex;
	FrameManager frames;

	std::atomic<int> currentBorrowedImage = -1;
	std::optional<AllocatedImage> msaaImage;
	ImageSwapchain imageSwapchain;
	std::vector<std::shared_ptr<ImGuiRenderer::ImGuiDescriptorSet>> imguiTextures;
	std::atomic<bool> imageReady = false;
	std::atomic<unsigned int> imagesReady = 0;
	std::atomic<bool> updateViewData = true;
	std::mutex imageMux;

	std::chrono::time_point<std::chrono::high_resolution_clock> lastUpdateRender;
};

#endif
