import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:http/http.dart' as http;
import '../models/server_config.dart';
import '../models/chat_message.dart';

class ApiException implements Exception {
  final int statusCode;
  final String message;
  ApiException(this.statusCode, this.message);

  @override
  String toString() => 'ApiException($statusCode): $message';
}

class ApiService {
  ServerConfig _config;

  ApiService(this._config);

  void updateConfig(ServerConfig config) => _config = config;

  // ── helper ────────────────────────────────────────────────────────────────

  Uri _uri(String path, [Map<String, String>? query]) {
    final uri = Uri.parse('${_config.baseUrl}$path');
    return query != null ? uri.replace(queryParameters: query) : uri;
  }

  Future<Map<String, dynamic>> _postJson(
      String path, Map<String, dynamic> body) async {
    final resp = await http
        .post(_uri(path),
            headers: _config.headers,
            body: jsonEncode(body))
        .timeout(const Duration(seconds: 60));
    if (resp.statusCode != 200) {
      throw ApiException(resp.statusCode,
          _extractError(resp.body) ?? 'Errore ${resp.statusCode}');
    }
    return jsonDecode(resp.body) as Map<String, dynamic>;
  }

  Future<Map<String, dynamic>> _getJson(String path,
      [Map<String, String>? query]) async {
    final resp = await http
        .get(_uri(path, query), headers: _config.headers)
        .timeout(const Duration(seconds: 30));
    if (resp.statusCode != 200) {
      throw ApiException(resp.statusCode,
          _extractError(resp.body) ?? 'Errore ${resp.statusCode}');
    }
    return jsonDecode(resp.body) as Map<String, dynamic>;
  }

  String? _extractError(String body) {
    try {
      final m = jsonDecode(body) as Map<String, dynamic>;
      return (m['error'] ?? m['message'])?.toString();
    } catch (_) {
      return body.isNotEmpty ? body : null;
    }
  }

  // ── /api/tags — lista modelli ─────────────────────────────────────────────

  Future<List<String>> fetchModels() async {
    final data = await _getJson('/api/tags');
    final models = data['models'] as List<dynamic>? ?? [];
    return models
        .map((m) => (m is Map ? m['name'] : m)?.toString() ?? '')
        .where((s) => s.isNotEmpty)
        .toList();
  }

  // ── /api/chat — chat AI con streaming ────────────────────────────────────

  Stream<String> chatStream({
    required List<ChatMessage> messages,
    String model = '',
    String systemPrompt = '',
  }) async* {
    final msgs = <Map<String, dynamic>>[];
    if (systemPrompt.isNotEmpty) {
      msgs.add({'role': 'system', 'content': systemPrompt});
    }
    msgs.addAll(messages.map((m) => m.toJson()));

    final body = jsonEncode({
      'model': model,
      'messages': msgs,
    });

    final request = http.Request('POST', _uri('/api/chat'))
      ..headers.addAll(_config.headers)
      ..body = body;

    final resp = await request.send().timeout(const Duration(minutes: 5));

    if (resp.statusCode != 200) {
      final err = await resp.stream.bytesToString();
      throw ApiException(resp.statusCode, _extractError(err) ?? 'Errore chat');
    }

    await for (final line in resp.stream
        .transform(utf8.decoder)
        .transform(const LineSplitter())) {
      if (line.trim().isEmpty) continue;
      try {
        final obj = jsonDecode(line) as Map<String, dynamic>;
        final content =
            (obj['message'] as Map<String, dynamic>?)?['content'] as String?;
        if (content != null && content.isNotEmpty) yield content;
      } catch (_) {
        // riga non-JSON (es. whitespace, header): ignora
      }
    }
  }

  // ── /api/generate — generazione testo ────────────────────────────────────

  Stream<String> generateStream({
    required String prompt,
    String model = '',
    String system = '',
  }) async* {
    final body = jsonEncode({
      'model': model,
      'prompt': prompt,
      if (system.isNotEmpty) 'system': system,
    });

    final request = http.Request('POST', _uri('/api/generate'))
      ..headers.addAll(_config.headers)
      ..body = body;

    final resp = await request.send().timeout(const Duration(minutes: 5));

    if (resp.statusCode != 200) {
      final err = await resp.stream.bytesToString();
      throw ApiException(
          resp.statusCode, _extractError(err) ?? 'Errore generate');
    }

    await for (final line in resp.stream
        .transform(utf8.decoder)
        .transform(const LineSplitter())) {
      if (line.trim().isEmpty) continue;
      try {
        final obj = jsonDecode(line) as Map<String, dynamic>;
        final response = obj['response'] as String?;
        if (response != null && response.isNotEmpty) yield response;
      } catch (_) {}
    }
  }

  // ── /api/finanza/cf — codice fiscale ─────────────────────────────────────

  Future<Map<String, dynamic>> calcolaCodiceFiscale({
    required String cognome,
    required String nome,
    required String data, // YYYY-MM-DD
    required String sesso,
    required String comune,
  }) async {
    return _postJson('/api/finanza/cf', {
      'cognome': cognome,
      'nome': nome,
      'data': data,
      'sesso': sesso,
      'comune': comune,
    });
  }

  // ── /api/finanza/tfr — calcolo TFR ───────────────────────────────────────

  Future<Map<String, dynamic>> calcolaTfr({
    required double stipendioAnnuo,
    required int anni,
    double inflazione = 2.0,
    bool fondoPensione = false,
  }) async {
    return _postJson('/api/finanza/tfr', {
      'stipendio_annuo': stipendioAnnuo,
      'anni': anni,
      'inflazione': inflazione,
      'fondo_pensione': fondoPensione,
    });
  }

