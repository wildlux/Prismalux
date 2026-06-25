"""
Prismalux — versione Flet (Android / desktop)
Navigazione: NavigationDrawer nativo Flutter/Material 3.
"""
import json
import os
import flet as ft

from utils.ai_client import AiClient
from views.chat_view import ChatView
from views.agenti_view import AgentiView
from views.rag_view import RagView
from views.grafo_view import GrafoView
from views.knowledge_view import KnowledgeView
from views.finanza_view import FinanzaView
from views.tfr_view import TfrView
from views.lavoro_view import LavoroView
from views.file_ai_view import FileAiView
from views.repl_view import ReplView
from views.impara_view import ImparaView
from views.multimedia_view import MultimediaView
from views.voce_view import VoceView
from views.app_view import AppView
from views.coding_view import CodingView
from views.math_view import MathView
from views.graphviz_view import GraphvizView
from views.git_view import GitView
from views.wiki_view import WikiView
from views.sistema_view import SistemaView
from views.settings_view import SettingsView, SETTINGS_PATH

ACCENT  = "#6B5FFF"
BG_DARK = "#0F1117"
BG_CARD = "#1A1D2E"
TXT     = "#E0E0F0"
DIM     = "#8888AA"

# (chiave, icona, etichetta, factory)
NAV_ITEMS = [
    ("chat", ft.icons.CHAT_BUBBLE_OUTLINE,      "Chat"),
    ("age",  ft.icons.SMART_TOY_OUTLINED,        "Agenti AI"),
    ("rag",  ft.icons.LIBRARY_BOOKS_OUTLINED,    "RAG"),
    ("grf",  ft.icons.ACCOUNT_TREE_OUTLINED,     "Grafo"),
    ("knw",  ft.icons.BOOK_OUTLINED,             "Conoscenza"),
    ("fin",  ft.icons.EURO_OUTLINED,             "Finanza"),
    ("tfr",  ft.icons.BAR_CHART_OUTLINED,        "TFR"),
    ("lav",  ft.icons.WORK_OUTLINE,              "Lavoro"),
    ("fil",  ft.icons.FOLDER_OUTLINED,           "File AI"),
    ("repl", ft.icons.CODE,                      "REPL Python"),
    ("imp",  ft.icons.SCHOOL_OUTLINED,           "Impara"),
    ("med",  ft.icons.MOVIE_OUTLINED,            "Multimedia"),
    ("voc",  ft.icons.MIC_NONE,                  "Voce"),
    ("app",  ft.icons.GAMEPAD_OUTLINED,          "App Controller"),
    ("cod",  ft.icons.TERMINAL,                  "Coding"),
    ("mat",  ft.icons.CALCULATE_OUTLINED,        "Matematica"),
    ("gvz",  ft.icons.HUB_OUTLINED,              "Graphviz"),
    ("git",  ft.icons.MERGE_TYPE,                "Git"),
    ("wik",  ft.icons.PUBLIC_OUTLINED,           "Wiki & Web"),
    ("sys",  ft.icons.MONITOR_HEART_OUTLINED,    "Sistema"),
]

SETTINGS_KEY = "__settings__"


def load_settings() -> dict:
    if os.path.exists(SETTINGS_PATH):
        try:
            with open(SETTINGS_PATH) as f:
                return json.load(f)
        except Exception:
            pass
    return {"server": "http://192.168.1.100:11500", "token": "", "model": ""}


