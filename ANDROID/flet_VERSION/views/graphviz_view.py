import flet as ft
from utils.ai_client import AiClient
import base64

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"

_DEFAULT_DOT = """digraph G {
  bgcolor="#0F1117"
  node [style=filled fillcolor="#1A1D2E" color="#6B5FFF" fontcolor="#E0E0F0"]
  edge [color="#8888AA"]
  A -> B -> C
  A -> C
}"""


def GraphvizView(page: ft.Page, ai: AiClient) -> ft.Column:
    dot_field = ft.TextField(
        label="DOT language",
        value=_DEFAULT_DOT,
        multiline=True, min_lines=6, max_lines=12,
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        text_style=ft.TextStyle(font_family="monospace", size=12),
    )
    img_container = ft.Container(
        content=ft.Text("Premi Genera per visualizzare il grafo", color=DIM, size=13),
        bgcolor=BG, border_radius=8, padding=12,
        border=ft.border.all(1, "#2A2D40"),
        alignment=ft.alignment.center, min_height=200,
    )
    status = ft.Text("", color=DIM, size=12, italic=True)

    def generate(_):
        dot = dot_field.value.strip()
        if not dot:
            return
        status.value = "Generazione grafo..."
        page.update()

        def done(png_bytes: bytes):
            b64 = base64.b64encode(png_bytes).decode()
            img_container.content = ft.Image(
                src_base64=b64,
                fit=ft.ImageFit.CONTAIN,
                expand=True,
            )
            status.value = f"{len(png_bytes)//1024} KB"
            page.update()

        def err(e):
            img_container.content = ft.Text(f"Errore: {e}", color="#FF6666", size=13)
            status.value = ""
            page.update()

        ai.graphviz(dot, done, err)

    gen_btn = ft.ElevatedButton("🕸 Genera", bgcolor=ACCENT, color=TXT, on_click=generate)

    return ft.Column(
        [
            ft.Text("🕸 Graphviz", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            dot_field,
            ft.Row([gen_btn, status]),
            img_container,
        ],
        expand=True, spacing=10, scroll=ft.ScrollMode.AUTO,
    )
