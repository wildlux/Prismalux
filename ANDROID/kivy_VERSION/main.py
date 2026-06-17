"""
Prismalux — versione Kivy (mobile + desktop)
20 schermate con parità rispetto alla webchat HTML.
Si connette al server LAN Qt (porta 11500).
"""
import os
os.environ.setdefault("KIVY_NO_ENV_CONFIG", "1")

from kivy.app import App
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.button import Button
from kivy.uix.label import Label
from kivy.uix.screenmanager import ScreenManager, FadeTransition
from kivy.metrics import dp
from kivy.core.window import Window
from kivy.storage.jsonstore import JsonStore
from kivy.clock import Clock

from utils.ai_client import AiClient

from screens.chat_screen import ChatScreen
from screens.agenti_screen import AgentiScreen
from screens.rag_screen import RagScreen
from screens.grafo_screen import GrafoScreen
from screens.knowledge_screen import KnowledgeScreen
from screens.finanza_screen import FinanzaScreen
from screens.tfr_screen import TFRScreen
from screens.lavoro_screen import LavoroScreen
from screens.file_ai_screen import FileAiScreen
from screens.repl_screen import ReplScreen
from screens.impara_screen import ImpararScreen
from screens.multimedia_screen import MultimediaScreen
from screens.voce_screen import VoceScreen
from screens.app_screen import AppScreen
from screens.coding_screen import CodingScreen
from screens.math_screen import MathScreen
from screens.graphviz_screen import GraphvizScreen
from screens.git_screen import GitScreen
from screens.wiki_screen import WikiScreen
from screens.sistema_screen import SistemaScreen
from screens.settings_screen import SettingsScreen, STORE_PATH

Window.clearcolor = (0.059, 0.066, 0.09, 1)

# ─── navigazione 2 righe ────────────────────────────────────────────────────
# ROW_1: sezione AI + Strumenti
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

# ROW_2: sezione Learn + Dev
ROW_2 = [
    ("imp",   "📚 Impara"),
    ("mul",   "🎬 Media"),
    ("wsp",   "🎙 Voce"),
    ("apps",  "🚀 App"),
    ("cod",   "💻 Coding"),
    ("mat",   "π Matema."),
    ("gvz",   "🗺 Graphviz"),
    ("git",   "🌿 Git"),
    ("wik",   "📰 Wiki"),
    ("sys",   "📊 Sistema"),
]

_BG_OFF = (0.10, 0.11, 0.17, 1)
_BG_ON  = (0.25, 0.23, 0.52, 1)
_FG_OFF = (0.55, 0.55, 0.65, 1)
_FG_ON  = (0.88, 0.88, 0.94, 1)
_SEC_BG = (0.08, 0.09, 0.14, 1)


def _nav_row(items, on_tap) -> tuple[ScrollView, dict]:
    """Riga di navigazione orizzontale scrollabile. Ritorna (ScrollView, {name: Button})."""
    sv = ScrollView(size_hint_y=None, height=dp(40), do_scroll_y=False,
                    bar_width=0, bar_color=(0,0,0,0))
    inner = BoxLayout(size_hint_x=None, height=dp(40), spacing=dp(2), padding=[dp(4), 0])
    inner.bind(minimum_width=inner.setter("width"))
    btns: dict[str, Button] = {}
    for name, label in items:
        b = Button(text=label, font_size=dp(11.5),
                   size_hint_x=None, width=dp(82), height=dp(36),
                   background_color=_BG_OFF, color=_FG_OFF,
                   background_normal="", background_down="")
        b.bind(on_release=lambda _, n=name: on_tap(n))
        btns[name] = b
        inner.add_widget(b)
    sv.add_widget(inner)
    return sv, btns


