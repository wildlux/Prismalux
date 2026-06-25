import flet as ft
from utils.ai_client import AiClient
from utils.log_bus import bus
import threading

ACCENT = "#6B5FFF"
BG = "#1A1D2E"
TXT = "#E0E0F0"
DIM = "#8888AA"
GREEN = "#44CC88"
RED = "#FF6666"
YELLOW = "#FFBB44"


def SistemaView(page: ft.Page, ai: AiClient) -> ft.Column:
    info_col = ft.Column(spacing=6, scroll=ft.ScrollMode.AUTO)
    status = ft.Text("", color=DIM, size=12, italic=True)

    def _stat_card(label: str, value: str, color: str = TXT) -> ft.Container:
        return ft.Container(
            content=ft.Row([
                ft.Text(label, color=DIM, size=13, expand=True),
                ft.Text(value, color=color, size=13, weight=ft.FontWeight.W_600),
            ]),
            bgcolor=BG, border_radius=8, padding=ft.padding.symmetric(horizontal=12, vertical=8),
            border=ft.border.all(1, "#2A2D40"),
        )

    def refresh(_=None):
        status.value = "Carico info sistema..."
        page.update()

        def done(data: dict):
            info_col.controls.clear()
            cpu = data.get("cpu_percent", 0)
            ram = data.get("ram_percent", 0)
            disk = data.get("disk_percent", 0)
            uptime = data.get("uptime", "N/A")
            hostname = data.get("hostname", "N/A")
            ip = data.get("ip", "N/A")
            processes = data.get("processes", 0)
            python_ver = data.get("python_version", "N/A")

            cpu_color = RED if cpu > 80 else YELLOW if cpu > 50 else GREEN
            ram_color = RED if ram > 85 else YELLOW if ram > 65 else GREEN
            disk_color = RED if disk > 90 else YELLOW if disk > 75 else TXT

            info_col.controls += [
                ft.Text("🖥️ Server PC", color=ACCENT, size=15, weight=ft.FontWeight.BOLD),
                _stat_card("Hostname", hostname),
                _stat_card("IP", ip, ACCENT),
                _stat_card("Uptime", uptime),
                ft.Divider(color="#2A2D40"),
                ft.Text("📊 Risorse", color=ACCENT, size=15, weight=ft.FontWeight.BOLD),
                _stat_card("CPU", f"{cpu:.1f}%", cpu_color),
                _stat_card("RAM", f"{ram:.1f}%", ram_color),
                _stat_card("Disco", f"{disk:.1f}%", disk_color),
                _stat_card("Processi", str(processes)),
                ft.Divider(color="#2A2D40"),
                ft.Text("🐍 Runtime", color=ACCENT, size=15, weight=ft.FontWeight.BOLD),
                _stat_card("Python", python_ver),
            ]

            gpu = data.get("gpu")
            if gpu:
                info_col.controls.append(
                    ft.Text("🎮 GPU", color=ACCENT, size=15, weight=ft.FontWeight.BOLD)
                )
                for k, v in gpu.items():
                    info_col.controls.append(_stat_card(k, str(v)))

            status.value = "Aggiornato"
            page.update()

        def err(e):
            info_col.controls.clear()
            info_col.controls.append(
                ft.Container(
                    content=ft.Column([
                        ft.Text("⚠️ Server non raggiungibile", color=YELLOW,
                                size=14, weight=ft.FontWeight.BOLD),
                        ft.Text(str(e), color=DIM, size=12),
                    ]),
                    bgcolor=BG, border_radius=8, padding=12,
                    border=ft.border.all(1, "#AA5500"),
                )
            )
            status.value = ""
            page.update()

        ai.sysinfo(done, err)

    refresh_btn = ft.ElevatedButton(
        "🔄 Aggiorna", bgcolor=ACCENT, color=TXT, on_click=refresh
    )

    # ── Log eventi — tab Sistema / AI ─────────────────────────────────────────
    log_sis_col = ft.Column(spacing=3, scroll=ft.ScrollMode.AUTO, expand=True)
    log_ai_col  = ft.Column(spacing=3, scroll=ft.ScrollMode.AUTO, expand=True)

    def _fill_logs():
        def _rows(cat: str) -> list:
            msgs = bus.get_all(cat)
            if not msgs:
                return [ft.Text("Nessun evento", color=DIM, italic=True, size=11)]
            return [ft.Text(m, color=DIM, size=11, selectable=True) for m in msgs]
        log_sis_col.controls = _rows("sistema")
        log_ai_col.controls  = _rows("ai")

    def _on_log(msg: str, category: str):
        target = log_sis_col if category == "sistema" else log_ai_col
        target.controls.append(ft.Text(msg, color=DIM, size=11, selectable=True))
        try:
            page.update()
        except Exception:
            pass

    bus.subscribe(_on_log)

    def clear_logs(_):
        bus.clear()
        _fill_logs()
        page.update()

    _fill_logs()

    clear_log_btn = ft.TextButton(
        "🗑 Svuota log",
        style=ft.ButtonStyle(color=DIM),
        on_click=clear_logs,
    )

    log_tabs = ft.Tabs(
        selected_index=0,
        animation_duration=200,
        expand=True,
        tabs=[
            ft.Tab(
                text="🖥 Sistema",
                content=ft.Container(
                    content=log_sis_col,
                    expand=True,
                    padding=8,
                    bgcolor=BG,
                    border_radius=8,
                ),
            ),
            ft.Tab(
                text="🤖 AI",
                content=ft.Container(
                    content=log_ai_col,
                    expand=True,
                    padding=8,
                    bgcolor=BG,
                    border_radius=8,
                ),
            ),
        ],
    )

    refresh()

    return ft.Column(
        [
            ft.Text("⚙️ Sistema", size=18, weight=ft.FontWeight.BOLD, color=TXT),
            ft.Row([refresh_btn, status]),
            ft.Container(content=info_col),
            ft.Divider(color="#2A2D40"),
            ft.Row([
                ft.Text("📋 Log eventi", color=ACCENT, size=15, weight=ft.FontWeight.BOLD),
                clear_log_btn,
            ], alignment=ft.MainAxisAlignment.SPACE_BETWEEN),
            ft.Container(content=log_tabs, expand=True),
        ],
        expand=True, spacing=10, scroll=ft.ScrollMode.AUTO,
    )
