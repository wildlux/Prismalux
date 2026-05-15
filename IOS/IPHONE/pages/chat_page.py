from __future__ import annotations

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout,
    QScrollArea, QLabel, QLineEdit, QPushButton, QFrame,
)
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QFont


class BubbleWidget(QFrame):
    def __init__(self, text: str, is_user: bool, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        color = "#1e3a5f" if is_user else "#1a1d27"
        align = Qt.AlignmentFlag.AlignRight if is_user else Qt.AlignmentFlag.AlignLeft
        self.setStyleSheet(f"background:{color}; border-radius:14px; padding:8px 12px;")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 8, 10, 8)
        lbl = QLabel(text)
        lbl.setWordWrap(True)
        lbl.setAlignment(align)
        layout.addWidget(lbl)


class ChatPage(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._build()

    def _build(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # Header
        header = QWidget()
        header.setFixedHeight(52)
        header.setStyleSheet("background:#12151f; border-bottom:1px solid #2a2d3a;")
        hlay = QHBoxLayout(header)
        hlay.setContentsMargins(16, 0, 16, 0)
        title = QLabel("💬  Chat AI")
        title.setStyleSheet("font-size:17px; font-weight:600;")
        hlay.addWidget(title)
        root.addWidget(header)

        # Scroll area bolle
        self._scroll = QScrollArea()
        self._scroll.setWidgetResizable(True)
        self._scroll.setStyleSheet("border:none; background:#0f1117;")
        self._bubble_container = QWidget()
        self._bubble_layout = QVBoxLayout(self._bubble_container)
        self._bubble_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self._bubble_layout.setContentsMargins(12, 12, 12, 12)
        self._bubble_layout.setSpacing(8)
        self._scroll.setWidget(self._bubble_container)
        root.addWidget(self._scroll, stretch=1)

        # Input bar
        bar = QWidget()
        bar.setFixedHeight(64)
        bar.setStyleSheet("background:#12151f; border-top:1px solid #2a2d3a;")
        blay = QHBoxLayout(bar)
        blay.setContentsMargins(12, 10, 12, 10)
        blay.setSpacing(8)
        self._input = QLineEdit()
        self._input.setPlaceholderText("Scrivi un messaggio…")
        self._input.returnPressed.connect(self._send)
        blay.addWidget(self._input)
        send_btn = QPushButton("⬆")
        send_btn.setObjectName("sendBtn")
        send_btn.clicked.connect(self._send)
        blay.addWidget(send_btn)
        root.addWidget(bar)

        self._add_bubble("Ciao! Sono Prismalux AI. Come posso aiutarti?", is_user=False)

    def _add_bubble(self, text: str, is_user: bool) -> None:
        row = QHBoxLayout()
        bubble = BubbleWidget(text, is_user)
        if is_user:
            row.addStretch()
            row.addWidget(bubble)
        else:
            row.addWidget(bubble)
            row.addStretch()
        self._bubble_layout.addLayout(row)

    def _send(self) -> None:
        text = self._input.text().strip()
        if not text:
            return
        self._input.clear()
        self._add_bubble(text, is_user=True)
        self._add_bubble("(risposta AI — collega Ollama)", is_user=False)
