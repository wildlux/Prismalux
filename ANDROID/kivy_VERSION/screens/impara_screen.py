from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.uix.spinner import Spinner
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App


ARGOMENTI = [
    "Matematica", "Fisica", "Programmazione", "Storia",
    "Inglese", "Economia", "Chimica", "Biologia", "Filosofia",
]
LIVELLI = ["Principiante", "Intermedio", "Avanzato", "Esperto"]


class ImpararScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._model = "ollama"
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", padding=dp(12), spacing=dp(10))

        root.add_widget(Label(
            text="📚 Impara con AI", font_size=dp(18), bold=True,
            size_hint_y=None, height=dp(36),
            color=(0.42, 0.39, 1.0, 1), halign="left",
        ))

        # ── argomento e livello ──
        row1 = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(8))
        self._arg_spin = Spinner(
            text="Matematica", values=ARGOMENTI, font_size=dp(13),
            background_color=(0.14, 0.15, 0.22, 1), color=(0.88, 0.88, 0.94, 1),
        )
        self._lv_spin = Spinner(
            text="Principiante", values=LIVELLI,
            size_hint_x=None, width=dp(130), font_size=dp(13),
            background_color=(0.14, 0.15, 0.22, 1), color=(0.88, 0.88, 0.94, 1),
        )
        row1.add_widget(self._arg_spin)
        row1.add_widget(self._lv_spin)
        root.add_widget(row1)

        # ── argomento personalizzato ──
        self._custom = TextInput(
            hint_text="Oppure scrivi argomento libero...",
            multiline=False, font_size=dp(13),
            size_hint_y=None, height=dp(44),
            background_color=(0.09, 0.10, 0.14, 1),
            foreground_color=(0.88, 0.88, 0.94, 1),
        )
        root.add_widget(self._custom)

        # ── pulsanti ──
        btn_row = BoxLayout(size_hint_y=None, height=dp(46), spacing=dp(6))
        for lbl, mode in [("📖 Spiega", "spiega"), ("❓ Quiz", "quiz"), ("💡 Esempi", "esempi")]:
            b = Button(text=lbl, font_size=dp(13),
                       background_color=(0.42, 0.39, 1.0, 1))
            b.bind(on_release=lambda _, m=mode: self._go(m))
            btn_row.add_widget(b)
        root.add_widget(btn_row)

        # ── output ──
        self._status = Label(text="", font_size=dp(12),
                             size_hint_y=None, height=dp(24),
                             color=(0.5, 0.5, 0.65, 1))
        root.add_widget(self._status)

        scroll = ScrollView()
        self._out = Label(
            text="", markup=True, halign="left", valign="top",
            font_size=dp(14), color=(0.88, 0.88, 0.94, 1),
            size_hint_y=None,
        )
        self._out.bind(texture_size=self._out.setter("size"))
        self._out.bind(width=lambda o, w: setattr(o, "text_size", (w, None)))
        scroll.add_widget(self._out)
        root.add_widget(scroll)

        self.add_widget(root)

    def _arg(self) -> str:
        return self._custom.text.strip() or self._arg_spin.text

    def _go(self, mode: str):
        arg = self._arg()
        if not arg:
            self._status.text = "Seleziona un argomento."
            return
        lv = self._lv_spin.text
        prompts = {
            "spiega": f"Spiega in modo chiaro per livello {lv}. Argomento: {arg}",
            "quiz":   f"Crea 3 domande quiz con risposta corretta. Argomento: {arg}",
            "esempi": f"Dai 3 esempi pratici e concreti di: {arg}",
        }
        prompt = prompts[mode]
        self._status.text = "⏳ Risposta AI in corso..."
        self._out.text = ""

        app = App.get_running_app()
        msgs = [
            {"role": "system", "content": "Sei un tutor esperto e paziente."},
            {"role": "user", "content": prompt},
        ]
        app.ai.chat_once(
            model=app.model,
            messages=msgs,
            on_done=self._on_done,
            on_error=self._on_error,
        )

    def _on_done(self, text: str):
        Clock.schedule_once(lambda _: self._show(text), 0)

    def _show(self, text: str):
        self._out.text = text
        self._status.text = "✅ Risposta ricevuta."

    def _on_error(self, err: str):
        Clock.schedule_once(lambda _: self._show_err(err), 0)

    def _show_err(self, err: str):
        self._out.text = f"❌ {err}"
        self._status.text = "Errore."
