import flet as ft
import threading
import re

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"
GREEN = "#4CAF50"
RED = "#F44336"


def ImparaView(page: ft.Page, ai) -> ft.Column:
    model = getattr(ai, "default_model", "llama3")

    state = {
        "domanda": "",
        "opzioni": {"A": "", "B": "", "C": "", "D": ""},
        "corretta": "",
        "risposto": False,
        "ok": 0,
        "ko": 0,
    }

    categoria_dd = ft.Dropdown(
        label="Categoria",
        color=TXT,
        bgcolor=BG,
        border_color=ACCENT,
        options=[
            ft.dropdown.Option("Python"),
            ft.dropdown.Option("Qt/C++"),
            ft.dropdown.Option("Linux"),
            ft.dropdown.Option("Reti/CCNA"),
            ft.dropdown.Option("Matematica"),
            ft.dropdown.Option("Generale"),
        ],
        value="Python",
        expand=True,
    )

    difficolta_dd = ft.Dropdown(
        label="Difficolta",
        color=TXT,
        bgcolor=BG,
        border_color=ACCENT,
        options=[
            ft.dropdown.Option("Base"),
            ft.dropdown.Option("Intermedio"),
            ft.dropdown.Option("Avanzato"),
        ],
        value="Base",
        expand=True,
    )

    domanda_text = ft.Text("", size=15, color=TXT, weight=ft.FontWeight.W_500, selectable=True)
    feedback_text = ft.Text("", size=13, weight=ft.FontWeight.BOLD)
    spiegazione_text = ft.Text("", size=13, color=TXT, selectable=True, italic=True)
    loading = ft.ProgressRing(visible=False, color=ACCENT, width=24, height=24)
    score_text = ft.Text("✅ 0 / ❌ 0", size=14, color=DIM, weight=ft.FontWeight.BOLD)

    btn_a = ft.ElevatedButton("A) —", expand=True, bgcolor="#1E2140", color=TXT)
    btn_b = ft.ElevatedButton("B) —", expand=True, bgcolor="#1E2140", color=TXT)
    btn_c = ft.ElevatedButton("C) —", expand=True, bgcolor="#1E2140", color=TXT)
    btn_d = ft.ElevatedButton("D) —", expand=True, bgcolor="#1E2140", color=TXT)
    btns = {"A": btn_a, "B": btn_b, "C": btn_c, "D": btn_d}

    def _reset_btns():
        for b in btns.values():
            b.bgcolor = "#1E2140"
            b.color = TXT
        feedback_text.value = ""
        spiegazione_text.value = ""

    def _update_score():
        score_text.value = f"✅ {state['ok']} / ❌ {state['ko']}"

    def risposta(lettera, e=None):
        if state["risposto"]:
            return
        state["risposto"] = True
        corretta = state["corretta"].upper()
        if lettera == corretta:
            state["ok"] += 1
            btns[lettera].bgcolor = GREEN
            feedback_text.value = "Corretto!"
            feedback_text.color = GREEN
        else:
            state["ko"] += 1
            btns[lettera].bgcolor = RED
            btns[corretta].bgcolor = GREEN
            feedback_text.value = f"Sbagliato. La risposta corretta era {corretta})"
            feedback_text.color = RED
        _update_score()
        spiegazione_text.value = "Premi 'Nuova Domanda' per continuare."
        page.update()

    btn_a.on_click = lambda e: risposta("A")
    btn_b.on_click = lambda e: risposta("B")
    btn_c.on_click = lambda e: risposta("C")
    btn_d.on_click = lambda e: risposta("D")

    def nuova_domanda(e):
        loading.visible = True
        domanda_text.value = ""
        spiegazione_text.value = ""
        feedback_text.value = ""
        _reset_btns()
        state["risposto"] = False
        for k in btns:
            btns[k].text = f"{k}) —"
        page.update()

        categoria = categoria_dd.value
        difficolta = difficolta_dd.value

        def _run():
            prompt = (
                f"Genera una domanda a scelta multipla su {categoria} livello {difficolta}.\n"
                f"Usa ESATTAMENTE questo formato:\n"
                f"DOMANDA:\n<testo domanda>\n"
                f"A)\n<opzione A>\n"
                f"B)\n<opzione B>\n"
                f"C)\n<opzione C>\n"
                f"D)\n<opzione D>\n"
                f"RISPOSTA_CORRETTA:\n<solo la lettera A, B, C o D>"
            )
            ai.chat_once(
                model,
                [{"role": "user", "content": prompt}],
                on_done=lambda r: _parse(r),
                on_error=lambda err: _err(err),
            )

        def _parse(r):
            loading.visible = False
            try:
                domanda_m = re.search(r'DOMANDA:\s*(.+?)(?=\nA\))', r, re.DOTALL)
                a_m = re.search(r'A\)\s*(.+?)(?=\nB\))', r, re.DOTALL)
                b_m = re.search(r'B\)\s*(.+?)(?=\nC\))', r, re.DOTALL)
                c_m = re.search(r'C\)\s*(.+?)(?=\nD\))', r, re.DOTALL)
                d_m = re.search(r'D\)\s*(.+?)(?=\nRISPOSTA)', r, re.DOTALL)
                risp_m = re.search(r'RISPOSTA_CORRETTA:\s*([ABCD])', r)

                state["domanda"] = domanda_m.group(1).strip() if domanda_m else r
                state["opzioni"]["A"] = a_m.group(1).strip() if a_m else "—"
                state["opzioni"]["B"] = b_m.group(1).strip() if b_m else "—"
                state["opzioni"]["C"] = c_m.group(1).strip() if c_m else "—"
                state["opzioni"]["D"] = d_m.group(1).strip() if d_m else "—"
                state["corretta"] = risp_m.group(1).strip() if risp_m else "A"

                domanda_text.value = state["domanda"]
                for k in ("A", "B", "C", "D"):
                    btns[k].text = f"{k}) {state['opzioni'][k]}"
            except Exception as ex:
                domanda_text.value = f"Errore parsing: {ex}\n\n{r}"
            page.update()

        def _err(err):
            loading.visible = False
            domanda_text.value = f"Errore: {err}"
            page.update()

        threading.Thread(target=_run, daemon=True).start()

    domanda_card = ft.Container(
        content=ft.Column([
            domanda_text,
            ft.Row([btn_a, btn_b], spacing=8),
            ft.Row([btn_c, btn_d], spacing=8),
            feedback_text,
            spiegazione_text,
        ], spacing=10),
        padding=14,
        border_radius=10,
        bgcolor="#12152A",
        border=ft.border.all(1, ACCENT),
    )

    return ft.Column([
        ft.Row([
            ft.Text("Impara", size=20, weight=ft.FontWeight.BOLD, color=ACCENT),
            ft.Container(expand=True),
            score_text,
        ]),
        ft.Row([categoria_dd, difficolta_dd], spacing=8),
        ft.Row([
            ft.ElevatedButton("🎲 Nuova Domanda", on_click=nuova_domanda, bgcolor=ACCENT, color=TXT),
            loading,
        ], spacing=12),
        domanda_card,
    ], spacing=10, scroll=ft.ScrollMode.AUTO, expand=True)
