import 'package:flutter/material.dart';

/// Palette CSS-variabili identica alla webchat Prismalux.
/// bg=--bg  hdr=--hdr  brd=--brd  acc=--acc  aim=--aim  txt=--txt  dim=--dim
class PlxColors {
  final String id;
  final String label;
  final bool isDark;

  final Color bg;   // sfondo principale
  final Color hdr;  // header / card
  final Color brd;  // bordi
  final Color acc;  // accent
  final Color usr;  // bolla utente
  final Color aim;  // bolla AI
  final Color txt;  // testo primario
  final Color dim;  // testo secondario
  final Color inp;  // sfondo input

  const PlxColors({
    required this.id,
    required this.label,
    required this.isDark,
    required this.bg,
    required this.hdr,
    required this.brd,
    required this.acc,
    required this.usr,
    required this.aim,
    required this.txt,
    required this.dim,
    required this.inp,
  });

  // ── conversione in ThemeData Flutter ──────────────────────────────────────

  ThemeData toThemeData() {
    final cs = ColorScheme(
      brightness: isDark ? Brightness.dark : Brightness.light,
      primary: acc,
      onPrimary: _contrastFor(acc),
      secondary: acc,
      onSecondary: _contrastFor(acc),
      error: const Color(0xFFFF5555),
      onError: Colors.white,
      surface: hdr,
      onSurface: txt,
      surfaceContainerLowest: bg,
      surfaceContainerLow: bg,
      surfaceContainer: hdr,
      surfaceContainerHigh: aim,
      surfaceContainerHighest: brd,
      outline: brd,
      outlineVariant: brd.withAlpha(120),
      onSurfaceVariant: dim,
      primaryContainer: acc.withAlpha(40),
      onPrimaryContainer: acc,
      errorContainer: const Color(0xFF3D0000),
      onErrorContainer: const Color(0xFFFF8A8A),
    );

    return ThemeData(
      useMaterial3: true,
      colorScheme: cs,
      scaffoldBackgroundColor: bg,
      appBarTheme: AppBarTheme(
        backgroundColor: hdr,
        foregroundColor: txt,
        elevation: 0,
        scrolledUnderElevation: 0,
        titleTextStyle: TextStyle(
          color: txt,
          fontSize: 16,
          fontWeight: FontWeight.w700,
        ),
        iconTheme: IconThemeData(color: dim),
      ),
      cardTheme: CardTheme(
        color: hdr,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(10),
          side: BorderSide(color: brd),
        ),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: inp,
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(8),
          borderSide: BorderSide(color: brd),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(8),
          borderSide: BorderSide(color: brd),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(8),
          borderSide: BorderSide(color: acc),
        ),
        labelStyle: TextStyle(color: dim),
        hintStyle: TextStyle(color: dim),
        contentPadding:
            const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
        isDense: true,
      ),
      dividerTheme: DividerThemeData(color: brd, space: 1, thickness: 1),
      listTileTheme: ListTileThemeData(
        tileColor: Colors.transparent,
        selectedTileColor: acc.withAlpha(30),
        iconColor: dim,
        textColor: txt,
        selectedColor: acc,
      ),
      switchTheme: SwitchThemeData(
        thumbColor: WidgetStateProperty.resolveWith(
            (s) => s.contains(WidgetState.selected) ? acc : dim),
        trackColor: WidgetStateProperty.resolveWith(
            (s) => s.contains(WidgetState.selected)
                ? acc.withAlpha(80)
                : brd),
      ),
      dropdownMenuTheme: DropdownMenuThemeData(
        inputDecorationTheme: InputDecorationTheme(
          filled: true,
          fillColor: inp,
          border: OutlineInputBorder(
            borderRadius: BorderRadius.circular(8),
            borderSide: BorderSide(color: brd),
          ),
        ),
      ),
      navigationBarTheme: NavigationBarThemeData(
        backgroundColor: hdr,
        indicatorColor: acc.withAlpha(40),
        labelTextStyle: WidgetStateProperty.resolveWith((s) => TextStyle(
              color: s.contains(WidgetState.selected) ? acc : dim,
              fontSize: 11,
              fontWeight: s.contains(WidgetState.selected)
                  ? FontWeight.w600
                  : FontWeight.normal,
            )),
        iconTheme: WidgetStateProperty.resolveWith((s) => IconThemeData(
              color: s.contains(WidgetState.selected) ? acc : dim,
              size: 22,
            )),
        elevation: 0,
        height: 62,
      ),
      tabBarTheme: TabBarTheme(
        labelColor: acc,
        unselectedLabelColor: dim,
        indicatorColor: acc,
        indicatorSize: TabBarIndicatorSize.label,
        labelStyle: const TextStyle(
            fontSize: 11, fontWeight: FontWeight.w600),
        unselectedLabelStyle:
            const TextStyle(fontSize: 11),
        overlayColor:
            WidgetStateProperty.all(acc.withAlpha(20)),
        dividerColor: brd,
      ),
      textTheme: TextTheme(
        bodyLarge: TextStyle(color: txt, fontSize: 14),
        bodyMedium: TextStyle(color: txt, fontSize: 13),
        bodySmall: TextStyle(color: dim, fontSize: 11),
        labelLarge: TextStyle(color: txt, fontSize: 14),
        labelSmall: TextStyle(color: dim, fontSize: 10),
        titleMedium:
            TextStyle(color: txt, fontWeight: FontWeight.w600),
        titleSmall: TextStyle(color: dim, fontSize: 12),
      ),
      filledButtonTheme: FilledButtonThemeData(
        style: ButtonStyle(
          backgroundColor: WidgetStateProperty.resolveWith((s) =>
              s.contains(WidgetState.disabled) ? brd : acc),
          foregroundColor: WidgetStateProperty.all(_contrastFor(acc)),
          shape: WidgetStateProperty.all(
              RoundedRectangleBorder(borderRadius: BorderRadius.circular(8))),
          padding: WidgetStateProperty.all(
              const EdgeInsets.symmetric(horizontal: 16, vertical: 10)),
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: ButtonStyle(
          foregroundColor: WidgetStateProperty.all(txt),
          side: WidgetStateProperty.all(BorderSide(color: brd)),
          shape: WidgetStateProperty.all(
              RoundedRectangleBorder(borderRadius: BorderRadius.circular(8))),
        ),
      ),
      iconButtonTheme: IconButtonThemeData(
        style: ButtonStyle(
          foregroundColor: WidgetStateProperty.all(dim),
          overlayColor: WidgetStateProperty.all(acc.withAlpha(30)),
        ),
      ),
      drawerTheme: DrawerThemeData(
        backgroundColor: hdr,
        elevation: 0,
        shape: const RoundedRectangleBorder(
            borderRadius: BorderRadius.zero),
      ),
      bottomSheetTheme: BottomSheetThemeData(
        backgroundColor: hdr,
        shape: const RoundedRectangleBorder(
          borderRadius:
              BorderRadius.vertical(top: Radius.circular(14)),
        ),
      ),
      dialogTheme: DialogTheme(
        backgroundColor: hdr,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
          side: BorderSide(color: brd),
        ),
        titleTextStyle:
            TextStyle(color: txt, fontSize: 16, fontWeight: FontWeight.w700),
        contentTextStyle: TextStyle(color: dim),
      ),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: aim,
        contentTextStyle: TextStyle(color: txt),
        shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(8),
            side: BorderSide(color: brd)),
        behavior: SnackBarBehavior.floating,
      ),
    );
  }

  static Color _contrastFor(Color c) {
    final lum = c.computeLuminance();
    return lum > 0.35 ? Colors.black : Colors.white;
  }
}

