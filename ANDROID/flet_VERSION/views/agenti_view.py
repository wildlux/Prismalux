import flet as ft
from utils.ai_client import AiClient
import threading
import json

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"

_DECOMPOSE_SYSTEM = (
    "Sei un orchestratore. Dato un obiettivo, rispondi SOLO con JSON:\n"
    '{"subtasks":[{"id":1,"title":"...","prompt":"..."},{"id":2,"title":"...","prompt":"..."}]}\n'
    "Massimo 3 subtask. Niente testo fuori dal JSON."
)

_ICONS = {
    "pending": ("⏳", DIM),
    "running": ("🔄", ACCENT),
    "done": ("✅", "#44CC88"),
    "error": ("❌", "#FF6666"),
}


class _SubtaskRow:
    def __init__(self, idx: int, title: str):
        self.status_icon = ft.Text(_ICONS["pending"][0], size=18, color=_ICONS["pending"][1])
        self.title_text = ft.Text(f"[{idx}] {title}", color=TXT, size=14, expand=True)
        self.result_text = ft.Text("", color=DIM, size=12, italic=True)
        self.container = ft.Column(
            [
                ft.Row([self.status_icon, self.title_text]),
                ft.Container(content=self.result_text, padding=ft.padding.only(left=28)),
            ],
            spacing=2,
        )

    def set_state(self, state: str, page: ft.Page, result: str = ""):
        icon, color = _ICONS.get(state, _ICONS["pending"])
        self.status_icon.value = icon
        self.status_icon.color = color
        if result:
            self.result_text.value = result[:200] + ("…" if len(result) > 200 else "")
        page.update()


def AgentiView(page: ft.Page, ai: AiClient) -> ft.Column:
    running = [False]

    prompt_field = ft.TextField(
        label="Obiettivo",
        label_style=ft.TextStyle(color=DIM),
        hint_text="Es: Scrivi un piano marketing per una app mobile",
        hint_style=ft.TextStyle(color=DIM),
        multiline=True,
        min_lines=3,
        max_lines=6,
        text_color=TXT,
        border_color=ACCENT,
        focused_border_color=ACCENT,
        bgcolor=BG,
    )

    log_col = ft.Column(spacing=6, scroll=ft.ScrollMode.AUTO, expand=True)
    status_text = ft.Text("", color=DIM, size=13, italic=True)
    run_btn = ft.ElevatedButton(
        "🚀 Decomponsi + Esegui",
        bgcolor=ACCENT,
        color=TXT,
    )

    stop_btn = ft.IconButton(
        icon=ft.icons.STOP_CIRCLE_OUTLINED,
        icon_color="#FF4444",
        tooltip="Stop AI",
        visible=False,
        on_click=lambda _: ai.abort(),
    )

    def log_line(msg: str, color: str = TXT):
        log_col.controls.append(ft.Text(msg, color=color, size=13))
        page.update()

    def run_pipeline(_):
        if running[0]:
            return
        goal = prompt_field.value.strip()
        if not goal:
            page.snack_bar = ft.SnackBar(ft.Text("Inserisci un obiettivo", color=TXT), bgcolor="#AA3333")
            page.snack_bar.open = True
            page.update()
            return

        running[0] = True
        log_col.controls.clear()
        status_text.value = "Decomposizione in corso..."
        run_btn.disabled = True
        stop_btn.visible = True
        page.update()

        def pipeline():
            subtask_rows: list[_SubtaskRow] = []
            result = [None]
            error = [None]
            ev = threading.Event()

            def on_decompose_done(resp: str):
                result[0] = resp
                ev.set()

            def on_decompose_err(err):
                error[0] = err
                ev.set()

            ai.chat_once(
                "",
                [
                    {"role": "system", "content": _DECOMPOSE_SYSTEM},
                    {"role": "user", "content": goal},
                ],
                on_decompose_done,
                on_decompose_err,
            )
            ev.wait(timeout=60)

            if error[0]:
                log_line(f"Errore decomposizione: {error[0]}", "#FF6666")
                status_text.value = "Fallito"
                run_btn.disabled = False
                stop_btn.visible = False
                running[0] = False
                page.update()
                return

            try:
                raw = result[0] or ""
                start = raw.find("{")
                end = raw.rfind("}") + 1
                plan = json.loads(raw[start:end])
                subtasks = plan.get("subtasks", [])
            except Exception as exc:
                log_line(f"JSON non valido: {exc}", "#FF6666")
                log_line(result[0] or "", DIM)
                status_text.value = "Fallito"
                run_btn.disabled = False
                stop_btn.visible = False
                running[0] = False
                page.update()
                return

            if not subtasks:
                log_line("Nessun subtask trovato.", "#FF8844")
                status_text.value = "Completato (vuoto)"
                run_btn.disabled = False
                stop_btn.visible = False
                running[0] = False
                page.update()
                return

            for st in subtasks:
                row = _SubtaskRow(st["id"], st.get("title", f"Subtask {st['id']}"))
                subtask_rows.append(row)
                log_col.controls.append(
                    ft.Container(
                        content=row.container,
                        bgcolor="#161929",
                        border_radius=8,
                        padding=ft.padding.symmetric(horizontal=10, vertical=6),
                        border=ft.border.all(1, "#333355"),
                    )
                )
            page.update()

            context_results: list[str] = []
            for i, (st, row) in enumerate(zip(subtasks, subtask_rows)):
                status_text.value = f"Esecuzione subtask {i+1}/{len(subtasks)}..."
                row.set_state("running", page)

                ev2 = threading.Event()
                res2 = [None]
                err2 = [None]

                context_block = ""
                if context_results:
                    context_block = "\n\nContesto dai passaggi precedenti:\n" + "\n---\n".join(context_results)

                messages = [
                    {"role": "system", "content": "Sei un esperto assistente AI. Rispondi in modo conciso e utile."},
                    {"role": "user", "content": st.get("prompt", st.get("title", "")) + context_block},
                ]

                def make_done(r, e, ev_):
                    def on_d(s):
                        r[0] = s
                        ev_.set()
                    def on_e(err):
                        e[0] = err
                        ev_.set()
                    return on_d, on_e

                on_d, on_e = make_done(res2, err2, ev2)
                ai.chat_once("", messages, on_d, on_e)
                ev2.wait(timeout=90)

                if err2[0]:
                    row.set_state("error", page, str(err2[0]))
                else:
                    answer = res2[0] or ""
                    context_results.append(f"[{st['id']}] {st.get('title','')}: {answer}")
                    row.set_state("done", page, answer)

            log_col.controls.append(ft.Divider(color=ACCENT))
            log_col.controls.append(ft.Text("Pipeline completata.", color="#44CC88", size=14, weight=ft.FontWeight.BOLD))
            status_text.value = "Completato"
            run_btn.disabled = False
            running[0] = False
            page.update()

        threading.Thread(target=pipeline, daemon=True).start()

    run_btn.on_click = run_pipeline

    col = ft.Column(
        [
            ft.Text("🤖 Multi-Agente", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            prompt_field,
            ft.Row([run_btn, stop_btn, status_text], alignment=ft.MainAxisAlignment.START, spacing=12),
            ft.Container(
                content=log_col,
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
