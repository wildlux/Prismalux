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

_CMDS = [
    ("📋 Status", "status"),
    ("📜 Log", "log --oneline -20"),
    ("🔍 Diff", "diff"),
    ("📌 Diff staged", "diff --staged"),
    ("🌿 Branch", "branch -a"),
    ("📦 Stash", "stash list"),
    ("🏷 Tags", "tag --sort=-v:refname"),
    ("📊 Shortlog", "shortlog -sn --no-merges -10"),
]


class GitScreen(Screen):
    def __init__(self, **kw):
        super().__init__(**kw)
        self._build_ui()

    def _build_ui(self):
        root = BoxLayout(orientation="vertical", spacing=dp(6), padding=dp(8))

        root.add_widget(Label(text="🌿 Git — Strumenti repository", font_size=dp(16),
                              bold=True, color=_TXT, size_hint_y=None, height=dp(36),
                              halign="left", text_size=(None, None)))

        btns1 = BoxLayout(size_hint_y=None, height=dp(48), spacing=dp(4))
        btns2 = BoxLayout(size_hint_y=None, height=dp(48), spacing=dp(4))
        for i, (label, cmd) in enumerate(_CMDS):
            b = Button(text=label, font_size=dp(12),
                       background_color=(0.16, 0.17, 0.26, 1), color=_TXT)
            b.bind(on_release=lambda _, c=cmd: self._run(c))
            if i < 4:
                btns1.add_widget(b)
            else:
                btns2.add_widget(b)
        root.add_widget(btns1)
        root.add_widget(btns2)

        custom_row = BoxLayout(size_hint_y=None, height=dp(44), spacing=dp(6))
        self._custom = TextInput(
            hint_text="git …  (es: log --all --graph)",
            multiline=False, font_size=dp(13),
            background_color=_AIM, foreground_color=_TXT,
            font_name="RobotoMono-Regular",
        )
        btn_run = Button(text="▶", size_hint_x=None, width=dp(44),
                         font_size=dp(14), background_color=_ACC, color=(1,1,1,1))
        btn_run.bind(on_release=self._run_custom)
        custom_row.add_widget(self._custom)
        custom_row.add_widget(btn_run)
        root.add_widget(custom_row)

        self._status = Label(text="", font_size=dp(12), color=_DIM,
                             size_hint_y=None, height=dp(20))
        root.add_widget(self._status)

        self._out = TextInput(
            hint_text="Output git...", readonly=True,
            font_size=dp(12), size_hint_y=1,
            background_color=(0.06, 0.07, 0.10, 1), foreground_color=(0.46, 0.98, 0.46, 1),
            font_name="RobotoMono-Regular",
        )
        root.add_widget(self._out)
        self.add_widget(root)

    def _run(self, subcmd):
        self._status.text = f"⏳ git {subcmd}..."
        self._out.text = ""
        App.get_running_app().ai.git_cmd(
            cmd=subcmd,
            on_done=self._on_done,
            on_error=self._on_err,
        )

    def _run_custom(self, *_):
        cmd = self._custom.text.strip()
        if cmd.lower().startswith("git "):
            cmd = cmd[4:].strip()
        if not cmd:
            return
        self._run(cmd)

    def _on_done(self, result):
        Clock.schedule_once(lambda _: self._show(result), 0)

    def _show(self, result):
        if isinstance(result, dict):
            out = result.get("stdout", result.get("output", str(result)))
            err = result.get("stderr", "")
            self._out.text = out + ("\n---\n" + err if err else "")
        else:
            self._out.text = str(result)
        self._status.text = "✅"

    def _on_err(self, err):
        Clock.schedule_once(lambda _: setattr(self._status, "text", f"❌ {err}"), 0)
