#include "vulkanPipeline.h"

const std::vector<VkDynamicState> dynamicStates = {
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
};

void Pipeline::init(VulkanDevice* device, const PipelineInformation& info) {
	this->device = device;

	// shader stages
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = info.vertShader;
	vertShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = info.fragShader;
	fragShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	// set up dynamic states
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	// vertex input state (vertex buffers)
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(info.vertexBindingDescriptions.size());
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(info.vertexAttributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = info.vertexBindingDescriptions.data();
	vertexInputInfo.pVertexAttributeDescriptions = info.vertexAttributeDescriptions.data();

	// input assembly state
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// viewport state
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// rasterizer state
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = info.frontFace;

	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // unused
	rasterizer.depthBiasClamp = 0.0f; // unused
	rasterizer.depthBiasSlopeFactor = 0.0f; // unused

	// multi sampling
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = info.sampleCount;
	multisampling.minSampleShading = 1.0f; // unused
	multisampling.pSampleMask = nullptr; // unused
	multisampling.alphaToCoverageEnable = VK_FALSE; // unused
	multisampling.alphaToOneEnable = VK_FALSE; // unused

	// color attachment
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if (info.alphaBlending) {
		colorBlendAttachment.blendEnable = VK_TRUE;
		if (info.premultipliedAlpha) colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		else colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	else {
		colorBlendAttachment.blendEnable = VK_FALSE;
	}

	// color blending
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // unused
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // unused
	colorBlending.blendConstants[1] = 0.0f; // unused
	colorBlending.blendConstants[2] = 0.0f; // unused
	colorBlending.blendConstants[3] = 0.0f; // unused

	// pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	// descriptor sets (optional)
	pipelineLayoutInfo.setLayoutCount = info.descriptorSets.size();
	if (!info.descriptorSets.empty()) pipelineLayoutInfo.pSetLayouts = info.descriptorSets.data();

	// push constants (optional)
	size_t currentOffset = 0;
	for (auto& push : info.pushConstants) {
		VkPushConstantRange pushConstant{};
		pushConstant.offset = currentOffset;
		pushConstant.size = push.size;
		pushConstant.stageFlags = push.stage;

		pushConstants.push_back(pushConstant);

		currentOffset += push.size;
	}
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
	pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

	// create pipeline layout
	if (vkCreatePipelineLayout(device->getDevice(), &pipelineLayoutInfo, nullptr, &layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	// create pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // unused
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = layout;
	pipelineInfo.renderPass = info.renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // unused
	pipelineInfo.basePipelineIndex = -1; // unused

	if (vkCreateGraphicsPipelines(device->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &handle) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}
};

void Pipeline::cmdPushConstants(VkCommandBuffer commandBuffer, void* data) {
	for (const auto& constant : pushConstants) {
		vkCmdPushConstants(commandBuffer, layout, constant.stageFlags, constant.offset, constant.size, (char*)data + constant.offset);
	}
}

void Pipeline::cleanup() {
	vkDestroyPipeline(device->getDevice(), handle, nullptr);
	vkDestroyPipelineLayout(device->getDevice(), layout, nullptr);
}
