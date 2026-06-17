#include "vulkanPipeline.h"

const std::vector<vk::DynamicState> dynamicStates = {
	vk::DynamicState::eViewport,
	vk::DynamicState::eScissor
};

void Pipeline::init(VulkanDevice& device, const PipelineInformation& info) {
	this->device = &device;

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	vertShaderStageInfo.module = info.vertShader;
	vertShaderStageInfo.pName = "main";
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragShaderStageInfo.module = info.fragShader;
	fragShaderStageInfo.pName = "main";
	vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	vk::PipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(info.vertexBindingDescriptions.size());
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(info.vertexAttributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = info.vertexBindingDescriptions.data();
	vertexInputInfo.pVertexAttributeDescriptions = info.vertexAttributeDescriptions.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vk::PipelineViewportStateCreateInfo viewportState{};
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	vk::PipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace = info.frontFace;
	rasterizer.depthBiasEnable = VK_FALSE;

	vk::PipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = info.sampleCount;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	if (info.alphaBlending) {
		colorBlendAttachment.blendEnable = VK_TRUE;
		if (info.premultipliedAlpha) colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne;
		else colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
		colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
		colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
		colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eZero;
		colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOne;
		colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
	} else {
		colorBlendAttachment.blendEnable = VK_FALSE;
	}

	vk::PipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.setLayoutCount = info.descriptorSets.size();
	if (!info.descriptorSets.empty()) pipelineLayoutInfo.pSetLayouts = info.descriptorSets.data();

	size_t currentOffset = 0;
	for (auto& push : info.pushConstants) {
		vk::PushConstantRange pushConstant{};
		pushConstant.offset = currentOffset;
		pushConstant.size = push.size;
		pushConstant.stageFlags = push.stage;

		pushConstants.push_back(pushConstant);

		currentOffset += push.size;
	}
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
	pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

	auto layoutResult = this->device->getDevice().createPipelineLayoutUnique(pipelineLayoutInfo);
	if (layoutResult.result != vk::Result::eSuccess) {
		throwFatalError("failed to create pipeline layout!");
	}
	layout = std::move(layoutResult.value);

	vk::GraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = layout.get();
	pipelineInfo.renderPass = info.renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = nullptr;
	pipelineInfo.basePipelineIndex = -1;

	auto pipelineResult = this->device->getDevice().createGraphicsPipelineUnique(nullptr, pipelineInfo);
	if (pipelineResult.result != vk::Result::eSuccess) {
		throwFatalError("failed to create graphics pipeline!");
	}
	handle = std::move(pipelineResult.value);
}

void Pipeline::cmdPushConstants(vk::CommandBuffer commandBuffer, void* data) {
	for (const auto& constant : pushConstants) {
		commandBuffer.pushConstants(layout.get(), constant.stageFlags, constant.offset, constant.size, (char*)data + constant.offset);
	}
}

void Pipeline::cleanup() {
	handle.reset();
	layout.reset();
}
