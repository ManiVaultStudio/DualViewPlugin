#version 330 core
layout (location = 0) in vec2 aPos;

layout (location = 1) in int aMode; // background or foreground

//flat out int vMode;
out int vMode; 

uniform mat4 projection;

void main()
{
    vec4 pos = vec4(aPos, 0.0, 1.0);
	
	vec4 projectedPos = projection * pos;
	
	vMode = aMode;

    gl_Position = vec4(projectedPos.xy, 0.0, 1.0);

}