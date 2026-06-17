from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App

_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)
_ACC = (0.42, 0.39, 1.0, 1)
_BRD = (0.16, 0.18, 0.27, 1)


class RagScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="📚 RAG — Ricerca Semantica", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))
        root.add_widget(Label(
            text="Fai una domanda — l'AI cerca i chunk più rilevanti nel tuo indice RAG",
            font_size=dp(12), color=_DIM, size_hint_y=None, height=dp(22),
            halign="left", text_size=(None, None)))

        self._query = TextInput(
            hint_text="Es: Come funziona il protocollo TCP?",
            size_hint_y=None, height=dp(72), font_size=dp(13),
            background_color=_AIM, foreground_color=_TXT,
        )
        root.add_widget(self._query)

        self._btn = Button(
            text="🔍 Cerca nel RAG", font_size=dp(14), bold=True,
            size_hint_y=None, height=dp(48),
            background_color=_ACC, color=(1, 1, 1, 1),
        )
        self._btn.bind(on_release=self._search)
        root.add_widget(self._btn)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        self._scroll = ScrollView(size_hint_y=1)
        self._results = BoxLayout(orientation="vertical", spacing=dp(8),
                                  size_hint_y=None, padding=[0, dp(4)])
        self._results.bind(minimum_height=self._results.setter("height"))
        self._scroll.add_widget(self._results)
        root.add_widget(self._scroll)

        self.add_widget(root)

    def _search(self, *_):
        q = self._query.text.strip()
        if not q:
            return
        self._btn.disabled = True
        self._status.text = "⏳ Ricerca in corso..."
        self._results.clear_widgets()
        App.get_running_app().ai.rag(
            query=q, k=5,
            on_done=self._on_done,
            on_error=self._on_error,
        )

    def _on_done(self, chunks: list):
        Clock.schedule_once(lambda _: self._show(chunks), 0)

    def _show(self, chunks):
        self._btn.disabled = False
        if not chunks:
            self._status.text = "Nessun risultato trovato."
            return
        self._status.text = f"{len(chunks)} risultati"
        for c in chunks:
            card = BoxLayout(orientation="vertical", size_hint_y=None,
                             padding=dp(10), spacing=dp(4))
            card.canvas.before.clear()
            from kivy.graphics import Color, RoundedRectangle
            with card.canvas.before:
                Color(*_AIM)
                card.rect = RoundedRectangle(pos=card.pos, size=card.size, radius=[dp(8)])
            card.bind(pos=lambda w, _: setattr(w, 'rect', _upd(w)),
                      size=lambda w, _: setattr(w, 'rect', _upd(w)))

            txt = c.get("text", c.get("content", str(c)))[:400]
            src = c.get("source", c.get("file", ""))
            score = c.get("score", c.get("similarity", ""))

            body = Label(text=txt, font_size=dp(13), color=_TXT,
                         markup=True, size_hint_y=None,
                         halign="left", text_size=(None, None))
            body.bind(texture_size=body.setter("size"))
            card.add_widget(body)

            meta = f"[color=888888]{src}  {('%.3f' % score) if score else ''}[/color]"
            ml = Label(text=meta, font_size=dp(11), markup=True,
                       size_hint_y=None, height=dp(18),
                       halign="left", text_size=(None, None))
            card.add_widget(ml)
            card.bind(minimum_height=card.setter("height"))
            self._results.add_widget(card)

    def _on_error(self, err):
        Clock.schedule_once(lambda _: self._show_err(err), 0)

    def _show_err(self, err):
        self._btn.disabled = False
        self._status.text = f"❌ {err}"


def _upd(w):
    from kivy.graphics import RoundedRectangle
    from kivy.metrics import dp
    w.canvas.before.clear()
    from kivy.graphics import Color
    with w.canvas.before:
        Color(0.12, 0.13, 0.20, 1)
        r = RoundedRectangle(pos=w.pos, size=w.size, radius=[dp(8)])
    return r
