#ifndef viewportRenderData_h
#define viewportRenderData_h

#include <glm/ext/matrix_float4x4.hpp>

#include "logic/chunking/vulkanChunker.h"
#include "elements/elementRenderer.h"
#include "gpu/mainRendererDefs.h"

namespace Rml { class Element; }

struct ViewportViewData {
	glm::mat4 viewportViewMat;
	std::pair<FPosition, FPosition> viewBounds;
	float viewScale;
	VkViewport viewport;
};

class ViewportRenderData {
public:
	ViewportRenderData(VulkanDevice* device, Rml::Element* element);

	ViewportViewData getViewData();
	inline VulkanChunker& getChunker() { return chunker; }
	inline Evaluator* getEvaluator() {
		std::lock_guard<std::mutex> lock(evaluatorMux);
		return evaluator;
	}
	inline const Address& getAddress() {
		std::lock_guard<std::mutex> lock(addressMux);
		return address;
	}

	std::vector<BlockPreviewRenderData> getBlockPreviews();
	std::vector<BoxSelectionRenderData> getBoxSelections();
	std::vector<ConnectionPreviewRenderData> getConnectionPreviews();
	std::vector<ArrowRenderData> getArrows();

	// main flow
	void setEvaluator(Evaluator* evaluator, const Address& address);

	void updateViewFrame(glm::vec2 origin, glm::vec2 size);
	void updateView(FPosition topLeft, FPosition bottomRight);

	float getLastFrameTimeMs() const;

	// elements
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

private:
	// From the UI Side
	Rml::Element* element;

	Evaluator* evaluator = nullptr;
	std::mutex evaluatorMux;
	Address address;
	std::mutex addressMux;

	// Vulkan
	VulkanChunker chunker;

	// Elements
	ElementId currentElementId = 0;
	std::unordered_multimap<ElementId, BlockPreviewRenderData> blockPreviews;
	std::unordered_map<ElementId, std::vector<BoxSelectionRenderData>> boxSelections;
	std::unordered_map<ElementId, ConnectionPreviewRenderData> connectionPreviews;
	std::unordered_map<ElementId, std::vector<ArrowRenderData>> arrows;
	std::mutex elementsMux;

	// View data
	ViewportViewData viewData;
	std::mutex viewMux;
};

#endif
