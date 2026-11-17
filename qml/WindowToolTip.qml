/*
    SPDX-FileCopyrightText: 2013 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: GPL-2.0-or-later

    Tooltip of a window button: the header of ToolTipInstance.qml of plasma-desktop/applets/taskmanager as the
    task manager shows it without window thumbnails: application name, window title, the virtual desktops of
    the window, a close button. Hovering the header highlights it, a click activates the window.
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

import plasma.applet.io.github.krokuspokus.windowiconbar as WindowIconBar

// One instance for all buttons (as in the task manager): the hovered button sets itself as 'button'
Item {
    id: toolTip

    // The WindowButton the tooltip is for (its model data and index are used)
    property Item button: null
    required property WindowIconBar.WindowModel windowModel

    readonly property var windowData: button ? (button as WindowButton).model : null
    readonly property int row: button ? (button as WindowButton).index : -1

    readonly property string taskManagerDomain: "plasma_applet_org.kde.plasma.taskmanager"
    // Margins as in DefaultToolTip.qml of libplasma (and the task manager without thumbnails)
    readonly property real margins: Kirigami.Units.gridUnit / 2

    implicitWidth: header.implicitWidth + margins * 2
    implicitHeight: header.implicitHeight + margins * 2

    // The window title without a trailing " — <application name>" (the name is shown above it), as in
    // ToolTipInstance.qml
    readonly property string title: {
        let text = windowData ? windowData.display : "";
        if (!text.match(/\s+(—|-|–)/)) {
            return text;
        }
        // KWin appends increasing integers in between pointy brackets to otherwise equal window titles:
        // keep <number> as counter at the end
        text = `${(text.match(/.*(?=\s+(—|-|–))/) || [""])[0]}${(text.match(/<\d+>/) || [""]).pop()}`;
        // The title had only redundant information (i.e. the application name)
        if (text === "") {
            text = "—";
        }
        return text;
    }

    // The virtual desktops of the window, as generateSubText() of ToolTipInstance.qml: only if windows of all
    // desktops are shown and there is more than one desktop
    readonly property string subText: {
        if (!windowData || Plasmoid.configuration.showOnlyCurrentDesktop || windowModel.virtualDesktops.length <= 1) {
            return "";
        }
        if (windowData.IsOnAllVirtualDesktops) {
            return i18ndc(taskManagerDomain, "Comma-separated list of desktops", "Pinned to all desktops");
        }
        const names = windowModel.virtualDesktops
            .filter(desktop => windowData.VirtualDesktops.indexOf(desktop.id) >= 0)
            .map(desktop => desktop.name);
        return names.length > 0 ? i18ndc(taskManagerDomain, "Comma-separated list of desktops", "On %1", names.join(", ")) : "";
    }

    // Hover highlight of the header; a click activates the window (ToolTipWindowMouseArea.qml)
    PlasmaExtras.Highlight {
        anchors.fill: parent
        visible: headerMouseArea.containsMouse
        pressed: headerMouseArea.containsPress
    }

    MouseArea {
        id: headerMouseArea

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton

        onClicked: mouse => {
            const button = toolTip.button as WindowButton;
            if (!button) {
                return;
            }
            switch (mouse.button) {
            case Qt.LeftButton:
                toolTip.windowModel.activate(toolTip.row);
                button.hideImmediately();
                break;
            case Qt.MiddleButton:
                toolTip.windowModel.close(toolTip.row);
                break;
            case Qt.RightButton:
                button.hideImmediately();
                button.showContextMenu();
                break;
            }
        }
    }

    RowLayout {
        id: header

        anchors.fill: parent
        anchors.margins: toolTip.margins
        // Spacing as in DefaultToolTip.qml of libplasma
        spacing: Kirigami.Units.smallSpacing

        ColumnLayout {
            spacing: 0
            // This number controls the overall size of the window tooltips (ToolTipDelegate.qml)
            Layout.maximumWidth: Kirigami.Units.gridUnit * 16

            // Application name
            Kirigami.Heading {
                id: appNameHeading

                Layout.fillWidth: true
                level: 3
                maximumLineCount: 1
                elide: Text.ElideRight
                text: toolTip.windowData ? toolTip.windowData.AppName : ""
                color: headerMouseArea.containsPress ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                visible: text.length !== 0
                textFormat: Text.PlainText
            }

            // Window title
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                maximumLineCount: 2
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                text: toolTip.title === appNameHeading.text ? "" : toolTip.title
                color: headerMouseArea.containsPress ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                opacity: 0.75
                visible: text.length !== 0
                textFormat: Text.PlainText
            }

            // Virtual desktops
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                maximumLineCount: 2
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                text: toolTip.subText
                color: headerMouseArea.containsPress ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                opacity: 0.75
                visible: text.length !== 0
                textFormat: Text.PlainText
            }
        }

        // Close button in the corner (the negative margins move it into the margins of the tooltip)
        PlasmaComponents3.ToolButton {
            Layout.alignment: Qt.AlignTop
            Layout.rightMargin: -toolTip.margins
            Layout.leftMargin: toolTip.margins
            Layout.topMargin: -toolTip.margins
            visible: toolTip.windowData ? toolTip.windowData.IsClosable : false
            icon.name: "window-close"
            onClicked: toolTip.windowModel.close(toolTip.row)
            PlasmaComponents3.ToolTip.text: i18ndc(toolTip.taskManagerDomain, "@info:tooltip Close this window", "Close window")
            PlasmaComponents3.ToolTip.visible: hovered
            PlasmaComponents3.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }
}
