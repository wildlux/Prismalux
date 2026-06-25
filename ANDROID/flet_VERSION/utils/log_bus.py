"""Bus di log condiviso — equivalente Python di LogBus Qt + LogCategory."""
from collections import deque
from typing import Callable
import datetime

_MAX = 150


class _LogBus:
    def __init__(self) -> None:
        self._logs: dict[str, deque] = {
            "sistema": deque(maxlen=_MAX),
            "ai":      deque(maxlen=_MAX),
        }
        self._cbs: list[Callable] = []

    def append(self, msg: str, category: str = "sistema") -> None:
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        entry = f"[{ts}] {msg}"
        self._logs.setdefault(category, deque(maxlen=_MAX)).append(entry)
        for cb in list(self._cbs):
            try:
                cb(entry, category)
            except Exception:
                pass

    def subscribe(self, cb: Callable) -> None:
        if cb not in self._cbs:
            self._cbs.append(cb)

    def unsubscribe(self, cb: Callable) -> None:
        if cb in self._cbs:
            self._cbs.remove(cb)

    def get_all(self, category: str) -> list[str]:
        return list(self._logs.get(category, []))

    def clear(self, category: str | None = None) -> None:
        if category:
            self._logs.get(category, deque()).clear()
        else:
            for q in self._logs.values():
                q.clear()


bus = _LogBus()
