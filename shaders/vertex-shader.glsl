#version 330 core
layout (location = 0) in vec3 attribute_vertex;
layout (location = 1) in vec2 attribute_textureVertex;
layout (location = 2) in vec3 attribute_normalVertex;

uniform vec2 inputPos;
uniform vec2 inputSize;
uniform mat4 rotationMatrix;

void main(){

	vec4 vertexPosition = vec4(attribute_vertex.xyz, 1.0);

	vertexPosition.x *= inputSize.x;
	vertexPosition.x -= inputSize.x / 2.0;
	vertexPosition.y *= inputSize.y;

	vertexPosition *= rotationMatrix;

	vertexPosition.x /= 16.0 / 9.0;

	vertexPosition.x += inputPos.x;
	vertexPosition.y += -inputPos.y;

	gl_Position = vertexPosition;

}
