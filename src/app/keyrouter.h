#pragma once

#include <QJSValue>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

/**
 * Turns key presses into actions with the `keybindings` of settings.json. It sits
 * in front of every item of its window (an event filter), so a binding works
 * whichever pane, tab or box has the focus. `handler(actionId, args, inTerminal)`
 * says whether it ran the action; when it did not (Ctrl+C with nothing selected)
 * the key goes on to the item, which for a terminal sends it to the program.
 */
class KeyRouter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QJSValue handler READ handler WRITE setHandler NOTIFY handlerChanged)

public:
    explicit KeyRouter(QObject *parent = nullptr) : QObject(parent) {}
    QQuickWindow *window() const { return m_window; }
    void setWindow(QQuickWindow *window);
    QJSValue handler() const { return m_handler; }
    void setHandler(const QJSValue &handler);

Q_SIGNALS:
    void windowChanged();
    void handlerChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<QQuickWindow> m_window;
    QJSValue m_handler;
};
