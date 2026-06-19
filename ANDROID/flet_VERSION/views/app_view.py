import flet as ft
from utils.ai_client import AiClient
import threading

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"

APPS = [
    ("🟠", "Blender", "blender"),
    ("⚙️", "FreeCAD", "freecad"),
    ("🃏", "Anki", "anki"),
    ("🔌", "KiCAD", "kicad"),
    ("🎥", "OBS", "obs"),
    ("🎮", "Godot", "godot"),
    ("💻", "OpenCode", "opencode"),
    ("🧮", "Calcolatrice", "calcolatrice"),
]


def AppView(page: ft.Page, ai: AiClient) -> ft.Column:

    def make_app_btn(icon, name, key):
        def open_dialog(e):
            dlg = ft.AlertDialog(
                title=ft.Text(f"{icon} {name}", color=TXT),
                content=ft.Text(
                    f"Per avviare {name}, usala dal PC desktop con Prismalux Qt.",
                    color=DIM,
                ),
                actions=[
                    ft.TextButton(
                        "OK",
                        on_click=lambda _: page.close(dlg),
                        style=ft.ButtonStyle(color=ACCENT),
                    )
                ],
                bgcolor="#252840",
                shape=ft.RoundedRectangleBorder(radius=12),
            )
            page.open(dlg)

        return ft.Container(
            content=ft.ElevatedButton(
                content=ft.Column(
                    [
                        ft.Text(icon, size=28),
                        ft.Text(name, size=13, color=TXT, text_align=ft.TextAlign.CENTER),
                    ],
                    horizontal_alignment=ft.CrossAxisAlignment.CENTER,
                    spacing=4,
                ),
                on_click=open_dialog,
                bgcolor="#252840",
                style=ft.ButtonStyle(
                    shape=ft.RoundedRectangleBorder(radius=14),
                    padding=ft.padding.symmetric(vertical=16, horizontal=8),
                ),
                height=90,
            ),
            expand=True,
        )

    rows = []
    for i in range(0, len(APPS), 2):
        pair = APPS[i: i + 2]
        row_controls = [make_app_btn(icon, name, key) for icon, name, key in pair]
        if len(row_controls) == 1:
            row_controls.append(ft.Container(expand=True))
        rows.append(ft.Row(row_controls, spacing=10, expand=True))

    grid_section = ft.Column(rows, spacing=10)

    # Dev Agent
    agent_prompt = ft.TextField(
        label="Prompt agente",
        hint_text="es. Crea uno script Python che ordina file per estensione",
        multiline=True,
        min_lines=3,
        max_lines=6,
        bgcolor=BG,
        color=TXT,
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
        expand=True,
    )
    agent_output = ft.TextField(
        label="Output agente",
        multiline=True,
        min_lines=4,
        read_only=True,
        bgcolor=BG,
        color=TXT,
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
        expand=True,
    )
    agent_progress = ft.ProgressRing(visible=False, color=ACCENT)

    def avvia_agent(e):
        prompt = agent_prompt.value.strip()
        if not prompt:
            page.open(ft.SnackBar(ft.Text("Inserisci un prompt per l'agente.")))
            page.update()
            return
        agent_progress.visible = True
        agent_output.value = ""
        page.update()

        def run():
            msgs = [{"role": "user", "content": prompt}]
            def done(resp):
                agent_output.value = resp
                agent_progress.visible = False
                page.update()
            def err(ex):
                agent_progress.visible = False
                page.open(ft.SnackBar(ft.Text(f"Errore: {ex}")))
                page.update()
            ai.chat_once("", msgs, done, err)

        threading.Thread(target=run, daemon=True).start()

    agent_section = ft.Column(
        [
            ft.Divider(color=DIM, height=24),
            ft.Text("🤖 Dev Agent", size=16, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Row([agent_prompt]),
            ft.ElevatedButton("Avvia", on_click=avvia_agent, bgcolor=ACCENT, color=TXT),
            agent_progress,
            ft.Row([agent_output]),
        ],
        spacing=10,
    )

    return ft.Column(
        [
            ft.Text("🕹 AppController", size=20, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Divider(color=ACCENT, height=1),
            ft.Column(
                [grid_section, agent_section],
                scroll=ft.ScrollMode.AUTO,
                expand=True,
                spacing=0,
            ),
        ],
        expand=True,
        spacing=8,
    )
