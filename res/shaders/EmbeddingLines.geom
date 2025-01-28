#version 330 core

layout(lines) in;                         // Input is 2 vertices per primitive
layout(line_strip, max_vertices = 2) out; // We will emit exactly 2 vertices

in int vMode[];  // from vertex shader (two elements: vMode[0], vMode[1])
flat out int gMode; // going to fragment shader

void main()
{
    // Combine the highlight modes of both endpoints:
    int combined = max(vMode[0], vMode[1]);

    // Emit first vertex
    gl_Position = gl_in[0].gl_Position;
    gMode = combined; // the entire line will share this value
    EmitVertex();

    // Emit second vertex
    gl_Position = gl_in[1].gl_Position;
    gMode = combined;
    EmitVertex();

    EndPrimitive(); // finishes the line strip
}