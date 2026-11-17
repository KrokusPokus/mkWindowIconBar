/*
    SPDX-FileCopyrightText: 2016 Eike Hein <hein@kde.org>
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL

    The classes PlasmaWindow and PlasmaWindowManagement are adapted from
    plasma-workspace/libtaskmanager/waylandtasksmodel.cpp.
*/

#include "windowmodel.h"

#include <qwayland-plasma-window-management.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusArgument>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QIcon>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QWaylandClientExtension>
#include <QtConcurrentRun>
#include <qpa/qplatformwindow_p.h>

#include <KService>
#include <KSycoca>

#include <algorithm>

#include <fcntl.h>
#include <sys/poll.h>
#include <unistd.h>

namespace WindowIconBar
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
    quint32 pid = 0;
    QString appName; // cache of WindowModel::applicationName(), empty: not determined yet
    QIcon icon;
    QFlags<state> windowState;
    QRect geometry;
    QList<QString> virtualDesktops; // empty: on all virtual desktops
    quint64 lastActivated = 0;       // WindowModel::m_activationCounter when it was activated last, 0: never
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
    void pidChanged();
    void iconChanged();
    void stateChanged(uint32_t changedFlags);
    void geometryChanged();
    void virtualDesktopsChanged();
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
    void org_kde_plasma_window_pid_changed(uint32_t pid) override
    {
        this->pid = pid;
        Q_EMIT pidChanged();
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
            qWarning() << "WindowIconBar: failed creating pipe";
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
                    qWarning() << "WindowIconBar: polling for icon of window" << uuid << "failed";
                    return QIcon();
                } else if (ready == 0) {
                    qWarning() << "WindowIconBar: time out polling for icon of window" << uuid;
                    return QIcon();
                } else {
                    char buffer[4096];
                    const int n = read(fd, buffer, sizeof(buffer));
                    if (n < 0) {
                        qWarning() << "WindowIconBar: error reading icon of window" << uuid;
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
    void org_kde_plasma_window_geometry(int32_t x, int32_t y, uint32_t width, uint32_t height) override
    {
        geometry = QRect(x, y, width, height);
        Q_EMIT geometryChanged();
    }
    void org_kde_plasma_window_virtual_desktop_entered(const QString &id) override
    {
        virtualDesktops.push_back(id);
        Q_EMIT virtualDesktopsChanged();
    }
    void org_kde_plasma_window_virtual_desktop_left(const QString &id) override
    {
        virtualDesktops.removeAll(id);
        Q_EMIT virtualDesktopsChanged();
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
            qWarning() << "WindowIconBar: the PlasmaWindowManagement protocol is not active. Not running in a Plasma Wayland session?";
        }
    }
    ~PlasmaWindowManagement() override
    {
        if (isActive()) {
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(object()));
        }
    }

Q_SIGNALS:
    void windowCreated(WindowIconBar::PlasmaWindow *window);

protected:
    void org_kde_plasma_window_management_window_with_uuid(uint32_t id, const QString &uuid) override
    {
        Q_UNUSED(id)
        Q_EMIT windowCreated(new PlasmaWindow(uuid, get_window_by_uuid(uuid), nullptr));
    }
};

} // namespace WindowIconBar

using WindowIconBar::PlasmaWindow;
using WindowIconBar::PlasmaWindowManagement;
using State = PlasmaWindow::state;