// ── Catalogo temi ─────────────────────────────────────────────────────────────

class PlxThemes {
  PlxThemes._();

  static const webDefault = PlxColors(
    id: 'web_default', label: 'Web Default', isDark: true,
    bg:  Color(0xFF0F1117), hdr: Color(0xFF1A1D2E), brd: Color(0xFF2A2D4E),
    acc: Color(0xFF6C63FF), usr: Color(0xFF6C63FF), aim: Color(0xFF1E2235),
    txt: Color(0xFFE0E0F0), dim: Color(0xFF888888), inp: Color(0xFF0F1117),
  );

  static const darkCyan = PlxColors(
    id: 'dark_cyan', label: 'Dark Cyan', isDark: true,
    bg:  Color(0xFF0D0E17), hdr: Color(0xFF13141F), brd: Color(0xFF22233A),
    acc: Color(0xFF00B8D9), usr: Color(0xFF00B8D9), aim: Color(0xFF1A1B28),
    txt: Color(0xFFE8EAF6), dim: Color(0xFF7B7F9E), inp: Color(0xFF0D0E17),
  );

  static const darkPurple = PlxColors(
    id: 'dark_purple', label: 'Dark Purple', isDark: true,
    bg:  Color(0xFF0E0B14), hdr: Color(0xFF130F1C), brd: Color(0xFF261F34),
    acc: Color(0xFF9C5FF0), usr: Color(0xFF9C5FF0), aim: Color(0xFF1C1727),
    txt: Color(0xFFEDE8F5), dim: Color(0xFF6B5F88), inp: Color(0xFF0E0B14),
  );

