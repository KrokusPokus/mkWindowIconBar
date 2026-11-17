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
// The button itself is the tooltip area (as Task.qml of the task manager): the hover events of the mouse area
// inside reach it, a sibling would not get them.
PlasmaCore.ToolTipArea {
    id: button

    required property int index
    required property var model
    required property WindowIconBar.WindowModel windowModel
    required property ListView listView
    // Margins of the task frame, and the function that makes the frame prefixes for the panel edge
    property var taskFrameMargins
    property var edgePrefix
    property bool contextMenuOpen: false
    // The tooltip content shared by all buttons (WindowToolTip), set to this button while it is hovered
    property Item toolTipItem

    mainItem: toolTipItem
    location: Plasmoid.location
    // The tooltip has a close button and can be clicked
    interactive: true
    // Not while dragging, and the tooltip must not cover the context menu
    active: !mouseArea.dragging && !contextMenuOpen

    onContainsMouseChanged: {
        if (containsMouse && toolTipItem) {
            (toolTipItem as WindowToolTip).button = button;
        }
    }

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

    // Texts that the task manager already has are taken from its translations
    readonly property string taskManagerDomain: "plasma_applet_org.kde.plasma.taskmanager"

    function newMenuItem(menu: QtObject): PlasmaExtras.MenuItem {
        return Qt.createQmlObject("import org.kde.plasma.extras as PlasmaExtras\nPlasmaExtras.MenuItem {}", menu) as PlasmaExtras.MenuItem;
    }

    function newMenuSeparator(menu: QtObject): void {
        Qt.createQmlObject("import org.kde.plasma.extras as PlasmaExtras\nPlasmaExtras.MenuItem { separator: true }", menu);
    }

    // "Move to Desktop" submenu, as in ContextMenu.qml of the task manager. Filled when the context menu is
    // created; the context menu exists only while it is open.
    function fillDesktopsMenu(menu: QtObject): void {
        const desktops = windowModel.virtualDesktops;
        const current = windowModel.currentVirtualDesktop;
        const onDesktops = model.VirtualDesktops;
        const onAll = model.IsOnAllVirtualDesktops;

        let item = newMenuItem(menu);
        item.text = i18ndc(taskManagerDomain, "action:inmenu", "Move &To Current Desktop");
        item.enabled = current !== "" && onDesktops.indexOf(current) < 0;
        item.clicked.connect(() => windowModel.moveToVirtualDesktop(button.index, current));

        item = newMenuItem(menu);
        item.text = i18ndc(taskManagerDomain, "action:inmenu", "&All Desktops");
        item.checkable = true;
        item.checked = onAll;
        item.clicked.connect(() => windowModel.setOnAllVirtualDesktops(button.index));

        newMenuSeparator(menu);

        for (const desktop of desktops) {
            item = newMenuItem(menu);
            item.text = desktop.name;
            item.checkable = true;
            item.checked = !onAll && onDesktops.indexOf(desktop.id) >= 0;
            item.clicked.connect(() => windowModel.moveToVirtualDesktop(button.index, desktop.id));
        }

        newMenuSeparator(menu);

        item = newMenuItem(menu);
        item.text = i18ndc(taskManagerDomain, "action:inmenu", "&New Desktop");
        item.icon = "list-add";
        item.clicked.connect(() => windowModel.moveToNewVirtualDesktop(button.index));
    }

    // "Move to Screen" submenu, as in ContextMenu.qml of the task manager (newer than Plasma 6.8)
    function fillScreensMenu(menu: QtObject): void {
        const windowScreen = model.ScreenGeometry;
        for (const screen of Qt.application.screens) {
            const item = newMenuItem(menu);
            const builtIn = screen.name.startsWith("eDP-") || screen.name.startsWith("LVDS-") || screen.name.startsWith("DSI-");
            item.text = builtIn
                ? i18nc("@item:inmenu List of all Screens to send a window to.", "Built-in Screen")
                : i18nc("@item:inmenu List of all Screens to send a window to, manufacturer, model (output identifier)",
                        "%1 %2 (%3)", screen.manufacturer, screen.model, screen.name);
            item.checkable = true;
            item.checked = windowScreen.x === screen.virtualX && windowScreen.y === screen.virtualY
                && windowScreen.width === screen.width && windowScreen.height === screen.height;
            const name = screen.name;
            item.clicked.connect(() => windowModel.sendToScreen(button.index, name));
        }
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
        ? i18ndc(taskManagerDomain, "@info:usagetip %1 task name", "Activate %1", model.display)
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

    // Hover frame also for the keyboard focus, so that it can be seen which button Return activates, and for
    // something dragged from outside over the button (its window comes to the front)
    // No hover frame while the button is dragged, as in the task manager (there the system drag takes the mouse):
    // the button slides behind the mouse, the mouse leaves and enters it, and the frame would flicker.
    readonly property bool highlighted: (mouseArea.containsMouse && !mouseArea.dragging) || activeFocus || listView.dropHoverIndex === index

    // The dragged button stays in front of the others, which slide aside below it, until its own slide into
    // the last place is over: the move transition of the list (shortDuration, raiseTimer) or its own move
    // animation (distance dependent, moveAnimation)
    z: mouseArea.dragging || raiseTimer.running || moveAnimation.running ? 1 : 0

    Timer {
        id: raiseTimer
        interval: Kirigami.Units.shortDuration
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

    onXChanged: {
        publishGeometry();
        animateMove();
    }
    onYChanged: {
        publishGeometry();
        animateMove();
    }
    onWidthChanged: publishGeometry()
    onHeightChanged: publishGeometry()
    Component.onCompleted: {
        oldX = x;
        oldY = y;
        completed = true;
        publishGeometry();
    }

    // Distance dependent move animation (setting "distanceDependentAnimation"; otherwise the list animates the
    // moves with its transitions, see main.qml). As in Task.qml of the task manager: the button jumps to its
    // new place, a Translate shifts it back to where it was seen, and the shift runs down to 0. An interrupted
    // animation adds its remaining shift. Unlike the task manager the duration depends on the distance: up to
    // one button longDuration (as there), farther shorter, down to shortDuration from two buttons on, so that
    // buttons that fall behind while dragging fast catch up quickly.
    readonly property bool distanceDependentAnimation: Plasmoid.configuration.distanceDependentAnimation
    property bool completed: false
    property real oldX: 0
    property real oldY: 0

    onDistanceDependentAnimationChanged: {
        moveAnimation.stop();
        moveShift.x = 0;
        moveShift.y = 0;
    }

    transform: Translate {
        id: moveShift
    }

    function animateMove(): void {
        const dx = oldX - x + moveShift.x;
        const dy = oldY - y + moveShift.y;
        // Always tracked, so that switching the setting on starts from the right place
        oldX = x;
        oldY = y;
        if (!completed || !distanceDependentAnimation) {
            return;
        }
        moveAnimation.stop();
        const length = listView.orientation === ListView.Horizontal ? width : height;
        const distance = length > 0 ? Math.sqrt(dx * dx + dy * dy) / length : 0;
        const longDuration = Kirigami.Units.longDuration;
        const shortDuration = Math.min(Kirigami.Units.shortDuration, longDuration);
        moveAnimation.duration = distance > 1
            ? Math.max(shortDuration, Math.min(longDuration, longDuration / distance))
            : longDuration;
        moveShift.x = dx;
        moveShift.y = dy;
        moveAnimation.start();
    }

    ParallelAnimation {
        id: moveAnimation

        property int duration: Kirigami.Units.longDuration

        NumberAnimation {
            target: moveShift
            property: "x"
            to: 0
            duration: moveAnimation.duration
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: moveShift
            property: "y"
            to: 0
            duration: moveAnimation.duration
            easing.type: Easing.OutQuad
        }
    }

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

            // Window actions, as in ContextMenu.qml of the task manager (there partly in a "More" submenu)
            PlasmaExtras.MenuItem {
                id: desktopsMenuItem

                visible: button.windowModel.virtualDesktops.length > 1 && button.model.IsVirtualDesktopsChangeable
                enabled: visible
                text: i18ndc(button.taskManagerDomain, "action:inmenu", "Move to &Desktop")
                icon: "virtual-desktops"

                readonly property PlasmaExtras.Menu desktopsMenu: PlasmaExtras.Menu {
                    visualParent: desktopsMenuItem.action
                }

                Component.onCompleted: {
                    if (visible) {
                        button.fillDesktopsMenu(desktopsMenu);
                    }
                }
            }

            PlasmaExtras.MenuItem {
                id: screensMenuItem

                visible: Qt.application.screens.length > 1
                enabled: visible
                text: i18nc("action:inmenu", "Move to &Screen")
                icon: "computer"

                readonly property PlasmaExtras.Menu screensMenu: PlasmaExtras.Menu {
                    visualParent: screensMenuItem.action
                }

                Component.onCompleted: {
                    if (visible) {
                        button.fillScreensMenu(screensMenu);
                    }
                }
            }

            PlasmaExtras.MenuItem {
                separator: true
                visible: desktopsMenuItem.visible || screensMenuItem.visible
            }

            PlasmaExtras.MenuItem {
                enabled: button.model.IsMinimizable
                checkable: true
                checked: button.model.IsMinimized
                text: i18ndc(button.taskManagerDomain, "action:inmenu", "Mi&nimize")
                icon: "window-minimize"
                onClicked: button.windowModel.toggleMinimized(button.index)
            }

            PlasmaExtras.MenuItem {
                enabled: button.model.IsMaximizable
                checkable: true
                checked: button.model.IsMaximized
                text: i18ndc(button.taskManagerDomain, "action:inmenu", "Ma&ximize")
                icon: "window-maximize"
                onClicked: button.windowModel.toggleMaximized(button.index)
            }

            PlasmaExtras.MenuItem {
                checkable: true
                checked: button.model.IsKeepAbove
                text: i18ndc(button.taskManagerDomain, "action:inmenu", "Keep &Above Others")
                icon: "window-keep-above"
                onClicked: button.windowModel.toggleKeepAbove(button.index)
            }

            PlasmaExtras.MenuItem {
                separator: true
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
                text: i18ndc(button.taskManagerDomain, "@action:inmenu", "&Close")
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
            if (dragging) {
                raiseTimer.restart();
            }
            // After onClicked, which follows right away and must still see that it was a drag. Released
            // outside of the button there is no onClicked.
            Qt.callLater(() => {
                dragging = false;
            });
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
