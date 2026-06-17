from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App

_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)
_ACC = (0.42, 0.39, 1.0, 1)
_PRP = (0.49, 0.23, 0.93, 1)

_TYPE_ICONS = {
    "entity": "🔷", "concept": "💡", "fact": "📌",
    "task": "✅", "result": "📊", "documento": "📄",
    "rag_chunk": "🔍",
}


class GrafoScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="🕸 Grafo Memory", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))

        row = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(6))
        self._q = TextInput(
            hint_text="Filtra nodi...", multiline=False, font_size=dp(13),
            background_color=_AIM, foreground_color=_TXT, size_hint_x=1,
        )
        btn_s = Button(text="🔍 Cerca", size_hint_x=None, width=dp(80),
                       font_size=dp(12), background_color=_ACC, color=(1,1,1,1))
        btn_s.bind(on_release=self._search)
        btn_d = Button(text="🗺 DOT", size_hint_x=None, width=dp(70),
                       font_size=dp(12), background_color=_PRP, color=(1,1,1,1))
        btn_d.bind(on_release=self._get_dot)
        row.add_widget(self._q)
        row.add_widget(btn_s)
        row.add_widget(btn_d)
        root.add_widget(row)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        self._scroll = ScrollView(size_hint_y=1)
        self._list = BoxLayout(orientation="vertical", spacing=dp(4),
                               size_hint_y=None, padding=[0, dp(4)])
        self._list.bind(minimum_height=self._list.setter("height"))
        self._scroll.add_widget(self._list)
        root.add_widget(self._scroll)

        self._dot_out = TextInput(
            hint_text="Output DOT apparirà qui...", readonly=True,
            font_size=dp(11), size_hint_y=None, height=dp(160),
            background_color=(0.06, 0.07, 0.10, 1),
            foreground_color=_TXT, display=False,
        )
        root.add_widget(self._dot_out)
        self._dot_out.opacity = 0
        self._dot_out.size_hint_y = None
        self._dot_out.height = 0

        self.add_widget(root)

    def _search(self, *_):
        q = self._q.text.strip()
        self._status.text = "⏳ Ricerca..."
        self._list.clear_widgets()
        App.get_running_app().ai.graph_search(
            q=q,
            on_done=self._on_nodes,
            on_error=self._on_err,
        )

    def _on_nodes(self, nodes: list):
        Clock.schedule_once(lambda _: self._show_nodes(nodes), 0)

    def _show_nodes(self, nodes):
        self._status.text = f"{len(nodes)} nodi"
        for n in nodes:
            typ = n.get("type", "entity")
            icon = _TYPE_ICONS.get(typ, "◾")
            label = n.get("label", "")
            content = n.get("content", "")[:120]
            imp = n.get("importance", 0)
            row = BoxLayout(size_hint_y=None, height=dp(52),
                            padding=[dp(8), dp(4)], spacing=dp(6))
            ico_lbl = Label(text=icon, font_size=dp(20),
                            size_hint_x=None, width=dp(30))
            info = BoxLayout(orientation="vertical")
            info.add_widget(Label(text=f"[b]{label}[/b]  [color=888888]{typ}  ★{imp:.1f}[/color]",
                                  markup=True, font_size=dp(13), color=_TXT,
                                  halign="left", text_size=(None, None),
                                  size_hint_y=None, height=dp(22)))
            info.add_widget(Label(text=content, font_size=dp(11), color=_DIM,
                                  halign="left", text_size=(None, None),
                                  size_hint_y=None, height=dp(22)))
            row.add_widget(ico_lbl)
            row.add_widget(info)
            self._list.add_widget(row)

    def _get_dot(self, *_):
        self._status.text = "⏳ Caricamento DOT..."
        App.get_running_app().ai.graph_dot(
            on_done=self._on_dot,
            on_error=self._on_err,
        )

    def _on_dot(self, dot: str):
        Clock.schedule_once(lambda _: self._show_dot(dot), 0)

    def _show_dot(self, dot):
        self._status.text = "DOT caricato"
        self._dot_out.text = dot
        self._dot_out.height = dp(160)
        self._dot_out.opacity = 1

    def _on_err(self, err):
        Clock.schedule_once(lambda _: setattr(self._status, "text", f"❌ {err}"), 0)
