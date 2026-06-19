import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'providers/app_provider.dart';
import 'providers/theme_provider.dart';
import 'theme/plx_colors.dart';
import 'screens/chat_screen.dart';
import 'screens/tools_screen.dart';
import 'screens/research_screen.dart';
import 'screens/dev_screen.dart';
import 'screens/sysinfo_screen.dart';
import 'screens/settings_screen.dart';

class PrismaluxApp extends StatelessWidget {
  const PrismaluxApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => ThemeProvider()),
        ChangeNotifierProvider(create: (_) => AppProvider()),
      ],
      child: Consumer<ThemeProvider>(
        builder: (_, thProv, __) => MaterialApp(
          title: 'Prismalux',
          debugShowCheckedModeBanner: false,
          theme: thProv.current.toThemeData(),
          home: const _HomeShell(),
        ),
      ),
    );
  }
}

// ── Shell principale ──────────────────────────────────────────────────────────

class _HomeShell extends StatefulWidget {
  const _HomeShell();

  @override
  State<_HomeShell> createState() => _HomeShellState();
}

class _HomeShellState extends State<_HomeShell> {
  int _idx = 0;

  static const _tabs = [
    _Tab(icon: Icons.chat_bubble_outline, activeIcon: Icons.chat_bubble,
        label: 'Chat'),
    _Tab(icon: Icons.build_outlined, activeIcon: Icons.build,
        label: 'Strumenti'),
    _Tab(icon: Icons.search, activeIcon: Icons.search, label: 'Ricerca'),
    _Tab(icon: Icons.terminal_outlined, activeIcon: Icons.terminal,
        label: 'Dev'),
    _Tab(icon: Icons.monitor_heart_outlined, activeIcon: Icons.monitor_heart,
        label: 'Sistema'),
    _Tab(icon: Icons.settings_outlined, activeIcon: Icons.settings,
        label: 'Config'),
  ];

  static const _screens = <Widget>[
    ChatScreen(),
    ToolsScreen(),
    ResearchScreen(),
    DevScreen(),
    SysinfoScreen(),
    SettingsScreen(),
  ];

  @override
  Widget build(BuildContext context) {
    final thProv = context.watch<ThemeProvider>();
    final plx = thProv.current;
    final appProv = context.watch<AppProvider>();

    return Scaffold(
      backgroundColor: plx.bg,
      appBar: _PlxAppBar(
        plx: plx,
        thProv: thProv,
        isConnected: appProv.isConnected,
        selectedModel: appProv.selectedModel,
        currentTabIdx: _idx,
      ),
      body: IndexedStack(index: _idx, children: _screens),
      bottomNavigationBar: _PlxNavBar(
        plx: plx,
        currentIndex: _idx,
        tabs: _tabs,
        onTap: (i) => setState(() => _idx = i),
      ),
    );
  }
}

// ── AppBar personalizzata ─────────────────────────────────────────────────────

class _PlxAppBar extends StatelessWidget implements PreferredSizeWidget {
  final PlxColors plx;
  final ThemeProvider thProv;
  final bool isConnected;
  final String selectedModel;
  final int currentTabIdx;

  const _PlxAppBar({
    required this.plx,
    required this.thProv,
    required this.isConnected,
    required this.selectedModel,
    required this.currentTabIdx,
  });

