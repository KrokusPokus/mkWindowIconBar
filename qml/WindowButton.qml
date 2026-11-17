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
// Keyboard (as in the task manager): Tab reaches the buttons, Return/Enter/Space activate, the menu key opens
// the context menu, Ctrl+Shift+arrow moves the button.
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

    function activate(): void {
        windowModel.activateOrMinimize(index);
    }

    function showContextMenu(): void {
        const menu = contextMenuComponent.createObject(button) as PlasmaExtras.Menu;
        menu.openRelative();
    }

    // Moves this button by one place (keyboard), within the bounds of the list
    function moveBy(step: int): void {
        const target = index + step;
        if (target >= 0 && target < listView.count) {
            windowModel.move(index, target);
        }
    }

    // Accessibility, as in the task manager: a button named after the window
    Accessible.role: Accessible.Button
    Accessible.name: model.display
    // The translation of the task manager is used ("%1 aktivieren")
    Accessible.description: model.display
        ? i18ndc("plasma_applet_org.kde.plasma.taskmanager", "@info:usagetip %1 task name", "Activate %1", model.display)
        : ""
    Accessible.onPressAction: activate()

    activeFocusOnTab: true
    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()
    Keys.onSpacePressed: activate()
    Keys.onMenuPressed: showContextMenu()
    Keys.onLeftPressed: event => {
        if ((event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.ShiftModifier)) {
            moveBy(-1);
        } else {
            event.accepted = false;
        }
    }
    Keys.onRightPressed: event => {
        if ((event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.ShiftModifier)) {
            moveBy(1);
        } else {
            event.accepted = false;
        }
    }
    Keys.onUpPressed: event => Keys.leftPressed(event)
    Keys.onDownPressed: event => Keys.rightPressed(event)

    // Hover frame also for the keyboard focus, so that it can be seen which button Return activates
    readonly property bool highlighted: mouseArea.containsMouse || activeFocus

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
        prefix: button.highlighted
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
        active: button.highlighted
    }

    PlasmaCore.ToolTipArea {
        anchors.fill: parent
        mainText: button.model.display
        location: Plasmoid.location
        // Not while dragging, and the tooltip must not cover the context menu
        active: !mouseArea.dragging && !button.contextMenuOpen
    }

    // Created when it is opened and destroyed when it closes (as in the task manager)
    Component {
        id: contextMenuComponent

        PlasmaExtras.Menu {
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
            onStatusChanged: {
                button.contextMenuOpen = (status === PlasmaExtras.Menu.Open);
                if (status === PlasmaExtras.Menu.Closed) {
                    destroy();
                }
            }

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
            // Live reordering: the button moves to the place under the mouse, the others slide aside.
            // Only the position along the bar counts: the mouse may leave the bar while the button is held
            // (Wayland keeps sending the moves to the bar until the button is released). Across the bar the
            // middle of this button is taken; beyond the ends the first or last place.
            const list = button.listView;
            const pos = mapToItem(list.contentItem, mouse.x, mouse.y);
            const horizontal = list.orientation === ListView.Horizontal;
            const x = horizontal ? Math.max(0, Math.min(pos.x, list.contentWidth - 1)) : button.x + button.width / 2;
            const y = horizontal ? button.y + button.height / 2 : Math.max(0, Math.min(pos.y, list.contentHeight - 1));
            const target = list.indexAt(x, y);
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
                button.showContextMenu();
            } else {
                button.activate();
            }
        }
    }
}
