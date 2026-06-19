import flet as ft
import threading

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"


def TfrView(page: ft.Page, ai) -> ft.Column:
    model = getattr(ai, "default_model", "llama3")

    ral_field = ft.TextField(label="RAL annua (€)", color=TXT, bgcolor=BG, border_color=ACCENT, keyboard_type=ft.KeyboardType.NUMBER, expand=True)
    anni_field = ft.TextField(label="Anni di servizio", color=TXT, bgcolor=BG, border_color=ACCENT, keyboard_type=ft.KeyboardType.NUMBER, expand=True)
    inizio_field = ft.TextField(label="Data inizio (gg/mm/aaaa)", color=TXT, bgcolor=BG, border_color=ACCENT, expand=True)
    fine_field = ft.TextField(label="Data fine (gg/mm/aaaa)", color=TXT, bgcolor=BG, border_color=ACCENT, expand=True)

    card_quota = ft.Text("—", color=TXT, size=14)
    card_totale = ft.Text("—", color=ACCENT, size=16, weight=ft.FontWeight.BOLD)
    card_rival = ft.Text("—", color=TXT, size=13)
    ai_result = ft.Text("", color=TXT, size=13, selectable=True)
    loading = ft.ProgressRing(visible=False, color=ACCENT, width=20, height=20)

    def calcola(e):
        try:
            ral = float(ral_field.value.replace(",", "."))
            anni = float(anni_field.value.replace(",", "."))
        except Exception:
            page.snack_bar = ft.SnackBar(ft.Text("RAL e anni servizio richiesti"))
            page.snack_bar.open = True
            page.update()
            return
        quota = ral / 13.5
        totale = quota * anni
        rival_rate = 0.015 + 0.75 * 0.02
        rivalutazione = totale * rival_rate * anni / 2
        card_quota.value = f"Quota annua: {quota:,.2f} €"
        card_totale.value = f"Totale TFR: {totale:,.2f} €"
        card_rival.value = f"Rivalutazione ISTAT stimata: +{rivalutazione:,.2f} €  →  {totale + rivalutazione:,.2f} €"
        ai_result.value = ""
        page.update()

    def spiega_ai(e):
        try:
            ral = float(ral_field.value.replace(",", "."))
            anni = float(anni_field.value.replace(",", "."))
        except Exception:
            page.snack_bar = ft.SnackBar(ft.Text("Inserisci RAL e anni prima"))
            page.snack_bar.open = True
            page.update()
            return
        loading.visible = True
        ai_result.value = ""
        page.update()
        quota = ral / 13.5
        totale = quota * anni

        def _run():
            ai.chat_once(
                model,
                [{"role": "user", "content": (
                    f"Spiega in modo semplice il calcolo TFR per un lavoratore con RAL {ral:.0f}€, "
                    f"{anni:.0f} anni di servizio, quota annua {quota:.2f}€, totale {totale:.2f}€. "
                    f"Menziona la rivalutazione ISTAT e le opzioni di destinazione (fondo pensione vs INPS)."
                )}],
                on_done=lambda r: _done(r),
                on_error=lambda err: _err(err),
            )

        def _done(r):
            loading.visible = False
            ai_result.value = r
            page.update()

        def _err(err):
            loading.visible = False
            ai_result.value = f"Errore: {err}"
            page.update()

        threading.Thread(target=_run, daemon=True).start()

    def compila_ai(e):
        loading.visible = True
        ai_result.value = ""
        page.update()

        def _run():
            ai.chat_once(
                model,
                [{"role": "user", "content": (
                    "Genera dati fittizi di esempio per un calcolo TFR. "
                    "Rispondi SOLO in questo formato JSON:\n"
                    "{\"ral\": <numero>, \"anni\": <numero>, \"inizio\": \"gg/mm/aaaa\", \"fine\": \"gg/mm/aaaa\"}"
                )}],
                on_done=lambda r: _fill(r),
                on_error=lambda err: _err(err),
            )

        def _fill(r):
            import json, re
            loading.visible = False
            try:
                m = re.search(r'\{.*\}', r, re.DOTALL)
                data = json.loads(m.group(0)) if m else {}
                if "ral" in data:
                    ral_field.value = str(data["ral"])
                if "anni" in data:
                    anni_field.value = str(data["anni"])
                if "inizio" in data:
                    inizio_field.value = data["inizio"]
                if "fine" in data:
                    fine_field.value = data["fine"]
                ai_result.value = "Dati di esempio caricati."
            except Exception:
                ai_result.value = r
            page.update()

        def _err(err):
            loading.visible = False
            ai_result.value = f"Errore: {err}"
            page.update()

        threading.Thread(target=_run, daemon=True).start()

    result_card = ft.Container(
        content=ft.Column([
            card_quota,
            card_totale,
            card_rival,
        ], spacing=6),
        padding=14,
        border_radius=10,
        bgcolor="#12152A",
        border=ft.border.all(1, ACCENT),
    )

    return ft.Column([
        ft.Text("Calcolo TFR", size=20, weight=ft.FontWeight.BOLD, color=ACCENT),
        ft.Row([ral_field, anni_field], spacing=8),
        ft.Row([inizio_field, fine_field], spacing=8),
        ft.Row([
            ft.ElevatedButton("🧮 Calcola TFR", on_click=calcola, bgcolor=ACCENT, color=TXT),
            ft.ElevatedButton("🤖 Spiega con AI", on_click=spiega_ai, bgcolor="#2A2D4E", color=TXT),
            ft.ElevatedButton("📋 Compila da AI", on_click=compila_ai, bgcolor="#2A2D4E", color=TXT),
        ], spacing=8, wrap=True),
        result_card,
        ft.Row([loading]),
        ft.Container(
            content=ai_result,
            padding=10,
            border_radius=8,
            bgcolor="#12152A",
            width=float("inf"),
            visible=True,
        ),
    ], spacing=10, scroll=ft.ScrollMode.AUTO, expand=True)