  @override
  Size get preferredSize => const Size.fromHeight(48);

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 48 + MediaQuery.of(context).padding.top,
      padding: EdgeInsets.only(top: MediaQuery.of(context).padding.top),
      decoration: BoxDecoration(
        color: plx.hdr,
        border: Border(bottom: BorderSide(color: plx.brd)),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12),
        child: Row(
          children: [
            // Logo
            Image.asset('assets/icon.png',
                width: 26, height: 26,
                filterQuality: FilterQuality.medium),
            const SizedBox(width: 6),
            Text('Prismalux',
                style: TextStyle(
                    color: plx.txt,
                    fontSize: 15,
                    fontWeight: FontWeight.w700)),
            const SizedBox(width: 8),
            // Badge modello
            if (selectedModel.isNotEmpty)
              Container(
                padding: const EdgeInsets.symmetric(
                    horizontal: 8, vertical: 2),
                decoration: BoxDecoration(
                  border: Border.all(color: plx.brd),
                  borderRadius: BorderRadius.circular(20),
                ),
                constraints: const BoxConstraints(maxWidth: 110),
                child: Text(
                  selectedModel,
                  style: TextStyle(
                      color: plx.acc, fontSize: 10),
                  overflow: TextOverflow.ellipsis,
                ),
              ),
            const Spacer(),
            // Indicatore connessione
            Container(
              padding: const EdgeInsets.symmetric(
                  horizontal: 8, vertical: 3),
              decoration: BoxDecoration(
                color: isConnected
                    ? plx.acc.withAlpha(30)
                    : Colors.red.withAlpha(30),
                borderRadius: BorderRadius.circular(20),
                border: Border.all(
                    color: isConnected
                        ? plx.brd
                        : Colors.red.withAlpha(80)),
              ),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(
                    isConnected ? Icons.wifi : Icons.wifi_off,
                    size: 12,
                    color: isConnected ? plx.acc : Colors.red,
                  ),
                  const SizedBox(width: 4),
                  Text(
                    isConnected ? 'Online' : 'Offline',
                    style: TextStyle(
                      fontSize: 10,
                      color: isConnected ? plx.acc : Colors.red,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(width: 4),
            // Selettore tema
            _ThemeButton(plx: plx, thProv: thProv),
          ],
        ),
      ),
    );
  }
}

// ── Pulsante cambio tema ──────────────────────────────────────────────────────

class _ThemeButton extends StatelessWidget {
  final PlxColors plx;
  final ThemeProvider thProv;

  const _ThemeButton({required this.plx, required this.thProv});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: () => _showThemePicker(context),
      child: Container(
        padding: const EdgeInsets.all(6),
        decoration: BoxDecoration(
          border: Border.all(color: plx.brd),
          borderRadius: BorderRadius.circular(7),
        ),
        child: Icon(Icons.palette_outlined, size: 16, color: plx.dim),
      ),
    );
  }

  void _showThemePicker(BuildContext context) {
    showModalBottomSheet(
      context: context,
      backgroundColor: plx.hdr,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(14)),
      ),
      builder: (_) => _ThemePicker(plx: plx, thProv: thProv),
    );
  }
}

class _ThemePicker extends StatelessWidget {
  final PlxColors plx;
  final ThemeProvider thProv;

  const _ThemePicker({required this.plx, required this.thProv});

