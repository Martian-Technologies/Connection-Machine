#version 450

layout(location = 0) in vec2 screenCord;
layout(location = 1) in vec2 worldCord;

layout (location = 0) out vec4 outColor;

layout( push_constant ) uniform constants
{
	layout(offset = 64) vec3 background;
	layout(offset = 76) float gridFade;
	layout(offset = 80) vec4 gradientColor;
} push;

// background constants
// vec3 bgCol = vec3(0.69f * 1.3478f, 0.69f * 1.3478f, 0.69f * 1.3478f);
// float gradientIntensity = 0.1;

// grid constants
const float baseBackgroundValue = 0.69 * 1.3478;
const vec3 luminanceWeights = vec3(0.2126, 0.7152, 0.0722);
vec3 gridCol = vec3(0.89, 0.878, 0.878);
vec3 bigGridCol = vec3(0.8, 0.8, 0.8);
float gridLineWidth = 0.13;
float bigGridLineWidth = 0.7;

// thank you magic function from article
float grid(vec2 cord, float spacing, float width) {
	vec2 lineAA = fwidth(cord * spacing);
	vec2 lineUV = 1.0 - abs(fract(cord * spacing) * 2.0 - 1.0);
	vec2 line2 = smoothstep(width * spacing + lineAA, width * spacing - lineAA, lineUV);
	return mix(line2.x, 1.0, line2.y);
}

void main() {
	vec3 bgCol = push.background.rgb;
	float visibilityScale = dot(bgCol, luminanceWeights) / baseBackgroundValue;
	vec3 scaledGridCol = gridCol * visibilityScale;

	float smallGrid = grid(worldCord, 1.0, gridLineWidth) * push.gridFade;
	float bigGrid = grid(worldCord, 1.0/10.0, mix(gridLineWidth, bigGridLineWidth, 1 - push.gridFade));
	
	vec3 col = mix(bgCol, scaledGridCol, smallGrid);
	col = mix(col, mix(bgCol, scaledGridCol, bigGrid), 1 - push.gridFade);
	// col = mix(col, mix(bigGridCol * visibilityScale, scaledGridCol, 1 - push.gridFade), bigGrid);

	float gradientFactor = length(screenCord) * push.gradientColor.a;
	col = mix(col, push.gradientColor.rgb, gradientFactor);
	
	outColor = vec4(col, 1.0f);
}
