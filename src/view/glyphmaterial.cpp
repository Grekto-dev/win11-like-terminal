#include "glyphmaterial.h"

#include <QSGTexture>

namespace
{
class GlyphShader : public QSGMaterialShader
{
public:
    GlyphShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/glyph.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/glyph.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override
    {
        QByteArray *buf = state.uniformData();
        bool changed = false;
        if (state.isMatrixDirty()) {
            const QMatrix4x4 m = state.combinedMatrix();
            memcpy(buf->data(), m.constData(), 64);
            changed = true;
        }
        if (state.isOpacityDirty()) {
            const float opacity = state.opacity();
            memcpy(buf->data() + 64, &opacity, 4);
            changed = true;
        }
        return changed;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture, QSGMaterial *newMaterial,
                            QSGMaterial *) override
    {
        if (binding != 1) {
            return;
        }
        auto *material = static_cast<GlyphMaterial *>(newMaterial);
        if (QSGTexture *t = material->texture()) {
            t->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
            *texture = t;
        }
    }
};
}

GlyphMaterial::GlyphMaterial()
{
    setFlag(Blending, true);
}

QSGMaterialType *GlyphMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *GlyphMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new GlyphShader;
}

int GlyphMaterial::compare(const QSGMaterial *other) const
{
    const auto *o = static_cast<const GlyphMaterial *>(other);
    if (m_texture == o->m_texture) {
        return 0;
    }
    return m_texture < o->m_texture ? -1 : 1;
}
