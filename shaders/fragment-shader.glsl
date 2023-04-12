#version 330 core
in vec4 fragmentPosition;
in vec2 texturePosition;
in vec4 fragmentNormal;

out vec4 FragColor;

uniform vec4 inputColor;

void main(){


	FragColor = inputColor;

} 
