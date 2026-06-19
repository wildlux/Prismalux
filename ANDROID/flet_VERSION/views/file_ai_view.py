import flet as ft
import threading
import os

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"


def FileAiView(page: ft.Page, ai) -> ft.Column:
    model = getattr(ai, "default_model", "llama3")

    selected_path = ft.Ref[str]()
    selected_path.current = ""

    filename_text = ft.Text("Nessun file selezionato", color=DIM, size=13, italic=True)
    loading = ft.ProgressRing(visible=False, color=ACCENT, width=24, height=24)

    azione_dd = ft.Dropdown(
        label="Azione",
        color=TXT,
        bgcolor=BG,
        border_color=ACCENT,
        options=[
            ft.dropdown.Option("Riassumi"),
            ft.dropdown.Option("Traduci"),
            ft.dropdown.Option("Estrai dati"),
            ft.dropdown.Option("Domande & Risposte"),
        ],
        value="Riassumi",
        expand=True,
    )

    domanda_field = ft.TextField(
        label="Domanda",
        color=TXT,
        bgcolor=BG,
        border_color=ACCENT,
        expand=True,
        visible=False,
    )

    def on_azione_change(e):
        domanda_field.visible = azione_dd.value == "Domande & Risposte"
        page.update()

    azione_dd.on_change = on_azione_change

    result_area = ft.TextField(
        value="",
        color=TXT,
        bgcolor="#12152A",
        border_color=ACCENT,
        multiline=True,
        min_lines=10,
        read_only=True,
        expand=True,
    )

    def on_file_picked(e: ft.FilePickerResultEvent):
        if e.files:
            f = e.files[0]
            selected_path.current = f.path or f.name
            filename_text.value = f.name
            filename_text.color = TXT
            filename_text.italic = False
        else:
            filename_text.value = "Nessun file selezionato"
            filename_text.color = DIM
            filename_text.italic = True
        page.update()

    file_picker = ft.FilePicker(on_result=on_file_picked)
    page.overlay.append(file_picker)

    def scegli_file(e):
        file_picker.pick_files(
            allowed_extensions=["pdf", "txt", "csv", "docx"],
            allow_multiple=False,
        )

    def _read_file_content(path: str) -> str:
        try:
            _, ext = os.path.splitext(path.lower())
            if ext in (".txt", ".csv"):
                with open(path, "r", encoding="utf-8", errors="replace") as f:
                    return f.read()[:4000]
            return f"[File binario: {os.path.basename(path)} — contenuto non estraibile in locale]"
        except Exception as ex:
            return f"[Impossibile leggere il file: {ex}]"

    def analizza(e):
        path = selected_path.current
        if not path:
            page.snack_bar = ft.SnackBar(ft.Text("Seleziona un file prima"))
            page.snack_bar.open = True
            page.update()
            return
        azione = azione_dd.value
        domanda = domanda_field.value.strip()
        if azione == "Domande & Risposte" and not domanda:
            page.snack_bar = ft.SnackBar(ft.Text("Inserisci una domanda"))
            page.snack_bar.open = True
            page.update()
            return

        loading.visible = True
        result_area.value = ""
        page.update()

        def _run():
            contenuto = _read_file_content(path)
            nome = os.path.basename(path)
            if azione == "Riassumi":
                prompt = f"Riassumi il seguente documento '{nome}':\n\n{contenuto}"
            elif azione == "Traduci":
                prompt = f"Traduci in italiano il seguente documento '{nome}':\n\n{contenuto}"
            elif azione == "Estrai dati":
                prompt = f"Estrai tutti i dati strutturati (date, nomi, numeri, tabelle) dal documento '{nome}':\n\n{contenuto}"
            else:
                prompt = f"Rispondi alla domanda '{domanda}' basandoti sul documento '{nome}':\n\n{contenuto}"

            ai.chat_once(
                model,
                [{"role": "user", "content": prompt}],
                on_done=lambda r: _done(r),
                on_error=lambda err: _err(err),
            )

        def _done(r):
            loading.visible = False
            result_area.value = r
            page.update()

        def _err(err):
            loading.visible = False
            result_area.value = f"Errore: {err}"
            page.update()

        threading.Thread(target=_run, daemon=True).start()

    return ft.Column([
        ft.Text("File AI", size=20, weight=ft.FontWeight.BOLD, color=ACCENT),
        ft.Row([
            ft.ElevatedButton("📂 Seleziona file", on_click=scegli_file, bgcolor=ACCENT, color=TXT),
            filename_text,
        ], spacing=12),
        ft.Row([azione_dd], spacing=8),
        domanda_field,
        ft.ElevatedButton("🤖 Analizza", on_click=analizza, bgcolor=ACCENT, color=TXT),
        ft.Row([loading]),
        ft.Container(
            content=result_area,
            expand=True,
            border_radius=8,
        ),
    ], spacing=10, expand=True)
