from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.uix.filechooser import FileChooserListView
from kivy.uix.popup import Popup
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App

_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)
_ACC = (0.42, 0.39, 1.0, 1)


class FileAiScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._filepath = None
        self._extracted = ""
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="📁 File AI — Estrai testo", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))
        root.add_widget(Label(
            text="Carica PDF, DOCX o testo. Il server Qt estrae il testo e puoi inviarlo in Chat.",
            font_size=dp(12), color=_DIM, size_hint_y=None, height=dp(22),
            halign="left", text_size=(None, None)))

        btn_row = BoxLayout(size_hint_y=None, height=dp(48), spacing=dp(6))
        self._pick_btn = Button(text="📂 Scegli file", font_size=dp(13),
                                background_color=_ACC, color=(1,1,1,1))
        self._pick_btn.bind(on_release=self._open_picker)
        self._extract_btn = Button(text="🔍 Estrai testo", font_size=dp(13),
                                   background_color=(0.03, 0.59, 0.41, 1),
                                   color=(1,1,1,1), disabled=True)
        self._extract_btn.bind(on_release=self._extract)
        self._chat_btn = Button(text="💬 Invia in Chat", font_size=dp(13),
                                background_color=(0.18, 0.19, 0.28, 1),
                                color=_TXT, disabled=True)
        self._chat_btn.bind(on_release=self._send_chat)
        btn_row.add_widget(self._pick_btn)
        btn_row.add_widget(self._extract_btn)
        btn_row.add_widget(self._chat_btn)
        root.add_widget(btn_row)

        self._file_lbl = Label(text="Nessun file selezionato", font_size=dp(12),
                               color=_DIM, size_hint_y=None, height=dp(20),
                               halign="left", text_size=(None, None))
        root.add_widget(self._file_lbl)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        self._out = TextInput(
            hint_text="Il testo estratto apparirà qui...", readonly=True,
            font_size=dp(13), size_hint_y=1,
            background_color=_AIM, foreground_color=_TXT,
        )
        root.add_widget(self._out)
        self.add_widget(root)

    def _open_picker(self, *_):
        content = BoxLayout(orientation="vertical")
        fc = FileChooserListView(
            path="/", filters=["*.pdf", "*.docx", "*.txt", "*.md",
                               "*.csv", "*.json", "*.py", "*.js"],
            size_hint_y=1,
        )
        btn_sel = Button(text="Seleziona", size_hint_y=None, height=dp(48),
                         background_color=_ACC, color=(1,1,1,1))
        content.add_widget(fc)
        content.add_widget(btn_sel)
        popup = Popup(title="Scegli file", content=content,
                      size_hint=(0.95, 0.85))

        def on_sel(*_):
            if fc.selection:
                self._filepath = fc.selection[0]
                name = self._filepath.split("/")[-1]
                self._file_lbl.text = f"📎 {name}"
                self._extract_btn.disabled = False
            popup.dismiss()

        btn_sel.bind(on_release=on_sel)
        popup.open()

    def _extract(self, *_):
        if not self._filepath:
            return
        self._status.text = "⏳ Estrazione in corso..."
        self._extract_btn.disabled = True
        App.get_running_app().ai.file_extract(
            file_path=self._filepath,
            on_done=self._on_done,
            on_error=self._on_err,
        )

    def _on_done(self, text: str):
        self._extracted = text
        Clock.schedule_once(lambda _: self._show(text), 0)

    def _show(self, text):
        self._out.text = text
        self._status.text = f"Estratti {len(text)} caratteri"
        self._extract_btn.disabled = False
        self._chat_btn.disabled = False

    def _on_err(self, err):
        Clock.schedule_once(lambda _: self._show_err(err), 0)

    def _show_err(self, err):
        self._status.text = f"❌ {err}"
        self._extract_btn.disabled = False

    def _send_chat(self, *_):
        if not self._extracted:
            return
        app = App.get_running_app()
        chat = app.sm.get_screen("chat")
        fname = (self._filepath or "file").split("/")[-1]
        chat._input.text = f"[{fname}]:\n{self._extracted[:3000]}"
        app.sm.current = "chat"
