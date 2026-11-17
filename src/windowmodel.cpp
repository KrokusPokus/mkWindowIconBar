/*
    SPDX-FileCopyrightText: 2016 Eike Hein <hein@kde.org>
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL

    The classes PlasmaWindow and PlasmaWindowManagement are adapted from
    plasma-workspace/libtaskmanager/waylandtasksmodel.cpp.
*/

#include "windowmodel.h"

#include <qwayland-plasma-window-management.h>

#include <QDataStream>
#include <QFuture>
#include <QFutureWatcher>
#include <QIcon>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWaylandClientExtension>
#include <QtConcurrentRun>
#include <qpa/qplatformwindow_p.h>

#include <algorithm>

#include <fcntl.h>
#include <sys/poll.h>
#include <unistd.h>

namespace WindowSwitcher
{

class PlasmaWindow : public QObject, public QtWayland::org_kde_plasma_window
{
    Q_OBJECT
public:
    using state = QtWayland::org_kde_plasma_window_management::state;

    PlasmaWindow(const QString &uuid, ::org_kde_plasma_window *id, QObject *parent)
        : QObject(parent)
        , org_kde_plasma_window(id)
        , uuid(uuid)
    {
    }
    ~PlasmaWindow() override
    {
        destroy();
    }

    const QString uuid;
    QString title;
    QString appId;
    QIcon icon;
    QFlags<state> windowState;
    QPointer<PlasmaWindow> parentWindow;
    bool wasUnmapped = false;

    bool hasState(state s) const
    {
        return windowState.testFlag(s);
    }

Q_SIGNALS:
    void unmapped();
    void initialStateDone();
    void titleChanged();
    void appIdChanged();
    void iconChanged();
    void stateChanged(uint32_t changedFlags);
    void parentWindowChanged();

protected:
    void org_kde_plasma_window_unmapped() override
    {
        wasUnmapped = true;
        Q_EMIT unmapped();
    }
    void org_kde_plasma_window_initial_state() override
    {
        Q_EMIT initialStateDone();
    }
    void org_kde_plasma_window_title_changed(const QString &title) override
    {
        this->title = title;
        Q_EMIT titleChanged();
    }
    void org_kde_plasma_window_app_id_changed(const QString &app_id) override
    {
        appId = app_id;
        Q_EMIT appIdChanged();
    }
    void org_kde_plasma_window_themed_icon_name_changed(const QString &name) override
    {
        icon = QIcon::fromTheme(name);
        Q_EMIT iconChanged();
    }
    // A pixmap icon: KWin writes the serialized QIcon into a pipe, it is read in a worker thread.
    void org_kde_plasma_window_icon_changed() override
    {
        int pipeFds[2];
        if (pipe2(pipeFds, O_CLOEXEC) != 0) {
            qWarning() << "WindowSwitcher: failed creating pipe";
            return;
        }
        get_icon(pipeFds[1]);
        ::close(pipeFds[1]);
        auto readIcon = [uuid = uuid](int fd) {
            auto closeGuard = qScopeGuard([fd]() {
                ::close(fd);
            });
            pollfd pollFd;
            pollFd.fd = fd;
            pollFd.events = POLLIN;
            QByteArray data;
            while (true) {
                const int ready = poll(&pollFd, 1, 1000);
                if (ready < 0 && errno != EINTR) {
                    qWarning() << "WindowSwitcher: polling for icon of window" << uuid << "failed";
                    return QIcon();
                } else if (ready == 0) {
                    qWarning() << "WindowSwitcher: time out polling for icon of window" << uuid;
                    return QIcon();
                } else {
                    char buffer[4096];
                    const int n = read(fd, buffer, sizeof(buffer));
                    if (n < 0) {
                        qWarning() << "WindowSwitcher: error reading icon of window" << uuid;
                        return QIcon();
                    } else if (n > 0) {
                        data.append(buffer, n);
                    } else {
                        QIcon icon;
                        QDataStream ds(data);
                        ds >> icon;
                        return icon;
                    }
                }
            }
        };
        QFuture<QIcon> future = QtConcurrent::run(readIcon, pipeFds[0]);
        auto watcher = new QFutureWatcher<QIcon>();
        connect(watcher, &QFutureWatcher<QIcon>::finished, this, [this, watcher] {
            icon = watcher->future().result();
            Q_EMIT iconChanged();
        });
        connect(watcher, &QFutureWatcher<QIcon>::finished, watcher, &QObject::deleteLater);
        watcher->setFuture(future);
    }
    void org_kde_plasma_window_state_changed(uint32_t flags) override
    {
        const uint32_t changed = uint32_t(windowState) ^ flags;
        windowState = QFlags<state>::fromInt(flags);
        if (changed != 0) {
            Q_EMIT stateChanged(changed);
        }
    }
    void org_kde_plasma_window_parent_window(::org_kde_plasma_window *parent) override
    {
        PlasmaWindow *parentWindow = nullptr;
        if (parent) {
            parentWindow = dynamic_cast<PlasmaWindow *>(PlasmaWindow::fromObject(parent));
        }
        setParentWindow(parentWindow);
    }

private:
    void setParentWindow(PlasmaWindow *parent)
    {
        const auto old = parentWindow;
        QObject::disconnect(m_parentUnmappedConnection);
        QObject::disconnect(m_parentDestroyedConnection);

        if (parent && !parent->wasUnmapped) {
            parentWindow = QPointer<PlasmaWindow>(parent);
            m_parentUnmappedConnection = QObject::connect(parent, &PlasmaWindow::unmapped, this, [this] {
                setParentWindow(nullptr);
            });
            // QPointer nulling itself wouldn't cause the change signal to be emitted.
            m_parentDestroyedConnection = QObject::connect(parent, &QObject::destroyed, this, &PlasmaWindow::parentWindowChanged);
        } else {
            parentWindow = QPointer<PlasmaWindow>();
            m_parentUnmappedConnection = QMetaObject::Connection();
            m_parentDestroyedConnection = QMetaObject::Connection();
        }

        if (parentWindow.data() != old.data()) {
            Q_EMIT parentWindowChanged();
        }
    }

