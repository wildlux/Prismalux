import math
import threading
from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.uix.slider import Slider
from kivy.uix.spinner import Spinner
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App

_AIM = (0.12, 0.13, 0.20, 1)
_TXT = (0.88, 0.88, 0.94, 1)
_DIM = (0.55, 0.55, 0.65, 1)
_ACC = (0.42, 0.39, 1.0, 1)


def _labeled_slider(label, lo, hi, val, fmt="{:.0f}"):
    col = BoxLayout(orientation="vertical", size_hint_y=None, height=dp(60), spacing=dp(2))
    row = BoxLayout(size_hint_y=None, height=dp(20))
    lbl = Label(text=label, font_size=dp(12), color=_DIM, halign="left", text_size=(None, None))
    val_lbl = Label(text=fmt.format(val), font_size=dp(12), color=_TXT,
                    size_hint_x=None, width=dp(50), halign="right", text_size=(None, None))
    row.add_widget(lbl)
    row.add_widget(val_lbl)
    sl = Slider(min=lo, max=hi, value=val, size_hint_y=None, height=dp(36))
    sl.bind(value=lambda _, v: setattr(val_lbl, "text", fmt.format(v)))
    col.add_widget(row)
    col.add_widget(sl)
    return col, sl


class MultimediaScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._playing = False
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="🎬 Multimedia — Sintetizzatore Toni", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))
        root.add_widget(Label(
            text="Genera toni via WebAudio API sul server. Sequenza: note separate da spazi (es: 440 494 523).",
            font_size=dp(11), color=_DIM, size_hint_y=None, height=dp(22),
            halign="left", text_size=(None, None)))

        c1, self._freq = _labeled_slider("Frequenza (Hz)", 20, 4000, 440)
        c2, self._dur = _labeled_slider("Durata (ms)", 50, 3000, 500)
        c3, self._vol = _labeled_slider("Volume", 0.0, 1.0, 0.5, "{:.2f}")
        root.add_widget(c1)
        root.add_widget(c2)
        root.add_widget(c3)

        wave_row = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(6))
        wave_row.add_widget(Label(text="Forma d'onda:", font_size=dp(12), color=_DIM,
                                  size_hint_x=None, width=dp(100)))
        self._wave = Spinner(text="sine", values=["sine", "square", "sawtooth", "triangle"],
                             font_size=dp(13), background_color=(0.18, 0.19, 0.28, 1), color=_TXT)
        wave_row.add_widget(self._wave)
        root.add_widget(wave_row)

        btn_row = BoxLayout(size_hint_y=None, height=dp(52), spacing=dp(6))
        btn_tone = Button(text="🔊 Suona tono", font_size=dp(14), bold=True,
                          background_color=_ACC, color=(1,1,1,1))
        btn_tone.bind(on_release=self._play_tone)
        btn_row.add_widget(btn_tone)
        root.add_widget(btn_row)

        root.add_widget(Label(text="Sequenza note:", font_size=dp(12), color=_DIM,
                              size_hint_y=None, height=dp(20),
                              halign="left", text_size=(None, None)))
        self._seq = TextInput(
            hint_text="440 494 523 587 659 (una frequenza per nota)",
            multiline=False, font_size=dp(13),
            size_hint_y=None, height=dp(44),
            background_color=_AIM, foreground_color=_TXT,
        )
        root.add_widget(self._seq)

        seq_row = BoxLayout(size_hint_y=None, height=dp(48), spacing=dp(6))
        btn_seq = Button(text="▶ Suona sequenza", font_size=dp(13),
                         background_color=(0.03, 0.59, 0.41, 1), color=(1,1,1,1))
        btn_seq.bind(on_release=self._play_seq)
        btn_stop = Button(text="⏹ Stop", font_size=dp(13), size_hint_x=None, width=dp(80),
                          background_color=(0.65, 0.15, 0.15, 1), color=(1,1,1,1))
        btn_stop.bind(on_release=self._stop)
        seq_row.add_widget(btn_seq)
        seq_row.add_widget(btn_stop)
        root.add_widget(seq_row)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(24))
        root.add_widget(self._status)

        self.add_widget(root)

    def _play_tone(self, *_):
        freq = self._freq.value
        dur = int(self._dur.value)
        vol = self._vol.value
        wave = self._wave.text
        self._status.text = f"▶ {freq:.0f} Hz {wave} {dur}ms"
        app = App.get_running_app()
        app.ai.chat_once(
            model=app.model,
            messages=[{"role": "user", "content": f"/tone freq={freq:.0f} dur={dur} vol={vol:.2f} wave={wave}"}],
            on_done=lambda r: Clock.schedule_once(lambda _: setattr(self._status, "text", "✅"), 0),
            on_error=lambda e: Clock.schedule_once(lambda _: setattr(self._status, "text", f"❌ {e}"), 0),
        )

    def _play_seq(self, *_):
        notes_raw = self._seq.text.strip()
        if not notes_raw:
            return
        try:
            notes = [float(x) for x in notes_raw.split()]
        except ValueError:
            self._status.text = "❌ Frequenze non valide"
            return
        dur = int(self._dur.value)
        vol = self._vol.value
        wave = self._wave.text
        self._playing = True
        self._status.text = f"▶ Sequenza {len(notes)} note"

        def _run():
            for i, f in enumerate(notes):
                if not self._playing:
                    break
                Clock.schedule_once(
                    lambda _, fi=f, idx=i: setattr(self._status, "text", f"♩ {fi:.0f} Hz ({idx+1}/{len(notes)})"), 0)
                import time
                time.sleep(dur / 1000.0 + 0.05)
            if self._playing:
                Clock.schedule_once(lambda _: setattr(self._status, "text", "✅ Sequenza completata"), 0)
            self._playing = False

        threading.Thread(target=_run, daemon=True).start()

    def _stop(self, *_):
        self._playing = False
        self._status.text = "⏹ Stop"
