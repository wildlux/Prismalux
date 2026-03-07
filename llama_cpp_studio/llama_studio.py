"""
llama_cpp_studio/llama_studio.py
==================================
Wrapper richiesto da AVVIA_Prismalux.py:
  import llama_studio
  llama_studio.menu_llama()
"""
import os
import sys

_CART = os.path.dirname(os.path.abspath(__file__))
if _CART not in sys.path:
    sys.path.insert(0, _CART)

from main import menu_studio


def menu_llama() -> None:
    menu_studio()