    QMetaObject::Connection m_parentUnmappedConnection;
    QMetaObject::Connection m_parentDestroyedConnection;
};

class PlasmaWindowManagement : public QWaylandClientExtensionTemplate<PlasmaWindowManagement>, public QtWayland::org_kde_plasma_window_management
{
    Q_OBJECT
public:
    // The global must be bound with at least version 17 (window_with_uuid replaces window)
    static constexpr int s_version = 17;

    PlasmaWindowManagement()
        : QWaylandClientExtensionTemplate(s_version)
    {
        connect(this, &QWaylandClientExtension::activeChanged, this, [this] {
            if (!isActive()) {
                wl_proxy_destroy(reinterpret_cast<wl_proxy *>(object()));
            }
        });
        initialize();
        if (!isActive()) {
            qWarning() << "WindowSwitcher: the PlasmaWindowManagement protocol is not active. Not running in a Plasma Wayland session?";
        }
    }
    ~PlasmaWindowManagement() override
    {
        if (isActive()) {
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(object()));
        }
    }

Q_SIGNALS:
    void windowCreated(WindowSwitcher::PlasmaWindow *window);

protected:
    void org_kde_plasma_window_management_window_with_uuid(uint32_t id, const QString &uuid) override
    {
        Q_UNUSED(id)
        Q_EMIT windowCreated(new PlasmaWindow(uuid, get_window_by_uuid(uuid), nullptr));
    }
};

} // namespace WindowSwitcher

using WindowSwitcher::PlasmaWindow;
using WindowSwitcher::PlasmaWindowManagement;
using State = PlasmaWindow::state;

