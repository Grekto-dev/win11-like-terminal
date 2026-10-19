#pragma once

#include <QColor>
#include <QImage>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class FluentSymbol : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(int size READ size WRITE setSize NOTIFY changed)
    Q_PROPERTY(QString variant READ variant WRITE setVariant NOTIFY changed)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)

public:
    explicit FluentSymbol(QQuickItem *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &name);
    int size() const { return m_size; }
    void setSize(int size);
    QString variant() const { return m_variant; }
    void setVariant(const QString &variant);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    bool available() const { return !m_path.isEmpty(); }

    void componentComplete() override;

    static QString pathFor(const QString &name, int size, const QString &variant);
    static QStringList searchDirs();
    static QImage render(const QString &path, int px, const QColor &color);
    static int textureCount();

Q_SIGNALS:
    void changed();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    void resolve();

    QString m_name;
    int m_size = 16;
    QString m_variant = QStringLiteral("regular");
    QColor m_color = Qt::white;
    QString m_path;
    bool m_complete = false;
};
