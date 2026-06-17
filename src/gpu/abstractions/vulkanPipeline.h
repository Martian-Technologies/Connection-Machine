#ifndef vulkanPipeline_h
#define vulkanPipeline_h

#include "gpu/vulkanDevice.h"
#include <glm/glm.hpp>

struct PushConstantDescription {
	vk::ShaderStageFlags stage;
	size_t size;
};

struct PipelineInformation {
	vk::RenderPass renderPass;
	vk::ShaderModule vertShader;
	vk::ShaderModule fragShader;
	std::vector<vk::VertexInputBindingDescription> vertexBindingDescriptions;
	std::vector<vk::VertexInputAttributeDescription> vertexAttributeDescriptions;
	vk::FrontFace frontFace = vk::FrontFace::eCounterClockwise;
	bool alphaBlending = true;
	bool premultipliedAlpha = false;

	std::vector<PushConstantDescription> pushConstants;
	std::vector<vk::DescriptorSetLayout> descriptorSets;

	vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
};

class Pipeline {
public:
	void init(VulkanDevice& device, const PipelineInformation& info);
	void cleanup();

	void cmdPushConstants(vk::CommandBuffer commandBuffer, void* data);

	inline vk::Pipeline getHandle() { return handle.get(); }
	inline vk::PipelineLayout getLayout() { return layout.get(); }

private:
	vk::UniquePipeline handle;
	vk::UniquePipelineLayout layout;

	std::vector<vk::PushConstantRange> pushConstants;

	VulkanDevice* device = nullptr;
};

#endif /* vulkanPipeline_h */
