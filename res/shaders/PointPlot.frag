#version 330 core
out vec4 FragColor;

uniform vec4 pointColor;

void main() {
    float dist = length(gl_PointCoord - vec2(0.5, 0.5));
    if (dist > 0.5) {
        discard;
    }
    FragColor = pointColor;
}