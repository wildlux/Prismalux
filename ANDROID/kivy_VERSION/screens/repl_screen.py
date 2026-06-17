from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.uix.spinner import Spinner
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App

_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)
_ACC = (0.42, 0.39, 1.0, 1)

_PRESETS = {
    "Fibonacci": "def fib(n):\n    a,b=0,1\n    for _ in range(n): a,b=b,a+b\n    return a\nprint([fib(i) for i in range(10)])",
    "Primes": "def is_prime(n):\n    if n<2: return False\n    return all(n%i for i in range(2,int(n**.5)+1))\nprint([x for x in range(2,50) if is_prime(x)])",
    "List comp": "matrix=[[i*j for j in range(1,6)] for i in range(1,6)]\nfor r in matrix: print(r)",
    "Hello World": "print('Hello, Prismalux!')\nprint('Python REPL attivo')",
}


class ReplScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="🐍 REPL — Python interattivo", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))

        top = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(6))
        self._preset = Spinner(
            text="Hello World",
            values=list(_PRESETS.keys()),
            font_size=dp(13), size_hint_x=1,
            background_color=(0.18, 0.19, 0.28, 1), color=_TXT,
        )
        self._preset.bind(text=self._load_preset)
        btn_run = Button(text="▶ Esegui", size_hint_x=None, width=dp(90),
                         font_size=dp(13), background_color=_ACC, color=(1,1,1,1))
        btn_run.bind(on_release=self._run)
        btn_clr = Button(text="🗑", size_hint_x=None, width=dp(44),
                         font_size=dp(16), background_color=(0.20, 0.21, 0.30, 1),
                         color=_TXT)
        btn_clr.bind(on_release=lambda _: self._clear())
        top.add_widget(self._preset)
        top.add_widget(btn_run)
        top.add_widget(btn_clr)
        root.add_widget(top)

        self._code = TextInput(
            hint_text="Scrivi codice Python...",
            font_size=dp(13), size_hint_y=None, height=dp(180),
            background_color=_AIM, foreground_color=_TXT,
            font_name="RobotoMono-Regular",
        )
        root.add_widget(self._code)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        self._out = TextInput(
            hint_text="Output...", readonly=True,
            font_size=dp(13), size_hint_y=1,
            background_color=(0.06, 0.07, 0.10, 1), foreground_color=(0.46, 0.98, 0.46, 1),
            font_name="RobotoMono-Regular",
        )
        root.add_widget(self._out)

        self.add_widget(root)
        self._load_preset(None, "Hello World")

    def _load_preset(self, _, text):
        self._code.text = _PRESETS.get(text, "")

    def _run(self, *_):
        code = self._code.text.strip()
        if not code:
            return
        self._status.text = "⏳ Esecuzione..."
        self._out.text = ""
        App.get_running_app().ai.repl(
            code=code,
            on_done=self._on_done,
            on_error=self._on_err,
        )

    def _on_done(self, result: dict):
        Clock.schedule_once(lambda _: self._show(result), 0)

    def _show(self, result):
        stdout = result.get("stdout", result.get("output", ""))
        stderr = result.get("stderr", result.get("error", ""))
        self._status.text = "✅ Completato" if not stderr else "⚠ Errore"
        self._out.text = stdout + ("\n--- stderr ---\n" + stderr if stderr else "")

    def _on_err(self, err):
        Clock.schedule_once(lambda _: self._show_err(err), 0)

    def _show_err(self, err):
        self._status.text = f"❌ {err}"
        self._out.text = str(err)

    def _clear(self):
        self._code.text = ""
        self._out.text = ""
        self._status.text = ""
