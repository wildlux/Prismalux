import flet as ft
from utils.ai_client import AiClient
import threading
import json

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"

_GRAPH_STATS_PROMPT = "__GRAPH_STATS__"
_GRAPH_NODES_PROMPT = "__GRAPH_NODES__"
_GRAPH_SEARCH_PREFIX = "__GRAPH_SEARCH__:"


def GrafoView(page: ft.Page, ai: AiClient) -> ft.Column:
    stats_nodi = ft.Text("—", color=TXT, size=22, weight=ft.FontWeight.BOLD)
    stats_archi = ft.Text("—", color=TXT, size=22, weight=ft.FontWeight.BOLD)
    stats_label = ft.Text("Caricamento...", color=DIM, size=12, italic=True)

    nodes_table = ft.DataTable(
        columns=[
            ft.DataColumn(ft.Text("Label", color=ACCENT, size=12)),
            ft.DataColumn(ft.Text("Tipo", color=ACCENT, size=12)),
            ft.DataColumn(ft.Text("Imp.", color=ACCENT, size=12)),
        ],
        rows=[],
        border=ft.border.all(1, "#2A2D40"),
        border_radius=8,
        heading_row_color="#161929",
        data_row_color={"hovered": "#232640"},
        column_spacing=16,
        expand=True,
    )

    table_container = ft.Container(
        content=ft.Column(
            [nodes_table],
            scroll=ft.ScrollMode.AUTO,
            expand=True,
        ),
        expand=True,
        bgcolor=BG,
        border_radius=8,
        padding=4,
        border=ft.border.all(1, "#2A2D40"),
    )

    search_field = ft.TextField(
        label="Cerca nodo",
        label_style=ft.TextStyle(color=DIM),
        hint_text="Es: BLHM, clustering...",
        hint_style=ft.TextStyle(color=DIM),
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
        expand=True,
        on_submit=lambda _: do_search(None),
    )

    search_results = ft.Column(spacing=6, scroll=ft.ScrollMode.AUTO)

    def _call_graph(prompt: str, on_done, on_error):
        ai.chat_once("", [{"role": "user", "content": prompt}], on_done, on_error)

    def load_stats():
        ev = threading.Event()
        res = [None]
        err = [None]

        def on_done(r):
            res[0] = r
            ev.set()

        def on_error(e):
            err[0] = e
            ev.set()

        _call_graph(_GRAPH_STATS_PROMPT, on_done, on_error)
        ev.wait(timeout=30)

        if err[0]:
            stats_label.value = f"Errore: {err[0]}"
            page.update()
            return

        try:
            data = json.loads(res[0] or "{}")
            stats_nodi.value = str(data.get("nodes", "?"))
            stats_archi.value = str(data.get("edges", "?"))
            stats_label.value = f"Aggiornato"
        except Exception:
            stats_label.value = res[0][:80] if res[0] else "—"
        page.update()

    def load_nodes():
        ev = threading.Event()
        res = [None]
        err = [None]

        def on_done(r):
            res[0] = r
            ev.set()

        def on_error(e):
            err[0] = e
            ev.set()

        _call_graph(_GRAPH_NODES_PROMPT, on_done, on_error)
        ev.wait(timeout=30)

        if err[0]:
            page.snack_bar = ft.SnackBar(ft.Text(f"Errore nodi: {err[0]}", color=TXT), bgcolor="#AA3333")
            page.snack_bar.open = True
            page.update()
            return

        try:
            nodes = json.loads(res[0] or "[]")
        except Exception:
            nodes = []

        nodes_table.rows.clear()
        for n in nodes[:50]:
            label = str(n.get("label", ""))[:30]
            tipo = str(n.get("type", ""))[:15]
            imp = f"{float(n.get('importance', 0)):.2f}"
            nodes_table.rows.append(
                ft.DataRow(
                    cells=[
                        ft.DataCell(ft.Text(label, color=TXT, size=13, selectable=True)),
                        ft.DataCell(ft.Text(tipo, color=DIM, size=12)),
                        ft.DataCell(ft.Text(imp, color=ACCENT, size=12)),
                    ]
                )
            )
        if not nodes:
            nodes_table.rows.append(
                ft.DataRow(cells=[
                    ft.DataCell(ft.Text("Nessun nodo", color=DIM)),
                    ft.DataCell(ft.Text("")),
                    ft.DataCell(ft.Text("")),
                ])
            )
        page.update()

    def refresh(_):
        stats_nodi.value = "..."
        stats_archi.value = "..."
        stats_label.value = "Aggiornamento..."
        nodes_table.rows.clear()
        page.update()
        threading.Thread(target=load_stats, daemon=True).start()
        threading.Thread(target=load_nodes, daemon=True).start()

    def do_search(_):
        query = search_field.value.strip()
        if not query:
            return
        search_results.controls.clear()
        search_results.controls.append(ft.Text("Ricerca...", color=DIM, italic=True))
        page.update()

        ev = threading.Event()
        res = [None]
        err = [None]

        def on_done(r):
            res[0] = r
            ev.set()

        def on_error(e):
            err[0] = e
            ev.set()

        def _do():
            _call_graph(_GRAPH_SEARCH_PREFIX + query, on_done, on_error)
            ev.wait(timeout=30)
            search_results.controls.clear()
            if err[0]:
                search_results.controls.append(ft.Text(f"Errore: {err[0]}", color="#FF6666"))
            else:
                try:
                    nodes = json.loads(res[0] or "[]")
                except Exception:
                    nodes = []
                if not nodes:
                    search_results.controls.append(ft.Text("Nessun risultato.", color=DIM))
                for n in nodes[:10]:
                    search_results.controls.append(
                        ft.Container(
                            content=ft.Column(
                                [
                                    ft.Text(n.get("label", ""), color=ACCENT, size=14, weight=ft.FontWeight.BOLD),
                                    ft.Text(n.get("content", "")[:120], color=TXT, size=12),
                                    ft.Text(f"Tipo: {n.get('type','')} | Imp: {n.get('importance',0):.2f}", color=DIM, size=11),
                                ],
                                spacing=2,
                            ),
                            bgcolor="#161929",
                            border_radius=8,
                            padding=10,
                            border=ft.border.all(1, ACCENT),
                        )
                    )
            page.update()

        threading.Thread(target=_do, daemon=True).start()

    refresh_btn = ft.ElevatedButton("🔄 Aggiorna", bgcolor=ACCENT, color=TXT, on_click=refresh)
    search_btn = ft.IconButton(icon=ft.icons.SEARCH, icon_color=ACCENT, on_click=do_search)

    threading.Thread(target=load_stats, daemon=True).start()
    threading.Thread(target=load_nodes, daemon=True).start()

    col = ft.Column(
        [
            ft.Text("🕸 Grafo Conoscenza", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Container(
                content=ft.Row(
                    [
                        ft.Column(
                            [ft.Text("Nodi", color=DIM, size=12), stats_nodi],
                            horizontal_alignment=ft.CrossAxisAlignment.CENTER,
                        ),
                        ft.VerticalDivider(color="#2A2D40", width=20),
                        ft.Column(
                            [ft.Text("Archi", color=DIM, size=12), stats_archi],
                            horizontal_alignment=ft.CrossAxisAlignment.CENTER,
                        ),
                        ft.VerticalDivider(color="#2A2D40", width=20),
                        ft.Column(
                            [stats_label, refresh_btn],
                            horizontal_alignment=ft.CrossAxisAlignment.CENTER,
                        ),
                    ],
                    alignment=ft.MainAxisAlignment.START,
                    spacing=0,
                ),
                bgcolor=BG,
                border_radius=10,
                padding=ft.padding.symmetric(horizontal=16, vertical=10),
                border=ft.border.all(1, "#2A2D40"),
            ),
            ft.Row([search_field, search_btn], spacing=8),
            ft.Container(
                content=search_results,
                visible=True,
                max_height=180,
                bgcolor="#0F1117",
                border_radius=8,
                padding=6,
            ),
            ft.Text("Nodi (max 50)", color=DIM, size=12),
            table_container,
        ],
        expand=True,
        spacing=10,
    )
    return col
