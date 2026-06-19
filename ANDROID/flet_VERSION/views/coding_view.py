import flet as ft
from utils.ai_client import AiClient
import threading

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"

LANGUAGES = ["Python", "C++", "JavaScript", "Bash", "SQL", "Rust"]


def CodingView(page: ft.Page, ai: AiClient) -> ft.Column:
    lang_dropdown = ft.Dropdown(
        label="Linguaggio",
        options=[ft.dropdown.Option(l) for l in LANGUAGES],
        value="Python",
        bgcolor=BG,
        color=TXT,
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
        width=180,
    )

    code_input = ft.TextField(
        label="Codice",
        multiline=True,
        min_lines=12,
        max_lines=12,
        bgcolor="#0D1020",
        color="#A8FF78",
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
        text_style=ft.TextStyle(font_family="monospace"),
        expand=True,
    )

    output_field = ft.TextField(
        label="Output AI",
        multiline=True,
        min_lines=8,
        read_only=True,
        bgcolor=BG,
        color=TXT,
        border_color=DIM,
        label_style=ft.TextStyle(color=DIM),
        text_style=ft.TextStyle(font_family="monospace"),
        expand=True,
    )
    progress = ft.ProgressRing(visible=False, color=ACCENT)

    def call_ai(action_label: str, system_prompt: str):
        code = code_input.value.strip()
        if not code:
            page.open(ft.SnackBar(ft.Text("Incolla del codice prima.")))
            page.update()
            return
        progress.visible = True
        output_field.value = ""
        page.update()

        def run():
            lang = lang_dropdown.value
            msgs = [{"role": "user", "content": f"{system_prompt}\n\nLinguaggio: {lang}\n\n```{lang.lower()}\n{code}\n```"}]
            def done(resp):
                output_field.value = resp
                progress.visible = False
                page.update()
            def err(ex):
                progress.visible = False
                page.open(ft.SnackBar(ft.Text(f"Errore: {ex}")))
                page.update()
            ai.chat_once("", msgs, done, err)

        threading.Thread(target=run, daemon=True).start()

    def spiega(e):
        call_ai("Spiega", "Spiega in dettaglio cosa fa questo codice, riga per riga se necessario:")

    def correggi(e):
        call_ai("Correggi", "Analizza questo codice, trova bug e problemi, e fornisci una versione corretta:")

    def ottimizza(e):
        call_ai("Ottimizza", "Ottimizza questo codice per performance, leggibilità e best practice. Mostra il codice migliorato e spiega le modifiche:")

    def copia_output(e):
        if output_field.value:
            page.set_clipboard(output_field.value)
            page.open(ft.SnackBar(ft.Text("Output copiato negli appunti.")))
            page.update()

    action_row = ft.Row(
        [
            ft.ElevatedButton("💡 Spiega", on_click=spiega, bgcolor="#2A3A6A", color=TXT),
            ft.ElevatedButton("🔧 Correggi", on_click=correggi, bgcolor="#3A2A2A", color=TXT),
            ft.ElevatedButton("✨ Ottimizza", on_click=ottimizza, bgcolor="#1A3A2A", color=TXT),
        ],
        spacing=8,
        wrap=True,
    )

    copy_btn = ft.IconButton(
        icon=ft.icons.CONTENT_COPY,
        tooltip="Copia output",
        icon_color=DIM,
        on_click=copia_output,
    )

    return ft.Column(
        [
            ft.Text("💻 Programmazione", size=20, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Divider(color=ACCENT, height=1),
            ft.Column(
                [
                    lang_dropdown,
                    ft.Row([code_input]),
                    action_row,
                    progress,
                    ft.Divider(color=DIM, height=16),
                    ft.Row(
                        [
                            ft.Text("Output", color=DIM, size=13, expand=True),
                            copy_btn,
                        ],
                        vertical_alignment=ft.CrossAxisAlignment.CENTER,
                    ),
                    ft.Row([output_field]),
                ],
                scroll=ft.ScrollMode.AUTO,
                expand=True,
                spacing=10,
            ),
        ],
        expand=True,
        spacing=8,
    )
