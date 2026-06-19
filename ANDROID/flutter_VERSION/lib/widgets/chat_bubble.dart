import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_markdown/flutter_markdown.dart';
import 'package:provider/provider.dart';
import '../models/chat_message.dart';
import '../theme/plx_colors.dart';
import '../providers/theme_provider.dart';

class ChatBubble extends StatelessWidget {
  final ChatMessage message;

  const ChatBubble({super.key, required this.message});

  @override
  Widget build(BuildContext context) {
    final plx = context.watch<ThemeProvider>().current;
    final isUser = message.role == MessageRole.user;

    return Padding(
      padding: const EdgeInsets.only(bottom: 10),
      child: Column(
        crossAxisAlignment:
            isUser ? CrossAxisAlignment.end : CrossAxisAlignment.start,
        children: [
          ConstrainedBox(
            constraints: BoxConstraints(
              maxWidth: MediaQuery.of(context).size.width * 0.82,
            ),
            child: Container(
              padding: const EdgeInsets.symmetric(
                  horizontal: 14, vertical: 10),
              decoration: BoxDecoration(
                color: isUser ? plx.usr : plx.aim,
                borderRadius: BorderRadius.only(
                  topLeft: const Radius.circular(14),
                  topRight: const Radius.circular(14),
                  bottomLeft: Radius.circular(isUser ? 14 : 4),
                  bottomRight: Radius.circular(isUser ? 4 : 14),
                ),
                border: isUser
                    ? null
                    : Border.all(color: plx.brd),
              ),
              child: isUser
                  ? SelectableText(
                      message.content,
                      style: TextStyle(
                          color: _contrastFor(plx.usr),
                          fontSize: 14,
                          height: 1.55),
                    )
                  : _AiContent(
                      message: message, plx: plx),
            ),
          ),
          // azione copia (solo AI)
          if (!isUser && !message.isStreaming)
            _ActionBar(message: message, plx: plx),
        ],
      ),
    );
  }

  static Color _contrastFor(Color c) =>
      c.computeLuminance() > 0.35 ? Colors.black : Colors.white;
}

class _AiContent extends StatelessWidget {
  final ChatMessage message;
  final PlxColors plx;

  const _AiContent({required this.message, required this.plx});

  @override
  Widget build(BuildContext context) {
    final content = message.content;
    if (message.isStreaming && content.isEmpty) {
      return Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          _blinkDot(plx.acc, 0),
          _blinkDot(plx.acc, 100),
          _blinkDot(plx.acc, 200),
        ],
      );
    }
    return MarkdownBody(
      data: content,
      selectable: true,
      styleSheet: MarkdownStyleSheet(
        p: TextStyle(color: plx.txt, fontSize: 14, height: 1.55),
        h1: TextStyle(color: plx.txt, fontSize: 18, fontWeight: FontWeight.w700),
        h2: TextStyle(color: plx.txt, fontSize: 16, fontWeight: FontWeight.w700),
        h3: TextStyle(color: plx.txt, fontSize: 14, fontWeight: FontWeight.w600),
        strong: TextStyle(color: plx.txt, fontWeight: FontWeight.w700),
        em: TextStyle(color: plx.txt, fontStyle: FontStyle.italic),
        code: TextStyle(
          fontFamily: 'monospace',
          fontSize: 12,
          color: plx.acc,
          backgroundColor: plx.bg,
        ),
        codeblockDecoration: BoxDecoration(
          color: plx.bg,
          borderRadius: BorderRadius.circular(6),
          border: Border.all(color: plx.brd),
        ),
        codeblockPadding: const EdgeInsets.all(10),
        blockquoteDecoration: BoxDecoration(
          border: Border(left: BorderSide(color: plx.acc, width: 3)),
          color: plx.acc.withAlpha(15),
        ),
        blockquotePadding:
            const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
        listBullet: TextStyle(color: plx.dim),
        tableHead: TextStyle(color: plx.txt, fontWeight: FontWeight.w700),
        tableBody: TextStyle(color: plx.txt),
        tableBorder: TableBorder.all(color: plx.brd),
        horizontalRuleDecoration: BoxDecoration(
          border: Border(bottom: BorderSide(color: plx.brd)),
        ),
        a: TextStyle(color: plx.acc, decoration: TextDecoration.underline),
      ),
    );
  }

  Widget _blinkDot(Color color, int delayMs) {
    return TweenAnimationBuilder<double>(
      tween: Tween(begin: 0.3, end: 1.0),
      duration: const Duration(milliseconds: 600),
      builder: (_, v, __) => Container(
        width: 6,
        height: 6,
        margin: const EdgeInsets.symmetric(horizontal: 2),
        decoration: BoxDecoration(
          color: color.withAlpha((v * 255).toInt()),
          shape: BoxShape.circle,
        ),
      ),
    );
  }
}

class _ActionBar extends StatelessWidget {
  final ChatMessage message;
  final PlxColors plx;

  const _ActionBar({required this.message, required this.plx});

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        const SizedBox(height: 2),
        _ActionBtn(
          icon: Icons.copy,
          tooltip: 'Copia',
          plx: plx,
          onTap: () {
            Clipboard.setData(ClipboardData(text: message.content));
            ScaffoldMessenger.of(context).showSnackBar(
              SnackBar(
                content: const Text('Copiato'),
                duration: const Duration(seconds: 1),
                backgroundColor: plx.aim,
              ),
            );
          },
        ),
        Text(
          _fmt(message.timestamp),
          style: TextStyle(color: plx.dim, fontSize: 10),
        ),
        const SizedBox(width: 6),
      ],
    );
  }

  String _fmt(DateTime t) =>
      '${t.hour.toString().padLeft(2, '0')}:${t.minute.toString().padLeft(2, '0')}';
}

class _ActionBtn extends StatelessWidget {
  final IconData icon;
  final String tooltip;
  final PlxColors plx;
  final VoidCallback onTap;

  const _ActionBtn(
      {required this.icon,
      required this.tooltip,
      required this.plx,
      required this.onTap});

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: tooltip,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(6),
        child: Padding(
          padding: const EdgeInsets.all(4),
          child: Icon(icon, size: 13, color: plx.dim),
        ),
      ),
    );
  }
}
