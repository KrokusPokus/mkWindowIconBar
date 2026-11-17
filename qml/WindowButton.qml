/*
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: GPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick

import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg

import plasma.applet.io.github.krokuspokus.windowswitcher as WindowSwitcher

// One window: its icon on the task bar background of the Plasma theme ("widgets/tasks").
// Left click: activate, or minimize if it is the active window. Middle click: close.
Item {
    id: button

    required property int index
    required property var model
    required property WindowSwitcher.WindowModel windowModel
    property real size

    width: size
    height: size

    // Same frame states as the task manager: focus (active), attention, minimized, normal
    readonly property string basePrefix: {
        if (model.IsDemandingAttention) {
            return "attention";
        }
        if (model.IsActive) {
            return "focus";
        }
        if (model.IsMinimized) {
            return "minimized";
        }
        return "normal";
    }

    function edgePrefix(prefix: string): list<string> {
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

    function publishGeometry(): void {
        windowModel.publishGeometry(index, button);
    }

    onXChanged: publishGeometry()
    onYChanged: publishGeometry()
    onWidthChanged: publishGeometry()
    onHeightChanged: publishGeometry()
    Component.onCompleted: publishGeometry()

    KSvg.FrameSvgItem {
        id: frame

        anchors.fill: parent
        imagePath: "widgets/tasks"
        prefix: mouseArea.containsMouse
            ? [...button.edgePrefix(button.basePrefix + "-hover"), ...button.edgePrefix("hover"), ...button.edgePrefix(button.basePrefix)]
            : button.edgePrefix(button.basePrefix)
    }

    Kirigami.Icon {
        anchors.centerIn: parent
        // Same margins as the task manager: the icon sits inside the frame
        width: Math.max(Kirigami.Units.iconSizes.small, button.size - frame.margins.left - frame.margins.right - Kirigami.Units.smallSpacing)
        height: width
        source: button.model.decoration
        active: mouseArea.containsMouse
        opacity: button.model.IsMinimized ? 0.6 : 1
    }

    PlasmaCore.ToolTipArea {
        anchors.fill: parent
        mainText: button.model.display
        location: Plasmoid.location
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton

        onClicked: mouse => {
            if (mouse.button === Qt.MiddleButton) {
                button.windowModel.close(button.index);
            } else {
                button.windowModel.activateOrMinimize(button.index);
            }
        }
    }
}
