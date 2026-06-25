import flet as ft
from utils.ai_client import AiClient
from utils.model_caps import labeled, is_embedding_model
import threading

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"
EMB_COLOR = "#4A4A6A"
BG_USER = "#2A2560"
BG_AI = "#1E2235"


def ChatView(page: ft.Page, ai: AiClient) -> ft.Column:
    history: list[dict] = []
    streaming = False
    _cached_models: list[str] = []

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
        def on_done(models: list[str]):
            nonlocal _cached_models
            _cached_models = models
            non_emb = [m for m in models if not is_embedding_model(m)]
            emb = [m for m in models if is_embedding_model(m)]
            options = [ft.dropdown.Option(key=m, text=labeled(m)) for m in non_emb]
            if emb:
                options.append(ft.dropdown.Option(key="__sep__", text="── Embedding ──", disabled=True))
                for m in emb:
                    options.append(ft.dropdown.Option(key=m, text=labeled(m)))
            model_dd.options = options
            if non_emb and model_dd.value not in non_emb:
                model_dd.value = non_emb[0]
            try:
                page.update()
            except Exception:
                pass

        def on_error(err: str):
            try:
                page.snack_bar = ft.SnackBar(ft.Text(f"Errore modelli: {err}", color=TXT), bgcolor="#AA3333")
                page.snack_bar.open = True
                page.update()
            except Exception:
                pass

        threading.Thread(target=lambda: ai.fetch_models(on_done, on_error), daemon=True).start()

    def _auto_refresh_loop():
        while True:
            threading.Event().wait(300)  # 5 minuti
            refresh_models()

    threading.Thread(target=_auto_refresh_loop, daemon=True).start()

    def show_model_list(_):
        if not _cached_models:
            page.snack_bar = ft.SnackBar(
                ft.Text("Nessun modello ancora caricato. Premi aggiorna.", color=TXT),
                bgcolor="#555577",
            )
            page.snack_bar.open = True
            page.update()
            return

        rows = []
        for m in _cached_models:
            lab = labeled(m)
            color = EMB_COLOR if is_embedding_model(m) else TXT
            rows.append(ft.Text(lab, color=color, size=13, selectable=True))

        dlg = ft.AlertDialog(
            modal=True,
            title=ft.Text("📋 Modelli disponibili"),
            content=ft.Container(
                content=ft.Column(rows, spacing=6, scroll=ft.ScrollMode.AUTO),
                width=320,
                height=350,
            ),
            actions=[ft.TextButton("Chiudi", on_click=lambda _: page.close(dlg))],
        )
        page.open(dlg)

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
        stop_btn.visible = True
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
            stop_btn.visible = False
            page.update()

        def on_error(err: str):
            nonlocal streaming
            ai_bubble_text.value = f"[Errore: {err}]"
            ai_bubble_text.color = "#FF6666"
            typing_indicator.visible = False
            streaming = False
            stop_btn.visible = False
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

    stop_btn = ft.IconButton(
        icon=ft.icons.STOP_CIRCLE_OUTLINED,
        icon_color="#FF4444",
        tooltip="Stop",
        visible=False,
        on_click=lambda _: ai.abort(),
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

    # Equivalente del label "LLM:" cliccabile — apre lista modelli
    list_btn = ft.IconButton(
        icon=ft.icons.LIST_ALT,
        icon_color=DIM,
        on_click=show_model_list,
        tooltip="Lista modelli (LLM)",
    )

    refresh_models()

    col = ft.Column(
        [
            ft.Container(
                content=ft.Row(
                    [model_dd, list_btn, refresh_btn, new_chat_btn],
                    alignment=ft.MainAxisAlignment.START,
                ),
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
                content=ft.Row([input_field, stop_btn, send_btn], alignment=ft.MainAxisAlignment.CENTER),
                padding=ft.padding.symmetric(horizontal=8, vertical=4),
            ),
        ],
        expand=True,
        spacing=4,
    )
    return col
