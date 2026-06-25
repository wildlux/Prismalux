import flet as ft
from utils.ai_client import AiClient

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"


def WikiView(page: ft.Page, ai: AiClient) -> ft.Column:
    result_col = ft.Column(spacing=8, scroll=ft.ScrollMode.AUTO, expand=True)
    search_field = ft.TextField(
        label="Cerca su Wikipedia",
        hint_text="Es: Intelligenza artificiale",
        hint_style=ft.TextStyle(color=DIM),
        label_style=ft.TextStyle(color=DIM),
        text_color=TXT, border_color=ACCENT,
        focused_border_color=ACCENT, bgcolor=BG, expand=True,
    )
    lang_dd = ft.Dropdown(
        label="Lingua",
        value="it",
        options=[
            ft.dropdown.Option("it", "Italiano"),
            ft.dropdown.Option("en", "English"),
            ft.dropdown.Option("de", "Deutsch"),
            ft.dropdown.Option("fr", "Français"),
            ft.dropdown.Option("es", "Español"),
        ],
        text_color=TXT,
        label_style=ft.TextStyle(color=DIM),
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        width=120,
    )
    status = ft.Text("", color=DIM, size=12, italic=True)

    def search(_):
        q = search_field.value.strip()
        if not q:
            return
        status.value = "Ricerca..."
        result_col.controls.clear()
        page.update()

        def done(data: dict):
            status.value = ""
            title = data.get("title", q)
            summary = data.get("summary", "")
            url = data.get("url", "")
            sections = data.get("sections", [])

            result_col.controls.append(
                ft.Text(title, size=18, weight=ft.FontWeight.BOLD, color=TXT)
            )
            if url:
                result_col.controls.append(
                    ft.Text(url, color=ACCENT, size=11, selectable=True)
                )
            if summary:
                result_col.controls.append(
                    ft.Container(
                        content=ft.Text(summary, color=TXT, size=13, selectable=True),
                        bgcolor=BG, border_radius=8, padding=12,
                        border=ft.border.all(1, "#2A2D40"),
                    )
                )
            for sec in sections[:5]:
                result_col.controls.append(
                    ft.ExpansionTile(
                        title=ft.Text(sec.get("title", ""), color=ACCENT, size=14),
                        controls=[
                            ft.Container(
                                content=ft.Text(sec.get("content", "")[:800],
                                                color=TXT, size=12, selectable=True),
                                padding=8,
                            )
                        ],
                    )
                )
            page.update()

        def err(e):
            result_col.controls.append(
                ft.Text(f"Errore: {e}", color="#FF6666", size=13)
            )
            status.value = ""
            page.update()

        ai.wiki(q, lang_dd.value or "it", on_done=done, on_error=err)

    search_btn = ft.IconButton(
        icon=ft.icons.SEARCH, icon_color=ACCENT, on_click=search, tooltip="Cerca"
    )

    return ft.Column(
        [
            ft.Text("🌐 Wiki & Web", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Row([search_field, lang_dd, search_btn]),
            status,
            ft.Container(content=result_col, expand=True),
        ],
        expand=True, spacing=10,
    )
