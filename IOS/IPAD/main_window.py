from __future__ import annotations

import sys
import os

# Riusa le pagine dell'iPhone (stesse logiche, layout si adatta)
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../IPHONE"))

from PySide6.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QStackedWidget,
    QPushButton, QLabel, QButtonGroup, QFrame,
)
from PySide6.QtCore import Qt

from pages.chat_page import ChatPage
from pages.studio_page import StudioPage
from pages.settings_page import SettingsPage


STYLE = """
QWidget {
    background-color: #0f1117;
    color: #e8eaf0;
    font-family: "SF Pro Text", "Segoe UI", sans-serif;
    font-size: 16px;
}
#sidebar {
    background: #12151f;
    border-right: 1px solid #2a2d3a;
    min-width: 220px;
    max-width: 220px;
}
#sidebar QPushButton {
    background: transparent;
    border: none;
    border-radius: 12px;
    color: #8890a8;
    font-size: 15px;
    text-align: left;
    padding: 12px 16px;
}
#sidebar QPushButton:checked {
    background: #1e2236;
    color: #00c8ff;
}
#sidebar QPushButton:hover:!checked {
    background: #1a1d27;
}
#appName {
    font-size: 18px;
    font-weight: 700;
    color: #00c8ff;
    padding: 20px 16px 12px 16px;
}
QLineEdit {
    background: #1a1d27;
    border: 1px solid #2a2d3a;
    border-radius: 12px;
    padding: 10px 14px;
    font-size: 15px;
    color: #e8eaf0;
}
QLineEdit:focus { border-color: #00c8ff; }
QPushButton#sendBtn {
    background: #00c8ff;
    border-radius: 20px;
    color: #0f1117;
    font-size: 18px;
    min-width: 40px;
    min-height: 40px;
}
"""


class Sidebar(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("sidebar")
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(4)

        title = QLabel("🍺 Prismalux")
        title.setObjectName("appName")
        root.addWidget(title)

        self._group = QButtonGroup(self)
        self._group.setExclusive(True)

        tabs = [
            ("💬  Chat AI",       0),
            ("🔬  Studio AI",     1),
            ("⚙️   Impostazioni", 2),
        ]
        for label, idx in tabs:
            btn = QPushButton(label)
            btn.setCheckable(True)
            btn.setChecked(idx == 0)
            self._group.addButton(btn, idx)
            root.addWidget(btn)

        root.addStretch()

    @property
    def group(self) -> QButtonGroup:
        return self._group


class PrismaluxIpad(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.setStyleSheet(STYLE)
        self._build()

    def _build(self) -> None:
        root = QHBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # Sidebar sinistra (stile split-view iPad)
        self._sidebar = Sidebar(self)
        root.addWidget(self._sidebar)

        # Contenuto principale
        self._stack = QStackedWidget()
        self._stack.addWidget(ChatPage(self))
        self._stack.addWidget(StudioPage(self))
        self._stack.addWidget(SettingsPage(self))
        root.addWidget(self._stack, stretch=1)

        self._sidebar.group.idClicked.connect(self._stack.setCurrentIndex)
