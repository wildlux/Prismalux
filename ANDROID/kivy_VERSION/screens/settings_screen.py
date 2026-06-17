from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.uix.switch import Switch
from kivy.metrics import dp
from kivy.app import App
from kivy.storage.jsonstore import JsonStore
import os


STORE_PATH = os.path.expanduser("~/.prismalux/kivy_settings.json")


class SettingsScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        os.makedirs(os.path.dirname(STORE_PATH), exist_ok=True)
        self._store = JsonStore(STORE_PATH)
        self._build_ui()
        self._load()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", padding=dp(16), spacing=dp(14))

        root.add_widget(Label(
            text="⚙️ Impostazioni", font_size=dp(18), bold=True,
            size_hint_y=None, height=dp(36),
            color=(0.42, 0.39, 1.0, 1),
        ))

        form = GridLayout(cols=2, spacing=dp(10), size_hint_y=None,
                          row_default_height=dp(46))
        form.bind(minimum_height=form.setter("height"))

        def lbl(t):
            return Label(text=t, font_size=dp(13), color=(0.6, 0.6, 0.7, 1),
                         halign="right", size_hint_x=None, width=dp(100))

        def field(hint="", pw=False) -> TextInput:
            return TextInput(
                hint_text=hint, multiline=False, font_size=dp(13),
                password=pw,
                background_color=(0.09, 0.10, 0.14, 1),
                foreground_color=(0.88, 0.88, 0.94, 1),
            )

        form.add_widget(lbl("Server URL"))
        self._url = field("http://192.168.1.100:11500")
        form.add_widget(self._url)

        form.add_widget(lbl("Token LAN"))
        self._token = field("(opzionale)", pw=True)
        form.add_widget(self._token)

        root.add_widget(form)

        # ── pulsante salva ──
        btn = Button(
            text="💾 Salva e Applica",
            size_hint_y=None, height=dp(50), font_size=dp(15), bold=True,
            background_color=(0.42, 0.39, 1.0, 1),
        )
        btn.bind(on_release=self._save)
        root.add_widget(btn)

        self._status = Label(text="", font_size=dp(13),
                             size_hint_y=None, height=dp(28),
                             color=(0.5, 0.5, 0.65, 1))
        root.add_widget(self._status)

        # ── info connessione ──
        btn_test = Button(
            text="🔗 Testa Connessione",
            size_hint_y=None, height=dp(44), font_size=dp(13),
            background_color=(0.18, 0.40, 0.18, 1),
        )
        btn_test.bind(on_release=self._test_conn)
        root.add_widget(btn_test)

        self._conn_lbl = Label(text="", font_size=dp(12),
                               size_hint_y=None, height=dp(24),
                               color=(0.5, 0.5, 0.65, 1))
        root.add_widget(self._conn_lbl)

        root.add_widget(Label(
            text="Prismalux Kivy v1.0\nConnette al server Qt desktop/LAN.",
            font_size=dp(11), color=(0.4, 0.4, 0.5, 1),
            halign="center", size_hint_y=None, height=dp(48),
        ))

        root.add_widget(Label())
        self.add_widget(root)

    def _load(self):
        if self._store.exists("server"):
            d = self._store.get("server")
            self._url.text = d.get("url", "http://192.168.1.100:11500")
            self._token.text = d.get("token", "")

    def _save(self, *_):
        url = self._url.text.strip().rstrip("/")
        token = self._token.text.strip()
        self._store.put("server", url=url, token=token)
        app = App.get_running_app()
        app.ai.base_url = url
        app.ai.token = token
        app.ai.fetch_models(
            on_done=lambda models: app.sm.get_screen("chat").set_models(models),
            on_error=lambda e: None,
        )
        self._status.text = "✅ Impostazioni salvate."

    def _test_conn(self, *_):
        self._conn_lbl.text = "⏳ Test in corso..."
        App.get_running_app().ai.sysinfo(
            on_done=lambda j: self._on_test_ok(j),
            on_error=lambda e: self._on_test_err(e),
        )

    def _on_test_ok(self, info: dict):
        from kivy.clock import Clock
        msg = f"✅ {info.get('hostname','?')} — CPU {info.get('cpu_pct','?')}% RAM {info.get('ram_used_gb','?')}/{info.get('ram_total_gb','?')} GB"
        Clock.schedule_once(lambda _: setattr(self._conn_lbl, "text", msg), 0)

    def _on_test_err(self, err: str):
        from kivy.clock import Clock
        Clock.schedule_once(lambda _: setattr(self._conn_lbl, "text", f"❌ {err}"), 0)
