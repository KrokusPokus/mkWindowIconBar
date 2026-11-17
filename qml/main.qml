/*
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: GPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami

import plasma.applet.io.github.krokuspokus.windowswitcher as WindowSwitcher

PlasmoidItem {
    id: root

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    // A button is a square as large as the panel is thick
    readonly property real buttonSize: vertical ? width : height

    preferredRepresentation: fullRepresentation
    Plasmoid.constraintHints: Plasmoid.CanFillArea

    // Takes the free space of the panel like the task manager, but stays visible in edit mode when empty
    Layout.fillWidth: !vertical
    Layout.fillHeight: vertical
    Layout.minimumWidth: vertical ? 0 : Kirigami.Units.gridUnit
    Layout.minimumHeight: vertical ? Kirigami.Units.gridUnit : 0
    Layout.preferredWidth: vertical ? -1 : windowModel.count * buttonSize
    Layout.preferredHeight: vertical ? windowModel.count * buttonSize : -1

    WindowSwitcher.WindowModel {
        id: windowModel
    }

    ListView {
        id: list

        anchors.fill: parent
        orientation: root.vertical ? ListView.Vertical : ListView.Horizontal
        model: windowModel
        clip: true
        // Scrolls (wheel or drag) only if there are more windows than room
        interactive: root.vertical ? contentHeight > height : contentWidth > width
        boundsBehavior: Flickable.StopAtBounds
        highlightFollowsCurrentItem: false
        reuseItems: false

        delegate: WindowButton {
            windowModel: windowModel
            size: root.buttonSize
        }

        // Moving buttons must publish their new place for the minimize animation
        onContentXChanged: publishAll()
        onContentYChanged: publishAll()
        onWidthChanged: publishAll()
        onHeightChanged: publishAll()

        function publishAll(): void {
            for (let i = 0; i < count; ++i) {
                const item = itemAtIndex(i);
                if (item) {
                    (item as WindowButton).publishGeometry();
                }
            }
        }
    }
}
