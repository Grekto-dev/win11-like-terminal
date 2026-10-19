#include "fluentsymbol.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QCache>
#include <QMutex>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QtMath>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QSvgRenderer>

#include <memory>

namespace
{
/** name -> {size -> {variant -> path}}, read once from search dirs. */
using Index = QHash<QString, QHash<int, QHash<QString, QString>>>;

const Index &index()
{
    static const Index idx = [] {
        Index out;
        static const QRegularExpression file(QStringLiteral("^(.+)_(\\d+)_(regular|filled|light|color)\\.svg$"));
        const QStringList dirs = FluentSymbol::searchDirs();
        for (auto it = dirs.crbegin(); it != dirs.crend(); ++it) {
            const QDir dir(*it);
            for (const QString &f : dir.entryList({QStringLiteral("*.svg")}, QDir::Files)) {
                const QRegularExpressionMatch m = file.match(f);
                if (m.hasMatch()) {
                    out[m.captured(1)][m.captured(2).toInt()][m.captured(3)] = dir.filePath(f);
                }
            }
        }
        return out;
    }();
    return idx;
}

QMutex &lock()
{
    static QMutex m;
    return m;
}

QSvgRenderer *rendererFor(const QString &path)
{
    static QHash<QString, std::shared_ptr<QSvgRenderer>> cache;
    auto it = cache.find(path);
    if (it == cache.end()) {
        it = cache.insert(path, std::make_shared<QSvgRenderer>(path));
    }
    return it->get();
}

QString keyFor(const QString &path, int px, const QColor &color)
{
    return QStringLiteral("%1|%2|%3").arg(path).arg(px).arg(color.isValid() ? color.rgba() : 0u, 0, 16);
}

struct Shared {
    QSGTexture *texture = nullptr;
    int users = 0;
};
QHash<QQuickWindow *, QHash<QString, Shared>> &textures()
{
    static QHash<QQuickWindow *, QHash<QString, Shared>> t;
    return t;
}

void release(QQuickWindow *window, const QString &key)
{
    QMutexLocker locker(&lock());
    auto w = textures().find(window);
    if (w == textures().end()) {
        return;
    }
    auto it = w->find(key);
    if (it != w->end() && --it->users <= 0) {
        delete it->texture;
        w->erase(it);
    }
    if (w->isEmpty()) {
        textures().erase(w);
    }
}

class SymbolNode : public QSGSimpleTextureNode
{
public:
    ~SymbolNode() override
    {
        if (!key.isEmpty()) {
            release(window, key);
        }
    }
    QQuickWindow *window = nullptr;
    QString key;
};
}

QImage FluentSymbol::render(const QString &path, int px, const QColor &color)
{
    static QCache<QString, QImage> images(1024 * 1024);
    const QString key = keyFor(path, px, color);
    QMutexLocker locker(&lock());
    if (const QImage *hit = images.object(key)) {
        return *hit;
    }
    QSvgRenderer *svg = rendererFor(path);
    if (!svg->isValid() || px <= 0) {
        return {};
    }
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        svg->render(&p, QRectF(0, 0, px, px));
        if (color.isValid()) {
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(img.rect(), color);
        }
    }
    images.insert(key, new QImage(img), int(img.sizeInBytes()));
    return img;
}

FluentSymbol::FluentSymbol(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setImplicitSize(m_size, m_size);
}

QStringList FluentSymbol::searchDirs()
{
    QStringList out;
    out << QStringLiteral(":/icons/fluent");
#ifdef WIN11TERMINAL_SOURCE_DATA_DIR
    out << QStringLiteral(WIN11TERMINAL_SOURCE_DATA_DIR "/icons/fluent");
#endif
    for (const QString &d : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        out << d + QStringLiteral("/win11terminal/icons/fluent");
        out << d + QStringLiteral("/win11-like-terminal/icons/fluent");
    }
    return out;
}

QString FluentSymbol::pathFor(const QString &name, int size, const QString &variant)
{
    const auto sizes = index().value(name);
    if (sizes.isEmpty()) {
        return {};
    }
    QList<int> have;
    for (auto it = sizes.cbegin(); it != sizes.cend(); ++it) {
        if (it.value().contains(variant)) {
            have << it.key();
        }
    }
    if (have.isEmpty()) {
        return {};
    }
    std::sort(have.begin(), have.end());
    int best = have.last();
    for (int s : std::as_const(have)) {
        if (s >= size) {
            best = s;
            break;
        }
    }
    return sizes.value(best).value(variant);
}

void FluentSymbol::componentComplete()
{
    QQuickItem::componentComplete();
    m_complete = true;
    resolve();
}

void FluentSymbol::resolve()
{
    if (!m_complete) {
        return;
    }
    const QString path = pathFor(m_name, m_size, m_variant);
    m_path = path;
    update();
    Q_EMIT changed();
}

void FluentSymbol::setName(const QString &name)
{
    if (name != m_name) {
        m_name = name;
        resolve();
    }
}

void FluentSymbol::setSize(int size)
{
    if (size != m_size && size > 0) {
        m_size = size;
        setImplicitSize(size, size);
        resolve();
    }
}

void FluentSymbol::setVariant(const QString &variant)
{
    if (variant != m_variant) {
        m_variant = variant;
        resolve();
    }
}

void FluentSymbol::setColor(const QColor &color)
{
    if (color != m_color) {
        m_color = color;
        update();
        Q_EMIT changed();
    }
}

int FluentSymbol::textureCount()
{
    QMutexLocker locker(&lock());
    int n = 0;
    for (const auto &w : std::as_const(textures())) {
        n += int(w.size());
    }
    return n;
}

QSGNode *FluentSymbol::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *node = static_cast<SymbolNode *>(old);
    QQuickWindow *win = window();
    const qreal side = std::min<qreal>({qreal(m_size), width(), height()});
    if (m_path.isEmpty() || !win || side <= 0) {
        delete node;
        return nullptr;
    }
    const qreal dpr = win->effectiveDevicePixelRatio();
    const int px = qCeil(side * dpr);
    const QColor color = m_variant == QLatin1String("color") ? QColor() : m_color;
    const QString key = keyFor(m_path, px, color);
    if (!node || node->key != key) {
        QSGTexture *texture = nullptr;
        {
            QMutexLocker locker(&lock());
            const auto w = textures().constFind(win);
            if (w != textures().constEnd()) {
                const auto found = w->constFind(key);
                texture = found != w->constEnd() ? found->texture : nullptr;
            }
        }
        if (!texture) {
            const QImage img = render(m_path, px, color);
            if (img.isNull()) {
                delete node;
                return nullptr;
            }
            texture = win->createTextureFromImage(img, QQuickWindow::TextureHasAlphaChannel);
            QMutexLocker locker(&lock());
            textures()[win].insert(key, {texture, 0});
        }
        {
            QMutexLocker locker(&lock());
            ++textures()[win][key].users;
        }
        if (node) {
            release(node->window, node->key);
        } else {
            node = new SymbolNode;
            node->setOwnsTexture(false);
            node->setFiltering(QSGTexture::Linear);
        }
        node->window = win;
        node->key = key;
        node->setTexture(texture);
    }
    node->setRect(QRectF((width() - side) / 2, (height() - side) / 2, side, side));
    return node;
}

void FluentSymbol::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

void FluentSymbol::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change == ItemDevicePixelRatioHasChanged || change == ItemSceneChange) {
        update();
    }
}
