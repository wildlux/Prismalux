from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.uix.spinner import Spinner
from kivy.uix.scrollview import ScrollView
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App

_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)
_ACC = (0.42, 0.39, 1.0, 1)

_LANGS = ["Python", "JavaScript", "C++", "Rust", "Go", "Java", "TypeScript",
          "SQL", "Bash", "HTML/CSS", "Kotlin", "Swift"]


class CodingScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="💻 Coding — Analisi AI del codice", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))

        top = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(6))
        top.add_widget(Label(text="Linguaggio:", font_size=dp(12), color=_DIM,
                             size_hint_x=None, width=dp(85)))
        self._lang = Spinner(text="Python", values=_LANGS, font_size=dp(13),
                             background_color=(0.18, 0.19, 0.28, 1), color=_TXT)
        top.add_widget(self._lang)
        root.add_widget(top)

        self._code = TextInput(
            hint_text="Incolla qui il codice da analizzare...",
            font_size=dp(13), size_hint_y=None, height=dp(200),
            background_color=_AIM, foreground_color=_TXT,
            font_name="RobotoMono-Regular",
        )
        root.add_widget(self._code)

        btn_row = BoxLayout(size_hint_y=None, height=dp(48), spacing=dp(6))
        for label, action in [("🔍 Analizza", "analizza"), ("🐛 Debugga", "debugga"),
                               ("✨ Migliora", "migliora"), ("📝 Documenta", "documenta")]:
            b = Button(text=label, font_size=dp(12), background_color=_ACC, color=(1,1,1,1))
            b.bind(on_release=lambda btn, a=action: self._ask(a))
            btn_row.add_widget(b)
        root.add_widget(btn_row)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        sv = ScrollView(size_hint_y=1)
        self._out = TextInput(
            hint_text="Risposta AI...", readonly=True,
            font_size=dp(13), size_hint_y=None,
            background_color=(0.06, 0.07, 0.10, 1), foreground_color=_TXT,
        )
        self._out.bind(minimum_height=self._out.setter("height"))
        sv.add_widget(self._out)
        root.add_widget(sv)

        self.add_widget(root)

    def _ask(self, action):
        code = self._code.text.strip()
        lang = self._lang.text
        if not code:
            self._status.text = "❌ Inserisci il codice prima"
            return
        prompts = {
            "analizza": f"Analizza questo codice {lang} e spiega cosa fa:\n```{lang}\n{code}\n```",
            "debugga": f"Trova bug e problemi in questo codice {lang}:\n```{lang}\n{code}\n```",
            "migliora": f"Suggerisci miglioramenti per questo codice {lang}:\n```{lang}\n{code}\n```",
            "documenta": f"Aggiungi docstring e commenti a questo codice {lang}:\n```{lang}\n{code}\n```",
        }
        self._status.text = f"⏳ {action.capitalize()}..."
        self._out.text = ""
        app = App.get_running_app()
        app.ai.chat_once(
            model=app.model,
            messages=[{"role": "user", "content": prompts[action]}],
            on_done=self._on_done,
            on_error=self._on_err,
        )

    def _on_done(self, reply: str):
        Clock.schedule_once(lambda _: self._show(reply), 0)

    def _show(self, reply):
        self._out.text = reply
        self._status.text = "✅ Analisi completata"

    def _on_err(self, err):
        Clock.schedule_once(lambda _: setattr(self._status, "text", f"❌ {err}"), 0)
