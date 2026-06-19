import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/app_provider.dart';
import '../services/api_service.dart';

class SysinfoScreen extends StatefulWidget {
  const SysinfoScreen({super.key});

  @override
  State<SysinfoScreen> createState() => _SysinfoScreenState();
}

class _SysinfoScreenState extends State<SysinfoScreen> {
  Map<String, dynamic>? _data;
  String _error = '';
  bool _loading = false;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final api = context.read<AppProvider>().api;
    setState(() {
      _loading = true;
      _error = '';
    });
    try {
      final r = await api.sysinfo();
      setState(() => _data = r);
    } on ApiException catch (e) {
      setState(() => _error = e.message);
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colors = theme.colorScheme;

    return Scaffold(
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _error.isNotEmpty
              ? Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(Icons.error_outline,
                          color: colors.error, size: 48),
                      const SizedBox(height: 12),
                      Text(_error,
                          style: TextStyle(color: colors.error)),
                      const SizedBox(height: 16),
                      FilledButton.icon(
                        onPressed: _load,
                        icon: const Icon(Icons.refresh),
                        label: const Text('Riprova'),
                      ),
                    ],
                  ),
                )
              : _data == null
                  ? const SizedBox.shrink()
                  : RefreshIndicator(
                      onRefresh: _load,
                      child: ListView(
                        padding: const EdgeInsets.all(16),
                        children: [
                          _section('Sistema', [
                            _row('OS', _data!['os']),
                            _row('Hostname', _data!['hostname']),
                            _row('Kernel', _data!['kernel']),
                            _row('Uptime', _data!['uptime']),
                          ]),
                          const SizedBox(height: 16),
                          _section('CPU', [
                            _row('Modello', _data!['cpu_model']),
                            _row('Core logici',
                                _data!['cpu_cores']),
                            _row('Utilizzo',
                                _data!['cpu_usage'] != null
                                    ? '${_data!['cpu_usage']}%'
                                    : null),
                          ]),
                          const SizedBox(height: 16),
                          _section('Memoria', [
                            _row('RAM totale',
                                _data!['ram_total']),
                            _row('RAM usata',
                                _data!['ram_used']),
                            _row('RAM libera',
                                _data!['ram_free']),
                            if (_data!['ram_usage_pct'] != null)
                              _progressRow(
                                'Utilizzo',
                                (_data!['ram_usage_pct'] as num)
                                        .toDouble() /
                                    100,
                              ),
                          ]),
                          if (_data!['gpu_model'] != null) ...[
                            const SizedBox(height: 16),
                            _section('GPU', [
                              _row('Modello',
                                  _data!['gpu_model']),
                              _row('VRAM', _data!['gpu_vram']),
                              if (_data!['gpu_usage'] != null)
                                _progressRow(
                                  'Utilizzo',
                                  (_data!['gpu_usage'] as num)
                                          .toDouble() /
                                      100,
                                ),
                            ]),
                          ],
                          const SizedBox(height: 16),
                          _section('Disco', [
                            _row('Totale',
                                _data!['disk_total']),
                            _row('Usato', _data!['disk_used']),
                            _row('Libero', _data!['disk_free']),
                          ]),
                          const SizedBox(height: 16),
                          _section('Rete', [
                            _row('IP locale', _data!['ip_local']),
                            _row('IP pubblico',
                                _data!['ip_public']),
                          ]),
                        ],
                      ),
                    ),
      floatingActionButton: FloatingActionButton.small(
        onPressed: _load,
        tooltip: 'Aggiorna',
        child: const Icon(Icons.refresh),
      ),
    );
  }

  Widget _section(String title, List<Widget> children) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title,
                style: const TextStyle(
                    fontWeight: FontWeight.bold, fontSize: 16)),
            const Divider(),
            ...children.where((w) => w is! SizedBox),
          ],
        ),
      ),
    );
  }

  Widget _row(String label, dynamic value) {
    if (value == null) return const SizedBox.shrink();
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 120,
            child: Text(label,
                style: const TextStyle(
                    fontWeight: FontWeight.w500, fontSize: 13)),
          ),
          Expanded(
            child: SelectableText(
              value.toString(),
              style: const TextStyle(fontSize: 13),
            ),
          ),
        ],
      ),
    );
  }

  Widget _progressRow(String label, double value) {
    final pct = (value * 100).toInt();
    final color = value > 0.9
        ? Colors.red
        : value > 0.7
            ? Colors.orange
            : Colors.green;
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          SizedBox(
            width: 120,
            child: Text(label,
                style: const TextStyle(
                    fontWeight: FontWeight.w500, fontSize: 13)),
          ),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                ClipRRect(
                  borderRadius: BorderRadius.circular(4),
                  child: LinearProgressIndicator(
                    value: value.clamp(0.0, 1.0),
                    minHeight: 8,
                    valueColor:
                        AlwaysStoppedAnimation<Color>(color),
                  ),
                ),
                Text('$pct%',
                    style: const TextStyle(fontSize: 11)),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
