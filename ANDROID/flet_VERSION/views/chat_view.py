import flet as ft
from utils.ai_client import AiClient
import threading

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"
BG_USER = "#2A2560"
BG_AI = "#1E2235"


def ChatView(page: ft.Page, ai: AiClient) -> ft.Column:
    history: list[dict] = []
    streaming = False

    model_dd = ft.Dropdown(
        label="Modello",
        options=[],
        text_color=TXT,
        label_style=ft.TextStyle(color=DIM),
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        expand=True,
    )

    messages_col = ft.Column(
        spacing=8,
        scroll=ft.ScrollMode.AUTO,
        expand=True,
    )

    typing_indicator = ft.Text("...", color=DIM, visible=False, size=20)

    def make_bubble(role: str, content: str) -> ft.Container:
        is_user = role == "user"
        return ft.Container(
            content=ft.Text(content, color=TXT, selectable=True, size=14),
            bgcolor=BG_USER if is_user else BG_AI,
            border_radius=ft.border_radius.only(
                top_left=12, top_right=12,
                bottom_left=0 if is_user else 12,
                bottom_right=12 if is_user else 0,
            ),
            padding=ft.padding.symmetric(horizontal=12, vertical=8),
            margin=ft.margin.only(
                left=60 if is_user else 0,
                right=0 if is_user else 60,
            ),
            alignment=ft.alignment.center_right if is_user else ft.alignment.center_left,
        )

    def refresh_models():
        def on_done(models):
            model_dd.options = [ft.dropdown.Option(m) for m in models]
            if models:
                model_dd.value = models[0]
            page.update()

        def on_error(err):
            page.snack_bar = ft.SnackBar(ft.Text(f"Errore modelli: {err}", color=TXT), bgcolor="#AA3333")
            page.snack_bar.open = True
            page.update()

        threading.Thread(target=lambda: ai.fetch_models(on_done, on_error), daemon=True).start()

    def clear_chat(_):
        nonlocal history
        history = []
        messages_col.controls.clear()
        typing_indicator.visible = False
        page.update()

    def send_message(_):
        nonlocal streaming
        text = input_field.value.strip()
        if not text or streaming:
            return
        input_field.value = ""
        history.append({"role": "user", "content": text})
        messages_col.controls.append(make_bubble("user", text))
        typing_indicator.visible = True
        streaming = True
        page.update()

        ai_bubble_text = ft.Text("", color=TXT, selectable=True, size=14)
        ai_bubble = ft.Container(
            content=ai_bubble_text,
            bgcolor=BG_AI,
            border_radius=ft.border_radius.only(top_left=12, top_right=12, bottom_right=12),
            padding=ft.padding.symmetric(horizontal=12, vertical=8),
            margin=ft.margin.only(right=60),
        )
        messages_col.controls.append(ai_bubble)
        accumulated = [""]

        def on_token(token: str):
            accumulated[0] += token
            ai_bubble_text.value = accumulated[0]
            page.update()

        def on_done(full: str):
            nonlocal streaming
            history.append({"role": "assistant", "content": full or accumulated[0]})
            typing_indicator.visible = False
            streaming = False
            page.update()

        def on_error(err):
            nonlocal streaming
            ai_bubble_text.value = f"[Errore: {err}]"
            ai_bubble_text.color = "#FF6666"
            typing_indicator.visible = False
            streaming = False
            page.snack_bar = ft.SnackBar(ft.Text(str(err), color=TXT), bgcolor="#AA3333")
            page.snack_bar.open = True
            page.update()

        model = model_dd.value or ""
        threading.Thread(
            target=lambda: ai.chat_stream(model, list(history), on_token, on_done, on_error),
            daemon=True,
        ).start()

    input_field = ft.TextField(
        hint_text="Scrivi un messaggio...",
        hint_style=ft.TextStyle(color=DIM),
        multiline=True,
        min_lines=1,
        max_lines=4,
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        expand=True,
        on_submit=send_message,
    )

    send_btn = ft.IconButton(
        icon=ft.icons.SEND_ROUNDED,
        icon_color=ACCENT,
        on_click=send_message,
        tooltip="Invia",
    )

    new_chat_btn = ft.TextButton(
        "🗑 Nuova chat",
        style=ft.ButtonStyle(color=DIM),
        on_click=clear_chat,
    )

    refresh_btn = ft.IconButton(
        icon=ft.icons.REFRESH,
        icon_color=DIM,
        on_click=lambda _: refresh_models(),
        tooltip="Ricarica modelli",
    )

    refresh_models()

    col = ft.Column(
        [
            ft.Container(
                content=ft.Row([model_dd, refresh_btn, new_chat_btn], alignment=ft.MainAxisAlignment.START),
                padding=ft.padding.symmetric(horizontal=8, vertical=4),
            ),
            ft.Container(
                content=messages_col,
                bgcolor=BG,
                border_radius=8,
                padding=8,
                expand=True,
            ),
            typing_indicator,
            ft.Container(
                content=ft.Row([input_field, send_btn], alignment=ft.MainAxisAlignment.CENTER),
                padding=ft.padding.symmetric(horizontal=8, vertical=4),
            ),
        ],
        expand=True,
        spacing=4,
    )
    return col
