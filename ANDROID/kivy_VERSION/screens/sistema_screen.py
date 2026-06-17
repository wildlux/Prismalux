from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App

_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)
_ACC = (0.42, 0.39, 1.0, 1)


def _card(title):
    outer = BoxLayout(orientation="vertical", size_hint_y=None, height=dp(90),
                      padding=dp(10), spacing=dp(4))
    from kivy.graphics import Color, RoundedRectangle
    with outer.canvas.before:
        Color(0.14, 0.15, 0.22, 1)
        outer._bg = RoundedRectangle(pos=outer.pos, size=outer.size, radius=[dp(8)])
    outer.bind(pos=lambda w, _: setattr(w._bg, "pos", w.pos),
               size=lambda w, _: setattr(w._bg, "size", w.size))
    title_lbl = Label(text=title, font_size=dp(12), color=_DIM,
                      size_hint_y=None, height=dp(18),
                      halign="left", text_size=(None, None))
    val_lbl = Label(text="—", font_size=dp(22), bold=True, color=_TXT,
                    size_hint_y=None, height=dp(32),
                    halign="left", text_size=(None, None))
    sub_lbl = Label(text="", font_size=dp(11), color=_DIM,
                    size_hint_y=None, height=dp(18),
                    halign="left", text_size=(None, None))
    outer.add_widget(title_lbl)
    outer.add_widget(val_lbl)
    outer.add_widget(sub_lbl)
    return outer, val_lbl, sub_lbl


class SistemaScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._refresh_ev = None
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        hdr = BoxLayout(size_hint_y=None, height=dp(44))
        hdr.add_widget(Label(text="📊 Sistema — Monitoraggio", font_size=dp(16),
                             bold=True, color=_TXT, halign="left", text_size=(None, None)))
        btn_r = Button(text="↻", size_hint_x=None, width=dp(44),
                       font_size=dp(18), background_color=_ACC, color=(1,1,1,1))
        btn_r.bind(on_release=lambda _: self._fetch())
        hdr.add_widget(btn_r)
        root.add_widget(hdr)

        grid = GridLayout(cols=2, size_hint_y=None, spacing=dp(8))
        grid.bind(minimum_height=grid.setter("height"))

        c1, self._cpu_val, self._cpu_sub = _card("🖥  CPU")
        c2, self._ram_val, self._ram_sub = _card("🧠 RAM")
        c3, self._disk_val, self._disk_sub = _card("💾 Disco")
        c4, self._up_val, self._up_sub = _card("⏱  Uptime")
        grid.add_widget(c1)
        grid.add_widget(c2)
        grid.add_widget(c3)
        grid.add_widget(c4)
        root.add_widget(grid)

        c5, self._host_val, self._host_sub = _card("🌐 Host / IP")
        c5.size_hint_x = 1
        root.add_widget(c5)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        root.add_widget(BoxLayout())
        self.add_widget(root)

    def on_enter(self, *_):
        self._fetch()
        self._refresh_ev = Clock.schedule_interval(lambda _: self._fetch(), 3)

    def on_leave(self, *_):
        if self._refresh_ev:
            self._refresh_ev.cancel()
            self._refresh_ev = None

    def _fetch(self):
        App.get_running_app().ai.sysinfo(
            on_done=self._on_done,
            on_error=lambda e: Clock.schedule_once(
                lambda _: setattr(self._status, "text", f"❌ {e}"), 0),
        )

    def _on_done(self, result):
        Clock.schedule_once(lambda _: self._update(result), 0)

    def _update(self, data):
        if isinstance(data, str):
            import json
            try:
                data = json.loads(data)
            except Exception:
                self._status.text = data[:120]
                return

        cpu = data.get("cpu_percent", data.get("cpu", "—"))
        ram = data.get("ram_percent", data.get("memory_percent", "—"))
        ram_used = data.get("ram_used", data.get("memory_used", ""))
        ram_tot = data.get("ram_total", data.get("memory_total", ""))
        disk = data.get("disk_percent", data.get("disk", "—"))
        disk_used = data.get("disk_used", "")
        up = data.get("uptime", "—")
        host = data.get("hostname", data.get("host", "—"))
        ip = data.get("ip", data.get("local_ip", ""))

        self._cpu_val.text = f"{cpu}%"
        self._ram_val.text = f"{ram}%"
        self._ram_sub.text = f"{ram_used} / {ram_tot}"
        self._disk_val.text = f"{disk}%"
        self._disk_sub.text = str(disk_used)
        self._up_val.text = str(up)
        self._host_val.text = str(host)
        self._host_sub.text = str(ip)
        self._status.text = "Aggiornato"
