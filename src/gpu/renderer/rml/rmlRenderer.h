#ifndef rmlRenderer_h
#define rmlRenderer_h

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <RmlUi/Core/Vertex.h>

#include "gpu/abstractions/vulkanPipeline.h"
#include "gpu/renderer/frameManager.h"

#include "rmlResourceManager.h"

// =========================== RML INSTRUCTIONS =================================

struct RmlDrawInstruction {
	inline RmlDrawInstruction(std::shared_ptr<RmlGeometryAllocation> geometry, glm::vec2 translation, std::shared_ptr<RmlTexture> texture = nullptr)
		: geometry(geometry), translation(translation), texture(texture) {}

	std::shared_ptr<RmlGeometryAllocation> geometry;
	glm::vec2 translation;
	std::shared_ptr<RmlTexture> texture = nullptr; // optional
};

struct RmlSetScissorInstruction {
	inline RmlSetScissorInstruction(glm::vec2 offset, glm::vec2 size)
		: offset(offset), size(size) {}

	glm::vec2 offset, size;
};

struct RmlEnableScissorInstruction {
	inline RmlEnableScissorInstruction(bool state)
		: state(state) {}

	bool state;
};

typedef std::variant<RmlDrawInstruction, RmlSetScissorInstruction, RmlEnableScissorInstruction> RmlRenderInstruction;

// ========================= RML RENDERER ====================================

class RmlRenderer {
public:
	void init(VulkanDevice* device, VkRenderPass& renderPass);
	void cleanup();

	void prepareForRmlRender();
	void endRmlRender();

	void render(Frame& frame, VkExtent2D windowExtent);

public:
	// -- Rml::RenderInterface --
	void renderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture);

	void enableScissorRegion(bool enable);
	void setScissorRegion(Rml::Rectanglei region);
private:
	// pipelines
	Pipeline untexturedPipeline;
	Pipeline texturedPipeline;

	// render instructions
	std::vector<RmlRenderInstruction> renderInstructions;
	std::vector<RmlRenderInstruction> tempRenderInstructions;
	std::mutex rmlInstructionMux;

	VulkanDevice* device = nullptr;
};

#endif /* rmlRenderer_h */
