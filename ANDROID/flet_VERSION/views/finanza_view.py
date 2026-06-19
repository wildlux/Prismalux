import flet as ft
import threading

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"


def FinanzaView(page: ft.Page, ai) -> ft.Column:
    model = getattr(ai, "default_model", "llama3")

    # --- IBAN ---
    iban_field = ft.TextField(
        label="IBAN",
        color=TXT,
        bgcolor=BG,
        border_color=ACCENT,
        text_size=14,
        expand=True,
    )
    iban_result = ft.Text("", color=TXT, size=13, selectable=True)
    iban_loading = ft.ProgressRing(visible=False, color=ACCENT, width=20, height=20)

    def verify_iban(e):
        val = iban_field.value.strip()
        if not val:
            page.snack_bar = ft.SnackBar(ft.Text("Inserisci un IBAN"))
            page.snack_bar.open = True
            page.update()
            return
        iban_loading.visible = True
        iban_result.value = ""
        page.update()

        def _run():
            ai.chat_once(
                model,
                [{"role": "user", "content": f"Verifica IBAN: {val}. Rispondi indicando: banca emittente, paese, e se è valido o non valido."}],
                on_done=lambda r: _done(r),
                on_error=lambda err: _err(err),
            )

        def _done(r):
            iban_loading.visible = False
            iban_result.value = r
            page.update()

        def _err(err):
            iban_loading.visible = False
            iban_result.value = f"Errore: {err}"
            page.update()

        threading.Thread(target=_run, daemon=True).start()

    iban_tab = ft.Column([
        ft.Row([iban_field, ft.IconButton(ft.icons.CHECK_CIRCLE, icon_color=ACCENT, tooltip="Verifica", on_click=verify_iban)]),
        ft.Row([iban_loading]),
        ft.Container(
            content=iban_result,
            padding=10,
            border_radius=8,
            bgcolor="#12152A",
            width=float("inf"),
        ),
    ], spacing=10, scroll=ft.ScrollMode.AUTO)

    # --- P.IVA ---
    piva_field = ft.TextField(
        label="P.IVA italiana",
        color=TXT,
        bgcolor=BG,
        border_color=ACCENT,
        text_size=14,
        expand=True,
    )
    piva_result = ft.Text("", color=TXT, size=13, selectable=True)
    piva_loading = ft.ProgressRing(visible=False, color=ACCENT, width=20, height=20)

    def verify_piva(e):
        val = piva_field.value.strip()
        if not val:
            page.snack_bar = ft.SnackBar(ft.Text("Inserisci una P.IVA"))
            page.snack_bar.open = True
            page.update()
            return
        piva_loading.visible = True
        piva_result.value = ""
        page.update()

        def _run():
            ai.chat_once(
                model,
                [{"role": "user", "content": f"Verifica P.IVA italiana: {val}. Indica se è formalmente valida (algoritmo di controllo), la regione di registrazione se deducibile, e note rilevanti."}],
                on_done=lambda r: _done(r),
                on_error=lambda err: _err(err),
            )

        def _done(r):
            piva_loading.visible = False
            piva_result.value = r
            page.update()

        def _err(err):
            piva_loading.visible = False
            piva_result.value = f"Errore: {err}"
            page.update()

        threading.Thread(target=_run, daemon=True).start()

    piva_tab = ft.Column([
        ft.Row([piva_field, ft.IconButton(ft.icons.CHECK_CIRCLE, icon_color=ACCENT, tooltip="Verifica", on_click=verify_piva)]),
        ft.Row([piva_loading]),
        ft.Container(
            content=piva_result,
            padding=10,
            border_radius=8,
            bgcolor="#12152A",
            width=float("inf"),
        ),
    ], spacing=10, scroll=ft.ScrollMode.AUTO)

    # --- Calcolatore ---
    importo_field = ft.TextField(label="Importo (€)", color=TXT, bgcolor=BG, border_color=ACCENT, keyboard_type=ft.KeyboardType.NUMBER, expand=True)
    perc_field = ft.TextField(label="Percentuale (%)", color=TXT, bgcolor=BG, border_color=ACCENT, keyboard_type=ft.KeyboardType.NUMBER, expand=True)
    calc_result = ft.Text("", color=ACCENT, size=15, weight=ft.FontWeight.BOLD)

    def _parse():
        try:
            imp = float(importo_field.value.replace(",", "."))
            pct = float(perc_field.value.replace(",", "."))
            return imp, pct
        except Exception:
            return None, None

    def calc_iva(e):
        imp, pct = _parse()
        if imp is None:
            page.snack_bar = ft.SnackBar(ft.Text("Valori non validi"))
            page.snack_bar.open = True
            page.update()
            return
        iva = imp * pct / 100
        calc_result.value = f"IVA ({pct}%): {iva:.2f} €  |  Totale: {imp + iva:.2f} €"
        page.update()

    def calc_sconto(e):
        imp, pct = _parse()
        if imp is None:
            page.snack_bar = ft.SnackBar(ft.Text("Valori non validi"))
            page.snack_bar.open = True
            page.update()
            return
        sconto = imp * pct / 100
        calc_result.value = f"Sconto ({pct}%): -{sconto:.2f} €  |  Finale: {imp - sconto:.2f} €"
        page.update()

    def calc_netto_lordo(e):
        imp, pct = _parse()
        if imp is None:
            page.snack_bar = ft.SnackBar(ft.Text("Valori non validi"))
            page.snack_bar.open = True
            page.update()
            return
        lordo = imp * (1 + pct / 100)
        netto = imp / (1 + pct / 100)
        calc_result.value = f"Lordo: {lordo:.2f} €  |  Netto da lordo: {netto:.2f} €"
        page.update()

    calc_tab = ft.Column([
        ft.Row([importo_field, perc_field], spacing=8),
        ft.Row([
            ft.ElevatedButton("% IVA", on_click=calc_iva, bgcolor=ACCENT, color=TXT),
            ft.ElevatedButton("% Sconto", on_click=calc_sconto, bgcolor=ACCENT, color=TXT),
            ft.ElevatedButton("Netto/Lordo", on_click=calc_netto_lordo, bgcolor=ACCENT, color=TXT),
        ], spacing=8, wrap=True),
        calc_result,
    ], spacing=12, scroll=ft.ScrollMode.AUTO)

    tabs = ft.Tabs(
        selected_index=0,
        indicator_color=ACCENT,
        label_color=TXT,
        unselected_label_color=DIM,
        tabs=[
            ft.Tab(text="IBAN", content=ft.Container(iban_tab, padding=12)),
            ft.Tab(text="P.IVA", content=ft.Container(piva_tab, padding=12)),
            ft.Tab(text="Calcolatore", content=ft.Container(calc_tab, padding=12)),
        ],
        expand=True,
    )

    return ft.Column([
        ft.Text("Finanza", size=20, weight=ft.FontWeight.BOLD, color=ACCENT),
        tabs,
    ], spacing=8, expand=True)
