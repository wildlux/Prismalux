#!/usr/bin/env python3
"""
datetime_mcp — Data/ora locale, UTC/GMT, offset timezone e conversioni.
Protocollo: JSON-RPC 2.0 su stdio (MCP standard Prismalux).

Tool disponibili:
  get_datetime      — data/ora locale + UTC + timezone + Unix timestamp
  convert_timezone  — converte un istante ISO 8601 in un altro timezone
"""

import sys
import json
import re
import datetime
import time

try:
    from zoneinfo import ZoneInfo, available_timezones
    _HAS_ZONEINFO = True
except ImportError:
    _HAS_ZONEINFO = False


# ---------------------------------------------------------------------------

def _now_info() -> dict:
    """Restituisce un dict con data/ora attuale completa."""
    local = datetime.datetime.now().astimezone()
    utc   = datetime.datetime.now(datetime.timezone.utc)
    offset_sec = int(local.utcoffset().total_seconds())
    offset_h   = abs(offset_sec) // 3600
    offset_m   = (abs(offset_sec) % 3600) // 60
    sign       = "+" if offset_sec >= 0 else "-"
    return {
        "local":              local.strftime("%Y-%m-%d %H:%M:%S"),
        "local_iso":          local.isoformat(),
        "utc":                utc.strftime("%Y-%m-%d %H:%M:%S"),
        "utc_iso":            utc.isoformat(),
        "timezone":           local.tzname(),
        "utc_offset":         f"UTC{sign}{offset_h:02d}:{offset_m:02d}",
        "utc_offset_seconds": offset_sec,
        "unix":               int(time.time()),
        "day_of_week":        local.strftime("%A"),
        "week_number":        int(local.strftime("%V")),
    }


def _format_datetime_text(info: dict) -> str:
    return (
        f"Data/ora locale: {info['local']}\n"
        f"UTC/GMT:         {info['utc']}\n"
        f"Timezone:        {info['timezone']} ({info['utc_offset']})\n"
        f"Giorno:          {info['day_of_week']}, settimana {info['week_number']}\n"
        f"Unix timestamp:  {info['unix']}"
    )


def _tool_get_datetime(_args: dict) -> str:
    return _format_datetime_text(_now_info())


def _tool_convert_timezone(args: dict) -> str:
    """Converte un timestamp ISO 8601 (o 'now') in un timezone IANA."""
    if not _HAS_ZONEINFO:
        return "Errore: zoneinfo non disponibile (Python < 3.9). Aggiornare Python."

    target_tz = args.get("timezone", "").strip()
    if not target_tz:
        return "Errore: parametro 'timezone' obbligatorio (es. 'America/New_York', 'Asia/Tokyo')."

    try:
        tz = ZoneInfo(target_tz)
    except Exception:
        return f"Timezone '{target_tz}' non riconosciuto. Usa nomi IANA (es. 'Europe/Rome')."

    iso_input = args.get("datetime", "now").strip()
    try:
        if iso_input.lower() == "now":
            dt_src = datetime.datetime.now(datetime.timezone.utc)
        else:
            dt_src = datetime.datetime.fromisoformat(iso_input)
            if dt_src.tzinfo is None:
                # assumi UTC se nessun timezone fornito
                dt_src = dt_src.replace(tzinfo=datetime.timezone.utc)
    except Exception as e:
        return f"Formato datetime non valido: {e}. Usa ISO 8601 (es. '2026-06-16T14:00:00+02:00')."

    dt_target = dt_src.astimezone(tz)
    offset_sec = int(dt_target.utcoffset().total_seconds())
    offset_h   = abs(offset_sec) // 3600
    offset_m   = (abs(offset_sec) % 3600) // 60
    sign       = "+" if offset_sec >= 0 else "-"

    return (
        f"Sorgente:  {dt_src.isoformat()}\n"
        f"Timezone:  {target_tz} (UTC{sign}{offset_h:02d}:{offset_m:02d})\n"
        f"Risultato: {dt_target.strftime('%Y-%m-%d %H:%M:%S')} ({dt_target.tzname()})"
    )


# ---------------------------------------------------------------------------
# date_calc — calcolo durate e conversione unità di tempo

_UNIT_SEC: dict[str, float] = {
    "secondo": 1, "secondi": 1, "sec": 1,
    "minuto": 60, "minuti": 60, "min": 60,
    "ora": 3600, "ore": 3600,
    "giorno": 86400, "giorni": 86400,
    "settimana": 7*86400, "settimane": 7*86400,
    "mese": 30.4375*86400, "mesi": 30.4375*86400,
    "anno": 365.2422*86400, "anni": 365.2422*86400,
}

_NAMED_PERIODS: list[tuple[str, float]] = [
    ("anno gregoriano", 365.2425*86400),
    ("anno tropicale",  365.2422*86400),
    ("anno tropico",    365.2422*86400),
    ("anno bisestile",  366*86400),
    ("anno solare",     365.2422*86400),
    ("anno civile",     365*86400),
    ("anno comune",     365*86400),
    ("semestre",        365.2422*86400/2),
    ("trimestre",       365.2422*86400/4),
]

