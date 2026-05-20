# TODO — Emoji indicatori modello nella lista LLM

## Obiettivo
Nella lista di selezione modello (ChatPage → ModelPickerDialog) mostrare:
- 📱  Modello scaricato sul telefono (LocalLlmClient, locale)
- 📶  Modello dal server Prismalux LAN (IP privato o porta 11500)
- ☁️   Modello da server remoto / cloud (IP pubblico)

## Stato
- [x] Definire helper `isLanHost(host, port)` in ChatPage
- [x] Aggiungere param `serverEmoji` a ModelPickerDialog
- [x] Usare Qt::UserRole per nome pulito (separato dal testo visualizzato)
- [x] Aggiornare `onModelBtnClicked` per passare emoji corretto
- [x] Aggiungere 📱 nella lista se ci sono modelli locali scaricati
