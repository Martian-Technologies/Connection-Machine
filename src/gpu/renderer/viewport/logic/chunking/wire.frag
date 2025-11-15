#version 450

#extension GL_GOOGLE_include_directive : require

#include "../sharedLogic/wireConstants.glsl"

layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 fragColor;

void main() {
	fragColor = vec4(inColor, WIRE_OPACITY);
}
