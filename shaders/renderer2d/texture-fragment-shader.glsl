#version 330 core

precision mediump float;

in vec2 textureCoord;

uniform sampler2D tex;

void main(){

	float alpha = 1.0;

	gl_FragColor = texture2D(tex, textureCoord);

	gl_FragDepth = 0.0;

}
