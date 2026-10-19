#include "keyrouter.h"

#include "appsettings.h"
#include "view/terminalsession.h"
#include "view/terminalview.h"

#include <QKeyEvent>
#include <QQuickItem>

void KeyRouter::setWindow(QQuickWindow *window)
{
    if (m_window == window) {
        return;
    }
    if (m_window) {
        m_window->removeEventFilter(this);
    }
    m_window = window;
    if (m_window) {
        m_window->installEventFilter(this);
    }
    Q_EMIT windowChanged();
}

void KeyRouter::setHandler(const QJSValue &handler)
{
    m_handler = handler;
    Q_EMIT handlerChanged();
}

bool KeyRouter::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window || event->type() != QEvent::KeyPress || !m_handler.isCallable()) {
        return false;
    }
    const auto *ke = static_cast<QKeyEvent *>(event);
    if (ke->isAutoRepeat() && (ke->key() == Qt::Key_Shift || ke->key() == Qt::Key_Control)) {
        return false;
    }
    const QString keys = AppSettings::keysOf(ke->key(), ke->modifiers());
    if (keys.isEmpty()) {
        return false;
    }
    AppSettings *settings = AppSettings::instance();
    const QString id = settings->actionForKeys(keys);
    if (id.isEmpty()) {
        return false;
    }
    const bool inTerminal = qobject_cast<TerminalView *>(m_window->activeFocusItem()) != nullptr;
    QJSEngine *engine = qjsEngine(this);
    if (!engine) {
        return false;
    }
    const QJSValue result = m_handler.call({id, engine->toScriptValue(settings->argsForKeys(keys)), inTerminal});
    return result.toBool();
}