namespace
{

const QString kKWinService = QStringLiteral("org.kde.KWin");
const QString kVirtualDesktopPath = QStringLiteral("/VirtualDesktopManager");
const QString kVirtualDesktopInterface = QStringLiteral("org.kde.KWin.VirtualDesktopManager");

// Geometry of the screen a point is on, or of the nearest screen (from libtaskmanager's tasktools.cpp)
QRect screenGeometryAt(const QPoint &pos)
{
    if (pos.isNull()) {
        return {};
    }

    const QList<QScreen *> &screens = QGuiApplication::screens();
    QRect screenGeometry;
    int shortestDistance = INT_MAX;

    for (const QScreen *screen : screens) {
        const QRect &geometry = screen->geometry();

        if (geometry.contains(pos)) {
            return geometry;
        }

        int distance = QPoint(geometry.topLeft() - pos).manhattanLength();
        distance = qMin(distance, QPoint(geometry.topRight() - pos).manhattanLength());
        distance = qMin(distance, QPoint(geometry.bottomRight() - pos).manhattanLength());
        distance = qMin(distance, QPoint(geometry.bottomLeft() - pos).manhattanLength());

        if (distance < shortestDistance) {
            shortestDistance = distance;
            screenGeometry = geometry;
        }
    }

    return screenGeometry;
}

} // namespace

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

    // The current virtual desktop and the list of desktops come from KWin over D-Bus: change signals, and the
    // values read at start. The signal connections survive a restart of KWin's service; the values are read
    // again when it appears (also if it was not there yet at start).
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(kKWinService, kVirtualDesktopPath, kVirtualDesktopInterface, QStringLiteral("currentChanged"),
                this, SLOT(setCurrentVirtualDesktop(QString)));
    bus.connect(kKWinService, kVirtualDesktopPath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
                this, SLOT(onVirtualDesktopPropertiesChanged(QString, QVariantMap, QStringList)));
    auto *serviceWatcher = new QDBusServiceWatcher(kKWinService, bus, QDBusServiceWatcher::WatchForRegistration, this);
    connect(serviceWatcher, &QDBusServiceWatcher::serviceRegistered, this, &WindowModel::fetchVirtualDesktops);
    fetchVirtualDesktops();

    // Installed or removed applications: the names are determined again
    connect(KSycoca::self(), &KSycoca::databaseChanged, this, [this] {
        for (PlasmaWindow *window : std::as_const(m_windows)) {
            window->appName.clear();
        }
        if (!m_rows.empty()) {
            Q_EMIT dataChanged(index(0), index(count() - 1), {AppName});
        }
    });

    // A window can move to another screen because the screens change
    auto onScreensChanged = [this] {
        if (m_filterByScreen) {
            refilter();
        }
    };
    connect(qGuiApp, &QGuiApplication::screenAdded, this, onScreensChanged);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, onScreensChanged);
}

void WindowModel::fetchVirtualDesktops()
{
    QDBusMessage message = QDBusMessage::createMethodCall(kKWinService, kVirtualDesktopPath,
                                                          QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
    message << kVirtualDesktopInterface;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QVariantMap> reply = *call;
        if (reply.isValid()) {
            onVirtualDesktopPropertiesChanged(kVirtualDesktopInterface, reply.value(), {});
        }
        call->deleteLater();
    });
}

void WindowModel::onVirtualDesktopPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    Q_UNUSED(invalidated)
    if (interface != kVirtualDesktopInterface) {
        return;
    }
    if (const auto it = changed.constFind(QStringLiteral("current")); it != changed.constEnd()) {
        setCurrentVirtualDesktop(it->toString());
    }
    if (const auto it = changed.constFind(QStringLiteral("desktops")); it != changed.constEnd()) {
        setVirtualDesktopsFromDBus(*it);
    }
}

void WindowModel::setVirtualDesktopsFromDBus(const QVariant &value)
{
    if (!value.canConvert<QDBusArgument>()) {
        return;
    }
    // a(uss): position, id, name
    const QDBusArgument argument = value.value<QDBusArgument>();
    QList<std::pair<uint, QVariantMap>> desktops;
    argument.beginArray();
    while (!argument.atEnd()) {
        uint position = 0;
        QString id;
        QString name;
        argument.beginStructure();
        argument >> position >> id >> name;
        argument.endStructure();
        desktops.append({position, QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}}});
    }
    argument.endArray();
    std::ranges::stable_sort(desktops, {}, &std::pair<uint, QVariantMap>::first);

    QVariantList list;
    for (const auto &desktop : std::as_const(desktops)) {
        list.append(desktop.second);
    }
    if (list != m_virtualDesktops) {
        m_virtualDesktops = list;
        Q_EMIT virtualDesktopsChanged();
    }
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

