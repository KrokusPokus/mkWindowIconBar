/*
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami

KCMUtils.SimpleKCM {
    property alias cfg_showOnlyCurrentDesktop: showOnlyCurrentDesktop.checked
    property alias cfg_showOnlyCurrentScreen: showOnlyCurrentScreen.checked

    Kirigami.FormLayout {
        QQC2.CheckBox {
            id: showOnlyCurrentDesktop
            Kirigami.FormData.label: i18nc("@label for checkbox group", "Filter:")
            text: i18nc("@option:check", "Only the current virtual desktop")
        }

        QQC2.CheckBox {
            id: showOnlyCurrentScreen
            text: i18nc("@option:check", "Only the current screen")
        }
    }
}
