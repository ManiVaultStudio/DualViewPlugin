#version 330 core
out vec4 FragColor;

uniform vec4 backgroundColor;
uniform vec4 foregroundColor;

//flat in int vMode; // 0 for background, 1 for foreground
flat in int gMode; // for geometry shader


void main()
{
    if (gMode == 1) {
        FragColor = foregroundColor;
    } else {
        FragColor = backgroundColor;
    }
}
