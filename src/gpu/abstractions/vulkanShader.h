#ifndef vulkanShader_h
#define vulkanShader_h

#include "gpu/vulkanDevice.h"

vk::UniqueShaderModule createShaderModule(vk::Device device, std::vector<char> byteCode);

#endif /* vulkanShader_h */
