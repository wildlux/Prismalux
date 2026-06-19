import flet as ft
from utils.ai_client import AiClient
import threading
import requests

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"


def VoceView(page: ft.Page, ai: AiClient) -> ft.Column:
    state = {"recording": False}

    # TTS
    tts_text = ft.TextField(
        label="Testo da leggere",
        multiline=True,
        min_lines=5,
        max_lines=10,
        bgcolor=BG,
        color=TXT,
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
        expand=True,
    )

    speed_slider = ft.Slider(
        min=0.5,
        max=2.0,
        value=1.0,
        divisions=6,
        label="{value}x",
        active_color=ACCENT,
        thumb_color=ACCENT,
    )
    speed_label = ft.Text("Velocità: 1.0x", color=DIM, size=13)

    def on_speed_change(e):
        speed_label.value = f"Velocità: {speed_slider.value:.1f}x"
        page.update()

    speed_slider.on_change = on_speed_change

    voice_dropdown = ft.Dropdown(
        label="Voce",
        options=[
            ft.dropdown.Option("it-IT", "Italiano (it-IT)"),
            ft.dropdown.Option("en-US", "Inglese (en-US)"),
            ft.dropdown.Option("fr-FR", "Francese (fr-FR)"),
        ],
        value="it-IT",
        bgcolor=BG,
        color=TXT,
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
    )

    tts_status = ft.Text("", color=TXT)
    tts_progress = ft.ProgressRing(visible=False, color=ACCENT)

    def parla(e):
        testo = tts_text.value.strip()
        if not testo:
            page.open(ft.SnackBar(ft.Text("Inserisci del testo prima di sintetizzare.")))
            page.update()
            return
        tts_progress.visible = True
        tts_status.value = "Invio richiesta TTS..."
        page.update()

        def run():
            try:
                headers = {}
                if ai.token:
                    headers["Authorization"] = f"Bearer {ai.token}"
                r = requests.post(
                    f"{ai.base_url}/api/tts",
                    json={
                        "text": testo,
                        "voice": voice_dropdown.value,
                        "speed": speed_slider.value,
                    },
                    headers=headers,
                    timeout=30,
                )
                if r.status_code == 200:
                    tts_status.value = "Audio sintetizzato e riprodotto sul server."
                else:
                    tts_status.value = f"Errore server: HTTP {r.status_code}"
            except Exception as ex:
                tts_status.value = f"Connessione fallita: {ex}"
            finally:
                tts_progress.visible = False
                page.update()

        threading.Thread(target=run, daemon=True).start()

    tts_section = ft.Column(
        [
            ft.Text("🔊 Sintesi Vocale (TTS)", size=16, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Row(
                [voice_dropdown, ft.Column([speed_label, speed_slider], expand=True)],
                spacing=12,
                vertical_alignment=ft.CrossAxisAlignment.CENTER,
            ),
            ft.Row([tts_text]),
            ft.ElevatedButton("🔊 Parla", on_click=parla, bgcolor=ACCENT, color=TXT),
            tts_progress,
            tts_status,
        ],
        spacing=10,
    )

    # STT
    stt_btn = ft.ElevatedButton(
        "🎤 Registra",
        bgcolor="#2E3A5F",
        color=TXT,
        height=64,
        style=ft.ButtonStyle(
            shape=ft.RoundedRectangleBorder(radius=12),
        ),
    )
    stop_btn = ft.ElevatedButton(
        "⏹ Stop + Trascrivi",
        bgcolor="#5F2E2E",
        color=TXT,
        visible=False,
    )
    stt_status = ft.Text("Premi Registra per iniziare.", color=DIM)
    stt_result = ft.TextField(
        label="Trascrizione",
        multiline=True,
        min_lines=3,
        read_only=True,
        bgcolor=BG,
        color=TXT,
        border_color=ACCENT,
        label_style=ft.TextStyle(color=DIM),
        expand=True,
    )
    stt_progress = ft.ProgressRing(visible=False, color=ACCENT)

    def toggle_record(e):
        if not state["recording"]:
            state["recording"] = True
            stt_btn.bgcolor = "#3A1E1E"
            stt_btn.text = "🔴 Registrazione in corso..."
            stop_btn.visible = True
            stt_status.value = "Registrazione attiva..."
        else:
            state["recording"] = False
            stt_btn.bgcolor = "#2E3A5F"
            stt_btn.text = "🎤 Registra"
            stop_btn.visible = False
            stt_status.value = "Premi Registra per iniziare."
        page.update()

    def stop_trascrivi(e):
        state["recording"] = False
        stt_btn.bgcolor = "#2E3A5F"
        stt_btn.text = "🎤 Registra"
        stop_btn.visible = False
        stt_status.value = "Invio audio al server..."
        stt_progress.visible = True
        page.update()

        def run():
            try:
                headers = {}
                if ai.token:
                    headers["Authorization"] = f"Bearer {ai.token}"
                r = requests.post(
                    f"{ai.base_url}/api/whisper",
                    json={"audio": "__dummy__"},
                    headers=headers,
                    timeout=30,
                )
                if r.status_code == 200:
                    data = r.json()
                    stt_result.value = data.get("text", "(nessun testo)")
                    stt_status.value = "Trascrizione completata."
                else:
                    stt_status.value = "Nota: STT reale disponibile sul server Qt desktop."
                    stt_result.value = "(Funzione disponibile sul server Qt)"
            except Exception as ex:
                stt_status.value = f"Connessione fallita: {ex}"
                stt_result.value = "(Funzione disponibile sul server Qt)"
            finally:
                stt_progress.visible = False
                page.update()

        threading.Thread(target=run, daemon=True).start()

    stt_btn.on_click = toggle_record
    stop_btn.on_click = stop_trascrivi

    stt_section = ft.Column(
        [
            ft.Divider(color=DIM, height=24),
            ft.Text("🎤 Riconoscimento Vocale (STT)", size=16, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Container(
                ft.Row([stt_btn, stop_btn], spacing=12, wrap=True),
                padding=ft.padding.symmetric(vertical=8),
            ),
            stt_status,
            stt_progress,
            ft.Row([stt_result]),
        ],
        spacing=10,
    )

    return ft.Column(
        [
            ft.Text("🎙️ Voce", size=20, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Divider(color=ACCENT, height=1),
            ft.Column(
                [tts_section, stt_section],
                scroll=ft.ScrollMode.AUTO,
                expand=True,
                spacing=0,
            ),
        ],
        expand=True,
        spacing=8,
    )
