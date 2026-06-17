#version 450

layout(location = 0) in vec3 inTex;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2DArray displayTexture; // changed

void main() {
	outColor = texture(displayTexture, inTex);
}
