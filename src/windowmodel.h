/*
    SPDX-FileCopyrightText: 2016 Eike Hein <hein@kde.org>
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL

    Window list of the Plasma Wayland session, read directly from the org_kde_plasma_window_management
    protocol. The protocol handling is adapted from plasma-workspace/libtaskmanager/waylandtasksmodel.cpp.

    Unlike libtaskmanager there is no matching of windows to .desktop files: every window is shown with the
    icon KWin reports for it (the icon the application set, else the one of its .desktop file, else a generic
    icon). No launchers, no startup notifications, no grouping.
*/

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QQuickItem> // complete type for the Q_INVOKABLE publishGeometry()

#include <memory>
#include <vector>

namespace WindowSwitcher
{
class PlasmaWindow;
class PlasmaWindowManagement;
}

class WindowModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ count NOTIFY countChanged)

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

    // Activates the window, or minimizes it if it already is the active one.
    Q_INVOKABLE void activateOrMinimize(int row);
    Q_INVOKABLE void close(int row);
    // Moves a window to another place in the bar (drag and drop)
    Q_INVOKABLE void move(int from, int to);
    // Tells KWin where the button of the window is (target of the minimize animation).
    Q_INVOKABLE void publishGeometry(int row, QQuickItem *item);

Q_SIGNALS:
    void countChanged();

private:
    using PlasmaWindow = WindowSwitcher::PlasmaWindow;

    void addWindow(PlasmaWindow *window);
    void removeWindow(PlasmaWindow *window);
    void clear();

    // Windows of the task bar: not skipped by the task bar and not a transient (dialog) of another window
    bool isShown(const PlasmaWindow *window) const;
    void updateShown(PlasmaWindow *window);
    int rowOf(const PlasmaWindow *window) const;
    void emitChanged(const PlasmaWindow *window, const QList<int> &roles);

    // The top level window a transient belongs to (the window itself if it is no transient)
    static const PlasmaWindow *leaderOf(const PlasmaWindow *window);
    // Active / demanding attention: the window itself or one of its transients
    bool isActive(const PlasmaWindow *window) const;
    bool isDemandingAttention(const PlasmaWindow *window) const;

    std::unique_ptr<WindowSwitcher::PlasmaWindowManagement> m_management;
    std::vector<PlasmaWindow *> m_windows; // all windows that sent their initial state (owned, deleted with deleteLater())
    std::vector<PlasmaWindow *> m_rows;    // the shown windows, in the order they appeared
};
