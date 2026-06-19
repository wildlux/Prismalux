import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../theme/plx_colors.dart';

class ThemeProvider extends ChangeNotifier {
  PlxColors _current = PlxThemes.webDefault;

  PlxColors get current => _current;

  ThemeProvider() {
    _load();
  }

  Future<void> _load() async {
    final prefs = await SharedPreferences.getInstance();
    final id = prefs.getString('plx_theme') ?? 'web_default';
    _current = PlxThemes.byId(id);
    notifyListeners();
  }

  Future<void> setTheme(PlxColors t) async {
    _current = t;
    notifyListeners();
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('plx_theme', t.id);
  }
}
