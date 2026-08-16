#version 300 es
precision mediump float;

// Input vertex attributes (from the default raylib vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Output fragment color
out vec4 finalColor;

#define MAX_SPOTS 3

struct Spot {
    vec2 pos;      // window coords of spot
    float inner;   // inner fully transparent centre radius
    float radius;  // alpha fades out to this radius
};

uniform Spot spots[MAX_SPOTS];  // Spotlight positions array
uniform float screenWidth;      // Width of the screen

void main()
{
    float alpha = 1.0;

    // Get the position of the current fragment (screen coordinates!)
    vec2 pos = vec2(gl_FragCoord.x, gl_FragCoord.y);

    // Find out which spotlight is nearest. NOTE: uniform arrays are only
    // indexed with the (dynamically uniform) loop counters here — the
    // non-constant `spots[fi]` lookup from the desktop GLSL 330 variant is
    // rejected by strict ES GLSL compilers, so the nearest spot's inner/radius
    // are carried out in plain floats instead.
    float d = 65000.0;  // some high value
    float nearestInner = 0.0;
    float nearestRadius = 0.0;
    bool found = false;

    for (int i = 0; i < MAX_SPOTS; i++)
    {
        for (int j = 0; j < MAX_SPOTS; j++)
        {
            float dj = distance(pos, spots[j].pos) - spots[j].radius + spots[i].radius;

            if (d > dj)
            {
                d = dj;
                nearestInner = spots[i].inner;
                nearestRadius = spots[i].radius;
                found = true;
            }
        }
    }

    // d now equals distance to nearest spot...
    // allowing for the different radii of all spotlights
    if (found)
    {
        if (d > nearestRadius) alpha = 1.0;
        else
        {
            if (d < nearestInner) alpha = 0.0;
            else alpha = (d - nearestInner) / (nearestRadius - nearestInner);
        }
    }

    // Right hand side of screen is dimly lit,
    // could make the threshold value user definable
    if ((pos.x > screenWidth / 2.0) && (alpha > 0.9)) alpha = 0.9;

    finalColor = vec4(0.0, 0.0, 0.0, alpha);
}
