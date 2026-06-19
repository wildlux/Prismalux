"""
Prismalux — versione Flet (Android / desktop)
20 schermate, navigazione 2 righe scorrevoli.
Si connette al server LAN Qt (porta 11500).
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

ACCENT   = "#6B5FFF"
BG_DARK  = "#0F1117"
BG_CARD  = "#1A1D2E"
TXT      = "#E0E0F0"
TXT_DIM  = "#8888AA"

ROW_1 = [
    ("chat",  "💬 Chat"),
    ("age",   "🤖 Agenti"),
    ("rag",   "📚 RAG"),
    ("grf",   "🕸 Grafo"),
    ("knw",   "📖 Know."),
    ("fin",   "💰 Finanza"),
    ("tfr",   "📊 TFR"),
    ("lav",   "💼 Lavoro"),
    ("fil",   "📁 File AI"),
    ("repl",  "🐍 REPL"),
]
ROW_2 = [
    ("imp",   "📚 Impara"),
    ("med",   "🎬 Media"),
    ("voc",   "🎤 Voce"),
    ("app",   "🕹 App"),
    ("cod",   "💻 Coding"),
    ("mat",   "π  Math"),
    ("gvz",   "🕸 Graph"),
    ("git",   "🔀 Git"),
    ("wik",   "🌐 Wiki"),
    ("sys",   "⚙ Sistema"),
]


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

    cfg = load_settings()
    ai = AiClient(base_url=cfg.get("server", "http://192.168.1.100:11500"),
                  token=cfg.get("token", ""))

    # ── area contenuto ──────────────────────────────────────────────────────
    content_col = ft.Column(expand=True, scroll=ft.ScrollMode.HIDDEN)

    VIEWS: dict = {}
    _cache: dict = {}

    def switch_tab(key: str) -> None:
        if key not in VIEWS:
            return
        _cache.setdefault(key, VIEWS[key]())
        content_col.controls.clear()
        content_col.controls.append(_cache[key])
        for btn in all_btns:
            btn.style = _btn_style(btn.data == key)
        page.update()

    def _btn_style(active: bool) -> ft.ButtonStyle:
        return ft.ButtonStyle(
            color=ACCENT if active else TXT_DIM,
            bgcolor={"": BG_CARD if active else "transparent"},
            padding=ft.padding.symmetric(horizontal=10, vertical=6),
            shape=ft.RoundedRectangleBorder(radius=8),
            overlay_color="transparent",
        )

    all_btns: list[ft.TextButton] = []

    def make_btn(key: str, label: str) -> ft.TextButton:
        btn = ft.TextButton(
            content=ft.Text(label, size=12, weight=ft.FontWeight.W_500),
            data=key,
            style=_btn_style(False),
            on_click=lambda e: switch_tab(e.control.data),
        )
        all_btns.append(btn)
        return btn

    row1_controls = [make_btn(k, l) for k, l in ROW_1]
    row2_controls = [make_btn(k, l) for k, l in ROW_2]

    # ── pulsante impostazioni ────────────────────────────────────────────────
    def open_settings(e: ft.ControlEvent) -> None:
        nonlocal ai
        dlg = ft.AlertDialog(
            modal=True,
            title=ft.Text("⚙️ Impostazioni"),
            content=ft.Container(
                content=SettingsView(page, ai, on_save=_on_settings_save),
                width=340, height=420,
            ),
            actions=[ft.TextButton("Chiudi", on_click=lambda _: setattr(dlg, "open", False) or page.update())],
        )
        page.overlay.append(dlg)
        dlg.open = True
        page.update()

    def _on_settings_save(new_ai: AiClient) -> None:
        nonlocal ai
        ai = new_ai
        _cache.clear()

    header = ft.Container(
        content=ft.Row([
            ft.Text("🍺 Prismalux", size=15, weight=ft.FontWeight.BOLD, color=TXT),
            ft.IconButton(ft.Icons.SETTINGS_OUTLINED, icon_color=TXT_DIM,
                          on_click=open_settings, icon_size=20),
        ], alignment=ft.MainAxisAlignment.SPACE_BETWEEN),
        bgcolor=BG_CARD, padding=ft.padding.symmetric(horizontal=12, vertical=6),
    )

    nav = ft.Container(
        content=ft.Column([
            ft.Row(row1_controls, scroll=ft.ScrollMode.AUTO, spacing=2),
            ft.Row(row2_controls, scroll=ft.ScrollMode.AUTO, spacing=2),
        ], spacing=2),
        bgcolor=BG_CARD,
        padding=ft.padding.symmetric(horizontal=4, vertical=4),
    )

    # ── registra view factory (lazy) ─────────────────────────────────────────
    VIEWS["chat"] = lambda: ChatView(page, ai)
    VIEWS["age"]  = lambda: AgentiView(page, ai)
    VIEWS["rag"]  = lambda: RagView(page, ai)
    VIEWS["grf"]  = lambda: GrafoView(page, ai)
    VIEWS["knw"]  = lambda: KnowledgeView(page, ai)
    VIEWS["fin"]  = lambda: FinanzaView(page, ai)
    VIEWS["tfr"]  = lambda: TfrView(page, ai)
    VIEWS["lav"]  = lambda: LavoroView(page, ai)
    VIEWS["fil"]  = lambda: FileAiView(page, ai)
    VIEWS["repl"] = lambda: ReplView(page, ai)
    VIEWS["imp"]  = lambda: ImparaView(page, ai)
    VIEWS["med"]  = lambda: MultimediaView(page, ai)
    VIEWS["voc"]  = lambda: VoceView(page, ai)
    VIEWS["app"]  = lambda: AppView(page, ai)
    VIEWS["cod"]  = lambda: CodingView(page, ai)
    VIEWS["mat"]  = lambda: MathView(page, ai)
    VIEWS["gvz"]  = lambda: GraphvizView(page, ai)
    VIEWS["git"]  = lambda: GitView(page, ai)
    VIEWS["wik"]  = lambda: WikiView(page, ai)
    VIEWS["sys"]  = lambda: SistemaView(page, ai)

    page.add(ft.Column([
        header,
        nav,
        ft.Divider(height=1, color="#2A2D3E"),
        ft.Container(content=content_col, expand=True,
                     padding=ft.padding.symmetric(horizontal=8, vertical=4)),
    ], expand=True, spacing=0))

    switch_tab("chat")


ft.app(main)
