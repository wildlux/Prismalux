import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/app_provider.dart';
import '../providers/theme_provider.dart';
import '../theme/plx_colors.dart';
import '../models/chat_message.dart';
import '../widgets/chat_bubble.dart';

class ChatScreen extends StatefulWidget {
  const ChatScreen({super.key});

  @override
  State<ChatScreen> createState() => _ChatScreenState();
}

class _ChatScreenState extends State<ChatScreen> {
  final _controller = TextEditingController();
  final _scroll = ScrollController();
  String _systemPrompt = '';
  bool _showSysPrompt = false;
  bool _drawerOpen = false;

  @override
  void dispose() {
    _controller.dispose();
    _scroll.dispose();
    super.dispose();
  }

  void _scrollToBottom() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scroll.hasClients) {
        _scroll.animateTo(
          _scroll.position.maxScrollExtent,
          duration: const Duration(milliseconds: 180),
          curve: Curves.easeOut,
        );
      }
    });
  }

  Future<void> _send() async {
    final text = _controller.text.trim();
    if (text.isEmpty) return;
    _controller.clear();
    await context
        .read<AppProvider>()
        .sendMessage(text, systemPrompt: _systemPrompt);
    _scrollToBottom();
  }

  @override
  Widget build(BuildContext context) {
    final plx = context.watch<ThemeProvider>().current;
    final prov = context.watch<AppProvider>();

    WidgetsBinding.instance.addPostFrameCallback((_) => _scrollToBottom());

    return Column(
      children: [
        // toolbar modello
        _ModelBar(plx: plx, prov: prov, onSysToggle: () {
          setState(() => _showSysPrompt = !_showSysPrompt);
        }, onClear: () {
          showDialog(
            context: context,
            builder: (_) => AlertDialog(
              title: const Text('Cancella chat'),
              content: const Text('Eliminare tutta la conversazione?'),
              actions: [
                TextButton(
                    onPressed: () => Navigator.pop(context),
                    child: const Text('Annulla')),
                TextButton(
                  onPressed: () {
                    prov.clearChat();
                    Navigator.pop(context);
                  },
                  child: Text('Elimina',
                      style: TextStyle(color: plx.acc)),
                ),
              ],
            ),
          );
        }),
        // system prompt (collassabile)
        AnimatedSize(
          duration: const Duration(milliseconds: 180),
          child: _showSysPrompt
              ? Container(
                  color: plx.hdr,
                  padding: const EdgeInsets.fromLTRB(12, 6, 12, 8),
                  child: TextField(
                    maxLines: 3,
                    decoration: InputDecoration(
                      labelText: 'System prompt',
                      labelStyle: TextStyle(
                          color: plx.dim, fontSize: 12),
                    ),
                    style: TextStyle(
                        color: plx.txt, fontSize: 13),
                    onChanged: (v) => _systemPrompt = v,
                  ),
                )
              : const SizedBox.shrink(),
        ),
        // lista messaggi
        Expanded(
          child: prov.messages.isEmpty
              ? _EmptyState(plx: plx)
              : ListView.builder(
                  controller: _scroll,
                  padding: const EdgeInsets.fromLTRB(12, 12, 12, 4),
                  itemCount: prov.messages.length,
                  itemBuilder: (_, i) =>
                      ChatBubble(message: prov.messages[i]),
                ),
        ),
        // barra input stile web
        _InputBar(
          plx: plx,
          controller: _controller,
          isStreaming: prov.isStreaming,
          onSend: _send,
        ),
      ],
    );
  }
}

// ── Model toolbar ─────────────────────────────────────────────────────────────

class _ModelBar extends StatelessWidget {
  final PlxColors plx;
  final AppProvider prov;
  final VoidCallback onSysToggle;
  final VoidCallback onClear;

  const _ModelBar({
    required this.plx,
    required this.prov,
    required this.onSysToggle,
    required this.onClear,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 40,
      padding: const EdgeInsets.symmetric(horizontal: 10),
      decoration: BoxDecoration(
        color: plx.hdr,
        border: Border(bottom: BorderSide(color: plx.brd)),
      ),
      child: Row(
        children: [
          Expanded(
            child: prov.models.isEmpty
                ? Text('Nessun modello',
                    style: TextStyle(color: plx.dim, fontSize: 12))
                : _ModelDropdown(plx: plx, prov: prov),
          ),
          const SizedBox(width: 6),
          _BarBtn(
            icon: Icons.tune,
            plx: plx,
            tooltip: 'System prompt',
            onTap: onSysToggle,
          ),
          _BarBtn(
            icon: Icons.delete_outline,
            plx: plx,
            tooltip: 'Cancella',
            onTap: prov.isStreaming ? null : onClear,
          ),
        ],
      ),
    );
  }
}

