#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QRectF>
#include <QtQml/qqmlregistration.h>

/**
 * Properties on the window's X11 window for the window manager. `_WIN11_MAX_BUTTON`
 * (CARDINAL[4]: x, y, width, height in X pixels, relative to the window) tells the window manager
 * where this client's maximize button is, so that it can open window snap options when
 * the pointer rests there.
 */
class WindowProps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool blurBehind READ blurBehind WRITE setBlurBehind NOTIFY blurBehindChanged)
    Q_PROPERTY(QRectF maxButton READ maxButton WRITE setMaxButton NOTIFY maxButtonChanged)

public:
    explicit WindowProps(QObject *parent = nullptr) : QObject(parent) {}
    QQuickWindow *window() const { return m_window; }
    void setWindow(QQuickWindow *window);
    bool blurBehind() const { return m_blur; }
    void setBlurBehind(bool on);
    QRectF maxButton() const { return m_rect; }
    void setMaxButton(const QRectF &rect);

Q_SIGNALS:
    void windowChanged();
    void maxButtonChanged();
    void blurBehindChanged();

private:
    void publish();
    QQuickWindow *m_window = nullptr;
    QRectF m_rect;
    bool m_blur = false;
};
