/*
    SPDX-FileCopyrightText: 2016 Eike Hein <hein@kde.org>
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL

    Window list of the Plasma Wayland session, read directly from the org_kde_plasma_window_management
    protocol. The protocol handling is adapted from plasma-workspace/libtaskmanager/waylandtasksmodel.cpp.

    Unlike libtaskmanager there is no matching of windows to .desktop files: every window is shown with the
    icon KWin reports for it (the icon the application set, else the one of its .desktop file, else a generic
    icon). No launchers, no startup notifications, no grouping.

    Optional filters, as in the task manager: only the windows of the screen the bar is on, only the windows
    of the current virtual desktop. Hidden windows keep their place in the order and come back there.
*/

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QQuickItem> // complete type for the Q_INVOKABLE publishGeometry()
#include <QRect>

#include <memory>
#include <vector>

namespace WindowIconBar
{
class PlasmaWindow;
class PlasmaWindowManagement;
}

class WindowModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // Show only the windows whose center is on the screen with this geometry (the screen of the bar)
    Q_PROPERTY(bool filterByScreen READ filterByScreen WRITE setFilterByScreen NOTIFY filterByScreenChanged)
    Q_PROPERTY(QRect screenGeometry READ screenGeometry WRITE setScreenGeometry NOTIFY screenGeometryChanged)
    // Show only the windows of the current virtual desktop (and those on all desktops or demanding attention)
    Q_PROPERTY(bool filterByVirtualDesktop READ filterByVirtualDesktop WRITE setFilterByVirtualDesktop NOTIFY filterByVirtualDesktopChanged)

public:
    enum Roles {
        IsActive = Qt::UserRole + 1,
        IsMinimized,
        IsDemandingAttention,
        IsClosable,
        AppId,
    };
    Q_ENUM(Roles)

    explicit WindowModel(QObject *parent = nullptr);
    ~WindowModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    bool filterByScreen() const;
    void setFilterByScreen(bool filter);
    QRect screenGeometry() const;
    void setScreenGeometry(const QRect &geometry);
    bool filterByVirtualDesktop() const;
    void setFilterByVirtualDesktop(bool filter);

    // Activates the window, or minimizes it if it already is the active one.
    Q_INVOKABLE void activateOrMinimize(int row);
    Q_INVOKABLE void close(int row);
    // Moves a window to another place in the bar (drag and drop)
    Q_INVOKABLE void move(int from, int to);
    // Tells KWin where the button of the window is (target of the minimize animation).
    Q_INVOKABLE void publishGeometry(int row, QQuickItem *item);

Q_SIGNALS:
    void countChanged();
    void filterByScreenChanged();
    void screenGeometryChanged();
    void filterByVirtualDesktopChanged();

private Q_SLOTS:
    // KWin's org.kde.KWin.VirtualDesktopManager.currentChanged
    void setCurrentVirtualDesktop(const QString &id);

private:
    using PlasmaWindow = WindowIconBar::PlasmaWindow;

    void addWindow(PlasmaWindow *window);
    void removeWindow(PlasmaWindow *window);
    void clear();

    // Windows of the task bar: not skipped by the task bar, not a transient (dialog) of another window,
    // and accepted by the filters
    bool isShown(const PlasmaWindow *window) const;
    // Brings m_rows in line with isShown() (inserts and removes rows, keeps the order of m_windows)
    void refilter();
    int rowOf(const PlasmaWindow *window) const;
    void emitChanged(const PlasmaWindow *window, const QList<int> &roles);

    // The top level window a transient belongs to (the window itself if it is no transient)
    static const PlasmaWindow *leaderOf(const PlasmaWindow *window);
    // Active / demanding attention: the window itself or one of its transients
    bool isActive(const PlasmaWindow *window) const;
    bool isDemandingAttention(const PlasmaWindow *window) const;

    std::unique_ptr<WindowIconBar::PlasmaWindowManagement> m_management;
    // All windows that sent their initial state, in the order of the bar (owned, deleted with deleteLater()).
    // Drag and drop reorders this list, so hidden windows come back at their place.
    std::vector<PlasmaWindow *> m_windows;
    std::vector<PlasmaWindow *> m_rows; // the shown windows: the part of m_windows that passes isShown()

    bool m_filterByScreen = false;
    QRect m_screenGeometry;
    bool m_filterByVirtualDesktop = false;
    QString m_currentVirtualDesktop; // id (UUID) of KWin's current virtual desktop, empty while unknown
};
