#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 size;      // the item's size, in logical pixels
    float radius;   // the corner radius, in logical pixels
} ubuf;

layout(binding = 1) uniform sampler2D source;

// Clips the window's content to a rounded rectangle with an antialiased edge computed
// from the distance to the shape (a signed distance field), so that the four corners are
// true arcs whatever the content under them (opaque, translucent, square corners of the
// tab bar or of the panes). `clip: true` in QtQuick clips to the rectangle, never to the
// radius, which is what left square artifacts at the corners.
void main()
{
    vec2 p = qt_TexCoord0 * ubuf.size;
    vec2 half_size = ubuf.size * 0.5;
    vec2 q = abs(p - half_size) - (half_size - vec2(ubuf.radius));
    float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - ubuf.radius;
    float coverage = clamp(0.5 - dist, 0.0, 1.0);
    // The source is premultiplied: scaling all four channels keeps it so.
    fragColor = texture(source, qt_TexCoord0) * (coverage * ubuf.qt_Opacity);
}
