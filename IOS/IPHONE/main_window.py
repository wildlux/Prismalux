from __future__ import annotations

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QStackedWidget,
    QPushButton, QLabel, QTextEdit, QLineEdit,
    QScrollArea, QFrame, QButtonGroup,
)
from PySide6.QtCore import Qt, QSize
from PySide6.QtGui import QFont

from pages.chat_page import ChatPage
from pages.studio_page import StudioPage
from pages.settings_page import SettingsPage


STYLE = """
QWidget {
    background-color: #0f1117;
    color: #e8eaf0;
    font-family: "SF Pro Text", "Segoe UI", sans-serif;
    font-size: 15px;
}
#bottomBar {
    background-color: #12151f;
    border-top: 1px solid #2a2d3a;
}
#bottomBar QPushButton {
    background: transparent;
    border: none;
    color: #8890a8;
    font-size: 22px;
    padding: 6px;
    min-width: 60px;
    min-height: 50px;
}
#bottomBar QPushButton:checked {
    color: #00c8ff;
}
QLineEdit {
    background: #1a1d27;
    border: 1px solid #2a2d3a;
    border-radius: 12px;
    padding: 10px 14px;
    font-size: 15px;
    color: #e8eaf0;
}
QLineEdit:focus {
    border-color: #00c8ff;
}
QPushButton#sendBtn {
    background: #00c8ff;
    border-radius: 20px;
    color: #0f1117;
    font-size: 18px;
    min-width: 40px;
    min-height: 40px;
}
"""


class BottomBar(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("bottomBar")
        self.setFixedHeight(80)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self._group = QButtonGroup(self)
        self._group.setExclusive(True)

        tabs = [
            ("💬", "Chat"),
            ("🔬", "Studio"),
            ("⚙️", "Impostazioni"),
        ]
        for i, (icon, tooltip) in enumerate(tabs):
            btn = QPushButton(icon)
            btn.setCheckable(True)
            btn.setToolTip(tooltip)
            btn.setChecked(i == 0)
            self._group.addButton(btn, i)
            layout.addWidget(btn)

    @property
    def group(self) -> QButtonGroup:
        return self._group


class PrismaluxIphone(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self._build()

    def _build(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        self.setStyleSheet(STYLE)

        # Stack pagine
        self._stack = QStackedWidget()
        self._chat_page     = ChatPage(self)
        self._studio_page   = StudioPage(self)
        self._settings_page = SettingsPage(self)

        self._stack.addWidget(self._chat_page)
        self._stack.addWidget(self._studio_page)
        self._stack.addWidget(self._settings_page)

        # Bottom bar
        self._bar = BottomBar(self)
        self._bar.group.idClicked.connect(self._stack.setCurrentIndex)

        root.addWidget(self._stack, stretch=1)
        root.addWidget(self._bar)
