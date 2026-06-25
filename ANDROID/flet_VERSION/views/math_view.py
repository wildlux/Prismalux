import flet as ft
from utils.ai_client import AiClient
import threading

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"


def MathView(page: ft.Page, ai: AiClient) -> ft.Column:
    result_text = ft.Text("", color=TXT, size=14, selectable=True)
    expr_field = ft.TextField(
        label="Espressione matematica",
        hint_text="Es: integrate(x**2, x) oppure solve(x**2-4, x)",
        hint_style=ft.TextStyle(color=DIM),
        label_style=ft.TextStyle(color=DIM),
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
    )
    status = ft.Text("", color=DIM, size=12, italic=True)

    def calc(_):
        expr = expr_field.value.strip()
        if not expr:
            return
        status.value = "Calcolo..."
        result_text.value = ""
        page.update()

        def done(r):
            result_text.value = r
            status.value = ""
            page.update()

        def err(e):
            result_text.value = f"Errore: {e}"
            result_text.color = "#FF6666"
            status.value = ""
            page.update()

        ai.math({"expr": expr}, done, err)

    calc_btn = ft.ElevatedButton("= Calcola", bgcolor=ACCENT, color=TXT, on_click=calc)

    chat_field = ft.TextField(
        label="Chiedi all'AI (matematica)",
        hint_text="Es: Dimostra il teorema di Pitagora",
        hint_style=ft.TextStyle(color=DIM),
        label_style=ft.TextStyle(color=DIM),
        multiline=True, min_lines=2, max_lines=4,
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
    )
    ai_result = ft.Text("", color=TXT, size=13, selectable=True)

    def ask_ai(_):
        q = chat_field.value.strip()
        if not q:
            return
        ai_result.value = "..."
        page.update()
        messages = [
            {"role": "system", "content": "Sei un esperto di matematica. Rispondi in modo chiaro e preciso."},
            {"role": "user", "content": q},
        ]
        ev = threading.Event()

        def done(r):
            ai_result.value = r
            page.update()
            ev.set()

        def err(e):
            ai_result.value = f"Errore: {e}"
            ai_result.color = "#FF6666"
            page.update()
            ev.set()

        ai.chat_once("", messages, done, err)

    ask_btn = ft.ElevatedButton("🧠 Chiedi AI", bgcolor="#2A2560", color=TXT, on_click=ask_ai)

    return ft.Column(
        [
            ft.Text("π  Matematica", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Text("Calcolo simbolico (SymPy via server)", color=DIM, size=12),
            expr_field,
            ft.Row([calc_btn, status]),
            ft.Container(
                content=result_text,
                bgcolor=BG, border_radius=8, padding=12,
                border=ft.border.all(1, "#2A2D40"), min_height=60,
            ),
            ft.Divider(color="#2A2D40"),
            ft.Text("🤖 Chiedi all'AI", color=DIM, size=13, weight=ft.FontWeight.W_600),
            chat_field,
            ask_btn,
            ft.Container(
                content=ai_result,
                bgcolor=BG, border_radius=8, padding=12,
                border=ft.border.all(1, "#2A2D40"), expand=True,
            ),
        ],
        expand=True, spacing=10, scroll=ft.ScrollMode.AUTO,
    )
