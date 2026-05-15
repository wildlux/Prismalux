from __future__ import annotations

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit,
    QPushButton, QScrollArea,
)
from PySide6.QtCore import Qt


class SettingsRow(QWidget):
    def __init__(self, label: str, placeholder: str, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setStyleSheet("background:#1a1d27; border-radius:12px;")
        lay = QHBoxLayout(self)
        lay.setContentsMargins(14, 12, 14, 12)
        lbl = QLabel(label)
        lbl.setFixedWidth(110)
        lay.addWidget(lbl)
        self._edit = QLineEdit()
        self._edit.setPlaceholderText(placeholder)
        lay.addWidget(self._edit)

    @property
    def value(self) -> str:
        return self._edit.text()


class SettingsPage(QWidget):
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
        hlay.addWidget(QLabel("⚙️  Impostazioni"))
        root.addWidget(header)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("border:none;")
        container = QWidget()
        lay = QVBoxLayout(container)
        lay.setContentsMargins(12, 12, 12, 12)
        lay.setSpacing(10)
        lay.setAlignment(Qt.AlignmentFlag.AlignTop)

        section = QLabel("Connessione Ollama")
        section.setStyleSheet("font-size:13px; color:#8890a8; font-weight:600;")
        lay.addWidget(section)

        self._host = SettingsRow("Host", "http://192.168.1.x:11434")
        lay.addWidget(self._host)
        self._token = SettingsRow("API Token", "lascia vuoto per Ollama locale")
        lay.addWidget(self._token)

        section2 = QLabel("Modello")
        section2.setStyleSheet("font-size:13px; color:#8890a8; font-weight:600; margin-top:8px;")
        lay.addWidget(section2)
        self._model = SettingsRow("Modello", "llama3.2:3b")
        lay.addWidget(self._model)

        save_btn = QPushButton("Salva")
        save_btn.setStyleSheet(
            "background:#00c8ff; color:#0f1117; border-radius:12px;"
            "padding:12px; font-weight:600; margin-top:8px;"
        )
        save_btn.clicked.connect(self._save)
        lay.addWidget(save_btn)
        lay.addStretch()

        scroll.setWidget(container)
        root.addWidget(scroll, stretch=1)

    def _save(self) -> None:
        pass  # TODO: salva in QSettings
