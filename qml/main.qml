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
import org.kde.ksvg as KSvg

import plasma.applet.io.github.krokuspokus.windowiconbar as WindowIconBar

PlasmoidItem {
    id: root

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical

    // Size of a button, as in the icons-only task manager: as thick as the panel, and along the panel
    // as long as the panel is thick plus the margins of the task frame of the theme
    readonly property real buttonWidth: vertical ? width : height + taskFrame.margins.left + taskFrame.margins.right
    readonly property real buttonHeight: vertical ? width + taskFrame.margins.top + taskFrame.margins.bottom : height
    readonly property real buttonLength: vertical ? buttonHeight : buttonWidth

    preferredRepresentation: fullRepresentation
    Plasmoid.constraintHints: Plasmoid.CanFillArea

    // Takes the free space of the panel like the task manager, but stays visible in edit mode when empty
    Layout.fillWidth: !vertical
    Layout.fillHeight: vertical
    Layout.minimumWidth: vertical ? 0 : Kirigami.Units.gridUnit
    Layout.minimumHeight: vertical ? Kirigami.Units.gridUnit : 0
    Layout.preferredWidth: vertical ? -1 : windowModel.count * buttonLength
    Layout.preferredHeight: vertical ? windowModel.count * buttonLength : -1

    // Theme prefixes of the task frame for the panel edge (taskPrefix() of the task manager).
    // A plain JS array: FrameSvgItem.prefix takes a list of fallbacks.
    function edgePrefix(prefix: string): var {
        switch (Plasmoid.location) {
        case PlasmaCore.Types.LeftEdge:
            return ["west-" + prefix, prefix];
        case PlasmaCore.Types.TopEdge:
            return ["north-" + prefix, prefix];
        case PlasmaCore.Types.RightEdge:
            return ["east-" + prefix, prefix];
        default:
            return ["south-" + prefix, prefix];
        }
    }

    WindowIconBar.WindowModel {
        id: windowModel

        filterByScreen: Plasmoid.configuration.showOnlyCurrentScreen
        screenGeometry: Plasmoid.containment.screenGeometry
        filterByVirtualDesktop: Plasmoid.configuration.showOnlyCurrentDesktop
    }

    // Only for the margins of the task frame (the task manager does the same)
    KSvg.FrameSvgItem {
        id: taskFrame

        visible: false
        imagePath: "widgets/tasks"
        prefix: root.edgePrefix("normal")
    }

    ListView {
        id: list

        // Set by a button while it is dragged: the list must not scroll then
        property bool dragging: false

        anchors.fill: parent
        orientation: root.vertical ? ListView.Vertical : ListView.Horizontal
        model: windowModel
        clip: true
        // Scrolls (wheel or drag) only if there are more windows than room
        interactive: !dragging && (root.vertical ? contentHeight > height : contentWidth > width)
        boundsBehavior: Flickable.StopAtBounds
        highlightFollowsCurrentItem: false
        reuseItems: false

        // The other buttons slide aside while one is dragged
        moveDisplaced: Transition {
            NumberAnimation {
                properties: "x,y"
                duration: Kirigami.Units.shortDuration
                easing.type: Easing.OutQuad
            }
        }

        delegate: WindowButton {
            windowModel: windowModel
            listView: list
            width: root.buttonWidth
            height: root.buttonHeight
            taskFrameMargins: taskFrame.margins
            edgePrefix: root.edgePrefix
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
