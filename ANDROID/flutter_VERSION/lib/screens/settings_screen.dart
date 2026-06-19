import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/app_provider.dart';
import '../providers/theme_provider.dart';
import '../theme/plx_colors.dart';
import '../models/server_config.dart';
import 'qr_scan_screen.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late TextEditingController _host;
  late TextEditingController _port;
  late TextEditingController _token;
  bool _obscureToken = true;

  @override
  void initState() {
    super.initState();
    final cfg = context.read<AppProvider>().config;
    _host = TextEditingController(text: cfg.host);
    _port = TextEditingController(text: cfg.port.toString());
    _token = TextEditingController(text: cfg.token);
  }

  @override
  void dispose() {
    _host.dispose();
    _port.dispose();
    _token.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    final prov = context.read<AppProvider>();
    await prov.updateConfig(ServerConfig(
      host: _host.text.trim(),
      port: int.tryParse(_port.text) ?? 11500,
      token: _token.text.trim(),
    ));
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Configurazione salvata')));
    }
  }

  @override
  Widget build(BuildContext context) {
    final plx = context.watch<ThemeProvider>().current;
    final prov = context.watch<AppProvider>();

    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // Stato connessione
          _statusCard(plx, prov),
          const SizedBox(height: 20),
          _sectionLabel(plx, 'Server Prismalux'),
          const SizedBox(height: 10),
          // Pulsante scan QR
          _QrConnectBtn(plx: plx, onResult: (r) {
            setState(() {
              _host.text = r.host;
              _port.text = r.port.toString();
              _token.text = r.token;
            });
            _save();
          }),
          const SizedBox(height: 10),
          _field(plx, _host, 'Indirizzo IP / hostname',
              hint: '192.168.1.100',
              icon: Icons.computer),
          const SizedBox(height: 10),
          _field(plx, _port, 'Porta',
              hint: '11500',
              icon: Icons.settings_ethernet,
              keyboard: TextInputType.number),
          const SizedBox(height: 10),
          _tokenField(plx),
          const SizedBox(height: 16),
          _PlxBtn(
            plx: plx,
            label: 'Salva e connetti',
            icon: Icons.save,
            onTap: _save,
          ),
          const SizedBox(height: 24),
          // Tema
          _sectionLabel(plx, 'Tema'),
          const SizedBox(height: 10),
          _ThemeGrid(plx: plx),
          const SizedBox(height: 24),
          // Modelli
          if (prov.isConnected) ...[
            _sectionLabel(plx, 'Modelli disponibili'),
            const SizedBox(height: 10),
            if (prov.models.isEmpty)
              Text('Nessun modello trovato',
                  style: TextStyle(color: plx.dim, fontSize: 13))
            else
              ...prov.models.map((m) => _modelTile(plx, prov, m)),
            const SizedBox(height: 12),
            _PlxOutBtn(
              plx: plx,
              label: 'Aggiorna modelli',
              icon: Icons.refresh,
              onTap: prov.loadModels,
            ),
          ],
          const SizedBox(height: 24),
          _divider(plx),
          const SizedBox(height: 10),
          Text(
            'Prismalux Mobile v1.0 — porta default 11500\n'
            'Avvia il server LAN dalla scheda LAN & WAN dell\'app desktop.',
            style: TextStyle(color: plx.dim, fontSize: 11, height: 1.5),
          ),
        ],
      ),
    );
  }

  Widget _statusCard(PlxColors plx, AppProvider prov) {
    final ok = prov.isConnected;
    final accent = ok ? plx.acc : const Color(0xFFFF5555);
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: plx.hdr,
        border: Border.all(color: ok ? plx.brd : accent.withAlpha(80)),
        borderRadius: BorderRadius.circular(10),
      ),
      child: Row(
        children: [
          Icon(ok ? Icons.check_circle_outline : Icons.error_outline,
              color: accent, size: 20),
          const SizedBox(width: 10),
          Expanded(
              child: Text(prov.connectionStatus,
                  style: TextStyle(color: accent, fontSize: 13))),
          GestureDetector(
            onTap: prov.testConnection,
            child: Icon(Icons.refresh, color: plx.dim, size: 18),
          ),
        ],
      ),
    );
  }

  Widget _field(PlxColors plx, TextEditingController ctrl, String label,
      {String? hint, IconData? icon, TextInputType? keyboard}) {
    return TextField(
      controller: ctrl,
      keyboardType: keyboard,
      style: TextStyle(color: plx.txt, fontSize: 13),
      decoration: InputDecoration(
        labelText: label,
        hintText: hint,
        prefixIcon: icon != null
            ? Icon(icon, size: 18, color: plx.dim)
            : null,
      ),
    );
  }

  Widget _tokenField(PlxColors plx) {
    return TextField(
      controller: _token,
      obscureText: _obscureToken,
      style: TextStyle(color: plx.txt, fontSize: 13),
      decoration: InputDecoration(
        labelText: 'Token LAN (Bearer)',
        prefixIcon:
            Icon(Icons.vpn_key, size: 18, color: plx.dim),
        suffixIcon: GestureDetector(
          onTap: () =>
              setState(() => _obscureToken = !_obscureToken),
          child: Icon(
            _obscureToken
                ? Icons.visibility_off
                : Icons.visibility,
            size: 18,
            color: plx.dim,
          ),
        ),
      ),
    );
  }

  Widget _modelTile(PlxColors plx, AppProvider prov, String m) {
    final sel = prov.selectedModel == m;
    return GestureDetector(
      onTap: () => prov.selectModel(m),
      child: Container(
        margin: const EdgeInsets.only(bottom: 4),
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        decoration: BoxDecoration(
          color: sel ? plx.acc.withAlpha(25) : Colors.transparent,
          border: Border.all(color: sel ? plx.acc : plx.brd),
          borderRadius: BorderRadius.circular(8),
        ),
        child: Row(
          children: [
            Icon(Icons.smart_toy_outlined,
                size: 16, color: sel ? plx.acc : plx.dim),
            const SizedBox(width: 10),
            Expanded(
                child: Text(m,
                    style: TextStyle(
                        color: sel ? plx.acc : plx.txt,
                        fontSize: 13,
                        fontWeight: sel
                            ? FontWeight.w600
                            : FontWeight.normal))),
            if (sel) Icon(Icons.check, size: 14, color: plx.acc),
          ],
        ),
      ),
    );
  }

  Widget _sectionLabel(PlxColors plx, String t) => Text(
        t.toUpperCase(),
        style: TextStyle(
            color: plx.dim,
            fontSize: 10,
            letterSpacing: 0.8,
            fontWeight: FontWeight.w700),
      );

  Widget _divider(PlxColors plx) =>
      Container(height: 1, color: plx.brd);
}

