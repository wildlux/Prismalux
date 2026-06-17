from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App


class JobCard(BoxLayout):
    def __init__(self, offerta: dict, on_select, **kw):
        super().__init__(orientation="vertical", size_hint_y=None,
                         padding=dp(10), spacing=dp(4), **kw)
        self.bind(minimum_height=self.setter("height"))

        top = BoxLayout(size_hint_y=None, height=dp(22))
        az = Label(text=f"[b]{offerta.get('azienda', '')}[/b]",
                   markup=True, font_size=dp(13), halign="left",
                   color=(0.88, 0.88, 0.94, 1))
        az.bind(width=lambda o, w: setattr(o, "text_size", (w, None)))
        ruolo = Label(text=offerta.get("ruolo", ""), font_size=dp(12),
                      halign="right", color=(0.6, 0.6, 0.7, 1),
                      size_hint_x=None, width=dp(120))
        top.add_widget(az)
        top.add_widget(ruolo)
        self.add_widget(top)

        req = offerta.get("requisiti", "")
        if req and not req.startswith("http"):
            req_lbl = Label(text=req, font_size=dp(11), color=(0.5, 0.5, 0.6, 1),
                            halign="left", size_hint_y=None)
            req_lbl.bind(texture_size=req_lbl.setter("size"))
            req_lbl.bind(width=lambda o, w: setattr(o, "text_size", (w, None)))
            self.add_widget(req_lbl)

        btn = Button(text="💬 Analizza con AI", size_hint_y=None, height=dp(34),
                     font_size=dp(12), background_color=(0.22, 0.23, 0.34, 1))
        btn.bind(on_release=lambda _: on_select(offerta))
        self.add_widget(btn)


class LavoroScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._offerte: list = []
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", padding=dp(10), spacing=dp(8))

        root.add_widget(Label(
            text="💼 Cerca Lavoro", font_size=dp(18), bold=True,
            size_hint_y=None, height=dp(36),
            color=(0.42, 0.39, 1.0, 1), halign="left",
        ))

        # ── barra ricerca ──
        bar = BoxLayout(size_hint_y=None, height=dp(46), spacing=dp(8))
        self._q = TextInput(
            hint_text="Es: sviluppatore, Milano, Python...",
            multiline=False, font_size=dp(13),
            background_color=(0.09, 0.10, 0.14, 1),
            foreground_color=(0.88, 0.88, 0.94, 1),
        )
        btn = Button(text="🔍 Cerca", size_hint_x=None, width=dp(90),
                     font_size=dp(13), background_color=(0.42, 0.39, 1.0, 1))
        btn.bind(on_release=self._search)
        self._q.bind(on_text_validate=self._search)
        bar.add_widget(self._q)
        bar.add_widget(btn)
        root.add_widget(bar)

        self._status = Label(text="Premi Cerca per caricare le offerte.",
                             font_size=dp(12), size_hint_y=None, height=dp(24),
                             color=(0.5, 0.5, 0.65, 1))
        root.add_widget(self._status)

        self._scroll = ScrollView()
        self._list = BoxLayout(orientation="vertical", spacing=dp(8),
                               size_hint_y=None, padding=[0, dp(4)])
        self._list.bind(minimum_height=self._list.setter("height"))
        self._scroll.add_widget(self._list)
        root.add_widget(self._scroll)

        self.add_widget(root)

    def _search(self, *_):
        q = self._q.text.strip()
        self._status.text = "⏳ Ricerca in corso..."
        self._list.clear_widgets()
        App.get_running_app().ai.lavoro(
            query=q,
            on_done=self._on_done,
            on_error=self._on_error,
        )

    def _on_done(self, offerte: list):
        Clock.schedule_once(lambda _: self._render(offerte), 0)

    def _render(self, offerte: list):
        self._list.clear_widgets()
        self._offerte = offerte
        if not offerte:
            self._status.text = "Nessuna offerta trovata."
            return
        self._status.text = f"{len(offerte)} offerte trovate."
        for o in offerte:
            card = JobCard(o, on_select=self._analyze)
            self._list.add_widget(card)

    def _analyze(self, offerta: dict):
        app = App.get_running_app()
        testo = (
            f"Analizza questa offerta di lavoro e suggerisci come candidarmi:\n\n"
            f"Azienda: {offerta.get('azienda', '')}\n"
            f"Ruolo: {offerta.get('ruolo', '')}\n"
            f"Sede: {offerta.get('sede', '')}\n"
            f"Requisiti: {offerta.get('requisiti', '')}"
        )
        # invia alla Chat
        chat = app.sm.get_screen("chat")
        chat._input.text = testo
        app._goto("chat")

    def _on_error(self, err: str):
        Clock.schedule_once(lambda _: self._show_err(err), 0)

    def _show_err(self, err: str):
        self._status.text = f"❌ {err}"
