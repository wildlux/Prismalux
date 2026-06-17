#!/usr/bin/env python3
"""
DevAgent MCP — Prismalux
Agente AI locale che modifica il codice di Prismalux in autonomia.

Comunicazione con Qt: IPC JSON su stdin/stdout.
  Input  (una riga JSON): {"task": "...", "model": "deepseek-coder:6.7b",
                           "project_root": "/path"}
  Output (righe JSON):    eventi di progresso + risultato finale

Grafo:
  START -> read_context -> generate_patch -> apply_patch -> compile
                                 ^  (errori, max 3 retry) <---------+
                                 |                                   |
                                 +--- (compile_ok == False) ---------+
                                 |
                          run_tests -> done
"""

import sys
import os
import json
import subprocess
import re
import shlex
import shutil
import tempfile
import urllib.request
import urllib.error
import datetime
import hashlib
from pathlib import Path
from typing import TypedDict, List, Optional, Dict

# Directory storia persistente
HISTORY_DIR = os.path.expanduser("~/.prismalux/devagent_history")

# ---------------------------------------------------------------------------
# Stato del grafo
# ---------------------------------------------------------------------------

class DevAgentState(TypedDict):
    task: str
    project_root: str
    model: str
    context_files: List[dict]   # [{"path": "...", "content": "..."}]
    patch: str                   # unified diff
    modified_files: List[dict]   # [{"path": "...", "content": "..."}]
    compile_output: str
    compile_ok: bool
    test_output: str
    retries: int
    error_msg: str
    done: bool


# ---------------------------------------------------------------------------
# emit
# ---------------------------------------------------------------------------

def emit(obj: dict) -> None:
    """Scrive un evento JSON su stdout (flush immediato)."""
    print(json.dumps(obj, ensure_ascii=False), flush=True)


def _log(msg: str) -> None:
    """Log di debug su stderr (non visibile al client Qt)."""
    print(f"[devagent] {msg}", file=sys.stderr, flush=True)


# ---------------------------------------------------------------------------
# Tool functions (helper, NON nodi del grafo)
# ---------------------------------------------------------------------------

def read_file(path: str) -> str:
    """Legge un file testuale e ne restituisce il contenuto."""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    except Exception as e:
        _log(f"read_file({path}): {e}")
        return ""


