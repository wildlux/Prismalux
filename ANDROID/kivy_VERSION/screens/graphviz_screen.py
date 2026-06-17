from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
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

_EXAMPLE = """digraph G {
  rankdir=LR
  A [label="Prismalux"]
  B [label="AI"]
  C [label="RAG"]
  D [label="GraphMemory"]
  A -> B
  A -> C
  B -> D
  C -> D
}"""


class GraphvizScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="🗺 Graphviz — Generazione grafi", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))
        root.add_widget(Label(
            text="Scrivi DOT oppure descrivi il grafo e lascia che l'AI generi il codice DOT.",
            font_size=dp(11), color=_DIM, size_hint_y=None, height=dp(22),
            halign="left", text_size=(None, None)))

        self._dot = TextInput(
            text=_EXAMPLE, font_size=dp(12),
            size_hint_y=None, height=dp(180),
            background_color=_AIM, foreground_color=_TXT,
            font_name="RobotoMono-Regular",
        )
        root.add_widget(self._dot)

        self._desc = TextInput(
            hint_text="Oppure descrivi il grafo in italiano... (es: 'mappa concettuale di Python')",
            multiline=False, font_size=dp(13),
            size_hint_y=None, height=dp(44),
            background_color=_AIM, foreground_color=_TXT,
        )
        root.add_widget(self._desc)

        btn_row = BoxLayout(size_hint_y=None, height=dp(48), spacing=dp(6))
        btn_ai = Button(text="🤖 Genera DOT con AI", font_size=dp(13),
                        background_color=_ACC, color=(1,1,1,1))
        btn_ai.bind(on_release=self._ai_generate)
        btn_render = Button(text="🖼 Renderizza", font_size=dp(13),
                            background_color=(0.03, 0.59, 0.41, 1), color=(1,1,1,1))
        btn_render.bind(on_release=self._render)
        btn_ex = Button(text="📋 Esempio", size_hint_x=None, width=dp(90),
                        font_size=dp(12), background_color=(0.20, 0.21, 0.30, 1), color=_TXT)
        btn_ex.bind(on_release=lambda _: setattr(self._dot, "text", _EXAMPLE))
        btn_row.add_widget(btn_ai)
        btn_row.add_widget(btn_render)
        btn_row.add_widget(btn_ex)
        root.add_widget(btn_row)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(24))
        root.add_widget(self._status)

        self._out = TextInput(
            hint_text="URL immagine o messaggio dal server...", readonly=True,
            font_size=dp(12), size_hint_y=1,
            background_color=(0.06, 0.07, 0.10, 1), foreground_color=(0.46, 0.98, 0.46, 1),
            font_name="RobotoMono-Regular",
        )
        root.add_widget(self._out)
        self.add_widget(root)

    def _ai_generate(self, *_):
        desc = self._desc.text.strip()
        if not desc:
            self._status.text = "❌ Inserisci una descrizione"
            return
        self._status.text = "⏳ Generazione DOT con AI..."
        prompt = (f"Genera codice Graphviz DOT per: {desc}\n"
                  "Rispondi SOLO con il blocco DOT, nessun altro testo.")
        app = App.get_running_app()
        app.ai.chat_once(
            model=app.model,
            messages=[{"role": "user", "content": prompt}],
            on_done=self._on_ai_dot,
            on_error=lambda e: Clock.schedule_once(
                lambda _: setattr(self._status, "text", f"❌ {e}"), 0),
        )

    def _on_ai_dot(self, reply: str):
        Clock.schedule_once(lambda _: self._set_dot(reply), 0)

    def _set_dot(self, reply):
        start = reply.find("digraph")
        if start == -1:
            start = reply.find("graph")
        end = reply.rfind("}")
        if start >= 0 and end > start:
            self._dot.text = reply[start:end+1].strip()
        else:
            self._dot.text = reply.strip()
        self._status.text = "✅ DOT generato — premi Renderizza"

    def _render(self, *_):
        dot = self._dot.text.strip()
        if not dot:
            return
        self._status.text = "⏳ Rendering..."
        App.get_running_app().ai.graphviz(
            dot=dot,
            on_done=self._on_render,
            on_error=lambda e: Clock.schedule_once(
                lambda _: setattr(self._status, "text", f"❌ {e}"), 0),
        )

    def _on_render(self, result):
        Clock.schedule_once(lambda _: self._show_render(result), 0)

    def _show_render(self, result):
        if isinstance(result, dict):
            url = result.get("url", result.get("image", str(result)))
        else:
            url = str(result)
        self._out.text = url
        self._status.text = "✅ Grafo renderizzato"
