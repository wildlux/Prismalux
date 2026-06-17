from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.uix.spinner import Spinner
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.properties import StringProperty, ListProperty
from kivy.app import App


class BubbleLabel(Label):
    role = StringProperty("ai")

    def __init__(self, text="", role="ai", **kw):
        super().__init__(**kw)
        self.role = role
        self.text = text
        self.markup = True
        self.text_size = (None, None)
        self.size_hint_y = None
        self.bind(texture_size=self._update_height)
        self.padding = (dp(12), dp(8))
        if role == "user":
            self.halign = "right"
            self.color = (1, 1, 1, 1)
            self.canvas.before.clear()
        else:
            self.halign = "left"
            self.color = (0.88, 0.88, 0.94, 1)

    def _update_height(self, *_):
        self.height = self.texture_size[1] + dp(16)

    def on_width(self, *_):
        self.text_size = (self.width - dp(24), None)

    def append_text(self, chunk: str):
        self.text += chunk


class ChatScreen(Screen):
    history = ListProperty([])

    def __init__(self, **kw):
        super().__init__(**kw)
        self._ai_bubble: BubbleLabel | None = None
        self._model = "ollama"
        self._full_response = ""
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(4))

        # ── top bar ──
        top = BoxLayout(size_hint_y=None, height=dp(48), spacing=dp(8),
                        padding=[dp(8), dp(4)])
        self._model_spin = Spinner(
            text=self._model, values=[self._model],
            size_hint_x=1, font_size=dp(13),
            background_color=(0.14, 0.15, 0.22, 1),
            color=(0.88, 0.88, 0.94, 1),
        )
        btn_new = Button(
            text="🗑 Nuova", size_hint_x=None, width=dp(90),
            font_size=dp(12), background_color=(0.25, 0.25, 0.38, 1),
        )
        btn_new.bind(on_release=self._new_chat)
        top.add_widget(self._model_spin)
        top.add_widget(btn_new)
        root.add_widget(top)

        # ── log scroll ──
        self._scroll = ScrollView(size_hint_y=1)
        self._log = BoxLayout(orientation="vertical", spacing=dp(6),
                              size_hint_y=None, padding=[dp(8), dp(8)])
        self._log.bind(minimum_height=self._log.setter("height"))
        self._scroll.add_widget(self._log)
        root.add_widget(self._scroll)

        # ── input bar ──
        bar = BoxLayout(size_hint_y=None, height=dp(56), spacing=dp(6),
                        padding=[dp(8), dp(6)])
        self._input = TextInput(
            hint_text="Scrivi un messaggio...", multiline=False,
            font_size=dp(14), size_hint_x=1,
            background_color=(0.09, 0.10, 0.14, 1),
            foreground_color=(0.88, 0.88, 0.94, 1),
        )
        self._input.bind(on_text_validate=self._send)
        self._send_btn = Button(
            text="Invia", size_hint_x=None, width=dp(72),
            font_size=dp(14), bold=True,
            background_color=(0.42, 0.39, 1.0, 1),
        )
        self._send_btn.bind(on_release=self._send)
        self._stop_btn = Button(
            text="⏹", size_hint_x=None, width=dp(44),
            font_size=dp(16), background_color=(0.7, 0.2, 0.2, 1),
            disabled=True,
        )
        self._stop_btn.bind(on_release=self._stop)
        bar.add_widget(self._input)
        bar.add_widget(self._stop_btn)
        bar.add_widget(self._send_btn)
        root.add_widget(bar)

        self.add_widget(root)

    # ── chiamato da App quando i modelli sono pronti ──
    def set_models(self, models: list):
        self._model_spin.values = models
        if models:
            self._model_spin.text = models[0]
            self._model = models[0]
        self._model_spin.bind(text=self._on_model_change)

    def _on_model_change(self, _, value):
        self._model = value

    def _add_bubble(self, text: str, role: str) -> BubbleLabel:
        bbl = BubbleLabel(text=text, role=role)
        bbl.size_hint_x = 0.82
        if role == "user":
            wrap = BoxLayout(size_hint_y=None)
            wrap.bind(minimum_height=wrap.setter("height"))
            spacer = Label(size_hint_x=0.18)
            wrap.add_widget(spacer)
            wrap.add_widget(bbl)
            self._log.add_widget(wrap)
        else:
            self._log.add_widget(bbl)
        Clock.schedule_once(lambda _: setattr(self._scroll, "scroll_y", 0), 0.05)
        return bbl

    def _send(self, *_):
        msg = self._input.text.strip()
        if not msg:
            return
        self._input.text = ""
        self._add_bubble(msg, "user")
        self.history.append({"role": "user", "content": msg})
        self._ai_bubble = self._add_bubble("...", "ai")
        self._full_response = ""
        self._send_btn.disabled = True
        self._stop_btn.disabled = False

        app = App.get_running_app()
        msgs = list(self.history[:-1]) + [{"role": "user", "content": msg}]
        app.ai.chat_stream(
            model=self._model,
            messages=msgs,
            on_token=self._on_token,
            on_done=self._on_done,
            on_error=self._on_error,
        )

    def _on_token(self, tok: str):
        self._full_response += tok
        Clock.schedule_once(lambda _: self._update_bubble(), 0)

    def _update_bubble(self):
        if self._ai_bubble:
            self._ai_bubble.text = self._full_response
            Clock.schedule_once(lambda _: setattr(self._scroll, "scroll_y", 0), 0.02)

    def _on_done(self, full: str):
        self.history.append({"role": "assistant", "content": full})
        Clock.schedule_once(lambda _: self._reset_bar(), 0)

    def _on_error(self, err: str):
        Clock.schedule_once(lambda _: self._show_error(err), 0)

    def _reset_bar(self):
        self._send_btn.disabled = False
        self._stop_btn.disabled = True

    def _show_error(self, err: str):
        if self._ai_bubble:
            self._ai_bubble.text = f"[color=ff6666]❌ {err}[/color]"
        self._reset_bar()

    def _stop(self, *_):
        App.get_running_app().ai.abort()
        self._reset_bar()

    def _new_chat(self, *_):
        self.history.clear()
        self._log.clear_widgets()
        self._ai_bubble = None
