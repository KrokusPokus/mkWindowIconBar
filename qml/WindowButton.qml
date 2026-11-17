/*
    SPDX-FileCopyrightText: 2012-2016 Eike Hein <hein@kde.org>
    SPDX-FileCopyrightText: 2020 Nate Graham <nate@kde.org>
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: GPL-2.0-or-later

    Look and sizes follow Task.qml and code/TaskTools.js of plasma-desktop/applets/taskmanager:
    the frame prefixes per state, the hover prefixes and the icon margins.
*/
pragma ComponentBehavior: Bound

import QtQuick

import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg

import plasma.applet.io.github.krokuspokus.windowiconbar as WindowIconBar

// One window: its icon on the task frame of the Plasma theme ("widgets/tasks").
// Left click: activate, or minimize if it is the active window. Middle click: close.
// Right click: context menu. Drag: move the button to another place.
Item {
    id: button

    required property int index
    required property var model
    required property WindowIconBar.WindowModel windowModel
    required property ListView listView
    // Margins of the task frame, and the function that makes the frame prefixes for the panel edge
    property var taskFrameMargins
    property var edgePrefix
    property bool contextMenuOpen: false

    // Frame state as in the task manager: attention, minimized, focus (active) or normal
    readonly property string basePrefix: {
        if (model.IsDemandingAttention) {
            return "attention";
        }
        if (model.IsMinimized) {
            return "minimized";
        }
        if (model.IsActive) {
            return "focus";
        }
        return "normal";
    }

    function publishGeometry(): void {
        windowModel.publishGeometry(index, button);
    }

    // Small panels: the margins of the frame shrink so that the icon keeps a usable size (Task.qml)
    function adjustMargin(size: real, margins: real, margin: real): real {
        if (size && (size - margins) < Kirigami.Units.iconSizes.small) {
            return Math.ceil((margin * (Kirigami.Units.iconSizes.small / size)) / 2);
        }
        return margin;
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
        // Hovered: "<state>-hover", then "hover", then the plain state (taskPrefixHovered())
        prefix: mouseArea.containsMouse
            ? [...button.edgePrefix(button.basePrefix + "-hover"), ...button.edgePrefix("hover"), ...button.edgePrefix(button.basePrefix)]
            : button.edgePrefix(button.basePrefix)
    }

    Kirigami.Icon {
        readonly property real verticalMargins: button.taskFrameMargins.top + button.taskFrameMargins.bottom

        anchors.centerIn: parent
        height: button.height
            - button.adjustMargin(button.height, verticalMargins, button.taskFrameMargins.top)
            - button.adjustMargin(button.height, verticalMargins, button.taskFrameMargins.bottom)
        width: height
        source: button.model.decoration
        active: mouseArea.containsMouse
    }

    PlasmaCore.ToolTipArea {
        anchors.fill: parent
        mainText: button.model.display
        location: Plasmoid.location
        // Not while dragging, and the tooltip must not cover the context menu
        active: !mouseArea.dragging && !button.contextMenuOpen
    }

    PlasmaExtras.Menu {
        id: contextMenu

        visualParent: button
        placement: {
            switch (Plasmoid.location) {
            case PlasmaCore.Types.LeftEdge:
                return PlasmaExtras.Menu.RightPosedTopAlignedPopup;
            case PlasmaCore.Types.TopEdge:
                return PlasmaExtras.Menu.BottomPosedLeftAlignedPopup;
            case PlasmaCore.Types.RightEdge:
                return PlasmaExtras.Menu.LeftPosedTopAlignedPopup;
            default:
                return PlasmaExtras.Menu.TopPosedLeftAlignedPopup;
            }
        }
        onStatusChanged: button.contextMenuOpen = (status === PlasmaExtras.Menu.Open)

        // The right click is taken by this menu, so it has to offer what the default menu of an applet
        // offers: configure this widget, configure the panel (as ContextMenu.qml of the task manager does)
        PlasmaExtras.MenuItem {
            property PlasmaCore.Action configureAction: null

            enabled: configureAction && configureAction.enabled
            visible: configureAction && configureAction.visible
            text: configureAction ? configureAction.text : ""
            icon: configureAction ? configureAction.icon : ""
            onClicked: configureAction.trigger()

            Component.onCompleted: configureAction = Plasmoid.internalAction("configure")
        }

        PlasmaExtras.MenuItem {
            property PlasmaCore.Action editModeAction: null

            enabled: editModeAction && editModeAction.enabled
            visible: editModeAction && editModeAction.visible
            text: editModeAction ? editModeAction.text : ""
            icon: editModeAction ? editModeAction.icon : ""
            onClicked: editModeAction.trigger()

            Component.onCompleted: editModeAction = Plasmoid.containment.internalAction("configure")
        }

        PlasmaExtras.MenuItem {
            separator: true
        }

        PlasmaExtras.MenuItem {
            // The translation of the task manager is used ("&Schließen")
            text: i18ndc("plasma_applet_org.kde.plasma.taskmanager", "@action:inmenu", "&Close")
            icon: "window-close"
            enabled: button.model.IsClosable
            onClicked: button.windowModel.close(button.index)
        }
    }

    MouseArea {
        id: mouseArea

        // true while the button is dragged to another place (then a release is no click)
        property bool dragging: false
        property point pressPosition

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        // The list must not take over the mouse to scroll while a button is dragged
        preventStealing: true

        onPressed: mouse => {
            pressPosition = Qt.point(mouse.x, mouse.y);
            dragging = false;
        }

        onPositionChanged: mouse => {
            if (!(pressedButtons & Qt.LeftButton)) {
                return;
            }
            if (!dragging) {
                if (Math.abs(mouse.x - pressPosition.x) < Application.styleHints.startDragDistance
                        && Math.abs(mouse.y - pressPosition.y) < Application.styleHints.startDragDistance) {
                    return;
                }
                dragging = true;
                button.listView.dragging = true;
            }
            // Live reordering: the button moves to the place under the mouse, the others slide aside
            const pos = mapToItem(button.listView.contentItem, mouse.x, mouse.y);
            const target = button.listView.indexAt(pos.x, pos.y);
            if (target >= 0 && target !== button.index) {
                button.windowModel.move(button.index, target);
            }
        }

        onReleased: {
            button.listView.dragging = false;
        }

        onCanceled: {
            dragging = false;
            button.listView.dragging = false;
        }

        onClicked: mouse => {
            if (dragging) {
                dragging = false;
                return;
            }
            if (mouse.button === Qt.MiddleButton) {
                button.windowModel.close(button.index);
            } else if (mouse.button === Qt.RightButton) {
                contextMenu.openRelative();
            } else {
                button.windowModel.activateOrMinimize(button.index);
            }
        }
    }
}