_MESI_IT: dict[str, int] = {
    "gennaio": 1, "febbraio": 2, "marzo": 3, "aprile": 4,
    "maggio": 5, "giugno": 6, "luglio": 7, "agosto": 8,
    "settembre": 9, "ottobre": 10, "novembre": 11, "dicembre": 12,
    "gen": 1, "feb": 2, "mar": 3, "apr": 4,
    "mag": 5, "giu": 6, "lug": 7, "ago": 8,
    "set": 9, "ott": 10, "nov": 11, "dic": 12,
}


def _fmt(v: float, unit: str) -> str:
    rounded = round(v)
    if abs(v - rounded) < 0.005 and v >= 1:
        return f"{rounded:,} {unit}".replace(",", ".")
    if abs(v) < 0.01:
        return f"{v:.6f} {unit}"
    return f"{v:.2f} {unit}"


def _next_date(month: int, day: int, today: datetime.date) -> datetime.date:
    yr = today.year
    try:
        d = datetime.date(yr, month, day)
    except ValueError:
        d = datetime.date(yr, month, 28)  # fallback ultimo giorno valido del mese
    if d <= today:
        try:
            d = datetime.date(yr + 1, month, day)
        except ValueError:
            d = datetime.date(yr + 1, month, 28)
    return d


def _tool_date_calc(args: dict) -> str:
    q_raw = args.get("query", "").strip()
    q     = q_raw.lower()
    now   = datetime.datetime.now()
    today = now.date()

    # ── Pattern 1: "quanti X mancano [prep] TARGET" ──────────────────
    m1 = re.search(r"quanti\s+(\w+)\s+mancano\s+(.+)", q)
    if m1:
        unit_word = m1.group(1)
        rest      = re.sub(
            r"^(?:a|al|alla|allo|agli|alle|all'?)(?:\s*evento\s+di|\s*appuntamento\s+di|\s*scadenza\s+di)?\s*",
            "", m1.group(2).strip()
        )
        u_sec = _UNIT_SEC.get(unit_word)
        if not u_sec:
            return f"Unità non riconosciuta: {unit_word}"

        target: datetime.date | None = None
        # "giorno N [di mese]"
        mg = re.search(r"giorno\s+(\d{1,2})(?:\s+(?:di\s+)?(\w+))?", rest)
        if mg:
            day = int(mg.group(1))
            mon = today.month
            if mg.group(2):
                mon = _MESI_IT.get(mg.group(2).lower(), mon)
            try:
                target = datetime.date(today.year, mon, day)
            except ValueError:
                target = None
            if target is None or target <= today:
                if not mg.group(2):
                    # prossimo mese
                    if today.month == 12:
                        target = datetime.date(today.year+1, 1, day)
                    else:
                        target = datetime.date(today.year, today.month+1, day)
                else:
                    try:
                        target = datetime.date(today.year+1, mon, day)
                    except ValueError:
                        target = None

        # Solo nome mese ("dicembre", "15 dicembre")
        if target is None:
            for nome, num in _MESI_IT.items():
                if nome in rest:
                    dm = re.search(r"(\d{1,2})\s+\w+", rest)
                    day = int(dm.group(1)) if dm else 1
                    target = _next_date(num, max(1, day), today)
                    break

        if target is None:
            return f"Non riconosco la data: {rest}"

        target_dt = datetime.datetime.combine(target, datetime.time(0, 0))
        diff_sec  = (target_dt - now).total_seconds()
        if diff_sec < 0:
            return (f"La data {target.strftime('%d %B %Y')} è già passata "
                    f"({_fmt(-diff_sec / u_sec, unit_word)} fa).")

        if unit_word in ("mesi", "mese"):
            # Mesi calendario esatti
            yr_n, mo_n = today.year, today.month
            yr_t, mo_t = target.year, target.month
            months = (yr_t - yr_n) * 12 + (mo_t - mo_n)
            mid    = datetime.date(yr_n + (mo_n + months - 1) // 12,
                                   (mo_n + months - 1) % 12 + 1, today.day)
            rem_d  = (target - mid).days
            result = f"{months} mesi"
            if rem_d > 0:
                result += f" e {rem_d} giorni"
        elif unit_word in ("giorni", "giorno"):
            result = _fmt((target - today).days, "giorni")
        elif unit_word in ("settimane", "settimana"):
            days = (target - today).days
            result = _fmt(days / 7, "settimane") + f" ({days} giorni)"
        else:
            result = _fmt(diff_sec / u_sec, unit_word)

        return (f"Da oggi ({today.strftime('%d %B %Y')}) "
                f"al {target.strftime('%d %B %Y')}: **{result}**")

    # ── Pattern 2: "converti[mi] [N] X in Y" ─────────────────────────
    m2 = re.search(r"converti(?:mi)?\s+([\d.,]+)?\s*(\w+)\s+in\s+(\w+)", q)
    if m2:
        n   = float(m2.group(1).replace(",", ".")) if m2.group(1) else 1.0
        s1  = _UNIT_SEC.get(m2.group(2))
        s2  = _UNIT_SEC.get(m2.group(3))
        if not s1 or not s2:
            return "Unità non riconosciuta."
        return f"{_fmt(n, m2.group(2))} = **{_fmt(n * s1 / s2, m2.group(3))}**"

    # ── Pattern 3: "quanti X sono [un/una] [periodo/N Y]" ────────────
    m3 = re.search(r"quanti\s+(\w+)\s+(?:sono|ci\s+sono\s+in|equivalgono\s+a)\s+(?:un[ao]?\s+)?(.+?)[\?!\.\s]*$", q)
    if m3:
        unit_dest = m3.group(1)
        per       = m3.group(2).strip()
        u_dest    = _UNIT_SEC.get(unit_dest)
        if not u_dest:
            return f"Unità non riconosciuta: {unit_dest}"

        total_sec = 0.0
        for name, sec in _NAMED_PERIODS:
            if name in per:
                total_sec = sec
                break
        if not total_sec:
            mn = re.search(r"([\d.,]+)\s+(\w+)", per)
            if mn:
                n2 = float(mn.group(1).replace(",", "."))
                s2 = _UNIT_SEC.get(mn.group(2), 0.0)
                total_sec = n2 * s2
        if not total_sec:
            total_sec = _UNIT_SEC.get(per, 0.0)

        if not total_sec:
            return f"Non riconosco il periodo: {per}"
        # "3 giorni" già contiene la quantità — non aggiungere "1" davanti
        prefix = per if re.match(r"^\d", per) else f"1 {per}"
        return f"{prefix} = **{_fmt(total_sec / u_dest, unit_dest)}**"

    return (
        "Esempi d'uso:\n"
        "• quanti mesi mancano a dicembre\n"
        "• quanti giorni mancano al giorno 15\n"
        "• quanti minuti mancano all'evento di giorno 5\n"
        "• convertimi 120 minuti in ore\n"
        "• converti minuti in giorni\n"
        "• quanti secondi sono un anno solare\n"
        "• quanti minuti sono 3 giorni"
    )


