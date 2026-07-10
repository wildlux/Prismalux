"""MCPs/_shared — moduli di sicurezza condivisi tra i plugin MCP di Prismalux.

Estratto durante l'audit di sicurezza 2026-07-10 (vedi TODO.md OS-18): la
stessa logica di validazione URL/path era duplicata in 5 MCP diversi, con
copie leggermente divergenti — una delle quali (streamlink_mcp) più debole
delle altre proprio perché mai riallineata. Un solo punto da correggere
riduce sia il codice duplicato sia la superficie di attacco.
"""
