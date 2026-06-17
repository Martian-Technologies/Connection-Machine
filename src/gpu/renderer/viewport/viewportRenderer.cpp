#include "viewportRenderer.h"

#include <glm/ext/matrix_clip_space.hpp>

#include "backend/selection.h"
#include "gpu/abstractions/vulkanImage.h"
#include "gpu/mainRenderer.h"
#include "gpu/renderer/viewport/elements/elementRenderer.h"
#include "imgui/imgui_impl_vulkan.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

ViewportRenderer::ViewportRenderer(VulkanDevice& device, ImGuiRenderer& imGuiRenderer) : imGuiRenderer(imGuiRenderer), chunker(device), device(device) {
	createRenderPass();
	gridRenderer.init(device, renderPass.get());
	chunkRenderer.init(device, renderPass.get());
	elementRenderer.init(device, renderPass.get());

	vk::SamplerCreateInfo samplerInfo{};
	samplerInfo.magFilter = vk::Filter::eLinear;
	samplerInfo.minFilter = vk::Filter::eLinear;
	samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	auto samplerResult = device.getDevice().createSamplerUnique(samplerInfo);
	if (samplerResult.result != vk::Result::eSuccess) throwFatalError("failed to create sampler");
	sampler = std::make_shared<Sampler>(std::move(samplerResult.value));

	frames.init(device);

	imageSwapchain.init(device);
	imguiTextures.resize(FRAMES_IN_FLIGHT);
}

ViewportRenderer::~ViewportRenderer() {
	{
		std::lock_guard guard(MainRenderer::get().getVulkanInstance().getDevice().getGraphicsQueueLock());
		destroyImages();
	}

	imageSwapchain.cleanup();

	renderPass.reset();

	elementRenderer.cleanup();
	chunkRenderer.cleanup();
	gridRenderer.cleanup();
	frames.cleanup();
}

ViewportViewData ViewportRenderer::getViewData() {
	std::lock_guard<std::mutex> lock(viewMux);
	return viewData;
}

void ViewportRenderer::setSimulator(const EvalLogicSimulator* simulator, const Address& address) {
	std::lock_guard<std::mutex> lock1(simulatorMux);
	std::lock_guard<std::mutex> lock2(addressMux);
	this->simulator = simulator;
	this->address = address;
	chunker.setSimulator(simulator, address);
}

void ViewportRenderer::updateViewFrame(glm::vec2 size) {
	{
		std::lock_guard<std::mutex> lock(viewMux);
		newViewData.viewportSize.width = size.x;
		newViewData.viewportSize.height = size.y;
	}
	if (viewData.viewportSize.width != newViewData.viewportSize.width || viewData.viewportSize.height != newViewData.viewportSize.height) {
		updateViewData.store(true);
	}
}

void ViewportRenderer::updateView(FPosition topLeft, FPosition bottomRight) {
	{
		std::lock_guard<std::mutex> lock(viewMux);
		newViewData.viewportViewMat = glm::ortho(topLeft.x, bottomRight.x, topLeft.y, bottomRight.y);
		newViewData.viewBounds = { topLeft, bottomRight };
		float viewWidth = bottomRight.x - topLeft.x;
		float viewHeight = bottomRight.y - topLeft.y;
		newViewData.viewScale = std::min(viewWidth, viewHeight);
	}
	updateViewData.store(true);
}

class BorrowedImageDetector {
public:
	BorrowedImageDetector(std::atomic<int>& currentBorrowedImage) : currentBorrowedImage(currentBorrowedImage) { }
	~BorrowedImageDetector() { currentBorrowedImage.store(-1); }
private:
	std::atomic<int>& currentBorrowedImage;
};

