import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/app_provider.dart';
import '../providers/theme_provider.dart';
import '../theme/plx_colors.dart';
import '../services/api_service.dart';

class DevScreen extends StatelessWidget {
  const DevScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 3,
      child: Column(
        children: [
          const TabBar(
            isScrollable: true,
            tabAlignment: TabAlignment.start,
            tabs: [
              Tab(text: 'Git'),
              Tab(text: 'Matematica'),
              Tab(text: 'Graphviz'),
            ],
          ),
          const Expanded(
            child: TabBarView(
              children: [_GitTab(), _MathTab(), _GraphvizTab()],
            ),
          ),
        ],
      ),
    );
  }
}

// ── Git ───────────────────────────────────────────────────────────────────────

class _GitTab extends StatefulWidget {
  const _GitTab();

  @override
  State<_GitTab> createState() => _GitTabState();
}

class _GitTabState extends State<_GitTab> {
  String _cmd = 'status';
  String _output = '';
  String _error = '';
  bool _loading = false;

  static const _cmds = [
    'status',
    'log',
    'diff',
    'diff-staged',
    'branch',
  ];

  Future<void> _run() async {
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _output = '';
      _error = '';
    });
    try {
      final r = await api.gitCommand(_cmd);
      setState(() {
        _output = r['output'] as String? ??
            r['result'] as String? ??
            r.toString();
      });
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.all(12),
          child: Row(
            children: [
              Expanded(
                child: DropdownButtonFormField<String>(
                  value: _cmd,
                  decoration: const InputDecoration(
                      labelText: 'Comando git',
                      border: OutlineInputBorder(),
                      isDense: true),
                  items: _cmds
                      .map((c) => DropdownMenuItem(
                          value: c, child: Text('git $c')))
                      .toList(),
                  onChanged: (v) =>
                      setState(() => _cmd = v ?? 'status'),
                ),
              ),
              const SizedBox(width: 8),
              FilledButton.icon(
                onPressed: _loading ? null : _run,
                icon: _loading
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2))
                    : const Icon(Icons.terminal),
                label: const Text('Esegui'),
              ),
            ],
          ),
        ),
        Expanded(
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12),
            child: Container(
              decoration: BoxDecoration(
                color: colors.surfaceContainerHighest,
                borderRadius: BorderRadius.circular(8),
              ),
              padding: const EdgeInsets.all(12),
              child: SingleChildScrollView(
                child: SelectableText(
                  _error.isNotEmpty
                      ? 'Errore: $_error'
                      : _output.isNotEmpty
                          ? _output
                          : 'Seleziona un comando e premi Esegui',
                  style: TextStyle(
                    fontFamily: 'monospace',
                    fontSize: 12,
                    color: _error.isNotEmpty ? colors.error : null,
                  ),
                ),
              ),
            ),
          ),
        ),
        const SizedBox(height: 12),
      ],
    );
  }
}

// ── Matematica ────────────────────────────────────────────────────────────────

class _MathTab extends StatefulWidget {
  const _MathTab();

  @override
  State<_MathTab> createState() => _MathTabState();
}

class _MathTabState extends State<_MathTab> {
  final _expr = TextEditingController();
  String _action = 'solve';
  String _output = '';
  String _error = '';
  bool _loading = false;

  static const _actions = {
    'solve': 'Risolvi',
    'simplify': 'Semplifica',
    'expand': 'Espandi',
    'factor': 'Fattorizza',
    'diff': 'Derivata',
    'integrate': 'Integrale',
    'limit': 'Limite',
    'series': 'Serie',
    'all_constants': 'Costanti',
  };

