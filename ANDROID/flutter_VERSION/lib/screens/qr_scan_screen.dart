import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import '../theme/plx_colors.dart';

/// Risultato del parsing del QR Prismalux.
/// Formato atteso: http://HOST:PORT/web?token=TOKEN
class QrResult {
  final String host;
  final int port;
  final String token;

  const QrResult({
    required this.host,
    required this.port,
    required this.token,
  });

  static QrResult? parse(String raw) {
    try {
      final uri = Uri.parse(raw.trim());
      if (uri.host.isEmpty) return null;
      final host = uri.host;
      final port = uri.hasPort ? uri.port : 11500;
      final token = uri.queryParameters['token'] ?? '';
      return QrResult(host: host, port: port, token: token);
    } catch (_) {
      return null;
    }
  }
}

class QrScanScreen extends StatefulWidget {
  final PlxColors plx;

  const QrScanScreen({super.key, required this.plx});

  @override
  State<QrScanScreen> createState() => _QrScanScreenState();
}

class _QrScanScreenState extends State<QrScanScreen> {
  final _ctrl = MobileScannerController(
    detectionSpeed: DetectionSpeed.noDuplicates,
    facing: CameraFacing.back,
  );

  bool _scanned = false;
  String _error = '';

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  void _onDetect(BarcodeCapture capture) {
    if (_scanned) return;
    for (final barcode in capture.barcodes) {
      final raw = barcode.rawValue;
      if (raw == null) continue;
      final result = QrResult.parse(raw);
      if (result != null) {
        setState(() => _scanned = true);
        Navigator.of(context).pop(result);
        return;
      }
    }
    setState(() => _error = 'QR non riconosciuto — usa il QR di Prismalux');
  }

  @override
  Widget build(BuildContext context) {
    final plx = widget.plx;

    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        backgroundColor: Colors.black,
        leading: IconButton(
          icon: const Icon(Icons.close, color: Colors.white),
          onPressed: () => Navigator.of(context).pop(null),
        ),
        title: const Text('Scansiona QR Prismalux',
            style: TextStyle(color: Colors.white, fontSize: 15)),
        actions: [
          IconButton(
            icon: const Icon(Icons.flash_on, color: Colors.white),
            onPressed: () => _ctrl.toggleTorch(),
            tooltip: 'Torcia',
          ),
        ],
      ),
      body: Stack(
        children: [
          // Camera
          MobileScanner(
            controller: _ctrl,
            onDetect: _onDetect,
          ),
          // Overlay mira
          _ScanOverlay(plx: plx),
          // Istruzione
          Positioned(
            bottom: 0,
            left: 0,
            right: 0,
            child: Container(
              padding: const EdgeInsets.fromLTRB(16, 16, 16, 32),
              decoration: const BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                  colors: [Colors.transparent, Colors.black87],
                ),
              ),
              child: Column(
                children: [
                  if (_error.isNotEmpty)
                    Container(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 14, vertical: 8),
                      margin: const EdgeInsets.only(bottom: 12),
                      decoration: BoxDecoration(
                        color: const Color(0xFFFF5555).withAlpha(200),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Text(_error,
                          style: const TextStyle(
                              color: Colors.white, fontSize: 13),
                          textAlign: TextAlign.center),
                    ),
                  Text(
                    'Punta la fotocamera sul QR code mostrato\n'
                    'nella tab LAN & WAN di Prismalux desktop',
                    style: TextStyle(
                      color: Colors.white.withAlpha(200),
                      fontSize: 13,
                      height: 1.5,
                    ),
                    textAlign: TextAlign.center,
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

// ── Mirino di scansione ───────────────────────────────────────────────────────

class _ScanOverlay extends StatelessWidget {
  final PlxColors plx;

  const _ScanOverlay({required this.plx});

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;
    final boxSize = size.width * 0.70;
    const cornerLen = 28.0;
    const cornerW = 3.5;

    return Center(
      child: SizedBox(
        width: boxSize,
        height: boxSize,
        child: Stack(
          children: [
            // Bordi angolari accent
            ..._corners(plx.acc, cornerLen, cornerW),
          ],
        ),
      ),
    );
  }

  List<Widget> _corners(Color color, double len, double width) {
    return [
      _corner(color, len, width, top: 0, left: 0,
          borderTop: true, borderLeft: true),
      _corner(color, len, width, top: 0, right: 0,
          borderTop: true, borderRight: true),
      _corner(color, len, width, bottom: 0, left: 0,
          borderBottom: true, borderLeft: true),
      _corner(color, len, width, bottom: 0, right: 0,
          borderBottom: true, borderRight: true),
    ];
  }

  Widget _corner(Color color, double len, double width,
      {double? top, double? bottom, double? left, double? right,
      bool borderTop = false, bool borderBottom = false,
      bool borderLeft = false, bool borderRight = false}) {
    return Positioned(
      top: top, bottom: bottom, left: left, right: right,
      child: SizedBox(
        width: len, height: len,
        child: CustomPaint(
          painter: _CornerPainter(
            color: color, width: width,
            drawTop: borderTop, drawBottom: borderBottom,
            drawLeft: borderLeft, drawRight: borderRight,
          ),
        ),
      ),
    );
  }
}

class _CornerPainter extends CustomPainter {
  final Color color;
  final double width;
  final bool drawTop, drawBottom, drawLeft, drawRight;

  _CornerPainter({
    required this.color,
    required this.width,
    required this.drawTop,
    required this.drawBottom,
    required this.drawLeft,
    required this.drawRight,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color
      ..strokeWidth = width
      ..strokeCap = StrokeCap.round
      ..style = PaintingStyle.stroke;

    if (drawTop)
      canvas.drawLine(Offset.zero, Offset(size.width, 0), paint);
    if (drawBottom)
      canvas.drawLine(
          Offset(0, size.height), Offset(size.width, size.height), paint);
    if (drawLeft)
      canvas.drawLine(Offset.zero, Offset(0, size.height), paint);
    if (drawRight)
      canvas.drawLine(
          Offset(size.width, 0), Offset(size.width, size.height), paint);
  }

  @override
  bool shouldRepaint(_CornerPainter o) => color != o.color;
}
