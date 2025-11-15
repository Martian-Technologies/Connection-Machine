#ifndef vulkanPipeline_h
#define vulkanPipeline_h

#include "gpu/vulkanDevice.h"
#include <glm/glm.hpp>

struct PushConstantDescription {
	VkShaderStageFlags stage;
	size_t size;
};

struct PipelineInformation {
	VkRenderPass renderPass;
	VkShaderModule vertShader, fragShader;
	std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
	VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	bool alphaBlending = true;
	bool premultipliedAlpha = false;

	std::vector<PushConstantDescription> pushConstants;
	std::vector<VkDescriptorSetLayout> descriptorSets;

	VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
};

class Pipeline {
public:
	void init(VulkanDevice* device, const PipelineInformation& info);
	void cleanup();

	void cmdPushConstants(VkCommandBuffer commandBuffer, void* data);

	inline VkPipeline getHandle() { return handle; }
	inline VkPipelineLayout getLayout() { return layout; }

private:
	VkPipeline handle;
    VkPipelineLayout layout;

	std::vector<VkPushConstantRange> pushConstants;

	VulkanDevice* device;
};

#endif
