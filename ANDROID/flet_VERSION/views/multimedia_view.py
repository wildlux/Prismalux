import flet as ft
from utils.ai_client import AiClient
import threading
import requests

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"


def MultimediaView(page: ft.Page, ai: AiClient) -> ft.Column:
    # --- Genera Immagine ---
    img_prompt = ft.TextField(
        label="Descrivi l'immagine",
        hint_text="es. tramonto sul mare con colori vividi",
        multiline=True,
        min_lines=2,
        max_lines=4,
        bgcolor=BG,
        color=TXT,
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
        expand=True,
    )
    img_result = ft.Card(
        content=ft.Container(
            content=ft.Text("Il risultato apparirà qui.", color=DIM, selectable=True),
            padding=12,
        ),
        color="#252840",
        visible=False,
    )
    img_progress = ft.ProgressRing(visible=False, color=ACCENT)

    def genera_immagine(e):
        if not img_prompt.value.strip():
            page.open(ft.SnackBar(ft.Text("Inserisci un prompt.")))
            page.update()
            return
        img_progress.visible = True
        img_result.visible = False
        page.update()

        def run():
            msgs = [{"role": "user", "content": f"Descrivi come sarebbe un'immagine di: {img_prompt.value.strip()}"}]
            def done(resp):
                img_result.content.content.value = resp
                img_result.visible = True
                img_progress.visible = False
                page.update()
            def err(e):
                img_progress.visible = False
                page.open(ft.SnackBar(ft.Text(f"Errore: {e}")))
                page.update()
            ai.chat_once("", msgs, done, err)

        threading.Thread(target=run, daemon=True).start()

    tab_image = ft.Column(
        [
            ft.Row([img_prompt]),
            ft.ElevatedButton(
                "🎨 Genera",
                on_click=genera_immagine,
                bgcolor=ACCENT,
                color=TXT,
            ),
            img_progress,
            img_result,
        ],
        spacing=12,
        scroll=ft.ScrollMode.AUTO,
    )

    # --- Audio AI ---
    tts_text = ft.TextField(
        label="Testo da sintetizzare",
        multiline=True,
        min_lines=3,
        max_lines=6,
        bgcolor=BG,
        color=TXT,
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
        expand=True,
    )
    tts_status = ft.Text("", color=TXT)
    tts_progress = ft.ProgressRing(visible=False, color=ACCENT)

    def sintetizza(e):
        testo = tts_text.value.strip()
        if not testo:
            page.open(ft.SnackBar(ft.Text("Inserisci del testo.")))
            page.update()
            return
        tts_progress.visible = True
        tts_status.value = "Invio al server TTS..."
        page.update()

        def run():
            try:
                headers = {}
                if ai.token:
                    headers["Authorization"] = f"Bearer {ai.token}"
                r = requests.post(
                    f"{ai.base_url}/api/tts",
                    json={"text": testo},
                    headers=headers,
                    timeout=30,
                )
                if r.status_code == 200:
                    tts_status.value = "Sintesi completata. Audio riprodotto sul server."
                else:
                    tts_status.value = f"Errore server: {r.status_code}"
            except Exception as ex:
                tts_status.value = f"Connessione fallita: {ex}"
            finally:
                tts_progress.visible = False
                page.update()

        threading.Thread(target=run, daemon=True).start()

    tab_audio = ft.Column(
        [
            ft.Row([tts_text]),
            ft.ElevatedButton(
                "🔊 Sintetizza",
                on_click=sintetizza,
                bgcolor=ACCENT,
                color=TXT,
            ),
            tts_progress,
            tts_status,
        ],
        spacing=12,
        scroll=ft.ScrollMode.AUTO,
    )

    # --- OCR ---
    ocr_filename = ft.Text("Nessun file selezionato", color=DIM)
    ocr_result = ft.TextField(
        label="Testo estratto",
        multiline=True,
        min_lines=5,
        read_only=True,
        bgcolor=BG,
        color=TXT,
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
        expand=True,
    )
    ocr_progress = ft.ProgressRing(visible=False, color=ACCENT)
    selected_file = {"name": ""}

    def on_file_picked(e: ft.FilePickerResultEvent):
        if e.files:
            selected_file["name"] = e.files[0].name
            ocr_filename.value = e.files[0].name
            page.update()

    file_picker = ft.FilePicker(on_result=on_file_picked)
    page.overlay.append(file_picker)

    def estrai_testo(e):
        if not selected_file["name"]:
            page.open(ft.SnackBar(ft.Text("Seleziona prima un'immagine.")))
            page.update()
            return
        ocr_progress.visible = True
        ocr_result.value = ""
        page.update()

        def run():
            msgs = [{"role": "user", "content": f"Descrivi il contenuto testuale di questa immagine: {selected_file['name']}"}]
            def done(resp):
                ocr_result.value = resp
                ocr_progress.visible = False
                page.update()
            def err(ex):
                ocr_progress.visible = False
                page.open(ft.SnackBar(ft.Text(f"Errore: {ex}")))
                page.update()
            ai.chat_once("", msgs, done, err)

        threading.Thread(target=run, daemon=True).start()

    tab_ocr = ft.Column(
        [
            ft.ElevatedButton(
                "📁 Seleziona immagine",
                on_click=lambda _: file_picker.pick_files(
                    allowed_extensions=["png", "jpg", "jpeg", "webp", "bmp"]
                ),
                bgcolor="#333355",
                color=TXT,
            ),
            ocr_filename,
            ft.ElevatedButton(
                "🔍 Estrai testo",
                on_click=estrai_testo,
                bgcolor=ACCENT,
                color=TXT,
            ),
            ocr_progress,
            ft.Row([ocr_result]),
        ],
        spacing=12,
        scroll=ft.ScrollMode.AUTO,
    )

    tabs = ft.Tabs(
        selected_index=0,
        indicator_color=ACCENT,
        label_color=TXT,
        unselected_label_color=DIM,
        tabs=[
            ft.Tab(text="🎨 Genera Immagine", content=ft.Container(tab_image, padding=ft.padding.only(top=16))),
            ft.Tab(text="🎵 Audio AI", content=ft.Container(tab_audio, padding=ft.padding.only(top=16))),
            ft.Tab(text="📷 OCR", content=ft.Container(tab_ocr, padding=ft.padding.only(top=16))),
        ],
        expand=True,
    )

    return ft.Column(
        [
            ft.Text("🎬 Multimedia", size=20, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Divider(color=ACCENT, height=1),
            tabs,
        ],
        expand=True,
        spacing=8,
    )
