import flet as ft
import threading

BG = "#1A1D2E"
ACCENT = "#6B5FFF"
TXT = "#E0E0F0"
DIM = "#8888AA"
MONO_BG = "#0D0F1C"

PRESETS = {
    "Hello World": 'print("Hello, World!")',
    "Fibonacci": (
        "def fib(n):\n"
        "    a, b = 0, 1\n"
        "    for _ in range(n):\n"
        "        print(a, end=' ')\n"
        "        a, b = b, a + b\n\n"
        "fib(10)"
    ),
    "Primes": (
        "def is_prime(n):\n"
        "    if n < 2: return False\n"
        "    return all(n % i != 0 for i in range(2, int(n**0.5)+1))\n\n"
        "primes = [x for x in range(2, 50) if is_prime(x)]\n"
        "print(primes)"
    ),
    "List comprehension": (
        "squares = [x**2 for x in range(1, 11)]\n"
        "evens   = [x for x in range(20) if x % 2 == 0]\n"
        "print('Squares:', squares)\n"
        "print('Evens:  ', evens)"
    ),
}


def ReplView(page: ft.Page, ai) -> ft.Column:
    model = getattr(ai, "default_model", "llama3")

    code_field = ft.TextField(
        value=PRESETS["Hello World"],
        color=TXT,
        bgcolor=MONO_BG,
        border_color=ACCENT,
        multiline=True,
        min_lines=10,
        max_lines=20,
        text_style=ft.TextStyle(font_family="monospace", size=13),
        expand=True,
    )

    output_field = ft.TextField(
        value="",
        color="#A8FF78",
        bgcolor=MONO_BG,
        border_color=DIM,
        multiline=True,
        min_lines=6,
        read_only=True,
        text_style=ft.TextStyle(font_family="monospace", size=13),
        expand=True,
    )

    loading = ft.ProgressRing(visible=False, color=ACCENT, width=20, height=20)

    preset_dd = ft.Dropdown(
        label="Preset",
        color=TXT,
        bgcolor=BG,
        border_color=ACCENT,
        options=[ft.dropdown.Option(k) for k in PRESETS],
        value="Hello World",
        expand=True,
    )

    def on_preset(e):
        code_field.value = PRESETS.get(preset_dd.value, "")
        output_field.value = ""
        page.update()

    preset_dd.on_change = on_preset

    def esegui(e):
        code = code_field.value.strip()
        if not code:
            page.snack_bar = ft.SnackBar(ft.Text("Nessun codice da eseguire"))
            page.snack_bar.open = True
            page.update()
            return
        loading.visible = True
        output_field.value = ""
        page.update()

        def _run():
            ai.chat_once(
                model,
                [{"role": "user", "content": (
                    f"Esegui questo codice Python e mostra l'output. "
                    f"Rispondi SOLO con l'output prodotto, senza spiegazioni:\n\n```python\n{code}\n```"
                )}],
                on_done=lambda r: _done(r),
                on_error=lambda err: _err(err),
            )

        def _done(r):
            loading.visible = False
            r = r.strip()
            if r.startswith("```"):
                lines = r.split("\n")
                r = "\n".join(lines[1:-1] if lines[-1].strip() == "```" else lines[1:])
            output_field.value = r
            page.update()

        def _err(err):
            loading.visible = False
            output_field.value = f"Errore: {err}"
            page.update()

        threading.Thread(target=_run, daemon=True).start()

    def pulisci(e):
        code_field.value = ""
        output_field.value = ""
        page.update()

    return ft.Column([
        ft.Text("REPL Python (AI)", size=20, weight=ft.FontWeight.BOLD, color=ACCENT),
        ft.Row([preset_dd], spacing=8),
        code_field,
        ft.Row([
            ft.ElevatedButton("▶ Esegui", on_click=esegui, bgcolor=ACCENT, color=TXT),
            ft.ElevatedButton("🗑 Pulisci", on_click=pulisci, bgcolor="#2A2D4E", color=TXT),
            loading,
        ], spacing=8),
        ft.Container(
            content=output_field,
            border_radius=8,
            bgcolor=MONO_BG,
            expand=True,
        ),
    ], spacing=10, expand=True)