  // ── /api/repl — REPL Python ───────────────────────────────────────────────

  Future<Map<String, dynamic>> runRepl(String code) async {
    return _postJson('/api/repl', {'code': code});
  }

  // ── /api/file — estrai testo da file ─────────────────────────────────────

  Future<Map<String, dynamic>> uploadFile(
      Uint8List bytes, String filename) async {
    final request = http.MultipartRequest('POST', _uri('/api/file'))
      ..headers.addAll(
          Map.from(_config.headers)..remove('Content-Type'))
      ..files.add(http.MultipartFile.fromBytes('file', bytes,
          filename: filename));

    final resp = await request.send().timeout(const Duration(minutes: 2));
    final body = await resp.stream.bytesToString();
    if (resp.statusCode != 200) {
      throw ApiException(resp.statusCode,
          _extractError(body) ?? 'Errore upload file');
    }
    return jsonDecode(body) as Map<String, dynamic>;
  }

  // ── /api/math — calcoli matematici ───────────────────────────────────────

  Future<Map<String, dynamic>> math(
      String action, Map<String, dynamic> params) async {
    return _postJson('/api/math', {'action': action, ...params});
  }

  // ── /api/graphviz — rendering DOT ────────────────────────────────────────

  Future<Uint8List> renderGraphviz(String dot) async {
    final resp = await http
        .post(_uri('/api/graphviz'),
            headers: _config.headers,
            body: jsonEncode({'dot': dot}))
        .timeout(const Duration(seconds: 30));
    if (resp.statusCode != 200) {
      throw ApiException(
          resp.statusCode, _extractError(resp.body) ?? 'Errore graphviz');
    }
    return resp.bodyBytes;
  }

  // ── /api/wiki — Wikipedia ─────────────────────────────────────────────────

  Future<Map<String, dynamic>> wiki(String query,
      {String lang = 'it'}) async {
    return _getJson('/api/wiki', {'q': query, 'lang': lang});
  }

  // ── /api/lavoro — ricerca offerte ─────────────────────────────────────────

  Future<Map<String, dynamic>> searchJobs(String query) async {
    return _getJson('/api/lavoro', {'q': query});
  }

  Future<Map<String, dynamic>> analyzeJobPost(String text) async {
    return _postJson('/api/lavoro', {'text': text});
  }

  // ── /api/cv — curriculum vitae ────────────────────────────────────────────

  Future<Map<String, dynamic>> getCv() async {
    return _getJson('/api/cv');
  }

  // ── /api/git — comandi git ────────────────────────────────────────────────

  Future<Map<String, dynamic>> gitCommand(String cmd) async {
    return _postJson('/api/git', {'cmd': cmd});
  }

  // ── /api/rag — ricerca semantica ─────────────────────────────────────────

  Future<Map<String, dynamic>> ragQuery(String query,
      {int limit = 5}) async {
    return _postJson('/api/rag', {'query': query, 'limit': limit});
  }

  // ── /api/sysinfo — info sistema ──────────────────────────────────────────

  Future<Map<String, dynamic>> sysinfo() async {
    return _getJson('/api/sysinfo');
  }

  // ── /api/datetime — data/ora ─────────────────────────────────────────────

  Future<Map<String, dynamic>> datetime() async {
    return _getJson('/api/datetime');
  }

  // ── /api/graph — GraphMemory ─────────────────────────────────────────────

  Future<Map<String, dynamic>> graphDot() async {
    return _getJson('/api/graph/dot');
  }

  Future<Map<String, dynamic>> graphNodes(String query,
      {int limit = 20}) async {
    return _getJson('/api/graph/nodes', {'q': query, 'limit': '$limit'});
  }

  // ── /api/whisper — trascrizione audio ────────────────────────────────────

  Future<Map<String, dynamic>> transcribeAudio(
      Uint8List audioBytes, String filename) async {
    final request = http.MultipartRequest('POST', _uri('/api/whisper'))
      ..headers.addAll(
          Map.from(_config.headers)..remove('Content-Type'))
      ..files.add(http.MultipartFile.fromBytes('audio', audioBytes,
          filename: filename));

    final resp = await request.send().timeout(const Duration(minutes: 3));
    final body = await resp.stream.bytesToString();
    if (resp.statusCode != 200) {
      throw ApiException(
          resp.statusCode, _extractError(body) ?? 'Errore whisper');
    }
    return jsonDecode(body) as Map<String, dynamic>;
  }

  // ── /api/mcp — MCP tools ─────────────────────────────────────────────────

  Future<Map<String, dynamic>> mcpCall(
      String server, String tool, Map<String, dynamic> args) async {
    return _postJson('/api/mcp', {
      'server': server,
      'tool': tool,
      'args': args,
    });
  }

  // ── ping — verifica connessione ───────────────────────────────────────────
  // Usa GET / (endpoint pubblico, non richiede token) per il check.
  // Fallback su /api/tags (con token) se il server risponde 200.

  Future<bool> ping() async {
    try {
      final resp = await http
          .get(_uri('/'), headers: _config.headers)
          .timeout(const Duration(seconds: 6));
      // Il server risponde 200 su / (redirect alla webchat) → connesso
      if (resp.statusCode < 500) return true;
      return false;
    } catch (_) {
      return false;
    }
  }
}
