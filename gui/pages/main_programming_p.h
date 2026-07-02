#pragma once
// main_programming_p.h — header privato interno, incluso SOLO dai
// main_programming_*.cpp. Non fare mai #include "main_programming_p.h"
// da file esterni.

#include <QApplication>
#include <QFont>

/* Dimensione font mono DPI-aware: segue il font applicazione (già scalato da Qt/OS).
   Su 4K con HiDPI abilitato, QApplication::font().pointSize() sarà già più grande. */
inline int monoFontPt(int fallback = 11) {
    const int appPt = QApplication::font().pointSize();
    return (appPt > 0) ? appPt : fallback;
}
