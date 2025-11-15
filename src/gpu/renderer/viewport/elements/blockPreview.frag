#version 450

layout(location = 0) in vec3 tex;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2DArray displayTexture;

const float OPACITY = 0.5;

void main() {
	outColor = texture(displayTexture, tex) * vec4(1.0, 1.0, 1.0, OPACITY);
}
