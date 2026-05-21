// Pegasus Frontend
// Copyright (C) 2017-2024  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


import "../common"
import "../settings/common"
import "qrc:/qmlutils" as PegasusUtils
import QtQuick 2.6


MenuScreen {
    id: root

    Keys.onPressed: {
        if (api.keys.isCancel(event) && !event.isAutoRepeat) {
            event.accepted = true;
            root.close();
        }
    }

    readonly property int bodyFontSize: vpx(16)

    PegasusUtils.HorizontalSwipeArea {
        anchors.fill: parent
        onSwipeRight: root.close()
    }

    Column {
        width: parent.width
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        SectionTitle {
            text: qsTr("Logs") + api.tr
        }

        Row {
            width: parent.width * 0.9
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: vpx(10)

            SimpleButton {
                label: qsTr("Clear") + api.tr
                width: vpx(100)
                onActivate: Internal.log.clear()
            }

            Text {
                text: Internal.log.count + qsTr(" messages") + api.tr
                color: "#aaa"
                font.pixelSize: bodyFontSize
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Item {
            width: parent.width
            height: parent.height - y

            ListView {
                id: logList

                width: parent.width * 0.9
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.fill: parent
                anchors.bottomMargin: vpx(20)

                model: Internal.log.messages
                clip: true
                focus: true

                highlightRangeMode: ListView.ApplyRange
                highlightMoveDuration: 150
                preferredHighlightBegin: height * 0.3
                preferredHighlightEnd: height * 0.7

                delegate: Text {
                    required property string modelData
                    required property int index

                    width: logList.width
                    text: modelData
                    color: "#ccc"
                    font.pixelSize: bodyFontSize
                    font.family: globalFonts.mono
                    wrapMode: Text.WordWrap
                    lineHeight: 1.2

                    padding: vpx(4)
                }

                // Auto-scroll to bottom on new messages
                onCountChanged: {
                    if (atYEnd || count <= 1)
                        positionViewAtEnd()
                }

                // Scroll to bottom on first load
                Component.onCompleted: {
                    positionViewAtEnd()
                }
            }
        }
    }
}
