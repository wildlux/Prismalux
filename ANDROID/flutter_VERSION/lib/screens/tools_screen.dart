import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:file_picker/file_picker.dart';
import '../providers/app_provider.dart';
import '../providers/theme_provider.dart';
import '../theme/plx_colors.dart';
import '../services/api_service.dart';

class ToolsScreen extends StatelessWidget {
  const ToolsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 4,
      child: Column(
        children: [
          const TabBar(
            isScrollable: true,
            tabAlignment: TabAlignment.start,
            tabs: [
              Tab(text: 'Codice Fiscale'),
              Tab(text: 'TFR'),
              Tab(text: 'REPL Python'),
              Tab(text: 'File AI'),
            ],
          ),
          const Expanded(
            child: TabBarView(
              children: [
                _CfTab(),
                _TfrTab(),
                _ReplTab(),
                _FileTab(),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

// ── Codice Fiscale ────────────────────────────────────────────────────────────

class _CfTab extends StatefulWidget {
  const _CfTab();

  @override
  State<_CfTab> createState() => _CfTabState();
}

class _CfTabState extends State<_CfTab> {
  final _cognome = TextEditingController();
  final _nome = TextEditingController();
  final _data = TextEditingController();
  final _comune = TextEditingController();
  String _sesso = 'M';
  String _result = '';
  String _error = '';
  bool _loading = false;

  @override
  void dispose() {
    _cognome.dispose();
    _nome.dispose();
    _data.dispose();
    _comune.dispose();
    super.dispose();
  }

  Future<void> _calcola() async {
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _result = '';
      _error = '';
    });
    try {
      final r = await api.calcolaCodiceFiscale(
        cognome: _cognome.text,
        nome: _nome.text,
        data: _data.text,
        sesso: _sesso,
        comune: _comune.text,
      );
      setState(() {
        if (r.containsKey('error')) {
          _error = r['error'] as String;
        } else {
          _result =
              'Codice Fiscale: ${r['cf']}\nBelfiore: ${r['belfiore']}';
        }
      });
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(children: [
            Expanded(
                child: _field(_cognome, 'Cognome')),
            const SizedBox(width: 12),
            Expanded(child: _field(_nome, 'Nome')),
          ]),
          const SizedBox(height: 12),
          _field(_data, 'Data nascita (YYYY-MM-DD)',
              hint: '1990-01-15'),
          const SizedBox(height: 12),
          Row(children: [
            Expanded(child: _field(_comune, 'Comune di nascita')),
            const SizedBox(width: 12),
            DropdownButton<String>(
              value: _sesso,
              items: const [
                DropdownMenuItem(value: 'M', child: Text('Maschio')),
                DropdownMenuItem(value: 'F', child: Text('Femmina')),
              ],
              onChanged: (v) =>
                  setState(() => _sesso = v ?? 'M'),
            ),
          ]),
          const SizedBox(height: 16),
          FilledButton.icon(
            onPressed: _loading ? null : _calcola,
            icon: _loading
                ? const SizedBox(
                    width: 16,
                    height: 16,
                    child: CircularProgressIndicator(strokeWidth: 2))
                : const Icon(Icons.calculate),
            label: const Text('Calcola C.F.'),
          ),
          if (_result.isNotEmpty) ...[
            const SizedBox(height: 16),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: SelectableText(_result,
                    style: const TextStyle(
                        fontFamily: 'monospace', fontSize: 16)),
              ),
            ),
          ],
          if (_error.isNotEmpty) ...[
            const SizedBox(height: 16),
            Card(
              color: Theme.of(context).colorScheme.errorContainer,
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Text(_error,
                    style: TextStyle(
                        color: Theme.of(context)
                            .colorScheme
                            .onErrorContainer)),
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _field(TextEditingController ctrl, String label,
          {String? hint}) =>
      TextField(
        controller: ctrl,
        decoration: InputDecoration(
          labelText: label,
          hintText: hint,
          border: const OutlineInputBorder(),
          isDense: true,
        ),
      );
}

// ── TFR ───────────────────────────────────────────────────────────────────────

class _TfrTab extends StatefulWidget {
  const _TfrTab();

  @override
  State<_TfrTab> createState() => _TfrTabState();
}

class _TfrTabState extends State<_TfrTab> {
  final _stipendio = TextEditingController(text: '30000');
  final _anni = TextEditingController(text: '10');
  final _inflazione = TextEditingController(text: '2.0');
  bool _fondoPensione = false;
  Map<String, dynamic>? _result;
  String _error = '';
  bool _loading = false;

  Future<void> _calcola() async {
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _result = null;
      _error = '';
    });
    try {
      final r = await api.calcolaTfr(
        stipendioAnnuo: double.parse(_stipendio.text),
        anni: int.parse(_anni.text),
        inflazione: double.parse(_inflazione.text),
        fondoPensione: _fondoPensione,
      );
      setState(() => _result = r);
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  void dispose() {
    _stipendio.dispose();
    _anni.dispose();
    _inflazione.dispose();
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
          Row(children: [
            Expanded(
              child: TextField(
                controller: _stipendio,
                keyboardType: TextInputType.number,
                decoration: const InputDecoration(
                    labelText: 'Stipendio annuo (€)',
                    border: OutlineInputBorder(),
                    isDense: true),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: TextField(
                controller: _anni,
                keyboardType: TextInputType.number,
                decoration: const InputDecoration(
                    labelText: 'Anni lavorativi',
                    border: OutlineInputBorder(),
                    isDense: true),
              ),
            ),
          ]),
          const SizedBox(height: 12),
          TextField(
            controller: _inflazione,
            keyboardType: TextInputType.number,
            decoration: const InputDecoration(
                labelText: 'Inflazione % (default 2.0)',
                border: OutlineInputBorder(),
                isDense: true),
          ),
          const SizedBox(height: 12),
          SwitchListTile(
            title: const Text('Fondo Pensione'),
            subtitle: const Text('Imposta sostitutiva 15% (min 9%)'),
            value: _fondoPensione,
            onChanged: (v) => setState(() => _fondoPensione = v),
          ),
          const SizedBox(height: 16),
          FilledButton.icon(
            onPressed: _loading ? null : _calcola,
            icon: _loading
                ? const SizedBox(
                    width: 16,
                    height: 16,
                    child:
                        CircularProgressIndicator(strokeWidth: 2))
                : const Icon(Icons.calculate),
            label: const Text('Calcola TFR'),
          ),
          if (_result != null) ...[
            const SizedBox(height: 16),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  children: [
                    _tfrRow('Quota annua', _result!['quota_annua']),
                    _tfrRow('TFR semplice',
                        _result!['tfr_semplice']),
                    _tfrRow('TFR rivalutato',
                        _result!['tfr_rivalutato']),
                    _tfrRow('Plusvalenza', _result!['plusvalenza']),
                    _tfrRow(
                        'Netto stimato', _result!['netto_stimato'],
                        isTotal: true),
                    const Divider(),
                    Text(_result!['fiscalita'] as String? ?? '',
                        style: const TextStyle(fontSize: 12)),
                  ],
                ),
              ),
            ),
          ],
          if (_error.isNotEmpty) ...[
            const SizedBox(height: 16),
            Card(
              color: colors.errorContainer,
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Text(_error,
                    style: TextStyle(
                        color: colors.onErrorContainer)),
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _tfrRow(String label, dynamic value,
      {bool isTotal = false}) {
    final s = value is double
        ? '€ ${value.toStringAsFixed(2)}'
        : value.toString();
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label,
              style: isTotal
                  ? const TextStyle(fontWeight: FontWeight.bold)
                  : null),
          Text(s,
              style: isTotal
                  ? const TextStyle(
                      fontWeight: FontWeight.bold, fontSize: 18)
                  : null),
        ],
      ),
    );
  }
}

// ── REPL Python ───────────────────────────────────────────────────────────────

class _ReplTab extends StatefulWidget {
  const _ReplTab();

  @override
  State<_ReplTab> createState() => _ReplTabState();
}

class _ReplTabState extends State<_ReplTab> {
  final _code = TextEditingController(
      text: '# Codice Python\nimport math\nprint(math.pi)');
  String _output = '';
  String _error = '';
  bool _loading = false;

  Future<void> _run() async {
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _output = '';
      _error = '';
    });
    try {
      final r = await api.runRepl(_code.text);
      setState(() {
        _output = r['output'] as String? ?? '';
        _error = r['error'] as String? ?? '';
      });
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  void dispose() {
    _code.dispose();
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
              controller: _code,
              maxLines: null,
              expands: true,
              style: const TextStyle(
                  fontFamily: 'monospace', fontSize: 13),
              decoration: const InputDecoration(
                labelText: 'Codice Python',
                border: OutlineInputBorder(),
                alignLabelWithHint: true,
              ),
            ),
          ),
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 8),
          child: Row(
            children: [
              Expanded(
                child: FilledButton.icon(
                  onPressed: _loading ? null : _run,
                  icon: _loading
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(
                              strokeWidth: 2))
                      : const Icon(Icons.play_arrow),
                  label: const Text('Esegui'),
                ),
              ),
              const SizedBox(width: 8),
              OutlinedButton(
                onPressed: () =>
                    setState(() => _code.text = ''),
                child: const Text('Pulisci'),
              ),
            ],
          ),
        ),
        const SizedBox(height: 8),
        Expanded(
          flex: 1,
          child: Padding(
            padding: const EdgeInsets.all(8),
            child: Container(
              decoration: BoxDecoration(
                color: colors.surfaceContainerHighest,
                borderRadius: BorderRadius.circular(8),
              ),
              padding: const EdgeInsets.all(12),
              child: SingleChildScrollView(
                child: SelectableText(
                  _error.isNotEmpty
                      ? 'ERRORE:\n$_error'
                      : _output.isNotEmpty
                          ? _output
                          : 'Output...',
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
      ],
    );
  }
}

// ── File AI ───────────────────────────────────────────────────────────────────

class _FileTab extends StatefulWidget {
  const _FileTab();

  @override
  State<_FileTab> createState() => _FileTabState();
}

class _FileTabState extends State<_FileTab> {
  String _filename = '';
  String _extracted = '';
  String _error = '';
  bool _loading = false;

  Future<void> _pickFile() async {
    final result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['pdf', 'docx', 'txt', 'csv', 'py', 'cpp',
          'java', 'js', 'ts', 'html', 'md'],
      withData: true,
    );
    if (result == null || result.files.isEmpty) return;
    final file = result.files.first;
    if (file.bytes == null) return;

    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _filename = file.name;
      _extracted = '';
      _error = '';
    });
    try {
      final r = await api.uploadFile(file.bytes!, file.name);
      setState(() {
        _extracted = r['text'] as String? ?? r['content'] as String? ?? '';
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
    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          FilledButton.icon(
            onPressed: _loading ? null : _pickFile,
            icon: _loading
                ? const SizedBox(
                    width: 16,
                    height: 16,
                    child:
                        CircularProgressIndicator(strokeWidth: 2))
                : const Icon(Icons.upload_file),
            label:
                Text(_filename.isEmpty ? 'Carica File' : _filename),
          ),
          const SizedBox(height: 8),
          Text('Supportati: PDF, DOCX, TXT, CSV, codice sorgente',
              style: Theme.of(context).textTheme.bodySmall),
          const SizedBox(height: 16),
          Expanded(
            child: Container(
              decoration: BoxDecoration(
                border:
                    Border.all(color: colors.outlineVariant),
                borderRadius: BorderRadius.circular(8),
              ),
              padding: const EdgeInsets.all(12),
              child: SingleChildScrollView(
                child: SelectableText(
                  _error.isNotEmpty
                      ? 'Errore: $_error'
                      : _extracted.isNotEmpty
                          ? _extracted
                          : 'Carica un file per estrarne il testo...',
                  style: TextStyle(
                    fontSize: 13,
                    color: _error.isNotEmpty
                        ? colors.error
                        : null,
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