WindowModel::WindowModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_management(std::make_unique<PlasmaWindowManagement>())
{
    connect(m_management.get(), &PlasmaWindowManagement::activeChanged, this, [this] {
        // KWin restarted or the global went away: all windows are gone
        clear();
    });

    connect(m_management.get(), &PlasmaWindowManagement::windowCreated, this, [this](PlasmaWindow *window) {
        // Owned by the model until it is removed. A window is shown once it has sent its initial state.
        window->setParent(this);
        connect(window, &PlasmaWindow::initialStateDone, this, [this, window] {
            addWindow(window);
        });
        connect(window, &PlasmaWindow::unmapped, this, [this, window] {
            removeWindow(window);
        });
    });
}

WindowModel::~WindowModel()
{
    // The windows must be destroyed before the window management global
    const QList<PlasmaWindow *> windows = findChildren<PlasmaWindow *>(Qt::FindDirectChildrenOnly);
    qDeleteAll(windows);
}

int WindowModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int WindowModel::count() const
{
    return static_cast<int>(m_rows.size());
}

QHash<int, QByteArray> WindowModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert(IsActive, QByteArrayLiteral("IsActive"));
    roles.insert(IsMinimized, QByteArrayLiteral("IsMinimized"));
    roles.insert(IsDemandingAttention, QByteArrayLiteral("IsDemandingAttention"));
    roles.insert(AppId, QByteArrayLiteral("AppId"));
    return roles;
}

QVariant WindowModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::DoNotUseParent)) {
        return {};
    }
    const PlasmaWindow *window = m_rows.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return window->title;
    case Qt::DecorationRole:
        // Exactly what KWin reports: no .desktop file icon takes precedence
        return window->icon;
    case IsActive:
        return isActive(window);
    case IsMinimized:
        return window->hasState(State::state_minimized);
    case IsDemandingAttention:
        return isDemandingAttention(window);
    case AppId:
        return window->appId;
    default:
        return {};
    }
}

void WindowModel::addWindow(PlasmaWindow *window)
{
    if (std::ranges::find(m_windows, window) != m_windows.end()) {
        return;
    }
    m_windows.push_back(window);

    connect(window, &PlasmaWindow::titleChanged, this, [this, window] {
        emitChanged(window, {Qt::DisplayRole});
    });
    connect(window, &PlasmaWindow::iconChanged, this, [this, window] {
        emitChanged(window, {Qt::DecorationRole});
    });
    connect(window, &PlasmaWindow::appIdChanged, this, [this, window] {
        emitChanged(window, {AppId});
    });
    connect(window, &PlasmaWindow::stateChanged, this, [this, window](uint32_t changed) {
        if (changed & State::state_skiptaskbar) {
            updateShown(window);
        }
        if (changed & State::state_minimized) {
            emitChanged(window, {IsMinimized});
        }
        // Activity and attention of a transient are shown at its leader
        if (changed & State::state_active) {
            emitChanged(leaderOf(window), {IsActive});
        }
        if (changed & State::state_demands_attention) {
            emitChanged(leaderOf(window), {IsDemandingAttention});
        }
    });
    connect(window, &PlasmaWindow::parentWindowChanged, this, [this, window] {
        updateShown(window);
        // The leader of a transient that is active or demands attention changed
        for (PlasmaWindow *row : std::as_const(m_rows)) {
            emitChanged(row, {IsActive, IsDemandingAttention});
        }
    });

    updateShown(window);
    if (window->parentWindow) {
        emitChanged(leaderOf(window), {IsActive, IsDemandingAttention});
    }
}

void WindowModel::removeWindow(PlasmaWindow *window)
{
    const int row = rowOf(window);
    if (row >= 0) {
        beginRemoveRows(QModelIndex(), row, row);
        m_rows.erase(m_rows.begin() + row);
        endRemoveRows();
        Q_EMIT countChanged();
    }
    const PlasmaWindow *leader = window->parentWindow ? leaderOf(window) : nullptr;
    std::erase(m_windows, window);
    disconnect(window, nullptr, this, nullptr);
    // Not deleted right away: we are inside a signal of this window, emitted from its Wayland event handler
    window->deleteLater();

    if (leader) {
        emitChanged(leader, {IsActive, IsDemandingAttention});
    }
}

