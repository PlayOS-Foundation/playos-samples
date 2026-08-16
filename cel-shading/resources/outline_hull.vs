#version 300 es

precision mediump float;

// Input vertex attributes (bound by raylib to locations 0..3)
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in vec4 vertexColor;

// Input uniform values (mvp is updated by raylib)
uniform mat4 mvp;
uniform float outlineThickness;

void main()
{
    // Extrude vertex along its normal to create the hull.
    vec3 extruded = vertexPosition + vertexNormal * outlineThickness;
    gl_Position = mvp * vec4(extruded, 1.0);
}
