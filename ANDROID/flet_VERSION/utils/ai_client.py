"""
Client HTTP asincrono per il server LAN Prismalux (porta 11500).
Usa threading per non bloccare la UI Flet.
"""
import json
import threading
import requests
from typing import Callable, Optional
from utils.log_bus import bus


class AiClient:
    def __init__(self, base_url: str = "http://192.168.1.100:11500", token: str = ""):
        self.base_url = base_url.rstrip("/")
        self.token = token
        self._stop_flag = threading.Event()

    def _headers(self, content_type: str = "application/json") -> dict:
        h = {"Content-Type": content_type}
        if self.token:
            h["Authorization"] = f"Bearer {self.token}"
        return h

    def fetch_models(self, on_done: Callable[[list], None],
                     on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.get(f"{self.base_url}/api/tags",
                                 headers=self._headers(), timeout=5)
                r.raise_for_status()
                models = [m["name"] for m in r.json().get("models", [])]
                bus.append(f"[Modelli] {len(models)} trovati da {self.base_url}", "sistema")
                on_done(models)
            except Exception as e:
                bus.append(f"[Modelli errore] {e}", "sistema")
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def chat_stream(self, model: str, messages: list,
                    on_token: Callable[[str], None],
                    on_done: Callable[[str], None],
                    on_error: Callable[[str], None]) -> None:
        self._stop_flag.clear()

        def _run():
            full = ""
            try:
                payload = {"model": model, "messages": messages, "stream": True}
                with requests.post(
                    f"{self.base_url}/api/chat",
                    headers=self._headers(),
                    json=payload,
                    stream=True,
                    timeout=120,
                ) as r:
                    r.raise_for_status()
                    for line in r.iter_lines():
                        if self._stop_flag.is_set():
                            break
                        if not line:
                            continue
                        try:
                            obj = json.loads(line)
                            tok = (obj.get("message") or {}).get("content") or obj.get("response", "")
                            if tok:
                                full += tok
                                on_token(tok)
                        except json.JSONDecodeError:
                            pass
                bus.append(f"[Chat] stream completato ({len(full)} char)", "ai")
                on_done(full)
            except Exception as e:
                bus.append(f"[Chat errore] {e}", "ai")
                on_error(str(e))

        threading.Thread(target=_run, daemon=True).start()

    def chat_once(self, model: str, messages: list,
                  on_done: Callable[[str], None],
                  on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                payload = {"model": model, "messages": messages, "stream": False}
                r = requests.post(
                    f"{self.base_url}/api/chat",
                    headers=self._headers(),
                    json=payload,
                    timeout=120,
                )
                r.raise_for_status()
                j = r.json()
                text = (j.get("message") or {}).get("content") or j.get("response", "")
                bus.append(f"[Pipeline] risposta ricevuta ({len(text)} char)", "ai")
                on_done(text)
            except Exception as e:
                bus.append(f"[Pipeline errore] {e}", "ai")
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def abort(self) -> None:
        self._stop_flag.set()

    def math(self, params: dict,
             on_done: Callable[[str], None],
             on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.post(
                    f"{self.base_url}/api/math",
                    headers=self._headers(),
                    json=params,
                    timeout=60,
                )
                r.raise_for_status()
                j = r.json()
                if "error" in j:
                    on_error(j["error"])
                else:
                    on_done(j.get("result", ""))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def codice_fiscale(self, nome: str, cognome: str, data: str,
                       sesso: str, comune: str,
                       on_done: Callable[[str, str], None],
                       on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                payload = {
                    "nome": nome, "cognome": cognome,
                    "data_nascita": data, "sesso": sesso, "comune": comune,
                }
                r = requests.post(
                    f"{self.base_url}/api/finanza/cf",
                    headers=self._headers(),
                    json=payload,
                    timeout=15,
                )
                r.raise_for_status()
                j = r.json()
                if "error" in j:
                    on_error(j["error"])
                else:
                    on_done(j.get("cf", ""), j.get("info", ""))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def lavoro(self, query: str,
               on_done: Callable[[list], None],
               on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                url = f"{self.base_url}/api/lavoro"
                if query:
                    url += f"?q={requests.utils.quote(query)}"
                r = requests.get(url, headers=self._headers(), timeout=15)
                r.raise_for_status()
                on_done(r.json() if isinstance(r.json(), list) else [])
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def sysinfo(self, on_done: Callable[[dict], None],
                on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.get(f"{self.base_url}/api/sysinfo",
                                 headers=self._headers(), timeout=5)
                r.raise_for_status()
                on_done(r.json())
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def rag(self, query: str, k: int = 5, *,
            on_done: Callable[[list], None],
            on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.post(f"{self.base_url}/api/rag",
                                  headers=self._headers(),
                                  json={"query": query, "k": k}, timeout=30)
                r.raise_for_status()
                j = r.json()
                on_done(j.get("results", []))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def graph_search(self, q: str,
                     on_done: Callable[[list], None],
                     on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.get(f"{self.base_url}/api/graph/search",
                                 headers=self._headers(),
                                 params={"q": q, "limit": 30}, timeout=10)
                r.raise_for_status()
                on_done(r.json().get("nodes", []))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def graph_dot(self,
                  on_done: Callable[[str], None],
                  on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.get(f"{self.base_url}/api/graph/dot",
                                 headers=self._headers(), timeout=10)
                r.raise_for_status()
                on_done(r.json().get("dot", ""))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def knowledge_load(self,
                       on_done: Callable[[str], None],
                       on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.get(f"{self.base_url}/knowledge",
                                 headers=self._headers(), timeout=10)
                r.raise_for_status()
                on_done(r.json().get("content", ""))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def knowledge_save(self, content: str, append: bool = False,
                       on_done: Callable[[str], None] = lambda _: None,
                       on_error: Callable[[str], None] = lambda _: None) -> None:
        def _run():
            try:
                r = requests.post(f"{self.base_url}/knowledge",
                                  headers=self._headers(),
                                  json={"content": content, "append": append}, timeout=10)
                r.raise_for_status()
                on_done(r.json().get("status", "ok"))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def tfr(self, stipendio: float, anni: int, inflazione: float, in_fondo: bool,
            on_done: Callable[[dict], None],
            on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.post(f"{self.base_url}/api/finanza/tfr",
                                  headers=self._headers(),
                                  json={"stipendio": stipendio, "anni": anni,
                                        "inflazione": inflazione, "in_fondo": in_fondo},
                                  timeout=10)
                r.raise_for_status()
                j = r.json()
                if "error" in j:
                    on_error(j["error"])
                else:
                    on_done(j)
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def repl(self, code: str,
             on_done: Callable[[str, str], None],
             on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.post(f"{self.base_url}/api/repl",
                                  headers=self._headers(),
                                  json={"code": code}, timeout=20)
                r.raise_for_status()
                j = r.json()
                on_done(j)
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def launch_app(self, app: str,
                   on_done: Callable[[str], None],
                   on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.post(f"{self.base_url}/api/launch",
                                  headers=self._headers(),
                                  json={"app": app}, timeout=10)
                r.raise_for_status()
                on_done(r.json().get("status", "ok"))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def graphviz(self, dot: str,
                 on_done: Callable[[bytes], None],
                 on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.post(f"{self.base_url}/api/graphviz",
                                  headers=self._headers(),
                                  json={"dot": dot}, timeout=30)
                r.raise_for_status()
                on_done(r.content)
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def git_cmd(self, cmd: str,
                on_done: Callable[[str], None],
                on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.post(f"{self.base_url}/api/git",
                                  headers=self._headers(),
                                  json={"cmd": cmd}, timeout=15)
                r.raise_for_status()
                j = r.json()
                if "error" in j:
                    on_error(j["error"])
                else:
                    on_done(j.get("output", ""))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def wiki(self, q: str, lang: str = "it", *,
             on_done: Callable[[dict], None],
             on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                r = requests.get(f"{self.base_url}/api/wiki",
                                 headers=self._headers(),
                                 params={"q": q, "lang": lang}, timeout=15)
                r.raise_for_status()
                j = r.json()
                if "error" in j:
                    on_error(j["error"])
                else:
                    on_done(j)
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def file_extract(self, file_path: str,
                     on_done: Callable[[str], None],
                     on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                with open(file_path, "rb") as f:
                    r = requests.post(
                        f"{self.base_url}/api/file",
                        headers={"Authorization": f"Bearer {self.token}"} if self.token else {},
                        files={"file": f}, timeout=60,
                    )
                r.raise_for_status()
                j = r.json()
                if "error" in j:
                    on_error(j["error"])
                else:
                    on_done(j.get("text", ""))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()

    def whisper(self, audio_path: str,
                on_done: Callable[[str], None],
                on_error: Callable[[str], None]) -> None:
        def _run():
            try:
                with open(audio_path, "rb") as f:
                    r = requests.post(
                        f"{self.base_url}/api/whisper",
                        headers={"Authorization": f"Bearer {self.token}"} if self.token else {},
                        files={"audio": f},
                        timeout=60,
                    )
                r.raise_for_status()
                j = r.json()
                if "error" in j:
                    on_error(j["error"])
                else:
                    on_done(j.get("text", ""))
            except Exception as e:
                on_error(str(e))
        threading.Thread(target=_run, daemon=True).start()