bool WindowModel::filterByScreen() const
{
    return m_filterByScreen;
}

void WindowModel::setFilterByScreen(bool filter)
{
    if (m_filterByScreen != filter) {
        m_filterByScreen = filter;
        refilter();
        Q_EMIT filterByScreenChanged();
    }
}

QRect WindowModel::screenGeometry() const
{
    return m_screenGeometry;
}

void WindowModel::setScreenGeometry(const QRect &geometry)
{
    if (m_screenGeometry != geometry) {
        m_screenGeometry = geometry;
        if (m_filterByScreen) {
            refilter();
        }
        Q_EMIT screenGeometryChanged();
    }
}

bool WindowModel::filterByVirtualDesktop() const
{
    return m_filterByVirtualDesktop;
}

void WindowModel::setFilterByVirtualDesktop(bool filter)
{
    if (m_filterByVirtualDesktop != filter) {
        m_filterByVirtualDesktop = filter;
        refilter();
        Q_EMIT filterByVirtualDesktopChanged();
    }
}

void WindowModel::setCurrentVirtualDesktop(const QString &id)
{
    if (m_currentVirtualDesktop != id) {
        m_currentVirtualDesktop = id;
        if (m_filterByVirtualDesktop) {
            refilter();
        }
        Q_EMIT currentVirtualDesktopChanged();
    }
}

QVariantList WindowModel::virtualDesktops() const
{
    return m_virtualDesktops;
}

QString WindowModel::currentVirtualDesktop() const
{
    return m_currentVirtualDesktop;
}

