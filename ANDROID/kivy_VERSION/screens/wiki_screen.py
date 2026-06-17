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

_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)
_ACC = (0.42, 0.39, 1.0, 1)


class WikiScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._last_result = ""
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="📰 Wiki & Web — Ricerca", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))

        search_row = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(6))
        self._q = TextInput(
            hint_text="Es: Python programming language",
            multiline=False, font_size=dp(13),
            background_color=_AIM, foreground_color=_TXT,
        )
        self._lang = Spinner(text="it", values=["it", "en", "de", "fr", "es", "pt", "ja", "zh"],
                             size_hint_x=None, width=dp(52), font_size=dp(13),
                             background_color=(0.18, 0.19, 0.28, 1), color=_TXT)
        btn_s = Button(text="🔍", size_hint_x=None, width=dp(44),
                       font_size=dp(16), background_color=_ACC, color=(1,1,1,1))
        btn_s.bind(on_release=self._search)
        search_row.add_widget(self._q)
        search_row.add_widget(self._lang)
        search_row.add_widget(btn_s)
        root.add_widget(search_row)
        self._q.bind(on_text_validate=self._search)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        sv = ScrollView(size_hint_y=1)
        self._result = TextInput(
            hint_text="Il risultato Wiki apparirà qui...", readonly=True,
            font_size=dp(13), size_hint_y=None,
            background_color=_AIM, foreground_color=_TXT,
        )
        self._result.bind(minimum_height=self._result.setter("height"))
        sv.add_widget(self._result)
        root.add_widget(sv)

        btn_chat = Button(
            text="💬 Approfondisci in Chat", font_size=dp(13),
            size_hint_y=None, height=dp(44),
            background_color=(0.18, 0.19, 0.28, 1), color=_TXT,
        )
        btn_chat.bind(on_release=self._send_chat)
        root.add_widget(btn_chat)

        self.add_widget(root)

    def _search(self, *_):
        q = self._q.text.strip()
        if not q:
            return
        lang = self._lang.text
        self._status.text = "⏳ Ricerca in corso..."
        self._result.text = ""
        App.get_running_app().ai.wiki(
            q=q, lang=lang,
            on_done=self._on_done,
            on_error=self._on_err,
        )

    def _on_done(self, result):
        Clock.schedule_once(lambda _: self._show(result), 0)

    def _show(self, result):
        if isinstance(result, dict):
            text = result.get("summary", result.get("extract", result.get("text", str(result))))
        else:
            text = str(result)
        self._last_result = text
        self._result.text = text
        self._status.text = f"✅ {len(text)} caratteri"

    def _on_err(self, err):
        Clock.schedule_once(lambda _: setattr(self._status, "text", f"❌ {err}"), 0)

    def _send_chat(self, *_):
        if not self._last_result:
            return
        app = App.get_running_app()
        chat = app.sm.get_screen("chat")
        q = self._q.text.strip()
        chat._input.text = f"Riguardo a: {q}\n\n{self._last_result[:2000]}"
        app.sm.current = "chat"
