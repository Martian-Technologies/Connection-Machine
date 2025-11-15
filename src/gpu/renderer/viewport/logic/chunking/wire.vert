#version 450
#extension GL_GOOGLE_include_directive : require

#include "stateBuffer.glsl"
#include "../sharedLogic/wireConstants.glsl"

layout(location = 0) in vec2 pointA;
layout(location = 1) in vec2 pointB;
layout(location = 2) in float lineWidth;
layout(location = 3) in uint inStateIndex;

layout(location = 0) out vec3 outColor;

layout( push_constant ) uniform constants
{
	mat4 mvp;
} push;

void main() {
	// extract state from states array
	uint val = states[inStateIndex / statesPerWord];
	uint state = (val >> ((inStateIndex % statesPerWord) * 8)) & 0xFFu;

	// get line vertex
	vec2 xBasis = pointB - pointA;
	vec2 yBasis = normalize(vec2(-xBasis.y, xBasis.x));
	vec2 sampledPosition = positions[gl_VertexIndex % 6];
	vec2 position = pointA + xBasis * sampledPosition.x + yBasis * lineWidth * sampledPosition.y;

	gl_Position = push.mvp * vec4(position, 0.0, 1.0);

	outColor = vec3(0.0f);
	outColor += WIRE_LOW_COLOR * float(state == 0); // normal
	outColor += WIRE_HIGH_COLOR * float(state == 1); // powered on
	outColor += WIRE_Z_COLOR * float(state == 2); // z (floating)
	outColor += WIRE_X_COLOR * float(state == 3); // x
}
