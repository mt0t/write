import QtQuick
import QtQuick.Controls

// The files beside the document. Rows arrive from FileTreeModel already
// flattened, so one list draws the whole tree and the model decides what an
// opened folder contains.
Rectangle {
    id: root

    property var treeModel
    property string rootName: ""
    property url currentUrl
    property bool darkMode: true
    property real textScale: 1
    property color pageColor: "#fbfbfb"
    property color textColor: "#42464c"
    property color mutedColor: "#8a8b85"
    property color accentColor: "#428bca"

    signal fileActivated(url fileUrl)
    signal closeRequested()

    readonly property color panelColor: darkMode
        ? Qt.darker(pageColor, 1.15) : pageColor
    readonly property color borderColor: darkMode
        ? Qt.lighter(panelColor, 1.55) : Qt.darker(panelColor, 1.14)
    readonly property color hoverColor: darkMode
        ? Qt.lighter(panelColor, 1.18) : Qt.darker(panelColor, 1.04)
    readonly property color currentColor: Qt.rgba(accentColor.r, accentColor.g,
                                                  accentColor.b, darkMode ? 0.20 : 0.13)
    readonly property real rowHeight: scaled(26)
    readonly property real indentStep: scaled(14)
    readonly property real sidePadding: scaled(12)

    function scaled(pixels) {
        return Math.max(1, Math.round(pixels * textScale));
    }

    function entry(row) {
        return treeModel ? treeModel.entry(row) : ({});
    }

    function revealCurrent() {
        if (!treeModel)
            return;
        var row = treeModel.rowForUrl(currentUrl);
        if (row < 0)
            return;
        list.currentIndex = row;
        list.positionViewAtIndex(row, ListView.Contain);
    }

    function focusList() {
        list.forceActiveFocus();
    }

    // Back to the folder the tree started in: close every folder that was
    // opened underneath it and put the first row back under the eye.
    function backToRoot() {
        if (treeModel)
            treeModel.collapseAll();
        list.currentIndex = list.count > 0 ? 0 : -1;
        list.contentY = 0;
        list.forceActiveFocus();
    }

    function activateRow(row) {
        var item = entry(row);
        if (item.isDir === undefined)
            return;
        if (item.isDir)
            treeModel.toggle(row);
        else
            root.fileActivated(item.url);
    }

    function collapseOrParent(row) {
        var item = entry(row);
        if (item.isDir === undefined)
            return;
        if (item.isDir && item.expanded) {
            treeModel.toggle(row);
            return;
        }
        var parent = treeModel.parentRow(row);
        if (parent >= 0)
            list.currentIndex = parent;
    }

    color: panelColor
    border.width: 0

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: root.borderColor
    }

    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: root.scaled(10)
        anchors.leftMargin: root.sidePadding
        anchors.rightMargin: root.scaled(6)
        height: root.scaled(28)

        Label {
            anchors.left: parent.left
            anchors.right: rootButton.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: root.scaled(6)
            text: root.rootName
            color: root.textColor
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(12)
            font.bold: true
            elide: Text.ElideRight
        }

        Item {
            id: rootButton
            anchors.right: closeButton.left
            anchors.rightMargin: root.scaled(2)
            anchors.verticalCenter: parent.verticalCenter
            width: root.scaled(22)
            height: root.scaled(22)

            Label {
                anchors.centerIn: parent
                text: "\u2302"
                color: rootArea.containsMouse ? root.textColor : root.mutedColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(14)
            }

            MouseArea {
                id: rootArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.backToRoot()
            }

            ToolTip.visible: rootArea.containsMouse
            ToolTip.text: "Back to root"
        }

        Item {
            id: closeButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: root.scaled(22)
            height: root.scaled(22)

            Label {
                anchors.centerIn: parent
                text: "\u00d7"
                color: closeArea.containsMouse ? root.textColor : root.mutedColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(15)
            }

            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.closeRequested()
            }

            ToolTip.visible: closeArea.containsMouse
            ToolTip.text: "Close (Esc)"
        }
    }

    Rectangle {
        id: separator
        anchors.top: header.bottom
        anchors.topMargin: root.scaled(8)
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: root.borderColor
        opacity: 0.7
    }

    ListView {
        id: list
        objectName: "fileTreeList"
        anchors.top: separator.bottom
        anchors.topMargin: root.scaled(6)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.scaled(8)
        clip: true
        model: root.treeModel
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        Keys.onEscapePressed: root.closeRequested()
        Keys.onReturnPressed: root.activateRow(currentIndex)
        Keys.onEnterPressed: root.activateRow(currentIndex)
        Keys.onRightPressed: {
            var item = root.entry(currentIndex);
            if (item.isDir && !item.expanded)
                root.treeModel.toggle(currentIndex);
        }
        Keys.onLeftPressed: root.collapseOrParent(currentIndex)

        delegate: Item {
            id: rowItem
            width: list.width
            height: root.rowHeight

            readonly property bool currentFile: !model.isDir
                && String(model.url) === String(root.currentUrl)

            Rectangle {
                anchors.fill: parent
                color: rowItem.currentFile ? root.currentColor
                     : rowArea.containsMouse ? root.hoverColor : "transparent"
            }

            Row {
                anchors.verticalCenter: parent.verticalCenter
                x: root.sidePadding + model.depth * root.indentStep
                spacing: root.scaled(4)

                Label {
                    width: root.scaled(11)
                    anchors.verticalCenter: parent.verticalCenter
                    text: model.isDir ? (model.expanded ? "\u25be" : "\u25b8") : ""
                    color: root.mutedColor
                    font.family: "IBM Plex Mono"
                    font.pixelSize: root.scaled(11)
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, rowItem.width - parent.x - root.scaled(30))
                    text: model.name
                    color: rowItem.currentFile ? root.accentColor
                         : model.isDir ? root.textColor : root.mutedColor
                    font.family: "IBM Plex Mono"
                    font.pixelSize: root.scaled(12)
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                id: rowArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    list.currentIndex = index;
                    if (model.isDir) {
                        list.forceActiveFocus();
                        root.treeModel.toggle(index);
                    } else {
                        root.fileActivated(model.url);
                    }
                }
            }
        }
    }

    Label {
        anchors.centerIn: list
        width: Math.max(0, list.width - 2 * root.sidePadding)
        text: "No Markdown files here"
        visible: list.count === 0
        color: root.mutedColor
        opacity: 0.8
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.family: "IBM Plex Mono"
        font.pixelSize: root.scaled(11)
    }
}
