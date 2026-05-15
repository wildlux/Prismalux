"""
Prismalux — iPad app (PySide6 prototype)

Dimensioni prototipo: 820 × 1180 px (iPad Air 11" logici)

NOTA DEPLOY iOS:
  Stessa limitazione dell'iPhone: PySide6 non ha toolchain iOS.
  Questa app gira su desktop come prototipo fedele al layout iPad.
  Per il deploy reale vedere IOS/IPHONE/app.py per le opzioni.
"""

import sys
from PySide6.QtWidgets import QApplication
from main_window import PrismaluxIpad


def main() -> None:
    app = QApplication(sys.argv)
    app.setApplicationName("Prismalux")
    app.setApplicationVersion("1.0")
    app.setOrganizationName("Prismalux")

    win = PrismaluxIpad()
    # Simula iPad Air 11" (820×1180 dp)
    win.setFixedSize(820, 1180)
    win.setWindowTitle("Prismalux — iPad")
    win.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