ElementId ViewportRenderer::addSelectionObjectElement(const SelectionObjectElement& selection) {
	std::lock_guard<std::mutex> lock(elementsMux);

	ElementId newElement = ++currentElementId;

	std::stack<SharedSelection> selectionsLeft;
	selectionsLeft.push(selection.selection);
	while (!selectionsLeft.empty()) {
		SharedSelection selectionObj = selectionsLeft.top();
		selectionsLeft.pop();

		SharedCellSelection cellSelection = selectionCast<CellSelection>(selectionObj);
		if (cellSelection) {
			BoxSelectionRenderData newBoxSelection;
			Position position = cellSelection->getPosition();

			newBoxSelection.topLeft = glm::vec2(position.x, position.y);
			newBoxSelection.size = glm::vec2(1.0f);
			if (selection.renderMode == SelectionObjectElement::RenderMode::SELECTION) newBoxSelection.state = BoxSelectionRenderData::Normal;
			else if (selection.renderMode == SelectionObjectElement::RenderMode::SELECTION_INVERTED) newBoxSelection.state = BoxSelectionRenderData::Inverted;
			else if (selection.renderMode == SelectionObjectElement::RenderMode::ARROWS) newBoxSelection.state = BoxSelectionRenderData::Special;

			boxSelections[newElement].push_back(newBoxSelection);
		}

		SharedDimensionalSelection dimensionalSelection = selectionCast<DimensionalSelection>(selectionObj);
		if (dimensionalSelection) {
			if (selection.renderMode != SelectionObjectElement::RenderMode::ARROWS) {
				for (int i = 0; i < dimensionalSelection->size(); i++) {
					selectionsLeft.push(dimensionalSelection->getSelection(i));
				}
			} else {
				SharedProjectionSelection projectionSelection = selectionCast<ProjectionSelection>(selectionObj);
				if (projectionSelection) {
					SharedDimensionalSelection dSel = dimensionalSelection;
					Position origin;
					unsigned int height = 0;
					while (dSel) {
						SharedSelection sel = dSel->getSelection(0);
						SharedCellSelection cSel = selectionCast<CellSelection>(sel);
						if (cSel) {
							origin = cSel->getPosition();
							break;
						}
						height++;
						dSel = selectionCast<DimensionalSelection>(sel);
					}

					selectionsLeft.push(dimensionalSelection->getSelection(0));

					if (projectionSelection->size() == 1) {
						arrows[newElement].push_back(ArrowRenderData(origin, origin, height));
					} else {
						for (int i = 1; i < projectionSelection->size(); i++) {
							Position newOrigin = origin + projectionSelection->getStep();
							arrows[newElement].push_back(ArrowRenderData(origin, newOrigin, height));
							origin = newOrigin;
						}
					}
				} else {
					for (int i = 0; i < dimensionalSelection->size(); i++) {
						selectionsLeft.push(dimensionalSelection->getSelection(i));
					}
				}
			}
		}
	}

	return newElement;
}

ElementId ViewportRenderer::addSelectionElement(const SelectionElement& selection) {
	std::lock_guard<std::mutex> lock(elementsMux);

	ElementId newElement = ++currentElementId;

	FPosition topLeft = selection.topLeft.free();
	FPosition bottomRight = selection.bottomRight.free();
	if (selection.topLeft.x > selection.bottomRight.x) std::swap(topLeft.x, bottomRight.x);
	if (selection.topLeft.y > selection.bottomRight.y) std::swap(topLeft.y, bottomRight.y);

	BoxSelectionRenderData newBoxSelection;
	newBoxSelection.topLeft = glm::vec2(topLeft.x, topLeft.y);
	newBoxSelection.size = glm::vec2(bottomRight.x - topLeft.x + 1.0f, bottomRight.y - topLeft.y + 1.0f);
	newBoxSelection.state = selection.inverted ? BoxSelectionRenderData::Inverted : BoxSelectionRenderData::Normal;

	boxSelections[newElement].push_back(newBoxSelection);

	return newElement;
}

void ViewportRenderer::removeSelectionElement(ElementId id) {
	std::lock_guard<std::mutex> lock(elementsMux);
	boxSelections.erase(id);
	arrows.erase(id);
}

std::vector<ArrowRenderData> ViewportRenderer::getArrows() {
	std::lock_guard<std::mutex> lock(elementsMux);

	std::vector<ArrowRenderData> returnArrows;
	returnArrows.reserve(arrows.size());

	for (const auto& arrow : arrows) {
		returnArrows.insert(returnArrows.end(), arrow.second.begin(), arrow.second.end());
	}

	return returnArrows;
}

std::vector<BoxSelectionRenderData> ViewportRenderer::getBoxSelections() {
	std::lock_guard<std::mutex> lock(elementsMux);

	std::vector<BoxSelectionRenderData> returnBoxSelections;
	returnBoxSelections.reserve(boxSelections.size());

	for (const auto& boxSelection : boxSelections) {
		returnBoxSelections.insert(returnBoxSelections.end(), boxSelection.second.begin(), boxSelection.second.end());
	}

	return returnBoxSelections;
}