  @override
  Widget build(BuildContext context) {
    final darkThemes =
        PlxThemes.all.where((t) => t.isDark).toList();
    final lightThemes =
        PlxThemes.all.where((t) => !t.isDark).toList();

    return DraggableScrollableSheet(
      expand: false,
      initialChildSize: 0.55,
      maxChildSize: 0.85,
      builder: (_, ctrl) => Column(
        children: [
          Container(
            height: 4,
            width: 40,
            margin: const EdgeInsets.symmetric(vertical: 8),
            decoration: BoxDecoration(
              color: plx.brd,
              borderRadius: BorderRadius.circular(2),
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(
                horizontal: 16, vertical: 6),
            child: Row(
              children: [
                Text('Tema',
                    style: TextStyle(
                        color: plx.txt,
                        fontSize: 15,
                        fontWeight: FontWeight.w700)),
              ],
            ),
          ),
          Expanded(
            child: ListView(
              controller: ctrl,
              padding: const EdgeInsets.symmetric(horizontal: 12),
              children: [
                _themeSection('Scuri', darkThemes),
                _themeSection('Chiari', lightThemes),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _themeSection(String title, List<PlxColors> themes) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding:
              const EdgeInsets.symmetric(vertical: 8, horizontal: 4),
          child: Text(title.toUpperCase(),
              style: TextStyle(
                  color: plx.dim,
                  fontSize: 10,
                  letterSpacing: 0.8,
                  fontWeight: FontWeight.w700)),
        ),
        ...themes.map((t) => _themeRow(t)),
        const SizedBox(height: 4),
      ],
    );
  }

  Widget _themeRow(PlxColors t) {
    final isSelected = thProv.current.id == t.id;
    return GestureDetector(
      onTap: () => thProv.setTheme(t),
      child: Container(
        margin: const EdgeInsets.only(bottom: 4),
        padding:
            const EdgeInsets.symmetric(horizontal: 12, vertical: 9),
        decoration: BoxDecoration(
          color: isSelected
              ? t.acc.withAlpha(30)
              : Colors.transparent,
          border: Border.all(
              color: isSelected ? t.acc : plx.brd),
          borderRadius: BorderRadius.circular(8),
        ),
        child: Row(
          children: [
            // Anteprima colori
            Row(
              children: [
                _dot(t.bg),
                _dot(t.hdr),
                _dot(t.acc),
                _dot(t.aim),
              ],
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Text(t.label,
                  style: TextStyle(
                      color: isSelected ? t.acc : plx.txt,
                      fontSize: 13,
                      fontWeight: isSelected
                          ? FontWeight.w600
                          : FontWeight.normal)),
            ),
            if (isSelected)
              Icon(Icons.check, size: 14, color: t.acc),
          ],
        ),
      ),
    );
  }

  Widget _dot(Color c) => Container(
        width: 14,
        height: 14,
        margin: const EdgeInsets.only(right: 3),
        decoration: BoxDecoration(
          color: c,
          shape: BoxShape.circle,
          border: Border.all(
              color: Colors.white.withAlpha(30), width: 0.5),
        ),
      );
}

// ── NavigationBar personalizzata ─────────────────────────────────────────────

class _PlxNavBar extends StatelessWidget {
  final PlxColors plx;
  final int currentIndex;
  final List<_Tab> tabs;
  final ValueChanged<int> onTap;

  const _PlxNavBar({
    required this.plx,
    required this.currentIndex,
    required this.tabs,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: plx.hdr,
        border: Border(top: BorderSide(color: plx.brd)),
      ),
      child: SafeArea(
        child: SizedBox(
          height: 54,
          child: Row(
            children: List.generate(tabs.length, (i) {
              final t = tabs[i];
              final sel = i == currentIndex;
              return Expanded(
                child: GestureDetector(
                  behavior: HitTestBehavior.opaque,
                  onTap: () => onTap(i),
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      AnimatedContainer(
                        duration: const Duration(milliseconds: 150),
                        padding: const EdgeInsets.symmetric(
                            horizontal: 12, vertical: 4),
                        decoration: BoxDecoration(
                          color: sel
                              ? plx.acc.withAlpha(35)
                              : Colors.transparent,
                          borderRadius: BorderRadius.circular(20),
                        ),
                        child: Icon(
                          sel ? t.activeIcon : t.icon,
                          size: 20,
                          color: sel ? plx.acc : plx.dim,
                        ),
                      ),
                      const SizedBox(height: 1),
                      Text(
                        t.label,
                        style: TextStyle(
                          fontSize: 10,
                          color: sel ? plx.acc : plx.dim,
                          fontWeight: sel
                              ? FontWeight.w600
                              : FontWeight.normal,
                        ),
                      ),
                    ],
                  ),
                ),
              );
            }),
          ),
        ),
      ),
    );
  }
}

class _Tab {
  final IconData icon;
  final IconData activeIcon;
  final String label;
  const _Tab(
      {required this.icon,
      required this.activeIcon,
      required this.label});
}
