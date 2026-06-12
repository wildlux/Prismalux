import os, sys, json, threading, asyncio

# APScheduler 3.x + pytz compat (Python 3.14 usa stdlib timezone.utc)
try:
    import pytz as _pytz
    import apscheduler.schedulers.base as _aps_base
    from datetime import timezone as _stdlib_tz
    _orig_aps_tz = _aps_base.astimezone
    def _compat_aps_tz(obj):
        if isinstance(obj, _stdlib_tz):
            secs = obj.utcoffset(None).total_seconds()
            return _pytz.FixedOffset(int(secs / 60)) if secs else _pytz.UTC
        return _orig_aps_tz(obj)
    _aps_base.astimezone = _compat_aps_tz
except Exception:
    pass

# event loop esplicito obbligatorio in Python 3.14
asyncio.set_event_loop(asyncio.new_event_loop())

try:
    from telegram import Update
    from telegram.ext import (
        Application, CommandHandler, MessageHandler,
        filters, ContextTypes
    )
except ImportError as e:
    print(json.dumps({'type':'error','msg':'Modulo mancante: ' + str(e)}), flush=True)
    sys.exit(1)

TOKEN     = os.environ.get('TELEGRAM_TOKEN', '').strip()
WHITELIST = [x.strip() for x in
             os.environ.get('TELEGRAM_WHITELIST', '').split(',')
             if x.strip()]

if not TOKEN:
    print(json.dumps({'type':'error','msg':'TELEGRAM_TOKEN non impostato.'}), flush=True)
    sys.exit(1)

if not WHITELIST:
    print(json.dumps({'type':'warning',
        'msg':'WHITELIST vuota: nessun utente autorizzato. Configura almeno un ID Telegram nelle impostazioni.'}),
        flush=True)

pending      = {}
pending_lock = threading.Lock()

def allowed(update: Update) -> bool:
    # Fail-closed: senza whitelist nessuno è autorizzato (Telegram è una rete pubblica).
    if not WHITELIST: return False
    return str(update.effective_user.id) in WHITELIST

async def _query_and_wait(cid: int, text: str, update: Update) -> None:
    print(json.dumps({'type':'query','chat_id':cid,'text':text}), flush=True)
    evt = threading.Event()
    with pending_lock:
        pending[cid] = {'event': evt, 'reply': ''}
    loop = asyncio.get_running_loop()
    await loop.run_in_executor(None, evt.wait, 120.0)
    with pending_lock:
        reply = pending.pop(cid, {}).get('reply', '(timeout)')
    await update.message.reply_text(reply[:4096] if reply else '...')

async def on_message(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    if not update.message: return
    if not allowed(update):
        await update.message.reply_text('Non autorizzato.')
        return
    cid  = update.effective_chat.id
    text = update.message.text or ''
    await _query_and_wait(cid, text, update)

async def on_ask(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    if not allowed(update):
        await update.message.reply_text('Non autorizzato.')
        return
    text = ' '.join(ctx.args) if ctx.args else ''
    if not text:
        await update.message.reply_text('Uso: /ask <domanda>')
        return
    cid = update.effective_chat.id
    await _query_and_wait(cid, text, update)

async def on_status(update: Update, ctx: ContextTypes.DEFAULT_TYPE):
    if not allowed(update):
        await update.message.reply_text('Non autorizzato.')
        return
    await update.message.reply_text(
        '🟢 Prismalux Bot attivo. Invia un messaggio o usa /ask <testo>.')

def stdin_loop():
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw: continue
        try:
            obj = json.loads(raw)
            cid   = obj.get('chat_id', 0)
            reply = obj.get('reply', '')
            with pending_lock:
                entry = pending.get(cid)
            if entry:
                entry['reply'] = reply
                entry['event'].set()
        except Exception as exc:
            print('stdin error: ' + str(exc), flush=True)

async def post_init(app: Application):
    threading.Thread(target=stdin_loop, daemon=True).start()
    print(json.dumps({'type': 'ready'}), flush=True)

app = (
    Application.builder()
    .token(TOKEN)
    .job_queue(None)
    .post_init(post_init)
    .build()
)
app.add_handler(CommandHandler('ask',    on_ask))
app.add_handler(CommandHandler('status', on_status))
app.add_handler(MessageHandler(filters.TEXT & ~filters.COMMAND, on_message))
app.run_polling(drop_pending_updates=True)