ElementId ViewportRenderer::addBlockPreview(BlockPreview&& blockPreview) {
	std::lock_guard<std::mutex> lock(elementsMux);

	ElementId newElement = ++currentElementId;

	blockPreviews.reserve(blockPreviews.size() + blockPreview.blocks.size());

	for (const BlockPreview::Block& block : blockPreview.blocks) {
		BlockPreviewRenderData newPreview;
		newPreview.position = glm::vec2(block.position.x, block.position.y);
		newPreview.orientation = block.orientation;
		const BlockRenderDataManager::BlockRenderData* blockRenderData = MainRenderer::get().getBlockRenderDataManager().getBlockRenderData(block.blockRenderDataId);
		if (!blockRenderData) continue;
		Size size = block.orientation * blockRenderData->size;
		newPreview.size = glm::vec2(size.w, size.h);
		newPreview.textureIndex = blockRenderData->blockTextureCords.textureLayer;
		newPreview.texPos = blockRenderData->blockTextureCords.textureOriginUV;
		newPreview.texSize = blockRenderData->blockTextureCords.textureSizeUV;

		blockPreviews.emplace(newElement, newPreview);
	}

	return newElement;
}

void ViewportRenderer::shiftBlockPreview(ElementId id, Vector shift) {
	std::lock_guard<std::mutex> lock(elementsMux);

	auto iterPair = blockPreviews.equal_range(id);
	for (auto iter = iterPair.first; iter != iterPair.second; ++iter) {
		iter->second.position += glm::vec2(shift.dx, shift.dy);
	}
}

void ViewportRenderer::removeBlockPreview(ElementId id) {
	std::lock_guard<std::mutex> lock(elementsMux);
	blockPreviews.erase(id);
}

std::vector<BlockPreviewRenderData> ViewportRenderer::getBlockPreviews() {
	std::lock_guard<std::mutex> lock(elementsMux);

	std::vector<BlockPreviewRenderData> returnBlockPreviews;
	returnBlockPreviews.reserve(blockPreviews.size());

	for (const auto& preview : blockPreviews) {
		returnBlockPreviews.push_back(preview.second);
	}

	return returnBlockPreviews;
}

ElementId ViewportRenderer::addConnectionPreview(const ConnectionPreview& connectionPreview) {
	std::lock_guard<std::mutex> lock(elementsMux);

	ElementId newElement = ++currentElementId;

	ConnectionPreviewRenderData newPreview;
	FPosition pointA = connectionPreview.output;
	FPosition pointB = connectionPreview.input;
	newPreview.pointA = glm::vec2(pointA.x, pointA.y);
	newPreview.pointB = glm::vec2(pointB.x, pointB.y);
	connectionPreviews[newElement] = newPreview;

	return newElement;
}

void ViewportRenderer::removeConnectionPreview(ElementId id) {
	std::lock_guard<std::mutex> lock(elementsMux);
	connectionPreviews.erase(id);
}

ElementId ViewportRenderer::addHalfConnectionPreview(const HalfConnectionPreview& halfConnectionPreview) {
	std::lock_guard<std::mutex> lock(elementsMux);

	ElementId newElement = ++currentElementId;

	ConnectionPreviewRenderData newPreview;
	FPosition pointA = halfConnectionPreview.output;
	newPreview.pointA = glm::vec2(pointA.x, pointA.y);
	newPreview.pointB = glm::vec2(halfConnectionPreview.input.x, halfConnectionPreview.input.y);
	connectionPreviews[newElement] = newPreview;

	return newElement;
}

void ViewportRenderer::removeHalfConnectionPreview(ElementId id) {
	std::lock_guard<std::mutex> lock(elementsMux);
	connectionPreviews.erase(id);
}

std::vector<ConnectionPreviewRenderData> ViewportRenderer::getConnectionPreviews() {
	std::lock_guard<std::mutex> lock(elementsMux);

	std::vector<ConnectionPreviewRenderData> returnConnectionPreviews;
	returnConnectionPreviews.reserve(connectionPreviews.size());

	for (const auto& preview : connectionPreviews) {
		returnConnectionPreviews.push_back(preview.second);
	}

	return returnConnectionPreviews;
}

ElementId ViewportRenderer::addText(const TextRenderData& textRenderData) {
	std::lock_guard<std::mutex> lock(elementsMux);

	ElementId newElement = ++currentElementId;
	renderText.emplace(newElement, textRenderData);

	return newElement;
}

void ViewportRenderer::removeText(ElementId id) {
	std::lock_guard<std::mutex> lock(elementsMux);
	renderText.erase(id);
}