QHash<int, QByteArray> WindowModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert(IsActive, QByteArrayLiteral("IsActive"));
    roles.insert(IsMinimized, QByteArrayLiteral("IsMinimized"));
    roles.insert(IsDemandingAttention, QByteArrayLiteral("IsDemandingAttention"));
    roles.insert(IsClosable, QByteArrayLiteral("IsClosable"));
    roles.insert(IsMinimizable, QByteArrayLiteral("IsMinimizable"));
    roles.insert(IsMaximized, QByteArrayLiteral("IsMaximized"));
    roles.insert(IsMaximizable, QByteArrayLiteral("IsMaximizable"));
    roles.insert(IsKeepAbove, QByteArrayLiteral("IsKeepAbove"));
    roles.insert(VirtualDesktops, QByteArrayLiteral("VirtualDesktops"));
    roles.insert(IsOnAllVirtualDesktops, QByteArrayLiteral("IsOnAllVirtualDesktops"));
    roles.insert(IsVirtualDesktopsChangeable, QByteArrayLiteral("IsVirtualDesktopsChangeable"));
    roles.insert(ScreenGeometry, QByteArrayLiteral("ScreenGeometry"));
    roles.insert(AppName, QByteArrayLiteral("AppName"));
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
        // Exactly what KWin reports: no .desktop file icon takes precedence. A generic window icon if there is
        // none (a pixmap icon that could not be read, a theme icon name the icon theme does not know).
        if (!window->icon.isNull()) {
            return window->icon;
        }
        return QIcon::fromTheme(QStringLiteral("preferences-system-windows"));
    case IsActive:
        return isActive(window);
    case IsMinimized:
        return window->hasState(State::state_minimized);
    case IsDemandingAttention:
        return isDemandingAttention(window);
    case IsClosable:
        return window->hasState(State::state_closeable);
    case IsMinimizable:
        return window->hasState(State::state_minimizable);
    case IsMaximized:
        return window->hasState(State::state_maximized);
    case IsMaximizable:
        return window->hasState(State::state_maximizable);
    case IsKeepAbove:
        return window->hasState(State::state_keep_above);
    case VirtualDesktops:
        return QStringList(window->virtualDesktops);
    case IsOnAllVirtualDesktops:
        return window->hasState(State::state_on_all_desktops) || window->virtualDesktops.isEmpty();
    case IsVirtualDesktopsChangeable:
        return window->hasState(State::state_virtual_desktop_changeable);
    case ScreenGeometry:
        return screenGeometryAt(window->geometry.center());
    case AppName:
        if (window->appName.isEmpty()) {
            // Determined when it is needed (the tooltip), then cached. data() is const, the cache is not part of
            // the state of the model.
            const_cast<PlasmaWindow *>(window)->appName = applicationName(window);
        }
        return window->appName;
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
    if (window->hasState(State::state_active)) {
        window->lastActivated = ++m_activationCounter;
    }

    connect(window, &PlasmaWindow::titleChanged, this, [this, window] {
        emitChanged(window, {Qt::DisplayRole});
    });
    connect(window, &PlasmaWindow::iconChanged, this, [this, window] {
        emitChanged(window, {Qt::DecorationRole});
    });
    connect(window, &PlasmaWindow::appIdChanged, this, [this, window] {
        window->appName.clear();
        emitChanged(window, {AppId, AppName});
    });
    connect(window, &PlasmaWindow::pidChanged, this, [this, window] {
        window->appName.clear();
        emitChanged(window, {AppName});
    });
    connect(window, &PlasmaWindow::geometryChanged, this, [this, window] {
        if (m_filterByScreen) {
            refilter();
        }
        emitChanged(window, {ScreenGeometry});
    });
    connect(window, &PlasmaWindow::virtualDesktopsChanged, this, [this, window] {
        if (m_filterByVirtualDesktop) {
            refilter();
        }
        emitChanged(window, {VirtualDesktops, IsOnAllVirtualDesktops});
    });
    connect(window, &PlasmaWindow::stateChanged, this, [this, window](uint32_t changed) {
        // On all desktops and demanding attention matter for the virtual desktop filter
        if (changed & (State::state_skiptaskbar | State::state_on_all_desktops | State::state_demands_attention)) {
            refilter();
        }
        if (changed & State::state_minimized) {
            emitChanged(window, {IsMinimized});
        }
        if (changed & State::state_closeable) {
            emitChanged(window, {IsClosable});
        }
        if (changed & State::state_minimizable) {
            emitChanged(window, {IsMinimizable});
        }
        if (changed & State::state_maximized) {
            emitChanged(window, {IsMaximized});
        }
        if (changed & State::state_maximizable) {
            emitChanged(window, {IsMaximizable});
        }
        if (changed & State::state_keep_above) {
            emitChanged(window, {IsKeepAbove});
        }
        if (changed & State::state_on_all_desktops) {
            emitChanged(window, {IsOnAllVirtualDesktops});
        }
        if (changed & State::state_virtual_desktop_changeable) {
            emitChanged(window, {IsVirtualDesktopsChangeable});
        }
        // Activity and attention of a transient are shown at its leader
        if (changed & State::state_active) {
            if (window->hasState(State::state_active)) {
                window->lastActivated = ++m_activationCounter;
            }
            emitChanged(leaderOf(window), {IsActive});
        }
        if (changed & State::state_demands_attention) {
            emitChanged(leaderOf(window), {IsDemandingAttention});
        }
    });
    connect(window, &PlasmaWindow::parentWindowChanged, this, [this] {
        refilter();
        // The leader of a transient that is active or demands attention changed
        for (PlasmaWindow *row : std::as_const(m_rows)) {
            emitChanged(row, {IsActive, IsDemandingAttention});
        }
    });

    refilter();
    if (window->parentWindow) {
        emitChanged(leaderOf(window), {IsActive, IsDemandingAttention});
    }
}