# ---------------------------------------------------------------------------
# Dispatcher JSON-RPC 2.0

_TOOLS = [
    {
        "name": "get_datetime",
        "description": (
            "Restituisce data/ora locale del server Prismalux, equivalente UTC/GMT, "
            "nome timezone, offset UTC in secondi, Unix timestamp e numero settimana. "
            "Utile per calcoli temporali, scheduling e riferimenti temporali nelle risposte AI."
        ),
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "date_calc",
        "description": (
            "Calcola durate e converte unità di tempo. Supporta: "
            "(1) tempo mancante a una data ('quanti mesi mancano a dicembre', 'quanti minuti mancano al giorno 5'); "
            "(2) conversione con numero ('convertimi 120 minuti in ore', 'converti minuti in giorni'); "
            "(3) definizione di un periodo ('quanti secondi sono un anno solare', 'quanti minuti sono 3 giorni'). "
            "Usa query in linguaggio naturale italiano."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "Domanda in italiano (es. 'quanti mesi mancano a dicembre')",
                }
            },
            "required": ["query"],
        },
    },
    {
        "name": "convert_timezone",
        "description": (
            "Converte un istante (ISO 8601 o 'now') in un timezone IANA specifico. "
            "Esempi timezone: 'America/New_York', 'Asia/Tokyo', 'Europe/London', 'UTC'."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "timezone": {
                    "type": "string",
                    "description": "Nome timezone IANA di destinazione (es. 'America/New_York')",
                },
                "datetime": {
                    "type": "string",
                    "description": "Istante ISO 8601 da convertire, oppure 'now' (default: 'now')",
                },
            },
            "required": ["timezone"],
        },
    },
]

_TOOL_FNS = {
    "get_datetime":     _tool_get_datetime,
    "date_calc":        _tool_date_calc,
    "convert_timezone": _tool_convert_timezone,
}


def _handle(req: dict) -> dict:
    method = req.get("method", "")

    if method == "initialize":
        return {
            "capabilities":  {},
            "serverInfo":    {"name": "datetime_mcp", "version": "1.1"},
        }

    if method == "tools/list":
        return {"tools": _TOOLS}

    if method == "tools/call":
        params   = req.get("params") or {}
        name     = params.get("name", "")
        args     = params.get("arguments") or {}
        fn       = _TOOL_FNS.get(name)
        if fn is None:
            return {"content": [{"type": "text", "text": f"Tool sconosciuto: {name}"}]}
        try:
            text = fn(args)
        except Exception as e:
            text = f"Errore interno: {e}"
        return {"content": [{"type": "text", "text": text}]}

    # notifications/initialized e altri metodi: risposta vuota
    return {}


def main():
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            req    = json.loads(raw)
            result = _handle(req)
            resp   = {"jsonrpc": "2.0", "id": req.get("id"), "result": result}
        except Exception as e:
            resp = {"jsonrpc": "2.0", "id": None,
                    "error": {"code": -32603, "message": str(e)}}
        sys.stdout.write(json.dumps(resp) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