  static const darkOcean = PlxColors(
    id: 'dark_ocean', label: 'Dark Ocean', isDark: true,
    bg:  Color(0xFF060C10), hdr: Color(0xFF0A1318), brd: Color(0xFF162834),
    acc: Color(0xFF20C4DA), usr: Color(0xFF20C4DA), aim: Color(0xFF0E1C24),
    txt: Color(0xFFD0EAF8), dim: Color(0xFF4A7A90), inp: Color(0xFF060C10),
  );

  static const darkAmber = PlxColors(
    id: 'dark_amber', label: 'Dark Amber', isDark: true,
    bg:  Color(0xFF100E08), hdr: Color(0xFF17140C), brd: Color(0xFF2C2414),
    acc: Color(0xFFFFB300), usr: Color(0xFFFFB300), aim: Color(0xFF211C10),
    txt: Color(0xFFF5E8CC), dim: Color(0xFF7A6A44), inp: Color(0xFF100E08),
  );

  static const darkSunset = PlxColors(
    id: 'dark_sunset', label: 'Dark Sunset', isDark: true,
    bg:  Color(0xFF100A05), hdr: Color(0xFF17100A), brd: Color(0xFF2C1E12),
    acc: Color(0xFFFD7E14), usr: Color(0xFFFD7E14), aim: Color(0xFF21160E),
    txt: Color(0xFFF8E8D0), dim: Color(0xFF8A6040), inp: Color(0xFF100A05),
  );

  static const darkGreen = PlxColors(
    id: 'dark_green', label: 'Dark Green', isDark: true,
    bg:  Color(0xFF080F0A), hdr: Color(0xFF0D1510), brd: Color(0xFF1A2C1F),
    acc: Color(0xFF4CAF50), usr: Color(0xFF4CAF50), aim: Color(0xFF121E15),
    txt: Color(0xFFD8F0DC), dim: Color(0xFF5A8060), inp: Color(0xFF080F0A),
  );

  static const darkLavender = PlxColors(
    id: 'dark_lavender', label: 'Dark Lavender', isDark: true,
    bg:  Color(0xFF08060F), hdr: Color(0xFF0F0C18), brd: Color(0xFF1E162E),
    acc: Color(0xFFB07FE0), usr: Color(0xFFB07FE0), aim: Color(0xFF161022),
    txt: Color(0xFFE8E0F5), dim: Color(0xFF7060A0), inp: Color(0xFF08060F),
  );

  static const darkRainbow = PlxColors(
    id: 'dark_rainbow', label: 'Dark Rainbow', isDark: true,
    bg:  Color(0xFF0A0A0A), hdr: Color(0xFF111118), brd: Color(0xFF232330),
    acc: Color(0xFFFF6B9D), usr: Color(0xFFFF6B9D), aim: Color(0xFF18181F),
    txt: Color(0xFFF0F0FF), dim: Color(0xFF7070A0), inp: Color(0xFF0A0A0A),
  );

  static const hacker = PlxColors(
    id: 'hacker', label: 'Hacker', isDark: true,
    bg:  Color(0xFF001100), hdr: Color(0xFF000000), brd: Color(0xFF003300),
    acc: Color(0xFF00FF00), usr: Color(0xFF004400), aim: Color(0xFF001A00),
    txt: Color(0xFF00FF00), dim: Color(0xFF00AA00), inp: Color(0xFF000800),
  );

  static const neon = PlxColors(
    id: 'neon', label: 'Neon', isDark: true,
    bg:  Color(0xFF000000), hdr: Color(0xFF0A0A0A), brd: Color(0xFF2A2A2A),
    acc: Color(0xFF00FF9D), usr: Color(0xFF00FF9D), aim: Color(0xFF1A1A1A),
    txt: Color(0xFFFFFFFF), dim: Color(0xFF80C0A0), inp: Color(0xFF000000),
  );

  static const solar = PlxColors(
    id: 'solar', label: 'Solar Dark', isDark: true,
    bg:  Color(0xFF001B26), hdr: Color(0xFF002B36), brd: Color(0xFF0F4C5C),
    acc: Color(0xFF268BD2), usr: Color(0xFF268BD2), aim: Color(0xFF073642),
    txt: Color(0xFF93A1A1), dim: Color(0xFF657B83), inp: Color(0xFF001B26),
  );

  static const military = PlxColors(
    id: 'military', label: 'Military', isDark: true,
    bg:  Color(0xFF0F1F0F), hdr: Color(0xFF1A2E1A), brd: Color(0xFF3D5A3D),
    acc: Color(0xFF6B8E23), usr: Color(0xFF6B8E23), aim: Color(0xFF2D4A2D),
    txt: Color(0xFFCCE0AA), dim: Color(0xFF7A9060), inp: Color(0xFF0F1F0F),
  );