void WindowModel::removeWindow(PlasmaWindow *window)
{
    const PlasmaWindow *leader = window->parentWindow ? leaderOf(window) : nullptr;
    std::erase(m_windows, window);
    refilter(); // removes its row
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
    if (window->hasState(State::state_skiptaskbar) || window->parentWindow) {
        return false;
    }

    // Virtual desktop, as in the task manager: windows on all desktops and windows demanding attention
    // are always shown
    if (m_filterByVirtualDesktop && !m_currentVirtualDesktop.isEmpty() && !window->hasState(State::state_on_all_desktops)
        && !window->virtualDesktops.isEmpty() && !window->virtualDesktops.contains(m_currentVirtualDesktop)
        && !isDemandingAttention(window)) {
        return false;
    }

    // Screen: the one the center of the window is on
    if (m_filterByScreen && m_screenGeometry.isValid()) {
        const QRect windowScreen = screenGeometryAt(window->geometry.center());
        if (windowScreen.isValid() && windowScreen != m_screenGeometry) {
            return false;
        }
    }

    return true;
}

void WindowModel::refilter()
{
    std::vector<PlasmaWindow *> shown;
    shown.reserve(m_windows.size());
    for (PlasmaWindow *window : std::as_const(m_windows)) {
        if (isShown(window)) {
            shown.push_back(window);
        }
    }
    if (shown == m_rows) {
        return;
    }
    const size_t oldCount = m_rows.size();

    // Both lists keep the order of m_windows. First the rows that are no longer shown go away, then the
    // missing ones are inserted at their place: the buttons that stay are not touched (no flicker).
    for (int row = static_cast<int>(m_rows.size()) - 1; row >= 0; --row) {
        if (std::ranges::find(shown, m_rows.at(row)) == shown.end()) {
            beginRemoveRows(QModelIndex(), row, row);
            m_rows.erase(m_rows.begin() + row);
            endRemoveRows();
        }
    }
    for (size_t i = 0; i < shown.size(); ++i) {
        if (i >= m_rows.size() || m_rows.at(i) != shown.at(i)) {
            const int row = static_cast<int>(i);
            beginInsertRows(QModelIndex(), row, row);
            m_rows.insert(m_rows.begin() + row, shown.at(i));
            endInsertRows();
        }
    }

    if (m_rows.size() != oldCount) {
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
    activate(row);
}

void WindowModel::activate(int row)
{
    if (row < 0 || row >= count()) {
        return;
    }
    PlasmaWindow *window = m_rows.at(row);

    // Which window of the group (the window and its transients, e.g. dialogs) gets the focus:
    // - a transient demanding attention (e.g. a dialog asking something),
    // - otherwise the one that was active last: KWin raises the window together with its dialogs, but does
    //   not reliably give the focus to an open dialog (see requestActivate() of libtaskmanager). The protocol
    //   does not tell whether a dialog is modal, so the one the user was in last is taken.
    PlasmaWindow *target = window;
    PlasmaWindow *attention = nullptr;
    for (PlasmaWindow *candidate : std::as_const(m_windows)) {
        if (candidate == window || leaderOf(candidate) != window) {
            continue;
        }
        if (!attention && candidate->hasState(State::state_demands_attention)) {
            attention = candidate;
        }
        if (candidate->lastActivated > target->lastActivated) {
            target = candidate;
        }
    }
    if (attention) {
        target = attention;
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

void WindowModel::move(int from, int to)
{
    if (from == to || from < 0 || from >= count() || to < 0 || to >= count()) {
        return;
    }
    // beginMoveRows() wants the row in front of which the moved row ends up, counted before the move
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to)) {
        return;
    }
    PlasmaWindow *window = m_rows.at(from);
    PlasmaWindow *target = m_rows.at(to);
    m_rows.erase(m_rows.begin() + from);
    m_rows.insert(m_rows.begin() + to, window);

    // The same move in the order of all windows: in front of or behind the window that was at 'to'.
    // Hidden windows in between keep their place relative to the others.
    std::erase(m_windows, window);
    auto it = std::ranges::find(m_windows, target);
    m_windows.insert(to > from ? it + 1 : it, window);

    endMoveRows();
}

QString WindowModel::applicationName(const PlasmaWindow *window)
{
    // .desktop file: the app id of a Wayland client is the name of its .desktop file (e.g. org.kde.dolphin),
    // XWayland clients get the class of their WM_CLASS, which usually matches a .desktop file name in lower case
    if (!window->appId.isEmpty()) {
        KService::Ptr service = KService::serviceByDesktopName(window->appId);
        if (!service) {
            service = KService::serviceByDesktopName(window->appId.toLower());
        }
        if (service && !service->name().isEmpty()) {
            return service->name();
        }
    }

    // Executable of the process. /proc/<pid>/exe has the full name, /proc/<pid>/comm is cut after 15 characters
    // but is also readable for processes of other users.
    if (window->pid != 0) {
        const QString proc = QStringLiteral("/proc/%1/").arg(window->pid);
        QString exe = QFileInfo(proc + QStringLiteral("exe")).symLinkTarget();
        // The executable was replaced (e.g. updated) since the process started
        exe.remove(QRegularExpression(QStringLiteral(" \\(deleted\\)$")));
        if (!exe.isEmpty()) {
            return QFileInfo(exe).fileName();
        }
        QFile comm(proc + QStringLiteral("comm"));
        if (comm.open(QIODevice::ReadOnly)) {
            const QString name = QString::fromUtf8(comm.readAll()).trimmed();
            if (!name.isEmpty()) {
                return name;
            }
        }
    }

    return window->appId;
}

WindowModel::PlasmaWindow *WindowModel::windowAt(int row) const
{
    return (row >= 0 && row < count()) ? m_rows.at(row) : nullptr;
}

// The window actions of the context menu, as in libtaskmanager's WaylandTasksModel

void WindowModel::toggleMinimized(int row)
{
    if (PlasmaWindow *window = windowAt(row)) {
        const bool minimized = window->hasState(State::state_minimized);
        window->set_state(State::state_minimized, minimized ? 0 : State::state_minimized);
    }
}

void WindowModel::toggleMaximized(int row)
{
    if (PlasmaWindow *window = windowAt(row)) {
        // Activated as well: maximizing a window in the background would be confusing
        const bool maximized = window->hasState(State::state_maximized);
        window->set_state(State::state_maximized | State::state_active,
                          maximized ? State::state_active : State::state_maximized | State::state_active);
    }
}

void WindowModel::toggleKeepAbove(int row)
{
    if (PlasmaWindow *window = windowAt(row)) {
        const bool keepAbove = window->hasState(State::state_keep_above);
        window->set_state(State::state_keep_above, keepAbove ? 0 : State::state_keep_above);
    }
}

void WindowModel::moveToVirtualDesktop(int row, const QString &desktopId)
{
    PlasmaWindow *window = windowAt(row);
    if (!window || desktopId.isEmpty()) {
        return;
    }
    // Enter the new one first: a window on no desktop at all counts as on all desktops
    const QList<QString> current = window->virtualDesktops;
    if (!current.contains(desktopId)) {
        window->request_enter_virtual_desktop(desktopId);
    }
    for (const QString &desktop : current) {
        if (desktop != desktopId) {
            window->request_leave_virtual_desktop(desktop);
        }
    }
}

void WindowModel::setOnAllVirtualDesktops(int row)
{
    if (PlasmaWindow *window = windowAt(row)) {
        // On no desktop is on all desktops
        const QList<QString> current = window->virtualDesktops;
        for (const QString &desktop : current) {
            window->request_leave_virtual_desktop(desktop);
        }
    }
}

void WindowModel::moveToNewVirtualDesktop(int row)
{
    if (PlasmaWindow *window = windowAt(row)) {
        window->request_enter_new_virtual_desktop();
    }
}

void WindowModel::sendToScreen(int row, const QString &screenName)
{
    PlasmaWindow *window = windowAt(row);
    if (!window) {
        return;
    }
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name() != screenName) {
            continue;
        }
        auto *waylandScreen = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
        if (waylandScreen && waylandScreen->output()) {
            window->send_to_output(waylandScreen->output());
        }
        return;
    }
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
