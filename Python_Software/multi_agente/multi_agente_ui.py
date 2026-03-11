"""
Wrapper per AVVIA_Prismalux.py — espone menu_multiagente()
che mostra il sottomenu Agenti AI:
  1. Pipeline 6 Agenti (avvia_menu)
  2. Motore Byzantino  (verifica a 4 agenti anti-allucinazione)
"""

from multi_agente import menu_agenti


def menu_multiagente() -> None:
    menu_agenti()