  static const pink = PlxColors(
    id: 'pink', label: 'Pink', isDark: true,
    bg:  Color(0xFF1A0F1A), hdr: Color(0xFF2D1B2D), brd: Color(0xFF6B2A6B),
    acc: Color(0xFFFF69B4), usr: Color(0xFFFF69B4), aim: Color(0xFF4A1A4A),
    txt: Color(0xFFFFC0D0), dim: Color(0xFFAA70A0), inp: Color(0xFF1A0F1A),
  );

  static const venomBlue = PlxColors(
    id: 'venom_blue', label: 'Venom Blue', isDark: true,
    bg:  Color(0xFF000000), hdr: Color(0xFF0A0A0A), brd: Color(0xFF1A1A1A),
    acc: Color(0xFF00BFFF), usr: Color(0xFF00BFFF), aim: Color(0xFF101010),
    txt: Color(0xFFFFFFFF), dim: Color(0xFF606060), inp: Color(0xFF050505),
  );

  static const venomGreen = PlxColors(
    id: 'venom_green', label: 'Venom Green', isDark: true,
    bg:  Color(0xFF000000), hdr: Color(0xFF0A0A0A), brd: Color(0xFF1A1A1A),
    acc: Color(0xFF00FF00), usr: Color(0xFF003300), aim: Color(0xFF101010),
    txt: Color(0xFFFFFFFF), dim: Color(0xFF606060), inp: Color(0xFF050505),
  );

  static const venomRed = PlxColors(
    id: 'venom_red', label: 'Venom Red', isDark: true,
    bg:  Color(0xFF000000), hdr: Color(0xFF0A0A0A), brd: Color(0xFF1A1A1A),
    acc: Color(0xFFFF0000), usr: Color(0xFF330000), aim: Color(0xFF101010),
    txt: Color(0xFFFFFFFF), dim: Color(0xFF606060), inp: Color(0xFF050505),
  );

  static const light = PlxColors(
    id: 'light', label: 'Light', isDark: false,
    bg:  Color(0xFFF0F2F8), hdr: Color(0xFFFFFFFF), brd: Color(0xFFD8DDF0),
    acc: Color(0xFF0072C6), usr: Color(0xFF0072C6), aim: Color(0xFFE8ECF5),
    txt: Color(0xFF1A1E30), dim: Color(0xFF6B7199), inp: Color(0xFFFFFFFF),
  );

  static const lightRose = PlxColors(
    id: 'light_rose', label: 'Light Rose', isDark: false,
    bg:  Color(0xFFFFF5F7), hdr: Color(0xFFFFFFFF), brd: Color(0xFFF8C8D8),
    acc: Color(0xFFC2185B), usr: Color(0xFFC2185B), aim: Color(0xFFFCE4EC),
    txt: Color(0xFF2E0A18), dim: Color(0xFF7A4058), inp: Color(0xFFFFFFFF),
  );

  static const lightMint = PlxColors(
    id: 'light_mint', label: 'Light Mint', isDark: false,
    bg:  Color(0xFFF2FAF7), hdr: Color(0xFFFFFFFF), brd: Color(0xFFC8EBE0),
    acc: Color(0xFF00897B), usr: Color(0xFF00897B), aim: Color(0xFFE4F5EF),
    txt: Color(0xFF0F2820), dim: Color(0xFF5A8070), inp: Color(0xFFFFFFFF),
  );

  static const lightSky = PlxColors(
    id: 'light_sky', label: 'Light Sky', isDark: false,
    bg:  Color(0xFFF0F8FF), hdr: Color(0xFFFFFFFF), brd: Color(0xFFC0E0F8),
    acc: Color(0xFF0277BD), usr: Color(0xFF0277BD), aim: Color(0xFFE0F0FF),
    txt: Color(0xFF0A1E30), dim: Color(0xFF507090), inp: Color(0xFFFFFFFF),
  );

  static const lightSand = PlxColors(
    id: 'light_sand', label: 'Light Sand', isDark: false,
    bg:  Color(0xFFFDF8F0), hdr: Color(0xFFFFFFFF), brd: Color(0xFFFCE0B0),
    acc: Color(0xFFB07820), usr: Color(0xFFB07820), aim: Color(0xFFFEF0D8),
    txt: Color(0xFF2E2010), dim: Color(0xFF806040), inp: Color(0xFFFFFFFF),
  );

  static const all = <PlxColors>[
    webDefault, darkCyan, darkPurple, darkOcean, darkAmber, darkSunset,
    darkGreen, darkLavender, darkRainbow, hacker, neon, solar,
    military, pink, venomBlue, venomGreen, venomRed,
    light, lightRose, lightMint, lightSky, lightSand,
  ];

  static PlxColors byId(String id) =>
      all.firstWhere((t) => t.id == id, orElse: () => webDefault);
}