void WindowModel::clear()
{
    beginResetModel();
    m_rows.clear();
    for (PlasmaWindow *window : std::as_const(m_windows)) {
        disconnect(window, nullptr, this, nullptr);
        window->deleteLater();
    }
    m_windows.clear();
    endResetModel();
    Q_EMIT countChanged();
}

bool WindowModel::isShown(const PlasmaWindow *window) const
{
    return !window->hasState(State::state_skiptaskbar) && !window->parentWindow;
}

void WindowModel::updateShown(PlasmaWindow *window)
{
    const int row = rowOf(window);
    const bool shown = isShown(window);
    if (shown && row < 0) {
        const int newRow = static_cast<int>(m_rows.size());
        beginInsertRows(QModelIndex(), newRow, newRow);
        m_rows.push_back(window);
        endInsertRows();
        Q_EMIT countChanged();
    } else if (!shown && row >= 0) {
        beginRemoveRows(QModelIndex(), row, row);
        m_rows.erase(m_rows.begin() + row);
        endRemoveRows();
        Q_EMIT countChanged();
    }
}

int WindowModel::rowOf(const PlasmaWindow *window) const
{
    const auto it = std::ranges::find(m_rows, window);
    return it == m_rows.end() ? -1 : static_cast<int>(it - m_rows.begin());
}

void WindowModel::emitChanged(const PlasmaWindow *window, const QList<int> &roles)
{
    const int row = rowOf(window);
    if (row >= 0) {
        const QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, roles);
    }
}

const PlasmaWindow *WindowModel::leaderOf(const PlasmaWindow *window)
{
    // Bounded: a broken parent chain must not hang the panel
    for (int depth = 0; window->parentWindow && depth < 32; ++depth) {
        window = window->parentWindow.data();
    }
    return window;
}

bool WindowModel::isActive(const PlasmaWindow *window) const
{
    return std::ranges::any_of(m_windows, [this, window](const PlasmaWindow *candidate) {
        return candidate->hasState(State::state_active) && leaderOf(candidate) == window;
    });
}

bool WindowModel::isDemandingAttention(const PlasmaWindow *window) const
{
    return std::ranges::any_of(m_windows, [this, window](const PlasmaWindow *candidate) {
        return candidate->hasState(State::state_demands_attention) && leaderOf(candidate) == window;
    });
}

void WindowModel::activateOrMinimize(int row)
{
    if (row < 0 || row >= count()) {
        return;
    }
    PlasmaWindow *window = m_rows.at(row);

    if (isActive(window) && !window->hasState(State::state_minimized)) {
        if (window->hasState(State::state_minimizable)) {
            window->set_state(State::state_minimized, State::state_minimized);
        }
        return;
    }

    // A transient demanding attention (e.g. a dialog asking something) is brought to the front instead
    PlasmaWindow *target = window;
    for (PlasmaWindow *candidate : std::as_const(m_windows)) {
        if (candidate != window && candidate->hasState(State::state_demands_attention) && leaderOf(candidate) == window) {
            target = candidate;
            break;
        }
    }
    target->set_state(State::state_active | State::state_minimized, State::state_active);
}

void WindowModel::close(int row)
{
    if (row < 0 || row >= count()) {
        return;
    }
    m_rows.at(row)->close();
}

void WindowModel::publishGeometry(int row, QQuickItem *item)
{
    if (row < 0 || row >= count() || !item || !item->parentItem() || !item->window()) {
        return;
    }
    auto *waylandWindow = item->window()->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    if (!waylandWindow || !waylandWindow->surface()) {
        return;
    }
    // Relative to the surface of the panel
    QRect rect(item->x(), item->y(), item->width(), item->height());
    rect.moveTopLeft(item->parentItem()->mapToScene(rect.topLeft()).toPoint());
    m_rows.at(row)->set_minimized_geometry(waylandWindow->surface(), rect.x(), rect.y(), rect.width(), rect.height());
}

#include "windowmodel.moc"