// ── Griglia selezione tema rapida ─────────────────────────────────────────────

class _ThemeGrid extends StatelessWidget {
  final PlxColors plx;

  const _ThemeGrid({required this.plx});

  @override
  Widget build(BuildContext context) {
    final thProv = context.watch<ThemeProvider>();
    return Wrap(
      spacing: 8,
      runSpacing: 8,
      children: PlxThemes.all.map((t) {
        final sel = thProv.current.id == t.id;
        return GestureDetector(
          onTap: () => thProv.setTheme(t),
          child: Tooltip(
            message: t.label,
            child: AnimatedContainer(
              duration: const Duration(milliseconds: 150),
              width: 40,
              height: 40,
              decoration: BoxDecoration(
                color: t.bg,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(
                  color: sel ? t.acc : plx.brd,
                  width: sel ? 2 : 1,
                ),
              ),
              child: Center(
                child: Container(
                  width: 20,
                  height: 20,
                  decoration: BoxDecoration(
                    color: t.acc,
                    shape: BoxShape.circle,
                  ),
                ),
              ),
            ),
          ),
        );
      }).toList(),
    );
  }
}

// ── Pulsante scan QR ─────────────────────────────────────────────────────────

class _QrConnectBtn extends StatelessWidget {
  final PlxColors plx;
  final void Function(QrResult) onResult;

  const _QrConnectBtn({required this.plx, required this.onResult});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: () => _scan(context),
      child: Container(
        padding: const EdgeInsets.symmetric(vertical: 11),
        decoration: BoxDecoration(
          color: plx.acc.withAlpha(20),
          border: Border.all(color: plx.acc),
          borderRadius: BorderRadius.circular(8),
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.qr_code_scanner, size: 18, color: plx.acc),
            const SizedBox(width: 8),
            Text(
              'Scansiona QR Prismalux',
              style: TextStyle(
                  color: plx.acc,
                  fontWeight: FontWeight.w700,
                  fontSize: 14),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _scan(BuildContext context) async {
    final result = await Navigator.of(context).push<QrResult>(
      MaterialPageRoute(
        builder: (_) => QrScanScreen(plx: plx),
        fullscreenDialog: true,
      ),
    );
    if (result != null) onResult(result);
  }
}

// ── Bottoni stile web ─────────────────────────────────────────────────────────

class _PlxBtn extends StatelessWidget {
  final PlxColors plx;
  final String label;
  final IconData icon;
  final VoidCallback onTap;

  const _PlxBtn(
      {required this.plx,
      required this.label,
      required this.icon,
      required this.onTap});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(vertical: 11),
        decoration: BoxDecoration(
          color: plx.acc,
          borderRadius: BorderRadius.circular(8),
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(icon,
                size: 16,
                color: plx.acc.computeLuminance() > 0.35
                    ? Colors.black
                    : Colors.white),
            const SizedBox(width: 8),
            Text(label,
                style: TextStyle(
                    color: plx.acc.computeLuminance() > 0.35
                        ? Colors.black
                        : Colors.white,
                    fontWeight: FontWeight.w700,
                    fontSize: 14)),
          ],
        ),
      ),
    );
  }
}

class _PlxOutBtn extends StatelessWidget {
  final PlxColors plx;
  final String label;
  final IconData icon;
  final VoidCallback onTap;

  const _PlxOutBtn(
      {required this.plx,
      required this.label,
      required this.icon,
      required this.onTap});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(vertical: 10),
        decoration: BoxDecoration(
          border: Border.all(color: plx.brd),
          borderRadius: BorderRadius.circular(8),
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(icon, size: 16, color: plx.txt),
            const SizedBox(width: 8),
            Text(label,
                style:
                    TextStyle(color: plx.txt, fontSize: 13)),
          ],
        ),
      ),
    );
  }
}
