import os, sys, json, threading

try:
    from telegram import Update
    from telegram.ext import (
        Updater, CommandHandler, MessageHandler,
        Filters, CallbackContext
    )
except ImportError:
    print('ERRORE: python-telegram-bot non installato.', flush=True)
    print('Installa con: pip install python-telegram-bot==13.*', flush=True)
    sys.exit(1)

TOKEN     = os.environ.get('TELEGRAM_TOKEN', '').strip()
WHITELIST = [x.strip() for x in
             os.environ.get('TELEGRAM_WHITELIST', '').split(',')
             if x.strip()]

if not TOKEN:
    print('ERRORE: TELEGRAM_TOKEN non impostato.', flush=True)
    sys.exit(1)

# Mappa chat_id -> risposta in attesa (threading.Event + container)
pending = {}  # chat_id -> {'event': Event, 'reply': str}
pending_lock = threading.Lock()

def check_whitelist(update: Update) -> bool:
    if not WHITELIST:
        return True
    return str(update.effective_user.id) in WHITELIST

def send_query(chat_id: int, text: str) -> None:
    msg = json.dumps({'type': 'query', 'chat_id': chat_id,
                      'text': text}, ensure_ascii=False)
    print(msg, flush=True)

def wait_reply(chat_id: int, timeout: float = 120.0) -> str:
    evt = threading.Event()
    with pending_lock:
        pending[chat_id] = {'event': evt, 'reply': ''}
    evt.wait(timeout)
    with pending_lock:
        result = pending.pop(chat_id, {}).get('reply', '(timeout)')
    return result

def handle_message(update: Update, context: CallbackContext) -> None:
    if not check_whitelist(update):
        update.message.reply_text('Non autorizzato.')
        return
    chat_id = update.effective_chat.id
    text    = update.message.text or ''
    send_query(chat_id, text)
    reply = wait_reply(chat_id)
    update.message.reply_text(reply[:4096] if reply else '...')

def cmd_ask(update: Update, context: CallbackContext) -> None:
    if not check_whitelist(update):
        update.message.reply_text('Non autorizzato.')
        return
    chat_id = update.effective_chat.id
    text    = ' '.join(context.args) if context.args else ''
    if not text:
        update.message.reply_text('Uso: /ask <domanda>')
        return
    send_query(chat_id, text)
    reply = wait_reply(chat_id)
    update.message.reply_text(reply[:4096] if reply else '...')

def cmd_status(update: Update, context: CallbackContext) -> None:
    if not check_whitelist(update):
        update.message.reply_text('Non autorizzato.')
        return
    update.message.reply_text('Prismalux bot attivo. Invia un messaggio o usa /ask <testo>.')

def cmd_stop(update: Update, context: CallbackContext) -> None:
    if not check_whitelist(update):
        update.message.reply_text('Non autorizzato.')
        return
    update.message.reply_text('Bot in arresto...')
    context.dispatcher.stop()

# Thread di lettura stdin per ricevere risposte da Prismalux
def stdin_reader():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
            chat_id = obj.get('chat_id', 0)
            reply   = obj.get('reply', '')
            with pending_lock:
                entry = pending.get(chat_id)
            if entry:
                entry['reply'] = reply
                entry['event'].set()
        except Exception as e:
            print('stdin parse error: ' + str(e), flush=True)

t = threading.Thread(target=stdin_reader, daemon=True)
t.start()

updater = Updater(TOKEN)
dp = updater.dispatcher
dp.add_handler(CommandHandler('ask',    cmd_ask))
dp.add_handler(CommandHandler('status', cmd_status))
dp.add_handler(CommandHandler('stop',   cmd_stop))
dp.add_handler(MessageHandler(Filters.text & ~Filters.command, handle_message))

print(json.dumps({'type': 'ready'}), flush=True)
updater.start_polling(drop_pending_updates=True)
updater.idle()
