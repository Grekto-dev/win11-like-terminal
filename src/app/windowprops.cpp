#include "windowprops.h"

#include <QGuiApplication>
#include <xcb/xcb.h>

void WindowProps::setWindow(QQuickWindow *window)
{
    if (m_window == window) {
        return;
    }
    m_window = window;
    Q_EMIT windowChanged();
    if (m_window && QGuiApplication::platformName() == QLatin1String("xcb")) {
        auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        if (x11 && x11->connection()) {
            xcb_connection_t *conn = x11->connection();
            const char typeName[] = "_NET_WM_WINDOW_TYPE_NORMAL";
            xcb_intern_atom_reply_t *typeReply = xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 0, sizeof(typeName) - 1, typeName), nullptr);
            const char propName[] = "_NET_WM_WINDOW_TYPE";
            xcb_intern_atom_reply_t *propReply = xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 0, sizeof(propName) - 1, propName), nullptr);
            if (typeReply && propReply) {
                xcb_atom_t normalType = typeReply->atom;
                xcb_change_property(conn, XCB_PROP_MODE_REPLACE, xcb_window_t(m_window->winId()), propReply->atom, XCB_ATOM_ATOM, 32, 1, &normalType);
                free(typeReply);
                free(propReply);
            }
            const char activeName[] = "_NET_ACTIVE_WINDOW";
            xcb_intern_atom_reply_t *activeReply = xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 0, sizeof(activeName) - 1, activeName), nullptr);
            if (activeReply) {
                const xcb_setup_t *setup = xcb_get_setup(conn);
                xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
                xcb_window_t root = iter.data->root;
                xcb_client_message_event_t ev;
                std::memset(&ev, 0, sizeof(ev));
                ev.response_type = XCB_CLIENT_MESSAGE;
                ev.format = 32;
                ev.window = xcb_window_t(m_window->winId());
                ev.type = activeReply->atom;
                ev.data.data32[0] = 2;
                ev.data.data32[1] = XCB_CURRENT_TIME;
                xcb_send_event(conn, 0, root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, (const char *)&ev);
                free(activeReply);
            }
            xcb_flush(conn);
        }
    }
    publish();
}

void WindowProps::setMaxButton(const QRectF &rect)
{
    if (m_rect == rect) {
        return;
    }
    m_rect = rect;
    Q_EMIT maxButtonChanged();
    publish();
}

void WindowProps::setBlurBehind(bool on)
{
    if (m_blur == on) {
        return;
    }
    m_blur = on;
    Q_EMIT blurBehindChanged();
    if (!m_window || QGuiApplication::platformName() != QLatin1String("xcb")) {
        return;
    }
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11 || !x11->connection()) {
        return;
    }
    // The compositor (picom) blurs what is behind the window where this property says: an
    // empty region means all of it (what Windows calls acrylic).
    xcb_connection_t *conn = x11->connection();
    const char name[] = "_KDE_NET_WM_BLUR_BEHIND_REGION";
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 0, sizeof(name) - 1, name), nullptr);
    if (!reply) {
        return;
    }
    const xcb_atom_t atom = reply->atom;
    free(reply);
    if (on) {
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, xcb_window_t(m_window->winId()), atom, XCB_ATOM_CARDINAL, 32, 0, nullptr);
    } else {
        xcb_delete_property(conn, xcb_window_t(m_window->winId()), atom);
    }
    xcb_flush(conn);
}

void WindowProps::publish()
{
    if (!m_window || m_rect.isEmpty() || QGuiApplication::platformName() != QLatin1String("xcb")) {
        return;
    }
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11 || !x11->connection()) {
        return;
    }
    xcb_connection_t *conn = x11->connection();
    static xcb_atom_t atom = XCB_ATOM_NONE;
    if (atom == XCB_ATOM_NONE) {
        const char name[] = "_WIN11_MAX_BUTTON";
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 0, sizeof(name) - 1, name), nullptr);
        if (!reply) {
            return;
        }
        atom = reply->atom;
        free(reply);
    }
    // In X pixels: the device pixel ratio turns Qt's logical pixels into them.
    const qreal d = m_window->devicePixelRatio();
    const uint32_t values[4] = {uint32_t(m_rect.x() * d), uint32_t(m_rect.y() * d), uint32_t(m_rect.width() * d),
                                uint32_t(m_rect.height() * d)};
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, xcb_window_t(m_window->winId()), atom, XCB_ATOM_CARDINAL, 32, 4, values);
    xcb_flush(conn);
}