const std::unordered_map<ElementId, TextRenderData>& ViewportRenderer::getTextOnViewport() const {
	return renderText;
}

void ViewportRenderer::render(Frame& frame) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	// Get simulator and address with proper locking
	const EvalLogicSimulator* sim = getSimulator();
	Address addr = getAddress();

	// Get view data with proper locking
	ViewportViewData localViewData = getViewData();

	// Only render if we have valid data
	gridRenderer.render(frame, localViewData.viewportViewMat, localViewData.viewScale, sim);

	if (sim != nullptr) {
		chunkRenderer.render(
			frame,
			localViewData.viewportViewMat,
			sim,
			addr,
			getChunker().getAllocations(localViewData.viewBounds.first.snap(), localViewData.viewBounds.second.snap())
		);
	}

	elementRenderer.renderBlockPreviews(frame, localViewData.viewportViewMat, getBlockPreviews());
	elementRenderer.renderConnectionPreviews(frame, localViewData.viewportViewMat, getConnectionPreviews());
	elementRenderer.renderBoxSelections(frame, localViewData.viewportViewMat, getBoxSelections());
	elementRenderer.renderArrows(frame, localViewData.viewportViewMat, getArrows());
}

void ViewportRenderer::createRenderPass() {
	vk::SampleCountFlagBits msaaSamples = device.getMaxUsableSampleCount();

	// MSAA color attachment (we render to this)
	vk::AttachmentDescription colorAttachment{};
	colorAttachment.format = vk::Format::eR8G8B8A8Unorm;
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare; // Don't need to store MSAA
	colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
	colorAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

	// Resolve attachment (non-MSAA, for ImGui to read)
	vk::AttachmentDescription resolveAttachment{};
	resolveAttachment.format = vk::Format::eR8G8B8A8Unorm;
	resolveAttachment.samples = vk::SampleCountFlagBits::e1;
	resolveAttachment.loadOp = vk::AttachmentLoadOp::eClear; // Clear on each frame
	resolveAttachment.storeOp = vk::AttachmentStoreOp::eStore; // We need this for ImGui
	resolveAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	resolveAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	resolveAttachment.initialLayout = vk::ImageLayout::eUndefined;// Don't care about previous contents
	resolveAttachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal; // Ready for ImGui sampling

	// Attachment references
	vk::AttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference resolveAttachmentRef{};
	resolveAttachmentRef.attachment = 1;
	resolveAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::SubpassDescription subpass{};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pResolveAttachments = &resolveAttachmentRef;

	// Subpass dependency for layout transitions
	std::array<vk::SubpassDependency, 2> dependencies{};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependencies[0].srcAccessMask = {};
	dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
	dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	dependencies[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;

	std::array<vk::AttachmentDescription, 2> attachments = { colorAttachment, resolveAttachment };
	vk::RenderPassCreateInfo renderPassInfo{};
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = dependencies.size();
	renderPassInfo.pDependencies = dependencies.data();

	auto rpResult = device.getDevice().createRenderPassUnique(renderPassInfo);
	if (rpResult.result != vk::Result::eSuccess) {
		throwFatalError("failed to create MSAA render pass!");
	}
	renderPass = std::move(rpResult.value);
}

#ifdef TRACY_PROFILER
const char* const viewportLoop_Tracy = "ViewportLoop";
#endif

std::tuple<vk::DescriptorSet, vk::Semaphore, std::vector<std::shared_ptr<void>>> ViewportRenderer::startImageRender() {
	uint32_t currentFrameIndex = frames.getCurrentFrameIndex();

	if (updateViewData.load()) {
		bool recreateImages = false;
		{
			if (viewData.viewportSize.width == 0 || viewData.viewportSize.height == 0) {
				viewData.viewportSize.width = 1;
				viewData.viewportSize.height = 1;
				recreateImages = true;
			}
			std::lock_guard<std::mutex> lock(viewMux);
			if (newViewData.viewportSize.width == 0 || newViewData.viewportSize.height == 0) {
				newViewData.viewportSize.width = 1;
				newViewData.viewportSize.height = 1;
			}
			if (viewData.viewportSize.width != newViewData.viewportSize.width || viewData.viewportSize.height != newViewData.viewportSize.height) {
				recreateImages = true;
			}
			updateViewData.store(false);
			viewData = newViewData;
		}
		if (recreateImages) {
			imagesReady.store(0);
			std::lock_guard guard(MainRenderer::get().getVulkanInstance().getDevice().getGraphicsQueueLock());
			destroyImages();
			createImages();
		}
	} else if (imguiTextures.empty()) {
		destroyImages();
		createImages();
	}

	frames.waitForCurrentFrameCompletion();

	std::shared_ptr<Frame> frame = frames.getCurrentFrame();

	frames.startCurrentFrame();

	{
		std::lock_guard guard(MainRenderer::get().getVulkanInstance().getDevice().getGraphicsQueueLock());
		frame->mainCommandBuffer.reset({});
	}
	renderToCommandBuffer(*frame, currentFrameIndex);

	std::shared_ptr<ImageSwapchain::Semaphore> semaphore = imageSwapchain.getImageSemaphores()[currentFrameIndex];

	vk::SubmitInfo submitInfo{};
	vk::Semaphore signalSem = semaphore->semaphore.get();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &signalSem;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &frame->mainCommandBuffer;

	if (device.submitGraphicsQueue(submitInfo, frame->renderFence.get()) != vk::Result::eSuccess) {
		return { vk::DescriptorSet{}, vk::Semaphore{}, {} };
	}

	{
		std::lock_guard<std::mutex> lock(imageMux);
		imageReady.store(true);
	}
	{
		std::lock_guard<std::mutex> lock(framesMutex);
		frames.incrementFrame();
		imagesReady.fetch_add(1);
	}
	return { imguiTextures[currentFrameIndex]->descriptorSet, signalSem, { frame, imguiTextures[currentFrameIndex], semaphore } };
}

void ViewportRenderer::renderToCommandBuffer(Frame& frame, uint32_t imageIndex) {
	vk::CommandBufferBeginInfo beginInfo{};

	if (frame.mainCommandBuffer.begin(beginInfo) != vk::Result::eSuccess) {
		throwFatalError("failed to begin recording command buffer!");
	}

	vk::ClearValue clearValues[2];
	clearValues[0].color = vk::ClearColorValue(std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f });
	clearValues[1].color = vk::ClearColorValue(std::array<float,4>{ 0.0f, 0.0f, 0.0f, 1.0f });

	vk::RenderPassBeginInfo renderPassInfo{};
	renderPassInfo.renderPass = renderPass.get();
	renderPassInfo.framebuffer = imageSwapchain.getFramebuffers()[imageIndex].get();
	renderPassInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderPassInfo.renderArea.extent = viewData.viewportSize;
	renderPassInfo.clearValueCount = 2;
	renderPassInfo.pClearValues = clearValues;

	frame.mainCommandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

	vk::Viewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(viewData.viewportSize.width);
	viewport.height = static_cast<float>(viewData.viewportSize.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	frame.mainCommandBuffer.setViewport(0, viewport);

	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{ 0, 0 };
	scissor.extent = viewData.viewportSize;
	frame.mainCommandBuffer.setScissor(0, scissor);

	if (viewData.viewportSize.width > 0 && viewData.viewportSize.height > 0) {
		render(frame);
	}

	frame.mainCommandBuffer.endRenderPass();

	if (frame.mainCommandBuffer.end() != vk::Result::eSuccess) {
		throwFatalError("failed to record command buffer!");
	}
}

void ViewportRenderer::destroyImages() {
	if (imageReady.load()) {
		device.waitIdleNoMux();
		imageReady.store(false);
		imguiTextures.clear();
		msaaImage.reset();
	}
}

void ViewportRenderer::createImages() {
	vk::Extent3D imageSize = { viewData.viewportSize.width, viewData.viewportSize.height, 1 };
	vk::SampleCountFlagBits msaaSamples = device.getMaxUsableSampleCount();

	msaaImage.emplace(device, imageSize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment, false, msaaSamples);

	imageSwapchain.recreate(renderPass.get(), { viewData.viewportSize.width, viewData.viewportSize.height }, *msaaImage);
	imguiTextures.resize(FRAMES_IN_FLIGHT);
	for (unsigned int i = 0; i < imguiTextures.size(); i++) {
		imguiTextures[i] = std::make_shared<ImGuiRenderer::ImGuiDescriptorSet>(
			ImGui_ImplVulkan_AddTexture(static_cast<VkSampler>(sampler->sampler.get()), static_cast<VkImageView>(imageSwapchain.getImages()[i]->imageView.get()), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			imGuiRenderer,
			std::vector<std::shared_ptr<void>>{ imageSwapchain.getImages()[i], sampler }
		);
	}

	imageReady.store(true);
}
