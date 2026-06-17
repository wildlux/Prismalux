"""
Tab Voce: TTS (plyer/pyttsx3) + STT Whisper via server LAN.
Su Android usa plyer.tts e plyer.audio; su desktop usa pyttsx3 e sounddevice.
"""
from kivy.uix.screenmanager import Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.uix.textinput import TextInput
from kivy.uix.button import Button
from kivy.clock import Clock
from kivy.metrics import dp
from kivy.app import App
import threading
import os

# TTS: plyer preferito (Android), fallback pyttsx3 (desktop)
try:
    from plyer import tts as _plyer_tts
    _TTS_MODE = "plyer"
except Exception:
    try:
        import pyttsx3
        _TTS_MODE = "pyttsx3"
    except ImportError:
        _TTS_MODE = None

# STT: plyer.audio preferito (Android), fallback sounddevice (desktop)
try:
    from plyer import audio as _plyer_audio
    _STT_MODE = "plyer"
except Exception:
    try:
        import sounddevice as sd
        import soundfile as sf
        import numpy as np
        _STT_MODE = "sounddevice"
    except ImportError:
        _STT_MODE = None

_AUDIO_TMP = "/tmp/prismalux_rec.wav"
_AUDIO_TMP_ANDROID = "/sdcard/prismalux_rec.wav"


def _audio_tmp() -> str:
    if os.path.exists("/sdcard"):
        return _AUDIO_TMP_ANDROID
    return _AUDIO_TMP


class VoceScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._recording = False
        self._rec_thread = None
        self._rec_data = []
        self._tts_engine = None
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", padding=dp(12), spacing=dp(12))

        root.add_widget(Label(
            text="Voce — TTS & STT", font_size=dp(18), bold=True,
            size_hint_y=None, height=dp(36),
            color=(0.42, 0.39, 1.0, 1),
        ))

        # ── TTS ──
        root.add_widget(self._sep("Sintesi Vocale (TTS)"))

        self._tts_txt = TextInput(
            hint_text="Scrivi il testo da leggere...",
            size_hint_y=None, height=dp(90), font_size=dp(13),
            background_color=(0.09, 0.10, 0.14, 1),
            foreground_color=(0.88, 0.88, 0.94, 1),
        )
        root.add_widget(self._tts_txt)

        tts_row = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(8))
        btn_speak = Button(text="Parla", font_size=dp(14), bold=True,
                           background_color=(0.42, 0.39, 1.0, 1))
        btn_stop = Button(text="Stop", size_hint_x=None, width=dp(80),
                          font_size=dp(13), background_color=(0.7, 0.2, 0.2, 1))
        btn_speak.bind(on_release=self._tts_speak)
        btn_stop.bind(on_release=self._tts_stop)
        tts_row.add_widget(btn_speak)
        tts_row.add_widget(btn_stop)
        root.add_widget(tts_row)

        tts_info = f"TTS: {_TTS_MODE or 'non disponibile'}"
        self._tts_status = Label(text=tts_info, font_size=dp(12),
                                 size_hint_y=None, height=dp(22),
                                 color=(0.5, 0.5, 0.65, 1))
        root.add_widget(self._tts_status)

        # ── STT ──
        root.add_widget(self._sep("Riconoscimento Vocale (STT via Whisper)"))

        stt_row = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(8))
        self._rec_btn = Button(text="Registra", font_size=dp(14),
                               background_color=(0.7, 0.15, 0.15, 1))
        self._rec_btn.bind(on_release=self._toggle_rec)
        self._stt_status = Label(text=f"STT: {_STT_MODE or 'non disponibile'}",
                                 font_size=dp(12),
                                 size_hint_x=None, width=dp(200),
                                 color=(0.5, 0.5, 0.65, 1))
        stt_row.add_widget(self._rec_btn)
        stt_row.add_widget(self._stt_status)
        root.add_widget(stt_row)

        self._stt_out = TextInput(
            readonly=True, font_size=dp(13),
            size_hint_y=None, height=dp(100),
            hint_text="Testo riconosciuto...",
            background_color=(0.09, 0.10, 0.14, 1),
            foreground_color=(0.88, 0.88, 0.94, 1),
        )
        root.add_widget(self._stt_out)

        act_row = BoxLayout(size_hint_y=None, height=dp(40), spacing=dp(8))
        btn_send = Button(text="Invia in Chat", font_size=dp(12),
                          background_color=(0.42, 0.39, 1.0, 1))
        btn_copy = Button(text="Copia", font_size=dp(12), size_hint_x=None,
                          width=dp(80), background_color=(0.22, 0.23, 0.34, 1))
        btn_send.bind(on_release=self._send_to_chat)
        btn_copy.bind(on_release=self._copy_stt)
        act_row.add_widget(btn_send)
        act_row.add_widget(btn_copy)
        root.add_widget(act_row)

        root.add_widget(Label())
        self.add_widget(root)

    def _sep(self, title: str) -> Label:
        return Label(text=title, font_size=dp(13), bold=True,
                     size_hint_y=None, height=dp(28),
                     color=(0.65, 0.65, 0.78, 1), halign="left")

    # ── TTS ──
    def _tts_speak(self, *_):
        txt = self._tts_txt.text.strip()
        if not txt:
            self._tts_status.text = "Inserisci testo prima."
            return
        if not _TTS_MODE:
            self._tts_status.text = "TTS non disponibile."
            return
        self._tts_status.text = "Riproduzione..."

        def _run():
            try:
                if _TTS_MODE == "plyer":
                    _plyer_tts.speak(txt)
                else:
                    eng = pyttsx3.init()
                    eng.setProperty("rate", 160)
                    for v in eng.getProperty("voices"):
                        lang = v.languages[0] if v.languages else ""
                        if "it" in lang.lower():
                            eng.setProperty("voice", v.id)
                            break
                    self._tts_engine = eng
                    eng.say(txt)
                    eng.runAndWait()
                Clock.schedule_once(lambda _: setattr(self._tts_status, "text", "Fine."), 0)
            except Exception as e:
                Clock.schedule_once(lambda _: setattr(self._tts_status, "text", f"Errore: {e}"), 0)

        threading.Thread(target=_run, daemon=True).start()

    def _tts_stop(self, *_):
        if _TTS_MODE == "pyttsx3" and self._tts_engine:
            try:
                self._tts_engine.stop()
            except Exception:
                pass
        self._tts_status.text = "Fermato."

    # ── STT ──
    def _toggle_rec(self, *_):
        if self._recording:
            self._stop_rec()
        else:
            self._start_rec()

    def _start_rec(self):
        if not _STT_MODE:
            self._stt_status.text = "STT non disponibile."
            return
        self._rec_data = []
        self._recording = True
        self._rec_btn.text = "Ferma"
        self._rec_btn.background_color = (0.13, 0.5, 0.13, 1)
        self._stt_status.text = "Registrazione..."

        if _STT_MODE == "plyer":
            try:
                _plyer_audio.start()
            except Exception as e:
                Clock.schedule_once(
                    lambda _: setattr(self._stt_status, "text", f"Errore mic: {e}"), 0)
                self._recording = False
        else:
            def _run():
                samplerate = 16000
                with sd.InputStream(samplerate=samplerate, channels=1,
                                    dtype="int16") as stream:
                    while self._recording:
                        data, _ = stream.read(1024)
                        self._rec_data.append(data.copy())
            self._rec_thread = threading.Thread(target=_run, daemon=True)
            self._rec_thread.start()

    def _stop_rec(self):
        self._recording = False
        self._rec_btn.text = "Registra"
        self._rec_btn.background_color = (0.7, 0.15, 0.15, 1)
        self._stt_status.text = "Trascrizione..."

        def _save_and_send():
            try:
                path = _audio_tmp()
                if _STT_MODE == "plyer":
                    _plyer_audio.stop()
                    # plyer salva in un path interno; proviamo il path standard
                    plyer_path = getattr(_plyer_audio, "_file_path", None) or path
                    if os.path.exists(plyer_path):
                        path = plyer_path
                else:
                    import numpy as _np
                    audio_data = _np.concatenate(self._rec_data, axis=0)
                    sf.write(path, audio_data, 16000, subtype="PCM_16")

                App.get_running_app().ai.whisper(
                    audio_path=path,
                    on_done=self._on_stt_done,
                    on_error=self._on_stt_error,
                )
            except Exception as e:
                Clock.schedule_once(
                    lambda _: setattr(self._stt_status, "text", f"Errore: {e}"), 0)

        threading.Thread(target=_save_and_send, daemon=True).start()

    def _on_stt_done(self, text: str):
        Clock.schedule_once(lambda _: self._show_stt(text), 0)

    def _show_stt(self, text: str):
        self._stt_out.text = text
        self._stt_status.text = "Trascrizione completata."

    def _on_stt_error(self, err: str):
        Clock.schedule_once(
            lambda _: setattr(self._stt_status, "text", f"Errore: {err}"), 0)

    def _send_to_chat(self, *_):
        txt = self._stt_out.text.strip()
        if not txt:
            return
        app = App.get_running_app()
        chat = app.sm.get_screen("chat")
        chat._input.text += ("\n" if chat._input.text else "") + txt
        app.sm.current = "chat"
        app._goto("chat")

    def _copy_stt(self, *_):
        txt = self._stt_out.text.strip()
        if not txt:
            return
        try:
            from kivy.core.clipboard import Clipboard
            Clipboard.copy(txt)
            self._stt_status.text = "Copiato."
        except Exception:
            self._stt_status.text = "Copia non disponibile."
