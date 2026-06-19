import flet as ft
import threading
import json
import re

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"


def LavoroView(page: ft.Page, ai) -> ft.Column:
    model = getattr(ai, "default_model", "llama3")

    posizione_field = ft.TextField(label="Posizione cercata", color=TXT, bgcolor=BG, border_color=ACCENT, expand=True)
    citta_field = ft.TextField(label="Città", color=TXT, bgcolor=BG, border_color=ACCENT, expand=True)
    settore_dd = ft.Dropdown(
        label="Settore",
        color=TXT,
        bgcolor=BG,
        border_color=ACCENT,
        options=[
            ft.dropdown.Option("IT"),
            ft.dropdown.Option("Sanità"),
            ft.dropdown.Option("Finanza"),
            ft.dropdown.Option("Educazione"),
            ft.dropdown.Option("Altro"),
        ],
        value="IT",
        expand=True,
    )

    results_col = ft.Column([], spacing=8, scroll=ft.ScrollMode.AUTO)
    loading = ft.ProgressRing(visible=False, color=ACCENT, width=24, height=24)

    def cerca(e):
        pos = posizione_field.value.strip()
        citta = citta_field.value.strip()
        settore = settore_dd.value
        if not pos:
            page.snack_bar = ft.SnackBar(ft.Text("Inserisci la posizione cercata"))
            page.snack_bar.open = True
            page.update()
            return
        loading.visible = True
        results_col.controls.clear()
        page.update()

        def _run():
            prompt = (
                f"Simula 4 offerte di lavoro realistiche per: '{pos}' a {citta or 'Italia'}, settore {settore}. "
                f"Rispondi SOLO con un array JSON:\n"
                f"[{{\"titolo\":\"...\",\"azienda\":\"...\",\"luogo\":\"...\",\"stipendio\":\"...\"}}]"
            )
            ai.chat_once(
                model,
                [{"role": "user", "content": prompt}],
                on_done=lambda r: _show(r),
                on_error=lambda err: _err(err),
            )

        def _show(r):
            loading.visible = False
            try:
                m = re.search(r'\[.*\]', r, re.DOTALL)
                offerte = json.loads(m.group(0)) if m else []
            except Exception:
                offerte = []

            results_col.controls.clear()
            if not offerte:
                results_col.controls.append(ft.Text(r, color=TXT, size=13, selectable=True))
            else:
                for o in offerte:
                    results_col.controls.append(
                        ft.Card(
                            content=ft.Container(
                                content=ft.Column([
                                    ft.Text(o.get("titolo", ""), size=15, weight=ft.FontWeight.BOLD, color=ACCENT),
                                    ft.Text(o.get("azienda", ""), size=13, color=TXT),
                                    ft.Row([
                                        ft.Icon(ft.icons.LOCATION_ON, size=14, color=DIM),
                                        ft.Text(o.get("luogo", ""), size=12, color=DIM),
                                        ft.Icon(ft.icons.EURO, size=14, color=DIM),
                                        ft.Text(o.get("stipendio", ""), size=12, color=DIM),
                                    ], spacing=4),
                                ], spacing=4),
                                padding=12,
                                bgcolor="#12152A",
                            ),
                            elevation=2,
                        )
                    )
            page.update()

        def _err(err):
            loading.visible = False
            results_col.controls.clear()
            results_col.controls.append(ft.Text(f"Errore: {err}", color="#FF6B6B", size=13))
            page.update()

        threading.Thread(target=_run, daemon=True).start()

    # CV dialog
    cv_obj_field = ft.TextField(
        label="Obiettivo professionale",
        color=TXT,
        bgcolor=BG,
        border_color=ACCENT,
        multiline=True,
        min_lines=3,
        expand=True,
    )
    cv_result = ft.Text("", color=TXT, size=13, selectable=True)
    cv_loading = ft.ProgressRing(visible=False, color=ACCENT, width=20, height=20)

    def genera_cv_ai(e):
        obj = cv_obj_field.value.strip()
        pos = posizione_field.value.strip()
        cv_loading.visible = True
        cv_result.value = ""
        page.update()

        def _run():
            ai.chat_once(
                model,
                [{"role": "user", "content": (
                    f"Prepara una struttura CV professionale per la posizione '{pos or 'non specificata'}'. "
                    f"Obiettivo: {obj or 'non specificato'}. "
                    f"Includi: Profilo, Competenze chiave, Esperienza (3 voci esempio), Formazione, Lingue."
                )}],
                on_done=lambda r: _done(r),
                on_error=lambda err: _err_cv(err),
            )

        def _done(r):
            cv_loading.visible = False
            cv_result.value = r
            page.update()

        def _err_cv(err):
            cv_loading.visible = False
            cv_result.value = f"Errore: {err}"
            page.update()

        threading.Thread(target=_run, daemon=True).start()

    cv_dialog = ft.AlertDialog(
        title=ft.Text("Prepara CV", color=ACCENT),
        bgcolor=BG,
        content=ft.Container(
            content=ft.Column([
                cv_obj_field,
                ft.ElevatedButton("🤖 Genera struttura", on_click=genera_cv_ai, bgcolor=ACCENT, color=TXT),
                ft.Row([cv_loading]),
                ft.Container(
                    content=cv_result,
                    padding=8,
                    border_radius=6,
                    bgcolor="#12152A",
                    width=300,
                ),
            ], spacing=10, scroll=ft.ScrollMode.AUTO),
            width=320,
            height=420,
        ),
        actions=[ft.TextButton("Chiudi", on_click=lambda e: setattr(cv_dialog, 'open', False) or page.update())],
    )

    def apri_cv(e):
        cv_obj_field.value = ""
        cv_result.value = ""
        page.dialog = cv_dialog
        cv_dialog.open = True
        page.update()

    return ft.Column([
        ft.Text("Cerca Lavoro", size=20, weight=ft.FontWeight.BOLD, color=ACCENT),
        ft.Row([posizione_field, citta_field], spacing=8),
        ft.Row([settore_dd], spacing=8),
        ft.Row([
            ft.ElevatedButton("🔍 Cerca Offerte", on_click=cerca, bgcolor=ACCENT, color=TXT),
            ft.ElevatedButton("📝 Prepara CV", on_click=apri_cv, bgcolor="#2A2D4E", color=TXT),
        ], spacing=8),
        ft.Row([loading]),
        results_col,
    ], spacing=10, scroll=ft.ScrollMode.AUTO, expand=True)
