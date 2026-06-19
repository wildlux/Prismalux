import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/app_provider.dart';
import '../providers/theme_provider.dart';
import '../theme/plx_colors.dart';
import '../services/api_service.dart';

class ResearchScreen extends StatelessWidget {
  const ResearchScreen({super.key});

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
              Tab(text: 'Wikipedia'),
              Tab(text: 'Lavoro'),
              Tab(text: 'RAG'),
            ],
          ),
          const Expanded(
            child: TabBarView(
              children: [_WikiTab(), _JobsTab(), _RagTab()],
            ),
          ),
        ],
      ),
    );
  }
}

// ── Wikipedia ─────────────────────────────────────────────────────────────────

class _WikiTab extends StatefulWidget {
  const _WikiTab();

  @override
  State<_WikiTab> createState() => _WikiTabState();
}

class _WikiTabState extends State<_WikiTab> {
  final _query = TextEditingController();
  String _lang = 'it';
  Map<String, dynamic>? _result;
  String _error = '';
  bool _loading = false;

  Future<void> _search() async {
    if (_query.text.trim().isEmpty) return;
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _result = null;
      _error = '';
    });
    try {
      final r = await api.wiki(_query.text.trim(), lang: _lang);
      setState(() => _result = r);
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  void dispose() {
    _query.dispose();
    super.dispose();
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
                child: TextField(
                  controller: _query,
                  decoration: const InputDecoration(
                    labelText: 'Cerca su Wikipedia',
                    border: OutlineInputBorder(),
                    isDense: true,
                  ),
                  onSubmitted: (_) => _search(),
                ),
              ),
              const SizedBox(width: 8),
              DropdownButton<String>(
                value: _lang,
                items: const [
                  DropdownMenuItem(value: 'it', child: Text('IT')),
                  DropdownMenuItem(value: 'en', child: Text('EN')),
                  DropdownMenuItem(value: 'de', child: Text('DE')),
                  DropdownMenuItem(value: 'fr', child: Text('FR')),
                ],
                onChanged: (v) => setState(() => _lang = v ?? 'it'),
              ),
              const SizedBox(width: 8),
              IconButton.filled(
                onPressed: _loading ? null : _search,
                icon: _loading
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2))
                    : const Icon(Icons.search),
              ),
            ],
          ),
        ),
        Expanded(
          child: _loading
              ? const Center(child: CircularProgressIndicator())
              : _error.isNotEmpty
                  ? Center(
                      child: Text(_error,
                          style: TextStyle(color: colors.error)))
                  : _result == null
                      ? Center(
                          child: Text('Cerca un argomento',
                              style: TextStyle(
                                  color: colors.outlineVariant)))
                      : SingleChildScrollView(
                          padding: const EdgeInsets.all(16),
                          child: Column(
                            crossAxisAlignment:
                                CrossAxisAlignment.start,
                            children: [
                              if (_result!['thumbnail'] != null)
                                ClipRRect(
                                  borderRadius:
                                      BorderRadius.circular(8),
                                  child: Image.network(
                                    _result!['thumbnail']['source']
                                        as String,
                                    height: 160,
                                    fit: BoxFit.cover,
                                    errorBuilder: (_, __, ___) =>
                                        const SizedBox.shrink(),
                                  ),
                                ),
                              if (_result!['thumbnail'] != null)
                                const SizedBox(height: 12),
                              Text(
                                _result!['title'] as String? ?? '',
                                style: Theme.of(context)
                                    .textTheme
                                    .headlineSmall,
                              ),
                              const SizedBox(height: 8),
                              SelectableText(
                                _result!['extract'] as String? ?? '',
                                style: const TextStyle(height: 1.5),
                              ),
                              if (_result!['content_urls'] != null)
                                ...[
                                  const SizedBox(height: 16),
                                  Text(
                                    (_result!['content_urls']
                                                as Map?)?['desktop']
                                            ?['page']
                                            ?.toString() ??
                                        '',
                                    style: TextStyle(
                                        color: colors.primary,
                                        fontSize: 12),
                                  ),
                                ],
                            ],
                          ),
                        ),
        ),
      ],
    );
  }
}

// ── Lavoro ────────────────────────────────────────────────────────────────────

class _JobsTab extends StatefulWidget {
  const _JobsTab();

  @override
  State<_JobsTab> createState() => _JobsTabState();
}

class _JobsTabState extends State<_JobsTab> {
  final _query = TextEditingController();
  List<dynamic> _jobs = [];
  String _error = '';
  bool _loading = false;

