import flet as ft
from utils.ai_client import AiClient

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"
GREEN = "#44CC88"

_QUICK_CMDS = [
    ("status", "git status"),
    ("log", "git log --oneline -10"),
    ("diff", "git diff HEAD"),
    ("branch", "git branch -a"),
    ("pull", "git pull"),
]


def GitView(page: ft.Page, ai: AiClient) -> ft.Column:
    output = ft.Text("", color=TXT, size=12, selectable=True,
                     font_family="monospace")
    cmd_field = ft.TextField(
        label="Comando git",
        hint_text="Es: status / log --oneline -5 / diff HEAD",
        hint_style=ft.TextStyle(color=DIM),
        label_style=ft.TextStyle(color=DIM),
        text_color=TXT, border_color=ACCENT,
        focused_border_color=ACCENT, bgcolor=BG,
        prefix_text="git ", prefix_style=ft.TextStyle(color=ACCENT),
    )

    def run_cmd(cmd: str):
        output.value = f"$ git {cmd}\n..."
        output.color = TXT
        page.update()

        def done(out):
            output.value = f"$ git {cmd}\n{out or '(nessun output)'}"
            page.update()

        def err(e):
            output.value = f"Errore: {e}"
            output.color = "#FF6666"
            page.update()

        ai.git_cmd(cmd, done, err)

    def on_run(_):
        c = cmd_field.value.strip()
        if c:
            run_cmd(c)

    run_btn = ft.ElevatedButton("▶ Esegui", bgcolor=ACCENT, color=TXT, on_click=on_run)

    quick_row = ft.Row(
        [
            ft.TextButton(
                label,
                style=ft.ButtonStyle(
                    color=DIM,
                    bgcolor={"": "#1E2235"},
                    padding=ft.padding.symmetric(horizontal=10, vertical=6),
                    shape=ft.RoundedRectangleBorder(radius=6),
                ),
                on_click=lambda e, c=cmd.split(" ", 1)[1]: run_cmd(c),
            )
            for label, cmd in _QUICK_CMDS
        ],
        scroll=ft.ScrollMode.AUTO, spacing=6,
    )

    return ft.Column(
        [
            ft.Text("🔀 Git", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Text("Comandi rapidi:", color=DIM, size=12),
            quick_row,
            ft.Row([cmd_field, run_btn], alignment=ft.MainAxisAlignment.START),
            ft.Container(
                content=ft.Column(
                    [output],
                    scroll=ft.ScrollMode.AUTO,
                ),
                bgcolor=BG, border_radius=8, padding=12,
                border=ft.border.all(1, "#2A2D40"),
                expand=True, min_height=200,
            ),
        ],
        expand=True, spacing=10,
    )
