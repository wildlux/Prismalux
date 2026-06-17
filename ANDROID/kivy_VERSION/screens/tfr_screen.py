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


def _field(label_text, hint, default, input_type="number"):
    col = BoxLayout(orientation="vertical", size_hint_y=None, height=dp(64), spacing=dp(2))
    col.add_widget(Label(text=label_text, font_size=dp(11), color=_DIM,
                         size_hint_y=None, height=dp(18),
                         halign="left", text_size=(None, None)))
    ti = TextInput(text=default, hint_text=hint, multiline=False,
                   font_size=dp(14), size_hint_y=None, height=dp(42),
                   input_filter=input_type,
                   background_color=_AIM, foreground_color=_TXT)
    col.add_widget(ti)
    return col, ti


class TFRScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="📊 TFR — Trattamento di Fine Rapporto",
                              font_size=dp(15), bold=True, color=_TXT,
                              size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))
        root.add_widget(Label(
            text="Calcolo TFR art. 2120 c.c. — logica eseguita dal server Qt",
            font_size=dp(12), color=_DIM, size_hint_y=None, height=dp(22),
            halign="left", text_size=(None, None)))

        # Riga 1: stipendio + anni
        row1 = BoxLayout(size_hint_y=None, height=dp(70), spacing=dp(8))
        c1, self._stip = _field("Stipendio lordo annuo (€)", "30000", "30000")
        c2, self._anni = _field("Anni lavorati", "10", "10")
        row1.add_widget(c1)
        row1.add_widget(c2)
        root.add_widget(row1)

        # Riga 2: inflazione + destinazione
        row2 = BoxLayout(size_hint_y=None, height=dp(70), spacing=dp(8))
        c3, self._infl = _field("Inflazione annua (%)", "2.0", "2.0", "float")
        dest_col = BoxLayout(orientation="vertical", size_hint_y=None,
                             height=dp(64), spacing=dp(2))
        dest_col.add_widget(Label(text="Destinazione", font_size=dp(11), color=_DIM,
                                  size_hint_y=None, height=dp(18),
                                  halign="left", text_size=(None, None)))
        self._dest = Spinner(text="In azienda",
                             values=["In azienda", "Fondo Pensione"],
                             font_size=dp(13), size_hint_y=None, height=dp(42),
                             background_color=(0.18, 0.19, 0.28, 1), color=_TXT)
        dest_col.add_widget(self._dest)
        row2.add_widget(c3)
        row2.add_widget(dest_col)
        root.add_widget(row2)

        btn = Button(text="📊 Calcola TFR", font_size=dp(15), bold=True,
                     size_hint_y=None, height=dp(52),
                     background_color=_ACC, color=(1, 1, 1, 1))
        btn.bind(on_release=self._calc)
        root.add_widget(btn)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        self._scroll = ScrollView(size_hint_y=1)
        self._result = BoxLayout(orientation="vertical", size_hint_y=None,
                                 padding=dp(10), spacing=dp(4))
        self._result.bind(minimum_height=self._result.setter("height"))
        self._scroll.add_widget(self._result)
        root.add_widget(self._scroll)

        self.add_widget(root)

    def _calc(self, *_):
        try:
            stip = float(self._stip.text or "0")
            anni = int(self._anni.text or "0")
            infl = float(self._infl.text or "2")
        except ValueError:
            self._status.text = "❌ Valori non validi"
            return
        in_fondo = self._dest.text == "Fondo Pensione"
        self._status.text = "⏳ Calcolo in corso..."
        self._result.clear_widgets()
        App.get_running_app().ai.tfr(
            stipendio=stip, anni=anni, inflazione=infl, in_fondo=in_fondo,
            on_done=self._on_done, on_error=self._on_err,
        )

    def _on_done(self, data: dict):
        Clock.schedule_once(lambda _: self._show(data), 0)

    def _show(self, data):
        self._status.text = ""
        self._result.clear_widgets()

        def row(k, v, accent=False):
            r = BoxLayout(size_hint_y=None, height=dp(28), padding=[dp(4), 0])
            c = _ACC if accent else _TXT
            r.add_widget(Label(text=k, font_size=dp(13), color=_DIM,
                               halign="left", text_size=(None, None)))
            r.add_widget(Label(text=str(v), font_size=dp(13) if not accent else dp(15),
                               color=c, bold=accent,
                               halign="right", text_size=(None, None)))
            self._result.add_widget(r)

        row("TFR maturato totale", f"€ {data.get('tfr_totale', data.get('totale', '—'))}", True)
        row("TFR netto stimato", f"€ {data.get('tfr_netto', data.get('netto', '—'))}")
        row("Quota annua media", f"€ {data.get('quota_annua', '—')}")
        row("Rivalutazione totale", f"€ {data.get('rivalutazione_totale', data.get('rivalutazione', '—'))}")

        years = data.get("anni_dettaglio", [])
        if years:
            self._result.add_widget(Label(
                text="[b]Dettaglio per anno:[/b]", markup=True, font_size=dp(12),
                color=_DIM, size_hint_y=None, height=dp(24),
                halign="left", text_size=(None, None)))
            for y in years[:20]:
                yr = y.get("anno", "")
                qt = y.get("quota", "")
                rv = y.get("rivalutazione", "")
                acc = y.get("accantonato", "")
                row(f"Anno {yr}", f"quota {qt}  riv. {rv}  acc. {acc}")

    def _on_err(self, err):
        Clock.schedule_once(lambda _: setattr(self._status, "text", f"❌ {err}"), 0)