class _ModelDropdown extends StatelessWidget {
  final PlxColors plx;
  final AppProvider prov;

  const _ModelDropdown({required this.plx, required this.prov});

  @override
  Widget build(BuildContext context) {
    final models = prov.models;
    final sel = models.contains(prov.selectedModel)
        ? prov.selectedModel
        : models.first;

    return DropdownButtonHideUnderline(
      child: DropdownButton<String>(
        value: sel,
        isExpanded: true,
        isDense: true,
        dropdownColor: plx.hdr,
        style: TextStyle(color: plx.txt, fontSize: 12),
        icon: Icon(Icons.expand_more, size: 16, color: plx.dim),
        items: models
            .map((m) => DropdownMenuItem(
                  value: m,
                  child: Text(m,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: TextStyle(color: plx.txt, fontSize: 12)),
                ))
            .toList(),
        onChanged: (v) {
          if (v != null) prov.selectModel(v);
        },
      ),
    );
  }
}

class _BarBtn extends StatelessWidget {
  final IconData icon;
  final PlxColors plx;
  final String tooltip;
  final VoidCallback? onTap;

  const _BarBtn({
    required this.icon,
    required this.plx,
    required this.tooltip,
    this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: tooltip,
      child: GestureDetector(
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.all(6),
          child: Icon(
            icon,
            size: 17,
            color: onTap != null ? plx.dim : plx.brd,
          ),
        ),
      ),
    );
  }
}

// ── Stato vuoto ───────────────────────────────────────────────────────────────

class _EmptyState extends StatelessWidget {
  final PlxColors plx;

  const _EmptyState({required this.plx});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Image.asset(
            'assets/icon.png',
            width: 90,
            height: 90,
            filterQuality: FilterQuality.high,
          ),
          const SizedBox(height: 14),
          Text(
            'Prismalux',
            style: TextStyle(
                color: plx.txt,
                fontSize: 22,
                fontWeight: FontWeight.w700,
                letterSpacing: 0.5),
          ),
          const SizedBox(height: 6),
          Text(
            'Costruito per i mortali che aspirano alla saggezza.',
            style: TextStyle(color: plx.dim, fontSize: 12),
            textAlign: TextAlign.center,
          ),
        ],
      ),
    );
  }
}

// ── Input bar stile webchat ───────────────────────────────────────────────────

class _InputBar extends StatelessWidget {
  final PlxColors plx;
  final TextEditingController controller;
  final bool isStreaming;
  final VoidCallback onSend;

  const _InputBar({
    required this.plx,
    required this.controller,
    required this.isStreaming,
    required this.onSend,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.fromLTRB(10, 8, 10, 8),
      decoration: BoxDecoration(
        color: plx.hdr,
        border: Border(top: BorderSide(color: plx.brd)),
      ),
      child: SafeArea(
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.end,
          children: [
            // textarea
            Expanded(
              child: Container(
                constraints:
                    const BoxConstraints(maxHeight: 160),
                decoration: BoxDecoration(
                  color: plx.inp,
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(color: plx.brd),
                ),
                child: TextField(
                  controller: controller,
                  maxLines: null,
                  minLines: 1,
                  style: TextStyle(
                      color: plx.txt, fontSize: 14),
                  decoration: InputDecoration(
                    hintText: 'Scrivi un messaggio...',
                    hintStyle:
                        TextStyle(color: plx.dim, fontSize: 14),
                    border: InputBorder.none,
                    enabledBorder: InputBorder.none,
                    focusedBorder: InputBorder.none,
                    contentPadding: const EdgeInsets.symmetric(
                        horizontal: 14, vertical: 10),
                    isDense: true,
                  ),
                  onSubmitted: (_) => onSend(),
                ),
              ),
            ),
            const SizedBox(width: 8),
            // pulsante invia
            GestureDetector(
              onTap: isStreaming ? null : onSend,
              child: AnimatedContainer(
                duration: const Duration(milliseconds: 150),
                padding: const EdgeInsets.symmetric(
                    horizontal: 16, vertical: 10),
                decoration: BoxDecoration(
                  color: isStreaming
                      ? plx.brd
                      : plx.acc,
                  borderRadius: BorderRadius.circular(10),
                ),
                child: isStreaming
                    ? SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: plx.dim,
                        ),
                      )
                    : Text(
                        'Invia',
                        style: TextStyle(
                          color: plx.acc.computeLuminance() > 0.35
                              ? Colors.black
                              : Colors.white,
                          fontWeight: FontWeight.w700,
                          fontSize: 14,
                        ),
                      ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
