from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.uix.spinner import Spinner
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App


class FinanzaScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._cf_result = ""
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", padding=dp(12), spacing=dp(10))

        root.add_widget(Label(
            text="💰 Finanza — Codice Fiscale", font_size=dp(18), bold=True,
            size_hint_y=None, height=dp(36),
            color=(0.42, 0.39, 1.0, 1), halign="left",
        ))
        root.add_widget(Label(
            text="Calcolo CF italiano (D.M. 1976) via server Qt.",
            font_size=dp(12), size_hint_y=None, height=dp(22),
            color=(0.5, 0.5, 0.65, 1),
        ))

        form = GridLayout(cols=2, spacing=dp(8), size_hint_y=None,
                          row_default_height=dp(44))
        form.bind(minimum_height=form.setter("height"))

        def field(hint, **kw) -> TextInput:
            return TextInput(
                hint_text=hint, multiline=False, font_size=dp(13),
                background_color=(0.09, 0.10, 0.14, 1),
                foreground_color=(0.88, 0.88, 0.94, 1), **kw,
            )

        def lbl(t):
            return Label(text=t, font_size=dp(13), color=(0.6, 0.6, 0.7, 1),
                         halign="right", size_hint_x=None, width=dp(90))

        form.add_widget(lbl("Nome"))
        self._nome = field("Mario")
        form.add_widget(self._nome)

        form.add_widget(lbl("Cognome"))
        self._cogn = field("Rossi")
        form.add_widget(self._cogn)

        form.add_widget(lbl("Data nascita"))
        self._data = field("AAAA-MM-GG")
        form.add_widget(self._data)

        form.add_widget(lbl("Sesso"))
        self._sesso = Spinner(
            text="M", values=["M", "F"], font_size=dp(13),
            background_color=(0.14, 0.15, 0.22, 1), color=(0.88, 0.88, 0.94, 1),
        )
        form.add_widget(self._sesso)

        form.add_widget(lbl("Comune"))
        self._comune = field("Roma")
        form.add_widget(self._comune)

        root.add_widget(form)

        btn = Button(
            text="🧮 Calcola Codice Fiscale",
            size_hint_y=None, height=dp(50), font_size=dp(15), bold=True,
            background_color=(0.42, 0.39, 1.0, 1),
        )
        btn.bind(on_release=self._calcola)
        root.add_widget(btn)

        self._status = Label(text="", font_size=dp(13),
                             size_hint_y=None, height=dp(24),
                             color=(0.5, 0.5, 0.65, 1))
        root.add_widget(self._status)

        # risultato
        self._result_box = BoxLayout(
            orientation="vertical", size_hint_y=None, height=dp(100),
            padding=dp(12), spacing=dp(6),
        )
        self._cf_lbl = Label(
            text="", font_size=dp(30), bold=True,
            color=(0.42, 0.39, 1.0, 1), font_name="RobotoMono-Regular",
        )
        self._result_box.add_widget(Label(
            text="Codice Fiscale", font_size=dp(11),
            color=(0.5, 0.5, 0.65, 1),
        ))
        self._result_box.add_widget(self._cf_lbl)
        btn_copy = Button(
            text="📋 Copia", size_hint_y=None, height=dp(34),
            font_size=dp(13), background_color=(0.22, 0.23, 0.34, 1),
        )
        btn_copy.bind(on_release=self._copy)
        self._result_box.add_widget(btn_copy)
        self._result_box.opacity = 0
        root.add_widget(self._result_box)

        root.add_widget(Label())  # spacer
        self.add_widget(root)

    def _calcola(self, *_):
        nome = self._nome.text.strip()
        cogn = self._cogn.text.strip()
        data = self._data.text.strip()
        sesso = self._sesso.text
        comune = self._comune.text.strip()

        if not all([nome, cogn, data, comune]):
            self._status.text = "⚠️ Compila tutti i campi."
            return

        self._status.text = "⏳ Calcolo in corso..."
        self._result_box.opacity = 0

        App.get_running_app().ai.codice_fiscale(
            nome=nome, cognome=cogn, data=data, sesso=sesso, comune=comune,
            on_done=self._on_done,
            on_error=self._on_error,
        )

    def _on_done(self, cf: str, info: str):
        Clock.schedule_once(lambda _: self._show(cf, info), 0)

    def _show(self, cf: str, info: str):
        self._cf_result = cf
        self._cf_lbl.text = cf
        self._result_box.opacity = 1
        self._status.text = info or "✅ Calcolato."

    def _on_error(self, err: str):
        Clock.schedule_once(lambda _: self._show_err(err), 0)

    def _show_err(self, err: str):
        self._status.text = f"❌ {err}"

    def _copy(self, *_):
        if not self._cf_result:
            return
        try:
            from kivy.core.clipboard import Clipboard
            Clipboard.copy(self._cf_result)
            self._status.text = "📋 Copiato!"
        except Exception:
            self._status.text = "Copia non disponibile."
