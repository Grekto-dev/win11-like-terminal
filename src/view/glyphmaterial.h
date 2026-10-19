#pragma once

#include <QSGMaterial>
#include <QSGMaterialShader>

class QSGTexture;

/** One vertex of a glyph quad: position, atlas coordinate, premultiplied colour. */
struct GlyphVertex
{
    float x, y;
    float u, v;
    uchar r, g, b, a;
};

/**
 * Draws quads from the glyph atlas: the atlas texel's coverage times the vertex
 * colour, or the texel itself when the vertex colour has alpha 0 (see glyph.frag).
 */
class GlyphMaterial : public QSGMaterial
{
public:
    GlyphMaterial();

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
    int compare(const QSGMaterial *other) const override;

    void setTexture(QSGTexture *texture) { m_texture = texture; }
    QSGTexture *texture() const { return m_texture; }

private:
    QSGTexture *m_texture = nullptr;
};