class PrismaluxApp(App):
    title = "Prismalux"

    def __init__(self, **kw):
        super().__init__(**kw)
        self._store = JsonStore(STORE_PATH) if os.path.exists(STORE_PATH) else None
        url   = "http://192.168.1.100:11500"
        token = ""
        if self._store and self._store.exists("server"):
            d     = self._store.get("server")
            url   = d.get("url",   url)
            token = d.get("token", token)
        self.ai    = AiClient(base_url=url, token=token)
        self._model = "ollama"
        self._nav_btns: dict[str, Button] = {}

    @property
    def model(self):
        return self._model

    @model.setter
    def model(self, v):
        self._model = v

    def build(self):
        root = BoxLayout(orientation="vertical", spacing=0)

        # ── header ──────────────────────────────────────────────────────────
        hdr = BoxLayout(size_hint_y=None, height=dp(44), padding=[dp(8), dp(4)], spacing=dp(6))
        hdr.canvas.before.clear()
        from kivy.graphics import Color, Rectangle
        with hdr.canvas.before:
            Color(*_SEC_BG)
            hdr._bg = Rectangle(pos=hdr.pos, size=hdr.size)
        hdr.bind(pos=lambda w, _: setattr(w._bg, "pos", w.pos),
                 size=lambda w, _: setattr(w._bg, "size", w.size))

        title_lbl = Label(text="[b]🍺 Prismalux[/b]", markup=True,
                          font_size=dp(15), color=(0.88, 0.88, 0.94, 1),
                          halign="left", text_size=(None, None))
        btn_cfg = Button(text="⚙️", font_size=dp(18),
                         size_hint_x=None, width=dp(44),
                         background_color=_BG_OFF, color=_FG_OFF,
                         background_normal="", background_down="")
        btn_cfg.bind(on_release=lambda _: self._goto("settings"))
        hdr.add_widget(title_lbl)
        hdr.add_widget(btn_cfg)
        root.add_widget(hdr)

        # ── 2 righe navigazione ──────────────────────────────────────────────
        nav_wrap = BoxLayout(orientation="vertical", size_hint_y=None,
                             height=dp(84), spacing=dp(2))
        sv1, btns1 = _nav_row(ROW_1, self._goto)
        sv2, btns2 = _nav_row(ROW_2, self._goto)

        sec1 = BoxLayout(size_hint_y=None, height=dp(42), spacing=0)
        sec1_lbl = Label(text="AI & Strumenti", font_size=dp(9.5), color=_FG_OFF,
                         size_hint_x=None, width=dp(70), halign="center",
                         text_size=(dp(70), None))
        sec1.add_widget(sec1_lbl)
        sec1.add_widget(sv1)

        sec2 = BoxLayout(size_hint_y=None, height=dp(42), spacing=0)
        sec2_lbl = Label(text="Learn & Dev", font_size=dp(9.5), color=_FG_OFF,
                         size_hint_x=None, width=dp(70), halign="center",
                         text_size=(dp(70), None))
        sec2.add_widget(sec2_lbl)
        sec2.add_widget(sv2)

        nav_wrap.add_widget(sec1)
        nav_wrap.add_widget(sec2)
        root.add_widget(nav_wrap)
        self._nav_btns = {**btns1, **btns2}

        # ── screen manager ───────────────────────────────────────────────────
        self.sm = ScreenManager(transition=FadeTransition(duration=0.10))
        for screen in [
            ChatScreen(name="chat"),
            AgentiScreen(name="age"),
            RagScreen(name="rag"),
            GrafoScreen(name="grf"),
            KnowledgeScreen(name="knw"),
            FinanzaScreen(name="fin"),
            TFRScreen(name="tfr"),
            LavoroScreen(name="lav"),
            FileAiScreen(name="fil"),
            ReplScreen(name="repl"),
            ImpararScreen(name="imp"),
            MultimediaScreen(name="mul"),
            VoceScreen(name="wsp"),
            AppScreen(name="apps"),
            CodingScreen(name="cod"),
            MathScreen(name="mat"),
            GraphvizScreen(name="gvz"),
            GitScreen(name="git"),
            WikiScreen(name="wik"),
            SistemaScreen(name="sys"),
            SettingsScreen(name="settings"),
        ]:
            self.sm.add_widget(screen)
        root.add_widget(self.sm)

        self._goto("chat")
        Clock.schedule_once(self._fetch_models, 1.0)
        return root

    def _goto(self, name: str):
        self.sm.current = name
        for n, btn in self._nav_btns.items():
            active = n == name
            btn.background_color = _BG_ON  if active else _BG_OFF
            btn.color            = _FG_ON  if active else _FG_OFF

    def _fetch_models(self, *_):
        def on_done(models):
            if models:
                self.model = models[0]
            chat = self.sm.get_screen("chat")
            if hasattr(chat, "set_models"):
                chat.set_models(models)
        self.ai.fetch_models(on_done=on_done, on_error=lambda e: None)


if __name__ == "__main__":
    PrismaluxApp().run()
