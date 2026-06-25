import flet as ft
from utils.ai_client import AiClient
from utils.model_caps import DEFAULT_EMBED_MODEL
import json
import os
from pathlib import Path
import threading

_SETTINGS_PATH = str(Path.home() / ".prismalux" / "flet_settings.json")

def _current_embed_model() -> str:
    try:
        with open(_SETTINGS_PATH) as f:
            return json.load(f).get("embed_model", DEFAULT_EMBED_MODEL)
    except Exception:
        return DEFAULT_EMBED_MODEL

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"

_RAG_SYSTEM = (
    "Sei un assistente RAG. L'utente fa una ricerca nella knowledge base. "
    "Rispondi con i concetti rilevanti trovati, citando le fonti se presenti. "
    "Sii conciso e strutturato."
)


def RagView(page: ft.Page, ai: AiClient) -> ft.Column:
    results_col = ft.Column(spacing=8, scroll=ft.ScrollMode.AUTO, expand=True)
    searching = [False]

    embed_label = ft.Text(
        f"Embedding: {_current_embed_model()}",
        color=DIM, size=12, italic=True,
    )

    embed_chip = ft.Container(
        content=ft.Row([
            ft.Icon(ft.icons.ACCOUNT_TREE, color=ACCENT, size=14),
            embed_label,
        ], spacing=6),
        bgcolor="#161929",
        border_radius=6,
        padding=ft.padding.symmetric(horizontal=10, vertical=4),
        border=ft.border.all(1, "#2A2D40"),
    )

    search_field = ft.TextField(
        label="Ricerca nella knowledge base",
        label_style=ft.TextStyle(color=DIM),
        hint_text="Es: algoritmi di clustering, BLHM, TFR...",
        hint_style=ft.TextStyle(color=DIM),
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        expand=True,
        on_submit=lambda _: do_search(None),
    )

    file_picker = ft.FilePicker()
    page.overlay.append(file_picker)

    selected_file_text = ft.Text("", color=DIM, size=12)

    def on_file_picked(e: ft.FilePickerResultEvent):
        if e.files:
            fname = e.files[0].name
            selected_file_text.value = f"Selezionato: {fname}"
            page.update()

            def send_filename():
                ev = threading.Event()
                err = [None]

                def on_done(_):
                    ev.set()

                def on_error(e_):
                    err[0] = e_
                    ev.set()

                ai.chat_once(
                    "",
                    [{"role": "user", "content": f"__RAG_ADD_FILE__:{fname}"}],
                    on_done,
                    on_error,
                )
                ev.wait(timeout=30)
                msg = f"File '{fname}' inviato al server RAG." if not err[0] else f"Errore invio: {err[0]}"
                color = TXT if not err[0] else "#FF6666"
                results_col.controls.insert(
                    0,
                    ft.Card(
                        content=ft.Container(
                            content=ft.Text(msg, color=color, size=13),
                            padding=10,
                            bgcolor=BG,
                        ),
                        elevation=2,
                    ),
                )
                page.update()

            threading.Thread(target=send_filename, daemon=True).start()

    file_picker.on_result = on_file_picked

    def do_search(_):
        if searching[0]:
            return
        query = search_field.value.strip()
        if not query:
            page.snack_bar = ft.SnackBar(ft.Text("Inserisci una query", color=TXT), bgcolor="#AA3333")
            page.snack_bar.open = True
            page.update()
            return

        searching[0] = True
        results_col.controls.clear()
        results_col.controls.append(ft.Text("Ricerca in corso...", color=DIM, italic=True))
        page.update()

        def _search():
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
                    {"role": "system", "content": _RAG_SYSTEM},
                    {"role": "user", "content": query},
                ],
                on_done,
                on_error,
            )
            ev.wait(timeout=60)
            results_col.controls.clear()

            if err[0]:
                page.snack_bar = ft.SnackBar(ft.Text(f"Errore: {err[0]}", color=TXT), bgcolor="#AA3333")
                page.snack_bar.open = True
            else:
                answer = res[0] or "(nessun risultato)"
                chunks = [c.strip() for c in answer.split("\n\n") if c.strip()]
                if not chunks:
                    chunks = [answer]
                for i, chunk in enumerate(chunks[:6]):
                    results_col.controls.append(
                        ft.Card(
                            content=ft.Container(
                                content=ft.Column(
                                    [
                                        ft.Text(f"Risultato {i+1}", color=ACCENT, size=12, weight=ft.FontWeight.BOLD),
                                        ft.Text(chunk, color=TXT, size=14, selectable=True),
                                    ],
                                    spacing=4,
                                ),
                                padding=12,
                                bgcolor=BG,
                            ),
                            elevation=2,
                        )
                    )

            searching[0] = False
            page.update()

        threading.Thread(target=_search, daemon=True).start()

    search_btn = ft.ElevatedButton(
        "🔍 Cerca",
        bgcolor=ACCENT,
        color=TXT,
        on_click=do_search,
    )

    add_doc_btn = ft.OutlinedButton(
        "📎 Aggiungi documento",
        style=ft.ButtonStyle(color=ACCENT, side=ft.BorderSide(1, ACCENT)),
        on_click=lambda _: file_picker.pick_files(
            dialog_title="Seleziona documento RAG",
            allowed_extensions=["pdf", "txt", "md", "docx", "csv"],
        ),
    )

    col = ft.Column(
        [
            ft.Text("📚 RAG — Ricerca Documenti", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            embed_chip,
            ft.Container(
                content=ft.Column([
                    ft.Text("Ricerca", color=DIM, size=12, weight=ft.FontWeight.BOLD),
                    ft.Row([search_field, search_btn], alignment=ft.MainAxisAlignment.START, spacing=8),
                    ft.Divider(height=8, color="#2A2D40"),
                    ft.Text("Documenti", color=DIM, size=12, weight=ft.FontWeight.BOLD),
                    ft.Row([add_doc_btn, selected_file_text], spacing=10, alignment=ft.MainAxisAlignment.START),
                ], spacing=8),
                bgcolor=BG,
                border_radius=8,
                padding=ft.padding.symmetric(horizontal=12, vertical=10),
                border=ft.border.all(1, "#2A2D40"),
            ),
            ft.Container(
                content=results_col,
                bgcolor=BG,
                border_radius=8,
                padding=8,
                expand=True,
                border=ft.border.all(1, "#2A2D40"),
            ),
        ],
        expand=True,
        spacing=10,
    )
    return col