  Future<void> _run() async {
    if (_action != 'all_constants' && _expr.text.trim().isEmpty) return;
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _output = '';
      _error = '';
    });
    try {
      final r = await api.math(_action, {
        if (_expr.text.isNotEmpty) 'expr': _expr.text.trim(),
        'digits': 50,
      });
      setState(() {
        _output = r['result'] as String? ??
            r['output'] as String? ??
            r.toString();
      });
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  void dispose() {
    _expr.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          DropdownButtonFormField<String>(
            value: _action,
            decoration: const InputDecoration(
                labelText: 'Operazione',
                border: OutlineInputBorder(),
                isDense: true),
            items: _actions.entries
                .map((e) => DropdownMenuItem(
                    value: e.key, child: Text(e.value)))
                .toList(),
            onChanged: (v) => setState(() => _action = v ?? 'solve'),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _expr,
            decoration: const InputDecoration(
              labelText: 'Espressione (es. x**2 - 4, x)',
              hintText: 'x**2 - 4',
              border: OutlineInputBorder(),
              isDense: true,
            ),
            onSubmitted: (_) => _run(),
          ),
          const SizedBox(height: 16),
          FilledButton.icon(
            onPressed: _loading ? null : _run,
            icon: _loading
                ? const SizedBox(
                    width: 16,
                    height: 16,
                    child: CircularProgressIndicator(strokeWidth: 2))
                : const Icon(Icons.functions),
            label: const Text('Calcola'),
          ),
          if (_output.isNotEmpty || _error.isNotEmpty) ...[
            const SizedBox(height: 16),
            Container(
              decoration: BoxDecoration(
                color: _error.isNotEmpty
                    ? colors.errorContainer
                    : colors.surfaceContainerHighest,
                borderRadius: BorderRadius.circular(8),
              ),
              padding: const EdgeInsets.all(16),
              child: SelectableText(
                _error.isNotEmpty ? _error : _output,
                style: TextStyle(
                  fontFamily: 'monospace',
                  fontSize: 14,
                  color: _error.isNotEmpty
                      ? colors.onErrorContainer
                      : null,
                ),
              ),
            ),
          ],
        ],
      ),
    );
  }
}

// ── Graphviz ──────────────────────────────────────────────────────────────────

class _GraphvizTab extends StatefulWidget {
  const _GraphvizTab();

  @override
  State<_GraphvizTab> createState() => _GraphvizTabState();
}

class _GraphvizTabState extends State<_GraphvizTab> {
  final _dot = TextEditingController(
    text: 'digraph G {\n'
        '  rankdir=LR\n'
        '  A -> B -> C\n'
        '  A -> C\n'
        '}',
  );
  Uint8List? _image;
  String _error = '';
  bool _loading = false;

  Future<void> _render() async {
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _image = null;
      _error = '';
    });
    try {
      final bytes = await api.renderGraphviz(_dot.text);
      setState(() => _image = bytes);
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  void dispose() {
    _dot.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return Column(
      children: [
        Expanded(
          flex: 2,
          child: Padding(
            padding: const EdgeInsets.all(8),
            child: TextField(
              controller: _dot,
              maxLines: null,
              expands: true,
              style: const TextStyle(
                  fontFamily: 'monospace', fontSize: 13),
              decoration: const InputDecoration(
                labelText: 'DOT language',
                border: OutlineInputBorder(),
                alignLabelWithHint: true,
              ),
            ),
          ),
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 8),
          child: FilledButton.icon(
            onPressed: _loading ? null : _render,
            icon: _loading
                ? const SizedBox(
                    width: 16,
                    height: 16,
                    child: CircularProgressIndicator(strokeWidth: 2))
                : const Icon(Icons.account_tree),
            label: const Text('Renderizza'),
          ),
        ),
        const SizedBox(height: 8),
        Expanded(
          flex: 3,
          child: Padding(
            padding: const EdgeInsets.all(8),
            child: _loading
                ? const Center(child: CircularProgressIndicator())
                : _error.isNotEmpty
                    ? Center(
                        child: Text(_error,
                            style: TextStyle(color: colors.error)))
                    : _image != null
                        ? InteractiveViewer(
                            child: Center(
                              child: Image.memory(_image!),
                            ),
                          )
                        : Center(
                            child: Text('Il grafo apparirà qui',
                                style: TextStyle(
                                    color: colors.outlineVariant))),
          ),
        ),
      ],
    );
  }
}