def write_file(path: str, content: str) -> None:
    """Scrive content in path, creando le directory intermedie se necessario."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)


def bash(cmd: str, timeout: int = 120) -> tuple:
    """
    Esegue un comando shell e restituisce (returncode, output_combinato).
    stdout e stderr sono unificati.
    """
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            capture_output=True,
            text=True,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
        )
        combined = result.stdout + result.stderr
        return result.returncode, combined
    except subprocess.TimeoutExpired:
        return -1, f"[TIMEOUT dopo {timeout}s] {cmd}"
    except Exception as e:
        return -1, str(e)


# Pattern di validazione per argomenti git — usati da _git_safe()
_RE_COMMIT  = re.compile(r'^[\w./^~@{}\[\]-]{1,200}$')   # hash, branch, tag, ref relativo
_RE_REMOTE  = re.compile(r'^[\w.\-]{1,100}$')              # "origin", "upstream"
_RE_BRANCH  = re.compile(r'^[\w./\-]{1,200}$')             # "master", "feature/x"
_RE_STASH   = re.compile(r'^stash@\{\d+\}$')               # "stash@{0}"
_RE_ROOT    = re.compile(r'^/')                             # path assoluto


def _git_safe(project_root: str, args: list, timeout: int = 30) -> tuple:
    """
    Esegue git con argomenti come lista (shell=False) per evitare injection.
    project_root deve essere un path assoluto esistente.
    """
    if not _RE_ROOT.match(project_root):
        return -1, f"project_root deve essere assoluto: {project_root!r}"
    if not os.path.isdir(project_root):
        return -1, f"project_root non esiste: {project_root!r}"
    try:
        result = subprocess.run(
            ["git", "-C", project_root] + args,
            shell=False,
            capture_output=True,
            text=True,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
        )
        combined = result.stdout + result.stderr
        return result.returncode, combined
    except subprocess.TimeoutExpired:
        return -1, f"[TIMEOUT dopo {timeout}s] git {' '.join(args)}"
    except Exception as e:
        return -1, str(e)


def search_code(query: str, root: str, max_files: int = 5) -> List[str]:
    """
    Cerca file rilevanti nel progetto con grep (parole chiave della task).
    Restituisce una lista di path assoluti (max max_files).
    """
    found: List[str] = []
    seen: set = set()

    # 1) Cerca file il cui nome contiene parole della query
    words = [w for w in re.split(r"\W+", query.lower()) if len(w) > 3]
    extensions = (".cpp", ".h", ".py", ".cmake", ".txt")

    for dirpath, _dirs, files in os.walk(root):
        # salta directory di build e git
        skip_dirs = {"build_gui", "build_tests", ".git", "__pycache__",
                     "node_modules", ".cache", "AppImage"}
        _dirs[:] = [d for d in _dirs if d not in skip_dirs]
        for fname in files:
            if not fname.endswith(extensions):
                continue
            fpath = os.path.join(dirpath, fname)
            fname_lower = fname.lower()
            for w in words:
                if w in fname_lower and fpath not in seen:
                    found.append(fpath)
                    seen.add(fpath)
                    break
        if len(found) >= max_files:
            break

    # 2) Se non basta, usa grep nel codice
    if len(found) < max_files:
        query_grep = " ".join(words[:3]) if words else query
        # grep: cerca file che contengono le parole chiave
        for word in words[:3]:
            if len(found) >= max_files:
                break
            cmd = (
                f"grep -rl --include='*.cpp' --include='*.h' --include='*.py' "
                f"-e {shlex.quote(word)} {shlex.quote(root)} 2>/dev/null | head -10"
            )
            _, output = bash(cmd, timeout=15)
            for line in output.strip().splitlines():
                line = line.strip()
                if line and line not in seen:
                    # salta build e git
                    skip = False
                    for skip_seg in ["build_gui", "build_tests", ".git",
                                     "__pycache__", "node_modules"]:
                        if skip_seg in line:
                            skip = True
                            break
                    if not skip:
                        found.append(line)
                        seen.add(line)
                        if len(found) >= max_files:
                            break

    return found[:max_files]


# ---------------------------------------------------------------------------
# Ollama HTTP call
# ---------------------------------------------------------------------------

OLLAMA_URL = "http://127.0.0.1:11434/api/chat"


def ollama_chat(model: str, messages: list, timeout: int = 180) -> str:
    """
    Chiama Ollama via HTTP diretto (no langchain).
    Restituisce il testo generato oppure lancia un'eccezione.
    """
    payload = json.dumps({
        "model": model,
        "messages": messages,
        "stream": False,
    }).encode("utf-8")

    req = urllib.request.Request(
        OLLAMA_URL,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            body = json.loads(resp.read())
        return body.get("message", {}).get("content", "")
    except urllib.error.URLError as e:
        raise RuntimeError(f"Ollama non raggiungibile ({OLLAMA_URL}): {e}") from e
    except Exception as e:
        raise RuntimeError(f"Errore Ollama: {e}") from e


# ---------------------------------------------------------------------------
# Parsing e applicazione del diff
# ---------------------------------------------------------------------------

def _extract_diff(text: str) -> str:
    """
    Estrae il blocco diff da una risposta LLM.
    Cerca prima ```diff ... ``` poi ``` ... ```.
    Se non trovato restituisce il testo intero (potrebbe già essere un diff).
    """
    # Cerca ```diff ... ```
    m = re.search(r"```diff\s*\n(.*?)```", text, re.DOTALL)
    if m:
        return m.group(1)
    # Cerca ``` ... ``` generico
    m = re.search(r"```\s*\n(.*?)```", text, re.DOTALL)
    if m:
        candidate = m.group(1)
        if candidate.startswith("---") or candidate.startswith("diff"):
            return candidate
    # Cerca il diff direttamente nel testo
    if text.strip().startswith(("---", "diff")):
        return text
    return text  # fallback: restituisce tutto


def _parse_unified_diff(diff_text: str) -> List[dict]:
    """
    Parsa un unified diff e restituisce una lista di operazioni:
    [{"path": str, "hunks": [{"old_start": int, "old_count": int,
                               "new_start": int, "new_count": int,
                               "lines": [str]}]}]

    Supporta il sottoinsieme standard usato da git diff / diff -u.
    """
    files: List[dict] = []
    current_file: Optional[dict] = None
    current_hunk: Optional[dict] = None

    for line in diff_text.splitlines():
        # Riga --- a/path  oppure --- /dev/null
        if line.startswith("--- "):
            pass  # il path lo prendiamo da +++
        # Riga +++ b/path  oppure +++ /dev/null
        elif line.startswith("+++ "):
            raw = line[4:].strip()
            # Rimuovi prefisso a/ o b/
            path = re.sub(r"^[ab]/", "", raw)
            if path == "/dev/null":
                path = None
            current_file = {"path": path, "hunks": []}
            files.append(current_file)
            current_hunk = None
        # Intestazione hunk: @@ -old_start,old_count +new_start,new_count @@
        elif line.startswith("@@") and current_file is not None:
            m = re.match(r"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", line)
            if m:
                current_hunk = {
                    "old_start": int(m.group(1)),
                    "old_count": int(m.group(2)) if m.group(2) is not None else 1,
                    "new_start": int(m.group(3)),
                    "new_count": int(m.group(4)) if m.group(4) is not None else 1,
                    "lines": [],
                }
                current_file["hunks"].append(current_hunk)
        # Linea di contenuto
        elif current_hunk is not None:
            if line.startswith(("+", "-", " ", "\\")):
                current_hunk["lines"].append(line)

    return files


def _apply_file_patch(original_lines: List[str], hunks: List[dict]) -> List[str]:
    """
    Applica una lista di hunk a original_lines (lista di stringhe CON newline).
    Restituisce le nuove righe.
    """
    result: List[str] = list(original_lines)
    # Offset accumulato per correggere gli indici dopo ogni hunk
    offset = 0

    for hunk in hunks:
        old_start = hunk["old_start"] - 1 + offset  # 0-indexed
        lines = hunk["lines"]

        # Verifica contesto (righe spazio) prima di applicare
        new_chunk: List[str] = []
        old_pos = old_start
        old_consumed = 0

        for hline in lines:
            if hline.startswith("\\"):
                continue  # "\ No newline at end of file"
            if hline.startswith(" "):
                # Riga di contesto: deve corrispondere
                new_chunk.append(hline[1:] + ("\n" if not hline[1:].endswith("\n") else ""))
                old_consumed += 1
            elif hline.startswith("-"):
                # Riga rimossa: salta nel risultato
                old_consumed += 1
            elif hline.startswith("+"):
                # Riga aggiunta
                new_chunk.append(hline[1:] + ("\n" if not hline[1:].endswith("\n") else ""))

        # Sostituisci il blocco old nel result
        old_end = old_start + old_consumed
        result[old_start:old_end] = new_chunk
        offset += len(new_chunk) - old_consumed

    return result


def _backup_fname(abs_path: str) -> str:
    """Nome file per backup temporaneo in /tmp (sep=_ singolo, usato solo per rollback)."""
    return _backup_fname(abs_path)


def _snap_fname(rel_path: str) -> str:
    """Nome file per snapshot permanente in HISTORY_DIR (sep=__ doppio, ricostruibile)."""
    return rel_path.replace(os.sep, "__")


def apply_diff(diff_text: str, project_root: str) -> tuple:
    """
    Applica un unified diff al filesystem.
    Restituisce (modified_paths: List[str], error: Optional[str]).
    Prima di modificare salva backup in /tmp/devagent_backup/.
    """
    backup_dir = os.path.join(tempfile.gettempdir(), "devagent_backup")
    os.makedirs(backup_dir, exist_ok=True)

    parsed = _parse_unified_diff(diff_text)
    if not parsed:
        return [], "Nessun file trovato nel diff"

    modified: List[str] = []

    for file_info in parsed:
        rel_path = file_info["path"]
        if rel_path is None:
            continue  # file eliminato — skip per sicurezza

        # Risolvi path assoluto
        if os.path.isabs(rel_path):
            abs_path = rel_path
        else:
            abs_path = os.path.join(project_root, rel_path)

        try:
            # Leggi contenuto originale (o vuoto se nuovo file)
            if os.path.exists(abs_path):
                with open(abs_path, "r", encoding="utf-8", errors="replace") as f:
                    original = f.readlines()
                # Backup
                backup_path = os.path.join(
                    backup_dir, _backup_fname(rel_path)
                )
                shutil.copy2(abs_path, backup_path)
            else:
                original = []

            # Applica hunk
            new_lines = _apply_file_patch(original, file_info["hunks"])

            # Scrivi risultato
            os.makedirs(os.path.dirname(abs_path), exist_ok=True)
            with open(abs_path, "w", encoding="utf-8") as f:
                f.writelines(new_lines)

            modified.append(abs_path)
            _log(f"Applicato patch a {abs_path}")

        except Exception as e:
            return modified, f"Errore applicando patch a {rel_path}: {e}"

    return modified, None


def rollback_from_backup(modified_paths: List[str]) -> None:
    """Ripristina i file originali dal backup in /tmp/devagent_backup/."""
    backup_dir = os.path.join(tempfile.gettempdir(), "devagent_backup")
    for abs_path in modified_paths:
        fname = _backup_fname(abs_path)
        backup_path = os.path.join(backup_dir, fname)
        if os.path.exists(backup_path):
            try:
                shutil.copy2(backup_path, abs_path)
                _log(f"Rollback: {abs_path}")
            except Exception as e:
                _log(f"Rollback fallito per {abs_path}: {e}")


# ---------------------------------------------------------------------------
# Storia persistente — snapshot per ogni esecuzione
# ---------------------------------------------------------------------------

def _snapshot_id(task: str) -> str:
    """Genera un ID univoco: timestamp + prime 3 parole del task."""
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    words = re.sub(r"[^a-z0-9 ]", "", task.lower()).split()[:3]
    slug = "_".join(words) if words else "task"
    return f"{ts}_{slug}"


def save_history_snapshot(task: str, model: str, project_root: str,
                           modified_paths: List[str], patch: str) -> str:
    """
    Salva uno snapshot dei file PRIMA delle modifiche in HISTORY_DIR/{id}/.
    Restituisce il backup_id (nome directory).
    """
    snap_id = _snapshot_id(task)
    snap_dir = os.path.join(HISTORY_DIR, snap_id)
    files_dir = os.path.join(snap_dir, "files")
    os.makedirs(files_dir, exist_ok=True)

    saved_files = []
    for abs_path in modified_paths:
        if not os.path.exists(abs_path):
            continue
        # Salva con path relativo al project_root come nome file
        try:
            rel = os.path.relpath(abs_path, project_root)
        except ValueError:
            rel = os.path.basename(abs_path)
        safe_name = _snap_fname(rel)
        dest = os.path.join(files_dir, safe_name)
        shutil.copy2(abs_path, dest)
        saved_files.append({"rel": rel, "abs": abs_path, "snap": dest})

    metadata = {
        "id":         snap_id,
        "task":       task,
        "model":      model,
        "timestamp":  datetime.datetime.now().isoformat(),
        "project_root": project_root,
        "files":      saved_files,
        "patch":      patch,
    }
    with open(os.path.join(snap_dir, "metadata.json"), "w") as f:
        json.dump(metadata, f, indent=2, ensure_ascii=False)

    _log(f"Snapshot salvato: {snap_id} ({len(saved_files)} file)")
    return snap_id


def list_history() -> List[dict]:
    """
    Restituisce la lista degli snapshot disponibili (dal più recente).
    """
    if not os.path.isdir(HISTORY_DIR):
        return []
    entries = []
    for name in sorted(os.listdir(HISTORY_DIR), reverse=True):
        meta_path = os.path.join(HISTORY_DIR, name, "metadata.json")
        if not os.path.exists(meta_path):
            continue
        try:
            with open(meta_path) as f:
                meta = json.load(f)
            entries.append({
                "id":        meta.get("id", name),
                "task":      meta.get("task", ""),
                "model":     meta.get("model", ""),
                "timestamp": meta.get("timestamp", ""),
                "n_files":   len(meta.get("files", [])),
            })
        except Exception:
            pass
    return entries


_RE_BACKUP_ID = re.compile(r'^[a-zA-Z0-9_\-]{1,80}$')

def restore_snapshot(backup_id: str) -> tuple:
    """
    Ripristina i file da uno snapshot.
    Restituisce (success: bool, message: str, restored_files: List[str]).
    """
    if not _RE_BACKUP_ID.match(backup_id):  # M-5: blocca path traversal via backup_id
        return False, f"backup_id non valido: {backup_id!r}", []
    snap_dir = os.path.join(HISTORY_DIR, backup_id)
    meta_path = os.path.join(snap_dir, "metadata.json")
    if not os.path.exists(meta_path):
        return False, f"Snapshot '{backup_id}' non trovato", []

    with open(meta_path) as f:
        meta = json.load(f)

    project_root = meta.get("project_root", "")
    if not project_root or not os.path.isabs(project_root):
        return False, "metadata.json: project_root mancante o non assoluto", []
    project_root = os.path.normpath(project_root)
    # snap_dir è costruito da codice controllato; normpath serve solo a normalizzare
    # il separatore finale per rendere startswith() affidabile su tutti i sistemi
    snap_dir_norm = os.path.normpath(snap_dir)

    restored = []
    errors = []
    for file_info in meta.get("files", []):
        snap_file = file_info.get("snap", "")
        abs_path  = file_info.get("abs", "")
        if not snap_file or not abs_path:
            continue
        # Impedisce lettura di file arbitrari tramite snap_file malevolo
        snap_file = os.path.normpath(snap_file)
        if not snap_file.startswith(snap_dir_norm + os.sep):
            errors.append(f"snap_file fuori dallo snapshot dir: {snap_file!r}")
            continue
        # Impedisce scrittura fuori dal project_root tramite abs_path malevolo
        abs_path = os.path.normpath(abs_path)
        if not abs_path.startswith(project_root + os.sep):
            errors.append(f"abs_path fuori dal project_root: {abs_path!r}")
            continue
        if not os.path.exists(snap_file):
            errors.append(f"File snapshot mancante: {snap_file}")
            continue
        try:
            os.makedirs(os.path.dirname(abs_path), exist_ok=True)
            shutil.copy2(snap_file, abs_path)
            restored.append(abs_path)
            _log(f"Ripristinato: {abs_path}")
        except Exception as e:
            errors.append(f"Errore ripristino {abs_path}: {e}")

    if errors:
        return False, f"{len(restored)} ripristinati, errori: {'; '.join(errors)}", restored
    return True, f"{len(restored)} file ripristinati dallo snapshot '{backup_id}'", restored


# ---------------------------------------------------------------------------
# Nodi del grafo
# ---------------------------------------------------------------------------

def node_read_context(state: DevAgentState) -> DevAgentState:
    """
    Cerca file rilevanti per la task e ne legge il contenuto.
    Max 5 file, troncati a 300 righe ciascuno per non saturare il contesto LLM.
    """
    MAX_LINES = 300

    relevant = search_code(state["task"], state["project_root"], max_files=5)
    _log(f"read_context: trovati {len(relevant)} file")

    context_files = []
    for fpath in relevant:
        content = read_file(fpath)
        lines = content.splitlines()
        if len(lines) > MAX_LINES:
            content = "\n".join(lines[:MAX_LINES]) + f"\n... [{len(lines) - MAX_LINES} righe troncate]"
        rel = os.path.relpath(fpath, state["project_root"])
        context_files.append({"path": rel, "content": content})

    emit({
        "event": "step",
        "node": "read_context",
        "files": [c["path"] for c in context_files],
    })

    state["context_files"] = context_files
    return state


def node_generate_patch(state: DevAgentState) -> DevAgentState:
    """
    Chiama il modello LLM per generare un unified diff che completa la task.
    Il prompt include i file di contesto e gli errori di compilazione (se presenti).
    """
    # Costruisci sezione file di contesto
    ctx_section = ""
    for cf in state["context_files"]:
        ctx_section += f"\n--- {cf['path']} ---\n{cf['content']}\n"

    # Sezione errori di compilazione (retry)
    compile_section = ""
    if state.get("compile_output") and not state.get("compile_ok", True):
        compile_section = (
            f"\nCOMPILATION ERRORS (retry {state['retries']}/3):\n"
            f"{state['compile_output']}\n"
            "Please fix the patch to resolve these errors.\n"
        )

    user_prompt = (
        f"TASK: {state['task']}\n"
        f"\nCONTEXT FILES:{ctx_section}"
        f"{compile_section}"
        "\nGenerate a unified diff patch to complete the task.\n"
        "Rules:\n"
        "  1. Output ONLY the diff block between ```diff and ```\n"
        "  2. Use standard unified diff format (diff -u / git diff)\n"
        "  3. Paths relative to project root (e.g. gui/pages/main_foo.cpp)\n"
        "  4. Do not explain — only the diff\n"
    )

    messages = [
        {
            "role": "system",
            "content": (
                "You are an expert C++/Qt6 and Python developer. "
                "You modify the Prismalux codebase by producing minimal, correct unified diffs. "
                "Always output valid unified diff format inside ```diff ... ``` fences."
            ),
        },
        {"role": "user", "content": user_prompt},
    ]

    _log(f"generate_patch: chiamata a Ollama model={state['model']}")
    raw = ollama_chat(state["model"], messages, timeout=180)

    patch = _extract_diff(raw)

    # Preview prime 5 righe
    preview_lines = patch.strip().splitlines()[:5]
    preview = "\n".join(preview_lines)

    emit({
        "event": "step",
        "node": "generate_patch",
        "preview": preview,
        "retries": state["retries"],
    })

    state["patch"] = patch
    return state


def node_apply_patch(state: DevAgentState) -> DevAgentState:
    """
    Applica il diff generato al filesystem.
    In caso di errore esegue rollback automatico.
    """
    if not state.get("patch", "").strip():
        state["error_msg"] = "Patch vuota — nessuna modifica da applicare"
        state["compile_ok"] = False
        emit({"event": "step", "node": "apply_patch", "files_modified": [],
              "error": state["error_msg"]})
        return state

    modified, err = apply_diff(state["patch"], state["project_root"])

    if err:
        _log(f"apply_patch errore: {err} — rollback")
        rollback_from_backup(modified)
        state["error_msg"] = err
        state["compile_ok"] = False
        state["modified_files"] = []
        emit({"event": "step", "node": "apply_patch", "files_modified": [],
              "error": err})
    else:
        state["error_msg"] = ""
        # Leggi il contenuto aggiornato dei file modificati
        updated = []
        for p in modified:
            rel = os.path.relpath(p, state["project_root"])
            updated.append({"path": rel, "content": read_file(p)})
        state["modified_files"] = updated

        # Salva snapshot persistente PRIMA delle modifiche (il backup era già fatto in apply_diff)
        # Recupera i file originali dal backup /tmp per salvarli nella storia
        tmp_backup = os.path.join(tempfile.gettempdir(), "devagent_backup")
        original_paths = []
        for p in modified:
            fname = _backup_fname(p)
            tmp_path = os.path.join(tmp_backup, fname)
            if os.path.exists(tmp_path):
                original_paths.append(tmp_path)

        # Copia i file originali in uno snapshot dedicato
        snap_id = _snapshot_id(state["task"])
        snap_dir = os.path.join(HISTORY_DIR, snap_id)
        files_dir = os.path.join(snap_dir, "files")
        os.makedirs(files_dir, exist_ok=True)
        saved_files = []
        for p in modified:
            fname = _backup_fname(p)
            tmp_path = os.path.join(tmp_backup, fname)
            if not os.path.exists(tmp_path):
                continue
            try:
                rel = os.path.relpath(p, state["project_root"])
            except ValueError:
                rel = os.path.basename(p)
            safe_name = _snap_fname(rel)
            dest = os.path.join(files_dir, safe_name)
            shutil.copy2(tmp_path, dest)
            saved_files.append({"rel": rel, "abs": p, "snap": dest})

        metadata = {
            "id":           snap_id,
            "task":         state["task"],
            "model":        state["model"],
            "timestamp":    datetime.datetime.now().isoformat(),
            "project_root": state["project_root"],
            "files":        saved_files,
            "patch":        state.get("patch", ""),
        }
        with open(os.path.join(snap_dir, "metadata.json"), "w") as f:
            json.dump(metadata, f, indent=2, ensure_ascii=False)

        emit({"event": "backup_created",
              "backup_id":  snap_id,
              "timestamp":  metadata["timestamp"],
              "task":       state["task"],
              "n_files":    len(saved_files)})

        emit({"event": "step", "node": "apply_patch",
              "files_modified": [os.path.relpath(p, state["project_root"])
                                  for p in modified]})

    return state


def node_compile(state: DevAgentState) -> DevAgentState:
    """
    Compila il progetto con cmake --build.
    Richiede che gui/build_gui/ esista già (cmake configurato).
    """
    build_dir = os.path.join(state["project_root"], "gui", "build_gui")
    _log(f"compile: cmake --build {build_dir} -j4")

    try:
        result = subprocess.run(
            ["cmake", "--build", build_dir, "-j4"],
            shell=False, capture_output=True, text=True, timeout=120,
            encoding="utf-8", errors="replace",
        )
        rc, output = result.returncode, result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        rc, output = -1, "[TIMEOUT] cmake --build"
    except Exception as e:
        rc, output = -1, str(e)
    compile_ok = (rc == 0)

    emit({
        "event": "compile_output",
        "output": output[-4000:] if len(output) > 4000 else output,  # troncato
        "ok": compile_ok,
        "retries": state["retries"],
    })

    state["compile_output"] = output
    state["compile_ok"] = compile_ok
    return state


def node_run_tests(state: DevAgentState) -> DevAgentState:
    """
    Esegue ctest nella directory di build dei test.
    Prova prima gui/build_gui (se contiene i test), poi Test/build_tests.
    """
    # Determina la directory di test
    test_dirs = [
        os.path.join(state["project_root"], "gui", "build_gui"),
        os.path.join(state["project_root"], "Test", "build_tests"),
    ]
    test_dir = None
    for td in test_dirs:
        if os.path.isdir(td):
            test_dir = td
            break

    if test_dir is None:
        state["test_output"] = "[WARN] Nessuna directory di build trovata per i test"
        emit({"event": "test_output", "output": state["test_output"]})
        return state

    _log(f"run_tests: ctest --test-dir {test_dir} -j4 --output-on-failure")
    try:
        result = subprocess.run(
            ["ctest", "--test-dir", test_dir, "-j4", "--output-on-failure"],
            shell=False, capture_output=True, text=True, timeout=120,
            encoding="utf-8", errors="replace",
        )
        _, output = result.returncode, result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        output = "[TIMEOUT] ctest"
    except Exception as e:
        output = str(e)

    emit({
        "event": "test_output",
        "output": output[-4000:] if len(output) > 4000 else output,
    })

    state["test_output"] = output
    return state


def node_done(state: DevAgentState, success: bool, message: str = "") -> DevAgentState:
    """
    Nodo finale: emette il risultato e segna lo stato come terminato.
    """
    n_files = len(state.get("modified_files", []))

    # Conta test passati (se disponibili)
    test_summary = ""
    if state.get("test_output"):
        m = re.search(r"(\d+)%\s+tests?\s+passed", state["test_output"])
        if m:
            test_summary = f" Tests: {m.group(0)}."
        else:
            m2 = re.search(r"(\d+/\d+)\s+Test", state["test_output"])
            if m2:
                test_summary = f" Tests: {m2.group(0)}."

    if not message:
        if success:
            message = (
                f"Task completato. {n_files} file modificati."
                f"{test_summary}"
            )
        else:
            message = (
                f"Task fallito dopo {state.get('retries', 0)} tentativi. "
                f"{state.get('error_msg', 'Errore sconosciuto')}"
            )

    emit({
        "event": "done",
        "success": success,
        "diff": state.get("patch", ""),
        "message": message,
        "files_modified": [f["path"] for f in state.get("modified_files", [])],
    })

    state["done"] = True
    return state


# ---------------------------------------------------------------------------
# Runner: loop semplice (fallback senza LangGraph)
# ---------------------------------------------------------------------------

def _run_simple_loop(state: DevAgentState) -> None:
    """
    Implementazione del grafo come loop Python semplice.
    Identico al grafo LangGraph ma senza dipendenza esterna.
    """
    MAX_RETRIES = 3

    # Fase 1: leggi contesto
    state = node_read_context(state)

    # Fase 2: genera patch + applica + compila (con retry)
    while state["retries"] <= MAX_RETRIES:
        if state["retries"] == MAX_RETRIES:
            node_done(state, success=False,
                      message=f"Superati {MAX_RETRIES} tentativi senza compilazione ok.")
            return

        state = node_generate_patch(state)

        if not state.get("patch", "").strip():
            state["retries"] += 1
            state["error_msg"] = "Patch vuota"
            state["compile_ok"] = False
            if state["retries"] >= MAX_RETRIES:
                node_done(state, success=False)
                return
            continue

        state = node_apply_patch(state)

        if state.get("error_msg"):
            state["retries"] += 1
            state["compile_ok"] = False
            if state["retries"] >= MAX_RETRIES:
                node_done(state, success=False)
                return
            continue

        state = node_compile(state)

        if state["compile_ok"]:
            break  # compilazione ok, esci dal loop

        # Compilazione fallita: incrementa retry e rigira
        state["retries"] += 1
        _log(f"Compilazione fallita (retry {state['retries']}/{MAX_RETRIES})")

    # Fase 3: run tests
    state = node_run_tests(state)

    # Fase 4: done
    node_done(state, success=True)


# ---------------------------------------------------------------------------
# Runner con LangGraph (opzionale)
# ---------------------------------------------------------------------------

def _build_langgraph(state_init: DevAgentState):
    """
    Costruisce e ritorna il grafo LangGraph compilato.
    Lancia ImportError se langgraph non è disponibile.
    """
    from langgraph.graph import StateGraph, END  # type: ignore

    MAX_RETRIES = 3

    def lg_read_context(state):
        return node_read_context(state)

    def lg_generate_patch(state):
        return node_generate_patch(state)

    def lg_apply_patch(state):
        return node_apply_patch(state)

    def lg_compile(state):
        return node_compile(state)

    def lg_run_tests(state):
        return node_run_tests(state)

    def lg_done_ok(state):
        return node_done(state, success=True)

    def lg_done_fail(state):
        return node_done(state, success=False)

    def route_after_compile(state):
        """Routing: dopo compile decide se retry, run_tests o done_fail."""
        if state["compile_ok"]:
            return "run_tests"
        if state["retries"] >= MAX_RETRIES:
            return "done_fail"
        # Incrementa retries e torna a generate_patch
        state["retries"] = state.get("retries", 0) + 1
        return "generate_patch"

    def route_after_apply(state):
        """Se apply_patch fallisce salta direttamente a compile (che fallirà)."""
        if state.get("error_msg"):
            state["retries"] = state.get("retries", 0) + 1
            if state["retries"] >= MAX_RETRIES:
                return "done_fail"
            return "generate_patch"
        return "compile"

    g = StateGraph(DevAgentState)
    g.add_node("read_context",    lg_read_context)
    g.add_node("generate_patch",  lg_generate_patch)
    g.add_node("apply_patch",     lg_apply_patch)
    g.add_node("compile",         lg_compile)
    g.add_node("run_tests",       lg_run_tests)
    g.add_node("done_ok",         lg_done_ok)
    g.add_node("done_fail",       lg_done_fail)

    g.set_entry_point("read_context")
    g.add_edge("read_context", "generate_patch")
    g.add_edge("generate_patch", "apply_patch")

    g.add_conditional_edges("apply_patch", route_after_apply, {
        "compile":        "compile",
        "generate_patch": "generate_patch",
        "done_fail":      "done_fail",
    })

    g.add_conditional_edges("compile", route_after_compile, {
        "run_tests":      "run_tests",
        "generate_patch": "generate_patch",
        "done_fail":      "done_fail",
    })

    g.add_edge("run_tests", "done_ok")
    g.add_edge("done_ok",   END)
    g.add_edge("done_fail", END)

    return g.compile()


# ---------------------------------------------------------------------------
# Funzione principale di esecuzione
# ---------------------------------------------------------------------------

def run_agent(task: str, model: str, project_root: str) -> None:
    """
    Punto di ingresso per una singola richiesta.
    Prova a usare LangGraph; se non disponibile usa il loop semplice.
    """
    project_root = os.path.expanduser(project_root)

    if not os.path.isdir(project_root):
        emit({"event": "error",
              "msg": f"project_root non trovato: {project_root}"})
        return

    state: DevAgentState = {
        "task":           task,
        "project_root":   project_root,
        "model":          model,
        "context_files":  [],
        "patch":          "",
        "modified_files": [],
        "compile_output": "",
        "compile_ok":     False,
        "test_output":    "",
        "retries":        0,
        "error_msg":      "",
        "done":           False,
    }

    emit({"event": "start", "task": task, "model": model,
          "project_root": project_root})

    # Prova LangGraph
    use_langgraph = False
    try:
        graph = _build_langgraph(state)
        use_langgraph = True
        _log("Usando LangGraph")
    except ImportError:
        _log("LangGraph non disponibile — uso loop semplice")
    except Exception as e:
        _log(f"Errore costruzione grafo LangGraph: {e} — uso loop semplice")

    try:
        if use_langgraph:
            for step_state in graph.stream(state):
                # LangGraph emette dict {node_name: state} per ogni step
                # I nodi emettono già i loro eventi via emit(), nulla da fare qui
                pass
        else:
            _run_simple_loop(state)
    except Exception as e:
        emit({"event": "error", "msg": str(e)})
        _log(f"Eccezione non gestita: {e}")


# ---------------------------------------------------------------------------
# Ripristino da Git / GitHub
# ---------------------------------------------------------------------------

def git_log(project_root: str, n: int = 20) -> List[dict]:
    """
    Restituisce gli ultimi N commit del branch corrente.
    Ogni entry: {hash, short_hash, author, date, subject}
    """
    code, out = _git_safe(
        project_root,
        ["log", "--oneline", f"--format=%H|%h|%an|%ad|%s", "--date=short", f"-n{n}"],
        timeout=10,
    )
    entries = []
    for line in out.strip().splitlines():
        parts = line.split("|", 4)
        if len(parts) < 5:
            continue
        entries.append({
            "hash":       parts[0],
            "short_hash": parts[1],
            "author":     parts[2],
            "date":       parts[3],
            "subject":    parts[4],
        })
    return entries


def git_restore_files(project_root: str, commit: str,
                       files: List[str]) -> tuple:
    """
    Ripristina file specifici a un dato commit con:
      git checkout {commit} -- {file1} {file2} ...
    Se `files` è vuoto ripristina TUTTO il worktree (reset hard).
    """
    if not _RE_COMMIT.match(commit):
        return False, f"commit non valido: {commit!r}"
    if not files:
        code, out = _git_safe(project_root, ["reset", "--hard", commit], timeout=30)
        if code != 0:
            return False, out
        return True, f"Reset --hard a {commit} completato."

    # Ripristina solo i file indicati (ogni file come argomento separato)
    code, out = _git_safe(
        project_root, ["checkout", commit, "--"] + list(files), timeout=20
    )
    if code != 0:
        return False, out
    return True, f"{len(files)} file ripristinati al commit {commit}."


def git_fetch_reset(project_root: str, remote: str = "origin",
                    branch: str = "master") -> tuple:
    """
    Esegue: git fetch {remote} && git reset --hard {remote}/{branch}
    Scarica lo stato da GitHub e sovrascrive il worktree locale.
    """
    if not _RE_REMOTE.match(remote):
        return False, f"remote non valido: {remote!r}"
    if not _RE_BRANCH.match(branch):
        return False, f"branch non valido: {branch!r}"
    code, out = _git_safe(project_root, ["fetch", remote], timeout=60)
    if code != 0:
        return False, f"git fetch fallito: {out}"

    ref = f"{remote}/{branch}"
    code2, out2 = _git_safe(project_root, ["reset", "--hard", ref], timeout=20)
    if code2 != 0:
        return False, f"git reset --hard fallito: {out2}"
    return True, f"Ripristinato a {ref}:\n{out2}"


def git_stash_push(project_root: str, message: str = "") -> tuple:
    """Salva le modifiche correnti in uno stash Git."""
    args = ["stash", "push"]
    if message:
        args += ["--message", message]
    code, out = _git_safe(project_root, args, timeout=15)
    if code != 0:
        return False, out
    return True, out.strip() or "Stash salvato."


def git_stash_list(project_root: str) -> List[dict]:
    """Lista gli stash git disponibili."""
    code, out = _git_safe(
        project_root, ["stash", "list", "--format=%gd|%s|%cr"], timeout=10
    )
    entries = []
    for line in out.strip().splitlines():
        parts = line.split("|", 2)
        if len(parts) >= 2:
            entries.append({
                "ref":     parts[0],
                "subject": parts[1] if len(parts) > 1 else "",
                "when":    parts[2] if len(parts) > 2 else "",
            })
    return entries


def git_stash_pop(project_root: str, ref: str = "stash@{0}") -> tuple:
    """Applica e rimuove uno stash git."""
    if not _RE_STASH.match(ref):
        return False, f"ref stash non valido: {ref!r}"
    code, out = _git_safe(project_root, ["stash", "pop", ref], timeout=15)
    if code != 0:
        return False, out
    return True, f"Stash applicato: {out.strip()}"


# ---------------------------------------------------------------------------
# Gestione stdin (protocollo IPC con Qt)
# ---------------------------------------------------------------------------

def main() -> None:
    """
    Loop principale: legge richieste JSON da stdin, una per riga.
    Formato input: {"task": "...", "model": "...", "project_root": "..."}
    """
    _log("DevAgent MCP avviato. In attesa di richieste su stdin...")

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError as e:
            emit({"event": "error", "msg": f"JSON non valido: {e}"})
            continue

        cmd = req.get("cmd", "").strip()

        # ── Comandi di storia ──────────────────────────────────────
        if cmd == "list_history":
            try:
                entries = list_history()
                emit({"event": "history_list", "entries": entries})
            except Exception as e:
                emit({"event": "error", "msg": f"list_history: {e}"})
            continue

        if cmd == "restore":
            backup_id = req.get("backup_id", "").strip()
            if not backup_id:
                emit({"event": "error", "msg": "Campo 'backup_id' mancante"})
                continue
            try:
                ok, msg, restored = restore_snapshot(backup_id)
                emit({"event": "restore_done",
                      "success":  ok,
                      "msg":      msg,
                      "restored": restored})
            except Exception as e:
                emit({"event": "error", "msg": f"restore: {e}"})
            continue

        # ── Comandi Git / GitHub ───────────────────────────────────
        if cmd == "git_log":
            pr = req.get("project_root", "").strip()
            n  = int(req.get("n", 20))
            try:
                entries = git_log(pr, n)
                emit({"event": "git_log", "entries": entries})
            except Exception as e:
                emit({"event": "error", "msg": f"git_log: {e}"})
            continue

        if cmd == "git_restore":
            pr     = req.get("project_root", "").strip()
            commit = req.get("commit", "HEAD").strip()
            files  = req.get("files", [])   # lista path relativi
            try:
                ok, msg = git_restore_files(pr, commit, files)
                emit({"event": "git_restore_done",
                      "success": ok, "msg": msg, "commit": commit})
            except Exception as e:
                emit({"event": "error", "msg": f"git_restore: {e}"})
            continue

        if cmd == "git_fetch_reset":
            pr     = req.get("project_root", "").strip()
            remote = req.get("remote", "origin").strip()
            branch = req.get("branch", "master").strip()
            try:
                ok, msg = git_fetch_reset(pr, remote, branch)
                emit({"event": "git_fetch_reset_done",
                      "success": ok, "msg": msg})
            except Exception as e:
                emit({"event": "error", "msg": f"git_fetch_reset: {e}"})
            continue

        if cmd == "git_stash_push":
            pr  = req.get("project_root", "").strip()
            msg = req.get("message", "devagent stash").strip()
            try:
                ok, out = git_stash_push(pr, msg)
                emit({"event": "git_stash_done",
                      "success": ok, "msg": out, "action": "push"})
            except Exception as e:
                emit({"event": "error", "msg": f"git_stash_push: {e}"})
            continue

        if cmd == "git_stash_list":
            pr = req.get("project_root", "").strip()
            try:
                entries = git_stash_list(pr)
                emit({"event": "git_stash_list", "entries": entries})
            except Exception as e:
                emit({"event": "error", "msg": f"git_stash_list: {e}"})
            continue

        if cmd == "git_stash_pop":
            pr  = req.get("project_root", "").strip()
            ref = req.get("ref", "stash@{0}").strip()
            try:
                ok, out = git_stash_pop(pr, ref)
                emit({"event": "git_stash_done",
                      "success": ok, "msg": out, "action": "pop"})
            except Exception as e:
                emit({"event": "error", "msg": f"git_stash_pop: {e}"})
            continue

        # ── Esecuzione agente ──────────────────────────────────────
        task         = req.get("task", "").strip()
        model        = req.get("model", "deepseek-coder:6.7b").strip()
        project_root = req.get("project_root", "").strip()

        if not task:
            emit({"event": "error", "msg": "Campo 'task' mancante o vuoto"})
            continue
        if not project_root:
            emit({"event": "error", "msg": "Campo 'project_root' mancante o vuoto"})
            continue

        try:
            run_agent(task, model, project_root)
        except Exception as e:
            emit({"event": "error", "msg": str(e)})


if __name__ == "__main__":
    main()
