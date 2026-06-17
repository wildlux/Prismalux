from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
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


class KnowledgeScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="📖 Knowledge Base", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))
        root.add_widget(Label(
            text="La knowledge base viene iniettata automaticamente nel contesto AI. Max 32 KB.",
            font_size=dp(12), color=_DIM, size_hint_y=None, height=dp(22),
            halign="left", text_size=(None, None)))

        btn_row = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(6))
        btn_load = Button(text="📥 Carica", font_size=dp(13),
                          background_color=_ACC, color=(1,1,1,1))
        btn_load.bind(on_release=self._load)
        btn_app = Button(text="➕ Aggiungi", font_size=dp(13),
                         background_color=(0.09, 0.64, 0.26, 1), color=(1,1,1,1))
        btn_app.bind(on_release=lambda _: self._save(append=True))
        btn_rep = Button(text="♻ Sostituisci", font_size=dp(13),
                         background_color=(0.86, 0.15, 0.15, 1), color=(1,1,1,1))
        btn_rep.bind(on_release=lambda _: self._save(append=False))
        btn_row.add_widget(btn_load)
        btn_row.add_widget(btn_app)
        btn_row.add_widget(btn_rep)
        root.add_widget(btn_row)

        self._status = Label(text="Premi Carica per leggere la knowledge base.",
                             font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        self._txt = TextInput(
            hint_text="La knowledge base è vuota. Premi 'Carica' per leggerla dal server.",
            font_size=dp(13), size_hint_y=1,
            background_color=_AIM, foreground_color=_TXT,
        )
        root.add_widget(self._txt)

        root.add_widget(Label(
            text="💡 Aggiungi = accoda in fondo. Sostituisci = sovrascrive tutto.",
            font_size=dp(11), color=_DIM, size_hint_y=None, height=dp(20),
            halign="left", text_size=(None, None)))

        self.add_widget(root)

    def _load(self, *_):
        self._status.text = "⏳ Caricamento..."
        App.get_running_app().ai.knowledge_load(
            on_done=self._on_loaded, on_error=self._on_err)

    def _on_loaded(self, content: str):
        Clock.schedule_once(lambda _: self._set_content(content), 0)

    def _set_content(self, content):
        self._txt.text = content
        self._status.text = f"Caricato — {len(content)} caratteri"

    def _save(self, append: bool):
        content = self._txt.text
        self._status.text = "⏳ Salvataggio..."
        App.get_running_app().ai.knowledge_save(
            content=content, append=append,
            on_done=lambda _: Clock.schedule_once(
                lambda __: setattr(self._status, "text", "✅ Salvato"), 0),
            on_error=self._on_err,
        )

    def _on_err(self, err):
        Clock.schedule_once(lambda _: setattr(self._status, "text", f"❌ {err}"), 0)
