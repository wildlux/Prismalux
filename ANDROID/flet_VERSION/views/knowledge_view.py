import flet as ft
from utils.ai_client import AiClient
import threading
import os

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"

KNOWLEDGE_PATH = "/tmp/prismalux_knowledge_mobile.md"

_EXTRACT_SYSTEM = (
    "Sei un estrattore di conoscenza. Dato un testo, estrai i fatti chiave, "
    "concetti importanti e informazioni strutturate. Formatta in Markdown con bullet points. "
    "Sii conciso ma esauriente."
)


def KnowledgeView(page: ft.Page, ai: AiClient) -> ft.Column:
    extracting = [False]

    note_field = ft.TextField(
        label="Note personali / Knowledge base",
        label_style=ft.TextStyle(color=DIM),
        hint_text="Scrivi appunti, conoscenza, note di progetto...",
        hint_style=ft.TextStyle(color=DIM),
        multiline=True,
        min_lines=10,
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        expand=True,
    )

    extraction_text = ft.Text("", color=TXT, size=13, selectable=True)
    extraction_card = ft.Card(
        content=ft.Container(
            content=ft.Column(
                [
                    ft.Row(
                        [
                            ft.Text("Estrazione AI", color=ACCENT, size=13, weight=ft.FontWeight.BOLD),
                        ]
                    ),
                    ft.Divider(color="#2A2D40", height=8),
                    extraction_text,
                ],
                spacing=4,
            ),
            padding=12,
            bgcolor=BG,
        ),
        elevation=3,
        visible=False,
    )

    status_text = ft.Text("", color=DIM, size=12, italic=True)

    def save_note(_):
        content = note_field.value or ""
        try:
            with open(KNOWLEDGE_PATH, "w", encoding="utf-8") as f:
                f.write(content)
            status_text.value = f"Salvato in {KNOWLEDGE_PATH}"
            status_text.color = "#44CC88"
        except Exception as exc:
            status_text.value = f"Errore salvataggio: {exc}"
            status_text.color = "#FF6666"
            page.snack_bar = ft.SnackBar(ft.Text(str(exc), color=TXT), bgcolor="#AA3333")
            page.snack_bar.open = True
        page.update()

    def load_note(_):
        try:
            if os.path.exists(KNOWLEDGE_PATH):
                with open(KNOWLEDGE_PATH, "r", encoding="utf-8") as f:
                    note_field.value = f.read()
                status_text.value = "Caricato."
                status_text.color = "#44CC88"
            else:
                status_text.value = "Nessun file salvato."
                status_text.color = DIM
        except Exception as exc:
            status_text.value = f"Errore lettura: {exc}"
            status_text.color = "#FF6666"
            page.snack_bar = ft.SnackBar(ft.Text(str(exc), color=TXT), bgcolor="#AA3333")
            page.snack_bar.open = True
        page.update()

    def extract_with_ai(_):
        if extracting[0]:
            return
        text = note_field.value.strip()
        if not text:
            page.snack_bar = ft.SnackBar(ft.Text("Nessun testo da estrarre", color=TXT), bgcolor="#AA3333")
            page.snack_bar.open = True
            page.update()
            return

        extracting[0] = True
        status_text.value = "Estrazione in corso..."
        status_text.color = ACCENT
        extraction_card.visible = True
        extraction_text.value = "..."
        page.update()

        def _extract():
            ev = threading.Event()
            res = [None]
            err = [None]

            def on_done(r):
                res[0] = r
                ev.set()

            def on_error(e):
                err[0] = e
                ev.set()

            ai.chat_once(
                "",
                [
                    {"role": "system", "content": _EXTRACT_SYSTEM},
                    {"role": "user", "content": text[:4000]},
                ],
                on_done,
                on_error,
            )
            ev.wait(timeout=90)

            if err[0]:
                extraction_text.value = f"Errore: {err[0]}"
                extraction_text.color = "#FF6666"
                status_text.value = "Estrazione fallita."
                status_text.color = "#FF6666"
                page.snack_bar = ft.SnackBar(ft.Text(str(err[0]), color=TXT), bgcolor="#AA3333")
                page.snack_bar.open = True
            else:
                extraction_text.value = res[0] or "(nessun risultato)"
                extraction_text.color = TXT
                status_text.value = "Estrazione completata."
                status_text.color = "#44CC88"

            extracting[0] = False
            page.update()

        threading.Thread(target=_extract, daemon=True).start()

    save_btn = ft.ElevatedButton(
        "💾 Salva",
        bgcolor=ACCENT,
        color=TXT,
        on_click=save_note,
    )
    load_btn = ft.OutlinedButton(
        "📖 Carica",
        style=ft.ButtonStyle(color=ACCENT, side=ft.BorderSide(1, ACCENT)),
        on_click=load_note,
    )
    extract_btn = ft.OutlinedButton(
        "🤖 Estrai con AI",
        style=ft.ButtonStyle(color=DIM, side=ft.BorderSide(1, DIM)),
        on_click=extract_with_ai,
    )

    load_note(None)

    col = ft.Column(
        [
            ft.Text("🧠 Knowledge Base", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            note_field,
            ft.Row(
                [save_btn, load_btn, extract_btn, status_text],
                alignment=ft.MainAxisAlignment.START,
                spacing=8,
                wrap=True,
            ),
            extraction_card,
        ],
        expand=True,
        spacing=10,
    )
    return col
