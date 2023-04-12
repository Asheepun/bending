#version 330 core
layout (location = 0) in vec2 vertexPosition_attribute;
layout (location = 1) in vec2 textureVertex_attribute;

out vec2 textureCoord;

uniform float posX;
uniform float posY;

uniform float width;
uniform float height;

uniform mat2 rotationMatrix = mat2(1.0);
uniform float aspectRatio = 1.0;

vec2 vertexPosition;

void main(){

	textureCoord = textureVertex_attribute;

	vertexPosition = vertexPosition_attribute;

	vertexPosition.x *= width;
	vertexPosition.y *= height;

	vertexPosition *= rotationMatrix;


	gl_Position = vec4(
		vertexPosition.x / aspectRatio - 1.0 + posX / aspectRatio,
		vertexPosition.y + 1.0 - posY,
		0.0,
		1.0
	);

}
