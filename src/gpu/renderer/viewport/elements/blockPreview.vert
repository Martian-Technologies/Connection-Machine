#version 450

layout( push_constant ) uniform constants
{
	mat4 mvp;
	vec2 position;
	vec2 size;
	uint rotation;
	uint texLayer;
	vec2 texPos;
	vec2 texSize;
} push;

layout(location = 0) out vec3 outTex;

// magic rotation bitmasks for getting rotation coordinates
uint bitmasksX[8] = uint[8](0x1C, 0x0E, 0x23, 0x31, 0x1C, 0x0E, 0x23, 0x31);
uint bitmasksY[8] = uint[8](0x0E, 0x23, 0x31, 0x1C, 0x31, 0x1C, 0x0E, 0x23);

void main() {
	// offsets
	uint b = 1 << (gl_VertexIndex % 6);
	vec2 posCoord = vec2((bitmasksX[0] & b) != 0, (bitmasksY[0] & b) != 0);
    vec2 uvCoord = vec2((bitmasksX[push.rotation] & b) != 0, (bitmasksY[push.rotation] & b) != 0);

	outTex = vec3(
		push.texPos.x + uvCoord.x * push.texSize.x,
		push.texPos.y + uvCoord.y * push.texSize.y,
		push.texLayer
		);
	gl_Position = push.mvp * vec4(push.position + (posCoord * push.size), 0.0, 1.0);
}