def main(page: ft.Page) -> None:
    page.title = "Prismalux"
    page.theme_mode = ft.ThemeMode.DARK
    page.bgcolor = BG_DARK
    page.padding = 0
    page.window.width = 400
    page.window.height = 800

    page.theme = ft.Theme(
        color_scheme_seed=ACCENT,
        use_material3=True,
    )

    cfg = load_settings()
    ai = AiClient(
        base_url=cfg.get("server", "http://192.168.1.100:11500"),
        token=cfg.get("token", ""),
    )

    # ── lazy cache view ──────────────────────────────────────────────────────
    _view_factories = {
        "chat": lambda: ChatView(page, ai),
        "age":  lambda: AgentiView(page, ai),
        "rag":  lambda: RagView(page, ai),
        "grf":  lambda: GrafoView(page, ai),
        "knw":  lambda: KnowledgeView(page, ai),
        "fin":  lambda: FinanzaView(page, ai),
        "tfr":  lambda: TfrView(page, ai),
        "lav":  lambda: LavoroView(page, ai),
        "fil":  lambda: FileAiView(page, ai),
        "repl": lambda: ReplView(page, ai),
        "imp":  lambda: ImparaView(page, ai),
        "med":  lambda: MultimediaView(page, ai),
        "voc":  lambda: VoceView(page, ai),
        "app":  lambda: AppView(page, ai),
        "cod":  lambda: CodingView(page, ai),
        "mat":  lambda: MathView(page, ai),
        "gvz":  lambda: GraphvizView(page, ai),
        "git":  lambda: GitView(page, ai),
        "wik":  lambda: WikiView(page, ai),
        "sys":  lambda: SistemaView(page, ai),
    }
    _cache: dict = {}
    _current_key = ["chat"]

    content_area = ft.Container(expand=True, padding=ft.padding.symmetric(horizontal=8, vertical=4))
    page_title   = ft.Text("💬 Chat", size=15, weight=ft.FontWeight.BOLD, color=TXT)

    def switch_view(key: str) -> None:
        if key not in _view_factories:
            return
        _current_key[0] = key
        if key not in _cache:
            _cache[key] = _view_factories[key]()
        content_area.content = _cache[key]
        label = next((l for k, _, l in NAV_ITEMS if k == key), key)
        icon  = next((i for k, i, _ in NAV_ITEMS if k == key), ft.icons.CIRCLE)
        page_title.value = f"{label}"
        page.update()

    # ── NavigationDrawer nativo Flutter ─────────────────────────────────────
    def on_drawer_change(e: ft.ControlEvent) -> None:
        idx = e.control.selected_index
        if idx is None:
            return
        key = NAV_ITEMS[idx][0]
        page.close(drawer)
        switch_view(key)

    drawer = ft.NavigationDrawer(
        on_change=on_drawer_change,
        controls=[
            ft.Container(
                content=ft.Row([
                    ft.Text("🍺", size=28),
                    ft.Column([
                        ft.Text("Prismalux", size=17, weight=ft.FontWeight.BOLD, color=TXT),
                        ft.Text("AI · Android", size=11, color=DIM),
                    ], spacing=0),
                ], spacing=12),
                padding=ft.padding.symmetric(horizontal=16, vertical=20),
            ),
            ft.Divider(height=1),
            *[
                ft.NavigationDrawerDestination(
                    icon=icon,
                    label=label,
                )
                for _, icon, label in NAV_ITEMS
            ],
        ],
        selected_index=0,
    )
    page.drawer = drawer

    # ── AppBar ───────────────────────────────────────────────────────────────
    def open_settings(_):
        nonlocal ai
        def on_save(new_ai: AiClient):
            nonlocal ai
            ai = new_ai
            _cache.clear()
        dlg = ft.AlertDialog(
            modal=True,
            title=ft.Text("⚙️ Impostazioni"),
            content=ft.Container(
                content=SettingsView(page, ai, on_save=on_save),
                width=340, height=420,
            ),
            actions=[ft.TextButton("Chiudi", on_click=lambda _: page.close(dlg))],
        )
        page.open(dlg)

    stop_btn = ft.IconButton(
        icon=ft.icons.STOP_CIRCLE_OUTLINED,
        icon_color="#FF4444",
        tooltip="Stop AI",
        on_click=lambda _: ai.abort(),
    )

    appbar = ft.AppBar(
        leading=ft.IconButton(
            icon=ft.icons.MENU,
            icon_color=TXT,
            on_click=lambda _: page.open(drawer),
        ),
        leading_width=48,
        title=page_title,
        bgcolor=BG_CARD,
        actions=[
            stop_btn,
            ft.IconButton(
                icon=ft.icons.SETTINGS_OUTLINED,
                icon_color=DIM,
                on_click=open_settings,
                tooltip="Impostazioni",
            ),
        ],
    )
    page.appbar = appbar

    # ── layout principale ────────────────────────────────────────────────────
    page.add(content_area)
    switch_view("chat")


ft.app(main)
