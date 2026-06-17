from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.uix.spinner import Spinner
from kivy.uix.gridlayout import GridLayout
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App
import re


COSTANTI = {
    "pi": "π  pi greco",
    "e": "e  Eulero",
    "phi": "φ  sezione aurea",
    "sqrt2": "√2",
    "euler_gamma": "γ  Euler-Mascheroni",
    "ln2": "ln(2)",
    "catalan": "C  Catalan",
}

ESEMPI_EXPR = [
    ("pi**2/6", "π²/6"),
    ("integrate(x**2,(x,0,1))", "∫x²dx"),
    ("factorial(100)", "100!"),
    ("gcd(144,180)", "mcd(144,180)"),
    ("sqrt(2+sqrt(3))", "√(2+√3)"),
]

SIMBOLI = ["π","√","²","³","Σ","∫","∂","∞","→","±","×","÷","≤","≥","≠","≈","α","β","γ","δ","θ","λ","φ","ω"]


class MathScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", padding=dp(10), spacing=dp(8))

        title = Label(text="π  Matematica", font_size=dp(18), bold=True,
                      size_hint_y=None, height=dp(36),
                      color=(0.42, 0.39, 1.0, 1), halign="left")
        root.add_widget(title)

        scroll = ScrollView()
        content = BoxLayout(orientation="vertical", spacing=dp(10),
                            size_hint_y=None, padding=[0, dp(4)])
        content.bind(minimum_height=content.setter("height"))

        # ── Sezione: Espressione ──
        content.add_widget(self._section("🧮 Espressione (sympy)"))

        self._expr_input = TextInput(
            hint_text="es. sqrt(2)+sin(pi/4)  oppure  integrate(x**2,(x,0,1))",
            multiline=False, font_size=dp(13),
            size_hint_y=None, height=dp(44),
            background_color=(0.09, 0.10, 0.14, 1),
            foreground_color=(0.88, 0.88, 0.94, 1),
        )
        content.add_widget(self._expr_input)

        # esempi rapidi
        eg_row = BoxLayout(size_hint_y=None, height=dp(36), spacing=dp(6))
        for val, lbl in ESEMPI_EXPR:
            b = Button(text=lbl, font_size=dp(11),
                       background_color=(0.18, 0.19, 0.28, 1))
            b.bind(on_release=lambda _, v=val: setattr(self._expr_input, "text", v))
            eg_row.add_widget(b)
        content.add_widget(eg_row)

        prec_row = BoxLayout(size_hint_y=None, height=dp(36), spacing=dp(8))
        prec_row.add_widget(Label(text="Cifre:", size_hint_x=None, width=dp(52),
                                  font_size=dp(13), color=(0.6, 0.6, 0.7, 1)))
        self._prec = TextInput(text="50", multiline=False, font_size=dp(13),
                               size_hint_x=None, width=dp(70),
                               background_color=(0.09, 0.10, 0.14, 1),
                               foreground_color=(0.88, 0.88, 0.94, 1))
        prec_row.add_widget(self._prec)
        prec_row.add_widget(Label())
        content.add_widget(prec_row)

        btn_row = BoxLayout(size_hint_y=None, height=dp(42), spacing=dp(8))
        for lbl, action in [("🧮 Calcola", "expr"), ("♾ Semplifica", "simplify")]:
            b = Button(text=lbl, font_size=dp(13),
                       background_color=(0.42, 0.39, 1.0, 1))
            b.bind(on_release=lambda _, a=action: self._math_call(a))
            btn_row.add_widget(b)
        content.add_widget(btn_row)

        # ── Sezione: N-esimo ──
        content.add_widget(self._section("# N-esimo"))
        self._nth_spin = Spinner(
            text="π  N-esima cifra di π",
            values=["π  N-esima cifra di π", "e  N-esima cifra di e",
                    "p  N-esimo primo", "F  N-esimo Fibonacci",
                    "n!  N-esimo fattoriale", "2^N  potenza di 2"],
            size_hint_y=None, height=dp(40), font_size=dp(13),
            background_color=(0.14, 0.15, 0.22, 1),
            color=(0.88, 0.88, 0.94, 1),
        )
        content.add_widget(self._nth_spin)
        nth_row = BoxLayout(size_hint_y=None, height=dp(40), spacing=dp(8))
        nth_row.add_widget(Label(text="N =", size_hint_x=None, width=dp(36),
                                 font_size=dp(13), color=(0.6, 0.6, 0.7, 1)))
        self._nth_n = TextInput(text="100", multiline=False, font_size=dp(13),
                                background_color=(0.09, 0.10, 0.14, 1),
                                foreground_color=(0.88, 0.88, 0.94, 1))
        b_nth = Button(text="#ₙ Calcola", size_hint_x=None, width=dp(110),
                       font_size=dp(13), background_color=(0.42, 0.39, 1.0, 1))
        b_nth.bind(on_release=self._calc_nth)
        nth_row.add_widget(self._nth_n)
        nth_row.add_widget(b_nth)
        content.add_widget(nth_row)

        # ── Sezione: Simboli rapidi ──
        content.add_widget(self._section("🔣 Simboli"))
        sym_grid = GridLayout(cols=8, size_hint_y=None, spacing=dp(4))
        sym_grid.bind(minimum_height=sym_grid.setter("height"))
        for sym in SIMBOLI:
            b = Button(text=sym, font_size=dp(16), size_hint_y=None, height=dp(38),
                       background_color=(0.18, 0.19, 0.28, 1))
            b.bind(on_release=lambda _, s=sym: self._insert_sym(s))
            sym_grid.add_widget(b)
        content.add_widget(sym_grid)

        # ── Output ──
        out_row = BoxLayout(size_hint_y=None, height=dp(32), spacing=dp(8))
        self._status = Label(text="Pronto.", font_size=dp(12),
                             color=(0.5, 0.5, 0.65, 1), halign="left")
        b_copy = Button(text="📋 Copia", size_hint_x=None, width=dp(80),
                        font_size=dp(12), background_color=(0.22, 0.23, 0.34, 1))
        b_copy.bind(on_release=self._copy_out)
        b_clr = Button(text="🗑", size_hint_x=None, width=dp(40),
                       font_size=dp(14), background_color=(0.22, 0.23, 0.34, 1))
        b_clr.bind(on_release=lambda _: setattr(self._out, "text", ""))
        out_row.add_widget(self._status)
        out_row.add_widget(b_copy)
        out_row.add_widget(b_clr)
        content.add_widget(out_row)

        self._out = TextInput(
            readonly=True, font_size=dp(13), font_name="RobotoMono-Regular",
            size_hint_y=None, height=dp(160),
            background_color=(0.12, 0.13, 0.20, 1),
            foreground_color=(0.88, 0.88, 0.94, 1),
        )
        content.add_widget(self._out)

        scroll.add_widget(content)
        root.add_widget(scroll)
        self.add_widget(root)

    def _section(self, title: str) -> Label:
        lbl = Label(text=title, font_size=dp(12), bold=True,
                    size_hint_y=None, height=dp(28),
                    color=(0.5, 0.5, 0.65, 1), halign="left")
        return lbl

    def _insert_sym(self, sym: str):
        self._expr_input.text += sym
        self._expr_input.focus = True

    def _math_call(self, action: str):
        expr = self._expr_input.text.strip()
        if not expr:
            self._set_status("Inserisci un'espressione.")
            return
        try:
            prec = int(self._prec.text or "50")
        except ValueError:
            prec = 50
        self._set_status("🧮 Calcolo in corso...")
        self._out.text = ""
        App.get_running_app().ai.math(
            {"action": action, "expr": expr, "prec": prec},
            on_done=self._on_result,
            on_error=self._on_error,
        )

    _NTH_TYPES = {
        "π  N-esima cifra di π": "pi_digit",
        "e  N-esima cifra di e": "e_digit",
        "p  N-esimo primo": "prime",
        "F  N-esimo Fibonacci": "fib",
        "n!  N-esimo fattoriale": "fact",
        "2^N  potenza di 2": "pow2",
    }

    def _calc_nth(self, *_):
        try:
            n = int(self._nth_n.text or "1")
        except ValueError:
            self._set_status("N deve essere un numero intero.")
            return
        kind = self._NTH_TYPES.get(self._nth_spin.text, "pi_digit")
        self._set_status(f"#ₙ Calcolo N={n}...")
        self._out.text = ""
        App.get_running_app().ai.math(
            {"action": "nth", "type": kind, "n": n},
            on_done=self._on_result,
            on_error=self._on_error,
        )

    def _on_result(self, result: str):
        Clock.schedule_once(lambda _: self._show_result(result), 0)

    def _show_result(self, result: str):
        self._out.text = result
        self._set_status("✅ Fatto.")

    def _on_error(self, err: str):
        Clock.schedule_once(lambda _: self._show_err(err), 0)

    def _show_err(self, err: str):
        self._out.text = f"❌ {err}"
        self._set_status("Errore.")

    def _set_status(self, msg: str):
        self._status.text = msg

    def _copy_out(self, *_):
        try:
            from kivy.core.clipboard import Clipboard
            Clipboard.copy(self._out.text)
            self._set_status("📋 Copiato!")
        except Exception:
            self._set_status("Copia non disponibile.")