  Future<void> _search() async {
    if (_query.text.trim().isEmpty) return;
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _jobs = [];
      _error = '';
    });
    try {
      final r = await api.searchJobs(_query.text.trim());
      setState(() {
        _jobs = r['jobs'] as List<dynamic>? ??
            r['results'] as List<dynamic>? ??
            [];
      });
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  void dispose() {
    _query.dispose();
    super.dispose();
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
                child: TextField(
                  controller: _query,
                  decoration: const InputDecoration(
                    labelText: 'Cerca offerte di lavoro',
                    border: OutlineInputBorder(),
                    isDense: true,
                  ),
                  onSubmitted: (_) => _search(),
                ),
              ),
              const SizedBox(width: 8),
              IconButton.filled(
                onPressed: _loading ? null : _search,
                icon: _loading
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2))
                    : const Icon(Icons.search),
              ),
            ],
          ),
        ),
        Expanded(
          child: _loading
              ? const Center(child: CircularProgressIndicator())
              : _error.isNotEmpty
                  ? Center(
                      child: Text(_error,
                          style: TextStyle(color: colors.error)))
                  : _jobs.isEmpty
                      ? Center(
                          child: Text(
                              'Cerca offerte di lavoro per ruolo',
                              style: TextStyle(
                                  color: colors.outlineVariant)))
                      : ListView.builder(
                          itemCount: _jobs.length,
                          itemBuilder: (_, i) {
                            final job = _jobs[i] as Map<String,
                                dynamic>;
                            return Card(
                              margin: const EdgeInsets.symmetric(
                                  horizontal: 12, vertical: 4),
                              child: ListTile(
                                title: Text(
                                    job['title'] as String? ??
                                        job['ruolo'] as String? ??
                                        'N/A'),
                                subtitle: Text(
                                    '${job['company'] ?? job['azienda'] ?? ''}'
                                    '${job['location'] != null ? ' • ${job['location']}' : ''}'),
                                trailing: job['salary'] != null
                                    ? Text(job['salary'].toString())
                                    : null,
                                onTap: () =>
                                    _showJobDetail(context, job),
                              ),
                            );
                          },
                        ),
        ),
      ],
    );
  }

  void _showJobDetail(
      BuildContext context, Map<String, dynamic> job) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      builder: (_) => DraggableScrollableSheet(
        expand: false,
        initialChildSize: 0.6,
        maxChildSize: 0.95,
        builder: (_, ctrl) => SingleChildScrollView(
          controller: ctrl,
          padding: const EdgeInsets.all(20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(job['title'] ?? job['ruolo'] ?? '',
                  style: Theme.of(context).textTheme.titleLarge),
              const SizedBox(height: 8),
              if (job['company'] != null)
                Text(job['company'].toString()),
              if (job['description'] != null) ...[
                const SizedBox(height: 12),
                Text(job['description'].toString()),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

// ── RAG ───────────────────────────────────────────────────────────────────────

class _RagTab extends StatefulWidget {
  const _RagTab();

  @override
  State<_RagTab> createState() => _RagTabState();
}

class _RagTabState extends State<_RagTab> {
  final _query = TextEditingController();
  int _limit = 5;
  List<dynamic> _chunks = [];
  String _error = '';
  bool _loading = false;

  Future<void> _search() async {
    if (_query.text.trim().isEmpty) return;
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _chunks = [];
      _error = '';
    });
    try {
      final r =
          await api.ragQuery(_query.text.trim(), limit: _limit);
      setState(() {
        _chunks = r['results'] as List<dynamic>? ??
            r['chunks'] as List<dynamic>? ??
            [];
      });
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  void dispose() {
    _query.dispose();
    super.dispose();
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
                child: TextField(
                  controller: _query,
                  decoration: const InputDecoration(
                    labelText: 'Cerca nella base RAG',
                    border: OutlineInputBorder(),
                    isDense: true,
                  ),
                  onSubmitted: (_) => _search(),
                ),
              ),
              const SizedBox(width: 8),
              DropdownButton<int>(
                value: _limit,
                items: [3, 5, 10]
                    .map((n) => DropdownMenuItem(
                        value: n, child: Text('Top $n')))
                    .toList(),
                onChanged: (v) => setState(() => _limit = v ?? 5),
              ),
              const SizedBox(width: 8),
              IconButton.filled(
                onPressed: _loading ? null : _search,
                icon: _loading
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2))
                    : const Icon(Icons.search),
              ),
            ],
          ),
        ),
        Expanded(
          child: _loading
              ? const Center(child: CircularProgressIndicator())
              : _error.isNotEmpty
                  ? Center(
                      child: Text(_error,
                          style: TextStyle(color: colors.error)))
                  : _chunks.isEmpty
                      ? Center(
                          child: Text('Cerca nei documenti RAG',
                              style: TextStyle(
                                  color: colors.outlineVariant)))
                      : ListView.builder(
                          itemCount: _chunks.length,
                          itemBuilder: (_, i) {
                            final c = _chunks[i] as Map<String,
                                dynamic>;
                            final score =
                                (c['score'] as num?)?.toDouble() ??
                                    0;
                            return Card(
                              margin: const EdgeInsets.symmetric(
                                  horizontal: 12, vertical: 4),
                              child: Padding(
                                padding: const EdgeInsets.all(12),
                                child: Column(
                                  crossAxisAlignment:
                                      CrossAxisAlignment.start,
                                  children: [
                                    Row(
                                      children: [
                                        Expanded(
                                          child: Text(
                                            c['source'] as String? ??
                                                'Documento ${i + 1}',
                                            style: const TextStyle(
                                                fontWeight:
                                                    FontWeight.bold,
                                                fontSize: 12),
                                          ),
                                        ),
                                        Container(
                                          padding:
                                              const EdgeInsets.symmetric(
                                                  horizontal: 8,
                                                  vertical: 2),
                                          decoration: BoxDecoration(
                                            color: colors.primaryContainer,
                                            borderRadius:
                                                BorderRadius.circular(12),
                                          ),
                                          child: Text(
                                            '${(score * 100).toStringAsFixed(0)}%',
                                            style: TextStyle(
                                                fontSize: 11,
                                                color: colors
                                                    .onPrimaryContainer),
                                          ),
                                        ),
                                      ],
                                    ),
                                    const SizedBox(height: 6),
                                    SelectableText(
                                      c['text'] as String? ??
                                          c['content'] as String? ??
                                          '',
                                      style: const TextStyle(
                                          fontSize: 13, height: 1.4),
                                    ),
                                  ],
                                ),
                              ),
                            );
                          },
                        ),
        ),
      ],
    );
  }
}
