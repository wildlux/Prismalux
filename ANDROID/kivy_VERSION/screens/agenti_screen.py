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

_RUOLI = ["Ricercatore", "Scrittore", "Revisore", "Programmatore", "Analista"]
_BG = (0.09, 0.10, 0.14, 1)
_ACC = (0.42, 0.39, 1.0, 1)
_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)


class AgentiScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._model = "ollama"
        self._running = False
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(4), padding=dp(8))

        # Titolo
        root.add_widget(Label(text="🤖 Pipeline Multi-Agente", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))

        # Task input
        root.add_widget(Label(text="Task da eseguire:", font_size=dp(12),
                              color=_DIM, size_hint_y=None, height=dp(22), halign="left",
                              text_size=(None, None)))
        self._task_in = TextInput(
            hint_text="Es: Scrivi un riassunto su X e revisiona il risultato",
            size_hint_y=None, height=dp(72), font_size=dp(13),
            background_color=_AIM, foreground_color=_TXT,
        )
        root.add_widget(self._task_in)

        # Ruoli agenti
        roles_row = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(6))
        self._spin = []
        defaults = ["Ricercatore", "Scrittore", "Revisore"]
        for i, d in enumerate(defaults):
            col = BoxLayout(orientation="vertical")
            col.add_widget(Label(text=f"Agente {i+1}", font_size=dp(10),
                                 color=_DIM, size_hint_y=None, height=dp(14)))
            sp = Spinner(text=d, values=_RUOLI, font_size=dp(11),
                         background_color=(0.18, 0.19, 0.28, 1), color=_TXT)
            col.add_widget(sp)
            self._spin.append(sp)
            roles_row.add_widget(col)
        root.add_widget(roles_row)

        # Pulsante Avvia
        self._run_btn = Button(
            text="▶ Avvia Pipeline", font_size=dp(14), bold=True,
            size_hint_y=None, height=dp(48),
            background_color=_ACC, color=(1, 1, 1, 1),
        )
        self._run_btn.bind(on_release=self._run)
        root.add_widget(self._run_btn)

        # Log
        self._scroll = ScrollView(size_hint_y=1)
        self._log = BoxLayout(orientation="vertical", spacing=dp(6),
                              size_hint_y=None, padding=[0, dp(4)])
        self._log.bind(minimum_height=self._log.setter("height"))
        self._scroll.add_widget(self._log)
        root.add_widget(self._scroll)

        self.add_widget(root)

    def set_models(self, models: list):
        if models:
            self._model = models[0]

    def _add_log(self, text: str, color=None):
        c = color or _TXT
        lbl = Label(text=text, markup=True, font_size=dp(13), color=c,
                    size_hint_y=None, halign="left",
                    text_size=(None, None))
        lbl.bind(texture_size=lbl.setter("size"))
        self._log.add_widget(lbl)
        Clock.schedule_once(lambda _: setattr(self._scroll, "scroll_y", 0), 0.05)

    def _run(self, *_):
        task = self._task_in.text.strip()
        if not task or self._running:
            return
        self._running = True
        self._run_btn.disabled = True
        self._log.clear_widgets()
        self._add_log(f"[b]Task:[/b] {task}")
        self._agents = [sp.text for sp in self._spin]
        self._ctx = ""
        self._step = 0
        self._run_next()

    def _run_next(self):
        if self._step >= len(self._agents):
            self._add_log("\n[b][color=50c878]✅ Pipeline completata[/color][/b]")
            self._running = False
            self._run_btn.disabled = False
            return
        role = self._agents[self._step]
        self._add_log(f"\n[b][color=6c63ff]🤖 Agente {self._step+1} — {role}[/color][/b]")
        sys = (f"Sei un agente con ruolo: {role}. "
               f"Leggi il contesto e contribuisci al task assegnato.")
        ctx_msg = (f"Task: {self._task_in.text.strip()}"
                   + (f"\n\nContesto precedente:\n{self._ctx}" if self._ctx else ""))
        msgs = [{"role": "user", "content": ctx_msg}]
        app = App.get_running_app()
        app.ai.chat_once(
            model=self._model,
            messages=[{"role": "system", "content": sys}] + msgs,
            on_done=self._on_agent_done,
            on_error=self._on_agent_err,
        )

    def _on_agent_done(self, text: str):
        role = self._agents[self._step]
        self._ctx += f"\n[{role}]: {text}"
        Clock.schedule_once(lambda _: self._after_agent(text), 0)

    def _after_agent(self, text: str):
        self._add_log(text)
        self._step += 1
        self._run_next()

    def _on_agent_err(self, err: str):
        Clock.schedule_once(lambda _: self._add_log(
            f"[color=ff6666]❌ {err}[/color]"), 0)
        self._running = False
        self._run_btn.disabled = False
