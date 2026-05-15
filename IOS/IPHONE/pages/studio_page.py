from __future__ import annotations

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QScrollArea,
)
from PySide6.QtCore import Qt


class StudioCard(QWidget):
    def __init__(self, icon: str, title: str, desc: str, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setStyleSheet("background:#1a1d27; border-radius:16px;")
        lay = QVBoxLayout(self)
        lay.setContentsMargins(16, 14, 16, 14)
        lay.setSpacing(4)

        top = QHBoxLayout()
        top.addWidget(QLabel(icon))
        top.addWidget(QLabel(f"<b>{title}</b>"))
        top.addStretch()
        lay.addLayout(top)

        d = QLabel(desc)
        d.setStyleSheet("color:#8890a8; font-size:13px;")
        d.setWordWrap(True)
        lay.addWidget(d)


class StudioPage(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._build()

    def _build(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        header = QWidget()
        header.setFixedHeight(52)
        header.setStyleSheet("background:#12151f; border-bottom:1px solid #2a2d3a;")
        hlay = QHBoxLayout(header)
        hlay.setContentsMargins(16, 0, 16, 0)
        hlay.addWidget(QLabel("🔬  Studio AI"))
        root.addWidget(header)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("border:none;")
        container = QWidget()
        lay = QVBoxLayout(container)
        lay.setContentsMargins(12, 12, 12, 12)
        lay.setSpacing(10)
        lay.setAlignment(Qt.AlignmentFlag.AlignTop)

        cards = [
            ("🦙", "llama.cpp Studio", "Compila e avvia modelli GGUF locali"),
            ("🤖", "Agente Autonomo", "Loop THOUGHT → ACTION → OBSERVATION"),
            ("📚", "RAG Locale", "Indicizza PDF e documenti"),
            ("🎨", "Stable Diffusion", "Genera immagini offline"),
            ("📊", "Grafico + Matematica", "Plot e calcoli simbolici"),
        ]
        for icon, title, desc in cards:
            lay.addWidget(StudioCard(icon, title, desc))

        lay.addStretch()
        scroll.setWidget(container)
        root.addWidget(scroll, stretch=1)
