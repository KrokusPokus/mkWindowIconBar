/*
    SPDX-FileCopyrightText: 2026 KrokusPokus

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami

KCMUtils.SimpleKCM {
    property alias cfg_showOnlyCurrentDesktop: showOnlyCurrentDesktop.checked
    property alias cfg_showOnlyCurrentScreen: showOnlyCurrentScreen.checked
    property alias cfg_distanceDependentAnimation: distanceDependentAnimation.checked

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

        Item {
            Kirigami.FormData.isSection: true
        }

        QQC2.CheckBox {
            id: distanceDependentAnimation
            Kirigami.FormData.label: i18nc("@label for checkbox", "Moving buttons:")
            text: i18nc("@option:check", "Animation duration depends on the distance")
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            leftPadding: distanceDependentAnimation.indicator.width + distanceDependentAnimation.spacing
            text: i18nc("@info explanation of the checkbox above",
                        "Short distances slowly, as in the original task manager; buttons that are far from their place catch up faster. Off: always the same short duration.")
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            opacity: 0.75
        }
    }
}
