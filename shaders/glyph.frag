#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
} ubuf;

layout(binding = 1) uniform sampler2D atlas;

// The atlas holds premultiplied RGBA. A text glyph is stored as white
// coverage (r = g = b = a) and tinted by the vertex colour (premultiplied);
// a colour glyph (emoji) is stored as it is and asks to be left alone with a
// vertex colour of alpha 0.
void main()
{
    vec4 t = texture(atlas, vTexCoord);
    if (vColor.a == 0.0) {
        fragColor = t * ubuf.qt_Opacity;
    } else {
        fragColor = vColor * (t.a * ubuf.qt_Opacity);
    }
}
