from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App

_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)
_ACC = (0.42, 0.39, 1.0, 1)

_APPS = [
    ("Blender", "🎨", "blender"),
    ("FreeCAD", "⚙️", "freecad"),
    ("Anki", "🃏", "anki"),
    ("KiCAD", "🔌", "kicad"),
    ("Godot", "🎮", "godot"),
    ("OBS", "🎥", "obs"),
    ("OpenCode", "🤖", "opencode"),
    ("LibreOffice", "📝", "libreoffice"),
    ("GIMP", "🖼", "gimp"),
    ("Inkscape", "✒️", "inkscape"),
    ("VLC", "▶️", "vlc"),
    ("Firefox", "🌐", "firefox"),
]


class AppScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="🚀 App Controller", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))
        root.add_widget(Label(
            text="Lancia applicazioni tramite il server Qt. Il server deve avere i comandi nel PATH.",
            font_size=dp(11), color=_DIM, size_hint_y=None, height=dp(22),
            halign="left", text_size=(None, None)))

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(24))
        root.add_widget(self._status)

        sv = ScrollView(size_hint_y=1)
        grid = GridLayout(cols=3, size_hint_y=None, spacing=dp(8), padding=dp(4))
        grid.bind(minimum_height=grid.setter("height"))

        for name, icon, cmd in _APPS:
            btn = Button(
                text=f"{icon}\n{name}", font_size=dp(13),
                size_hint_y=None, height=dp(70),
                background_color=(0.16, 0.17, 0.26, 1), color=_TXT,
            )
            btn.bind(on_release=lambda b, c=cmd, n=name: self._launch(c, n))
            grid.add_widget(btn)

        sv.add_widget(grid)
        root.add_widget(sv)
        self.add_widget(root)

    def _launch(self, cmd, name):
        self._status.text = f"⏳ Avvio {name}..."
        App.get_running_app().ai.launch_app(
            app=cmd,
            on_done=lambda r: Clock.schedule_once(
                lambda _: setattr(self._status, "text", f"✅ {name} avviato"), 0),
            on_error=lambda e: Clock.schedule_once(
                lambda _: setattr(self._status, "text", f"❌ {e}"), 0),
        )
