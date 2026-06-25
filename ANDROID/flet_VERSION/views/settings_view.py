import flet as ft
from utils.ai_client import AiClient
from utils.model_caps import DEFAULT_EMBED_MODEL
import threading
import json
import os
from pathlib import Path

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"

SETTINGS_PATH = str(Path.home() / ".prismalux" / "flet_settings.json")

_DEFAULTS = {
    "server_url": "http://192.168.1.100:11500",
    "token": "",
    "default_model": "",
    "embed_model": DEFAULT_EMBED_MODEL,
}


def _load_settings() -> dict:
    try:
        if os.path.exists(SETTINGS_PATH):
            with open(SETTINGS_PATH, "r", encoding="utf-8") as f:
                data = json.load(f)
                return {**_DEFAULTS, **data}
    except Exception:
        pass
    return dict(_DEFAULTS)


def _save_settings(data: dict) -> None:
    os.makedirs(os.path.dirname(SETTINGS_PATH), exist_ok=True)
    with open(SETTINGS_PATH, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


def SettingsView(page: ft.Page, ai: AiClient, on_save=None) -> ft.Column:
    cfg = _load_settings()
    testing = [False]

    url_field = ft.TextField(
        label="Server URL",
        label_style=ft.TextStyle(color=DIM),
        hint_text="http://192.168.1.100:11500",
        hint_style=ft.TextStyle(color=DIM),
        value=cfg.get("server_url", ""),
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        prefix_icon=ft.icons.LINK,
    )

    token_field = ft.TextField(
        label="Token Bearer",
        label_style=ft.TextStyle(color=DIM),
        hint_text="Lascia vuoto se non richiesto",
        hint_style=ft.TextStyle(color=DIM),
        value=cfg.get("token", ""),
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        password=True,
        can_reveal_password=True,
        prefix_icon=ft.icons.LOCK_OUTLINE,
    )

    model_field = ft.TextField(
        label="Modello default",
        label_style=ft.TextStyle(color=DIM),
        hint_text="Es: llama3.2:latest",
        hint_style=ft.TextStyle(color=DIM),
        value=cfg.get("default_model", ""),
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        prefix_icon=ft.icons.MEMORY,
    )

    embed_model_field = ft.TextField(
        label="Modello embedding (RAG)",
        label_style=ft.TextStyle(color=DIM),
        hint_text=f"Es: {DEFAULT_EMBED_MODEL}, nomic-embed-text",
        hint_style=ft.TextStyle(color=DIM),
        value=cfg.get("embed_model", DEFAULT_EMBED_MODEL),
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        prefix_icon=ft.icons.ACCOUNT_TREE,
    )

    status_text = ft.Text("", color=DIM, size=13, italic=True)

    def save_settings(_):
        new_cfg = {
            "server_url": url_field.value.strip(),
            "token": token_field.value.strip(),
            "default_model": model_field.value.strip(),
            "embed_model": embed_model_field.value.strip() or DEFAULT_EMBED_MODEL,
        }
        try:
            _save_settings(new_cfg)
            status_text.value = f"Salvato in {SETTINGS_PATH}"
            status_text.color = "#44CC88"
            page.snack_bar = ft.SnackBar(
                ft.Text("Impostazioni salvate. Riavvia l'app per applicarle.", color=TXT),
                bgcolor="#333355",
            )
            page.snack_bar.open = True
            if callable(on_save):
                new_ai = AiClient(
                    base_url=new_cfg["server_url"],
                    token=new_cfg["token"],
                )
                on_save(new_ai)
        except Exception as exc:
            status_text.value = f"Errore: {exc}"
            status_text.color = "#FF6666"
            page.snack_bar = ft.SnackBar(ft.Text(str(exc), color=TXT), bgcolor="#AA3333")
            page.snack_bar.open = True
        page.update()

    def test_connection(_):
        if testing[0]:
            return
        testing[0] = True
        status_text.value = "Test connessione..."
        status_text.color = ACCENT
        test_btn.disabled = True
        page.update()

        def _test():
            ev = threading.Event()
            res = [None]
            err = [None]

            def on_done(models):
                res[0] = models
                ev.set()

            def on_error(e):
                err[0] = e
                ev.set()

            ai.fetch_models(on_done, on_error)
            ev.wait(timeout=15)

            if err[0]:
                status_text.value = f"Connessione fallita: {err[0]}"
                status_text.color = "#FF6666"
                page.snack_bar = ft.SnackBar(ft.Text(f"Errore: {err[0]}", color=TXT), bgcolor="#AA3333")
                page.snack_bar.open = True
            else:
                count = len(res[0]) if res[0] else 0
                status_text.value = f"Connesso. {count} model{'i' if count != 1 else 'o'} disponibil{'i' if count != 1 else 'e'}."
                status_text.color = "#44CC88"
                page.snack_bar = ft.SnackBar(
                    ft.Text(f"OK — {count} modelli trovati", color=TXT),
                    bgcolor="#224422",
                )
                page.snack_bar.open = True

            testing[0] = False
            test_btn.disabled = False
            page.update()

        threading.Thread(target=_test, daemon=True).start()

    save_btn = ft.ElevatedButton(
        "💾 Salva",
        bgcolor=ACCENT,
        color=TXT,
        on_click=save_settings,
        icon=ft.icons.SAVE_OUTLINED,
    )

    test_btn = ft.OutlinedButton(
        "🔗 Test connessione",
        style=ft.ButtonStyle(color=ACCENT, side=ft.BorderSide(1, ACCENT)),
        on_click=test_connection,
    )

    col = ft.Column(
        [
            ft.Text("⚙️ Impostazioni", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Container(
                content=ft.Column(
                    [
                        ft.Text("Connessione server", color=DIM, size=12, weight=ft.FontWeight.BOLD),
                        url_field,
                        ft.Text("Autenticazione", color=DIM, size=12, weight=ft.FontWeight.BOLD),
                        token_field,
                        ft.Text("Modello AI", color=DIM, size=12, weight=ft.FontWeight.BOLD),
                        model_field,
                        ft.Text("Embedding RAG", color=DIM, size=12, weight=ft.FontWeight.BOLD),
                        embed_model_field,
                    ],
                    spacing=10,
                ),
                bgcolor=BG,
                border_radius=10,
                padding=ft.padding.symmetric(horizontal=16, vertical=14),
                border=ft.border.all(1, "#2A2D40"),
            ),
            ft.Row(
                [save_btn, test_btn],
                alignment=ft.MainAxisAlignment.START,
                spacing=12,
            ),
            status_text,
            ft.Container(
                content=ft.Column(
                    [
                        ft.Text("Percorso settings:", color=DIM, size=11),
                        ft.Text(SETTINGS_PATH, color="#555577", size=11, selectable=True),
                    ],
                    spacing=2,
                ),
                padding=ft.padding.only(top=8),
            ),
        ],
        expand=True,
        spacing=12,
    )
    return col
