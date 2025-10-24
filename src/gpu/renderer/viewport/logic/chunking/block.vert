#version 450
#extension GL_GOOGLE_include_directive : require

// per instance
layout(location = 0) in vec2 inPosition;
layout(location = 1) in uvec2 inSize;
layout(location = 2) in uint inTexLayer;
layout(location = 3) in vec2 inTexPos;
layout(location = 4) in vec2 inTexSize;
layout(location = 5) in uint inRotation;
layout(location = 6) in vec2 stateStep;

layout(location = 0) out vec3 outTex;

layout( push_constant ) uniform constants
{
	mat4 mvp;
} push;

#include "stateBuffer.glsl"

// magic rotation bitmasks for getting rotation coordinates
uint bitmasksX[8] = uint[8](0x1C, 0x0E, 0x23, 0x31, 0x1C, 0x0E, 0x23, 0x31);
uint bitmasksY[8] = uint[8](0x0E, 0x23, 0x31, 0x1C, 0x31, 0x1C, 0x0E, 0x23);

void main() {
	// extract state from states array
	uint val = states[gl_InstanceIndex / statesPerWord];
	uint state = (val >> ((gl_InstanceIndex % statesPerWord) * 8)) & 0xFFu;

	// offsets
	uint b = 1 << (gl_VertexIndex % 6);
	vec2 posCoord = vec2((bitmasksX[0] & b) != 0, (bitmasksY[0] & b) != 0);
    vec2 uvCoord = vec2((bitmasksX[inRotation] & b) != 0, (bitmasksY[inRotation] & b) != 0);

	outTex = vec3(
		inTexPos.x + uvCoord.x * inTexSize.x + float(state) * stateStep.x,
		inTexPos.y + uvCoord.y * inTexSize.y + float(state) * stateStep.y,
		inTexLayer
	);
	gl_Position = push.mvp * vec4(inPosition + (posCoord * inSize), 0.0, 1.0);
}
