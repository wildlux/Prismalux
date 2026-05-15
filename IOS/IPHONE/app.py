"""
Prismalux — iPhone app (PySide6 prototype)

Dimensioni prototipo: 390 × 844 px (iPhone 14 Pro logici)

NOTA DEPLOY iOS:
  PySide6 non ha una toolchain iOS ufficiale. Opzioni reali:
  1. BeeWare (briefcase) + Toga:  briefcase create iOS
  2. Kivy + kivy-ios:             toolchain build python3
  3. Port C++ Qt6 for iOS (Xcode): usa ../android_app/ come base
  Per ora questa app gira su desktop come prototipo fedele al layout iPhone.
"""

import sys
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import Qt
from main_window import PrismaluxIphone


def main() -> None:
    app = QApplication(sys.argv)
    app.setApplicationName("Prismalux")
    app.setApplicationVersion("1.0")
    app.setOrganizationName("Prismalux")

    win = PrismaluxIphone()
    # Simula risoluzione iPhone 14 Pro (390×844 dp)
    win.setFixedSize(390, 844)
    win.setWindowTitle("Prismalux — iPhone")
    win.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
