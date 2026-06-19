import 'package:shared_preferences/shared_preferences.dart';

class ServerConfig {
  final String host;
  final int port;
  final String token;

  const ServerConfig({
    required this.host,
    required this.port,
    required this.token,
  });

  String get baseUrl => 'http://$host:$port';

  Map<String, String> get headers => {
        'Content-Type': 'application/json',
        if (token.isNotEmpty) 'Authorization': 'Bearer $token',
      };

  static Future<ServerConfig> load() async {
    final prefs = await SharedPreferences.getInstance();
    return ServerConfig(
      host: prefs.getString('server_host') ?? '192.168.1.100',
      port: prefs.getInt('server_port') ?? 11500,
      token: prefs.getString('server_token') ?? '',
    );
  }

  Future<void> save() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('server_host', host);
    await prefs.setInt('server_port', port);
    await prefs.setString('server_token', token);
  }

  ServerConfig copyWith({String? host, int? port, String? token}) =>
      ServerConfig(
        host: host ?? this.host,
        port: port ?? this.port,
        token: token ?? this.token,
      );
}
