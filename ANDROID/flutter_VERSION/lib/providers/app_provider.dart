import 'package:flutter/foundation.dart';
import '../models/server_config.dart';
import '../models/chat_message.dart';
import '../services/api_service.dart';

class AppProvider extends ChangeNotifier {
  late ServerConfig _config;
  late ApiService _api;

  List<ChatMessage> _messages = [];
  List<String> _models = [];
  String _selectedModel = '';
  bool _isStreaming = false;
  String _connectionStatus = 'Non connesso';
  bool _isConnected = false;

  ServerConfig get config => _config;
  ApiService get api => _api;
  List<ChatMessage> get messages => List.unmodifiable(_messages);
  List<String> get models => List.unmodifiable(_models);
  String get selectedModel => _selectedModel;
  bool get isStreaming => _isStreaming;
  String get connectionStatus => _connectionStatus;
  bool get isConnected => _isConnected;

  AppProvider() {
    _config = const ServerConfig(
        host: '192.168.1.100', port: 11500, token: '');
    _api = ApiService(_config);
    _loadConfig();
  }

  Future<void> _loadConfig() async {
    _config = await ServerConfig.load();
    _api.updateConfig(_config);
    notifyListeners();
    await testConnection();
  }

  Future<void> updateConfig(ServerConfig config) async {
    _config = config;
    _api.updateConfig(config);
    await config.save();
    notifyListeners();
    await testConnection();
  }

  Future<void> testConnection() async {
    _connectionStatus = 'Connessione a ${_config.host}:${_config.port}...';
    _isConnected = false;
    notifyListeners();
    try {
      final ok = await _api.ping();
      _isConnected = ok;
      _connectionStatus = ok
          ? 'Connesso a ${_config.host}:${_config.port}'
          : 'Non raggiungibile — verifica IP e che il server LAN sia avviato';
    } catch (e) {
      _isConnected = false;
      _connectionStatus = 'Errore: $e';
    }
    notifyListeners();
    if (_isConnected) await loadModels();
  }

  Future<void> loadModels() async {
    try {
      _models = await _api.fetchModels();
      if (_models.isNotEmpty && _selectedModel.isEmpty) {
        _selectedModel = _models.first;
      }
      notifyListeners();
    } catch (_) {}
  }

  void selectModel(String model) {
    _selectedModel = model;
    notifyListeners();
  }

  void clearChat() {
    _messages = [];
    notifyListeners();
  }

  Future<void> sendMessage(String text, {String systemPrompt = ''}) async {
    if (text.trim().isEmpty || _isStreaming) return;

    _messages.add(ChatMessage(role: MessageRole.user, content: text.trim()));
    final assistantIdx = _messages.length;
    _messages.add(ChatMessage(
        role: MessageRole.assistant, content: '', isStreaming: true));
    _isStreaming = true;
    notifyListeners();

    final buffer = StringBuffer();
    try {
      await for (final chunk in _api.chatStream(
        messages: _messages
            .where((m) => m.role != MessageRole.assistant || !m.isStreaming)
            .toList()
          ..removeLast(),
        model: _selectedModel,
        systemPrompt: systemPrompt,
      )) {
        buffer.write(chunk);
        _messages[assistantIdx] =
            _messages[assistantIdx].copyWith(content: buffer.toString());
        notifyListeners();
      }
    } catch (e) {
      buffer.write('\n\n[Errore: $e]');
    }

    _messages[assistantIdx] = _messages[assistantIdx]
        .copyWith(content: buffer.toString(), isStreaming: false);
    _isStreaming = false;
    notifyListeners();
  }
}
