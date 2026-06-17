#include "vulkanShader.h"

vk::UniqueShaderModule createShaderModule(vk::Device device, std::vector<char> byteCode) {
	vk::ShaderModuleCreateInfo createInfo{};
	createInfo.codeSize = byteCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());

	auto result = device.createShaderModuleUnique(createInfo);
	if (result.result != vk::Result::eSuccess) {
		throwFatalError("failed to create vulkan shader module!");
	}
	return std::move(result.value);
}
