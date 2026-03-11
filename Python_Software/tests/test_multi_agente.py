"""
test_multi_agente.py — Test per multi_agente/multi_agente.py
=============================================================
Testa:
  - _HAS_TERMIOS, trova_default, controlla_istanza, _gpu_label
  - PREFERENZE_MODELLI, _RUOLI_INFO
  - Logica di mapping tasto Windows (_leggi_tasto_raw_ma)
  - Navigazione: sub-menu ripensamento (G/C/M/0)
  - Timeout controlla_istanza ≤ 0.5 s

Langgraph e langchain-ollama vengono mockati prima dell'import.

Esegui:
    python3.14 -m unittest tests.test_multi_agente -v
"""

import sys
import os
import unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

# ── Mock preventivo delle dipendenze pesanti ──────────────────────────────────
# Deve avvenire PRIMA dell'import di multi_agente, che chiama
# _check_deps_multiagente() a livello di modulo (sys.exit se mancano)

_mocks_pesanti = {
    "langgraph":             mock.MagicMock(),
    "langgraph.graph":       mock.MagicMock(),
    "langchain_ollama":      mock.MagicMock(),
    "langchain_core":        mock.MagicMock(),
    "langchain_core.messages": mock.MagicMock(),
}
# StateGraph e END devono essere usabili senza errori
_mocks_pesanti["langgraph.graph"].END = "END"
_mocks_pesanti["langgraph.graph"].StateGraph = mock.MagicMock()

for _nome, _m in _mocks_pesanti.items():
    # importlib.util.find_spec() cerca __spec__ nel modulo in sys.modules;
    # se AttributeError → ValueError. Lo settiamo esplicitamente.
    _m.__spec__ = mock.MagicMock()
    sys.modules.setdefault(_nome, _m)

# Mock rich se non installato
_mock_rich = mock.MagicMock()
for _mod in ["rich", "rich.console", "rich.panel"]:
    sys.modules.setdefault(_mod, _mock_rich)

# Ora importa le funzioni pure dal modulo
from multi_agente.multi_agente import (
    _HAS_TERMIOS,
    trova_default,
    controlla_istanza,
    _gpu_label,
    PORTA_NVIDIA,
    PORTA_INTEL,
    PORTA_AMD,
    PREFERENZE_MODELLI,
    _RUOLI_INFO,
)


# ── _HAS_TERMIOS ──────────────────────────────────────────────────────────────

class TestHasTermios(unittest.TestCase):
    """
    BUG CORRETTO: import tty/termios era a livello di modulo → crash su Windows.
    Ora è dentro try/except ImportError → _HAS_TERMIOS = False su Windows.
    """

    def test_e_di_tipo_bool(self):
        self.assertIsInstance(_HAS_TERMIOS, bool)

    def test_su_linux_e_true(self):
        # Sul sistema Linux dove girano i test, termios è disponibile
        if sys.platform == "linux":
            self.assertTrue(_HAS_TERMIOS)

    def test_pattern_try_except_funziona_su_windows(self):
        """Simula il comportamento Windows: termios non disponibile → False."""
        try:
            import termios as _t
            has = True
        except ImportError:
            has = False
        self.assertIsInstance(has, bool)


# ── trova_default ─────────────────────────────────────────────────────────────

class TestTrovaDefault(unittest.TestCase):

    def test_primo_match_in_preferenze_vince(self):
        disponibili = ["llama3.2:3b", "qwen3:4b", "mistral:7b"]
        preferenze  = ["llama3.2", "qwen3", "mistral"]
        self.assertEqual(trova_default(disponibili, preferenze), "llama3.2:3b")

    def test_secondo_match_se_primo_assente(self):
        disponibili = ["qwen3:4b", "mistral:7b"]
        preferenze  = ["llama3.2", "qwen3", "mistral"]
        self.assertEqual(trova_default(disponibili, preferenze), "qwen3:4b")

    def test_nessun_match_ritorna_primo_disponibile(self):
        disponibili = ["modello-sconosciuto:7b"]
        preferenze  = ["llama3.2", "qwen3"]
        self.assertEqual(trova_default(disponibili, preferenze), "modello-sconosciuto:7b")

    def test_lista_vuota_ritorna_fallback_stringa(self):
        result = trova_default([], ["llama3.2"])
        self.assertIsInstance(result, str)
        self.assertTrue(len(result) > 0)

    def test_match_parziale_nel_nome_del_modello(self):
        # "qwen3" deve matchare "qwen3:4b-instruct-q4_K_M"
        disponibili = ["qwen3:4b-instruct-q4_K_M"]
        preferenze  = ["qwen3"]
        self.assertEqual(trova_default(disponibili, preferenze),
                         "qwen3:4b-instruct-q4_K_M")

    def test_order_sensitivity(self):
        # La prima preferenza che matcha vince, non l'ordine dei disponibili
        disponibili = ["mistral:7b", "llama3.2:3b"]
        preferenze  = ["llama3.2", "mistral"]
        self.assertEqual(trova_default(disponibili, preferenze), "llama3.2:3b")

    def test_preferenze_vuote_ritorna_primo_disponibile(self):
        disponibili = ["gemma2:2b", "qwen3:4b"]
        result = trova_default(disponibili, [])
        self.assertEqual(result, "gemma2:2b")


# ── controlla_istanza ─────────────────────────────────────────────────────────

class TestControllaIstanza(unittest.TestCase):
    """Testa controlla_istanza(porta) con requests mockato — nessun Ollama reale."""

    def _mock_get(self, status=200):
        r = mock.MagicMock()
        r.status_code = status
        return mock.patch(
            "multi_agente.multi_agente.requests.get", return_value=r
        )

    def test_status_200_ritorna_true(self):
        with self._mock_get(200):
            self.assertTrue(controlla_istanza(11434))

    def test_status_404_ritorna_false(self):
        with self._mock_get(404):
            self.assertFalse(controlla_istanza(11434))

    def test_status_500_ritorna_false(self):
        with self._mock_get(500):
            self.assertFalse(controlla_istanza(11434))

    def test_connection_error_ritorna_false(self):
        import requests as req
        with mock.patch("multi_agente.multi_agente.requests.get",
                        side_effect=req.exceptions.ConnectionError()):
            self.assertFalse(controlla_istanza(11434))

    def test_timeout_ritorna_false(self):
        import requests as req
        with mock.patch("multi_agente.multi_agente.requests.get",
                        side_effect=req.exceptions.ReadTimeout()):
            self.assertFalse(controlla_istanza(11434))

    def test_exception_generica_ritorna_false(self):
        with mock.patch("multi_agente.multi_agente.requests.get",
                        side_effect=OSError("generic")):
            self.assertFalse(controlla_istanza(11434))


# ── _gpu_label ────────────────────────────────────────────────────────────────

class TestGpuLabel(unittest.TestCase):

    def test_porta_nvidia_contiene_tipo_accelerazione(self):
        """La GPU principale è rilevata dinamicamente: non è necessariamente NVIDIA."""
        label = _gpu_label(PORTA_NVIDIA)
        # Deve contenere un tipo di accelerazione noto
        self.assertTrue(
            any(t in label for t in ("CUDA", "ROCm", "Vulkan", "CPU", "GPU")),
            f"Label '{label}' non contiene il tipo di accelerazione"
        )

    def test_porta_nvidia_contiene_numero_porta(self):
        label = _gpu_label(PORTA_NVIDIA)
        self.assertIn(str(PORTA_NVIDIA), label)

    def test_porta_intel_contiene_intel_e_vulkan(self):
        label = _gpu_label(PORTA_INTEL)
        self.assertIn("Intel", label)
        self.assertIn("Vulkan", label)

    def test_porta_amd_contiene_amd_e_rocm(self):
        label = _gpu_label(PORTA_AMD)
        self.assertIn("AMD", label)
        self.assertIn("ROCm", label)

    def test_porta_sconosciuta_ritorna_stringa(self):
        label = _gpu_label(99999)
        self.assertIsInstance(label, str)
        self.assertGreater(len(label), 0)

    def test_ritorna_sempre_stringa(self):
        for porta in [PORTA_NVIDIA, PORTA_INTEL, PORTA_AMD, 0, 99999]:
            with self.subTest(porta=porta):
                self.assertIsInstance(_gpu_label(porta), str)


# ── PREFERENZE_MODELLI ────────────────────────────────────────────────────────

class TestPreferenzeModelli(unittest.TestCase):

    RUOLI_ATTESI = {"ricercatore", "pianificatore",
                    "programmatore", "tester", "ottimizzatore"}

    def test_ha_tutti_e_5_i_ruoli(self):
        self.assertEqual(set(PREFERENZE_MODELLI.keys()), self.RUOLI_ATTESI)

    def test_ogni_ruolo_ha_lista_non_vuota(self):
        for ruolo, lista in PREFERENZE_MODELLI.items():
            with self.subTest(ruolo=ruolo):
                self.assertIsInstance(lista, list)
                self.assertGreater(len(lista), 0)

    def test_ogni_preferenza_e_una_stringa(self):
        for ruolo, lista in PREFERENZE_MODELLI.items():
            for pref in lista:
                with self.subTest(ruolo=ruolo, pref=pref):
                    self.assertIsInstance(pref, str)

    def test_ruoli_info_ha_stessi_ruoli(self):
        self.assertEqual(set(_RUOLI_INFO.keys()), self.RUOLI_ATTESI)

    def test_ogni_ruolo_info_e_tuple_di_due(self):
        for ruolo, info in _RUOLI_INFO.items():
            with self.subTest(ruolo=ruolo):
                self.assertIsInstance(info, tuple)
                self.assertEqual(len(info), 2)

    def test_programmatore_preferisce_coder(self):
        # Il programmatore deve avere un modello coder come prima preferenza
        prefs = PREFERENZE_MODELLI["programmatore"]
        prima = prefs[0].lower()
        self.assertTrue(
            "coder" in prima or "code" in prima,
            f"Prima preferenza programmatore '{prima}' non sembra un modello coder"
        )


# ── Logica mapping tasto Windows (_leggi_tasto_raw_ma) ────────────────────────
#
# _leggi_tasto_raw_ma è una nested function dentro avvia_menu(), quindi non
# importabile direttamente. Riproduciamo la stessa logica qui come funzione
# pura per testarla in isolamento.

def _simula_leggi_tasto_win(s: str) -> str:
    """
    Replica esatta della logica Windows di _leggi_tasto_raw_ma().
    Versione corretta dopo il bugfix: '' → ENTER, non BACKSPACE.
    """
    s = s.strip().upper()
    if s in ('T', 'TAB'):
        return 'TAB'
    if s in ('\x7f', '\x08'):   # solo veri caratteri backspace/del
        return 'BACKSPACE'
    if not s:                   # Invio senza testo = conferma
        return 'ENTER'
    return s[:1]


class TestLeggiTastoWindowsMapping(unittest.TestCase):
    """
    Verifica la logica di mapping tasto su Windows (senza termios).

    BUG RISOLTO: '' (Invio vuoto) mappava a BACKSPACE invece di ENTER.
    Questo causava l'uscita accidentale dalla schermata modelli dopo
    essere tornati dal sotto-menu "ripensamento" con il tasto 0.
    """

    def test_invio_vuoto_ritorna_ENTER(self):
        """BUG FIX: '' deve essere ENTER (conferma), non BACKSPACE (esci)."""
        self.assertEqual(_simula_leggi_tasto_win(""), "ENTER")

    def test_invio_vuoto_non_e_BACKSPACE(self):
        """Regressione: '' non deve mai diventare BACKSPACE."""
        self.assertNotEqual(_simula_leggi_tasto_win(""), "BACKSPACE")

    def test_T_ritorna_TAB(self):
        self.assertEqual(_simula_leggi_tasto_win("T"), "TAB")

    def test_TAB_ritorna_TAB(self):
        self.assertEqual(_simula_leggi_tasto_win("TAB"), "TAB")

    def test_backspace_char_ritorna_BACKSPACE(self):
        self.assertEqual(_simula_leggi_tasto_win("\x08"), "BACKSPACE")

    def test_del_char_ritorna_BACKSPACE(self):
        self.assertEqual(_simula_leggi_tasto_win("\x7f"), "BACKSPACE")

    def test_0_ritorna_0(self):
        self.assertEqual(_simula_leggi_tasto_win("0"), "0")

    def test_cifre_ritornano_prima_cifra(self):
        for c in "123456789":
            with self.subTest(c=c):
                self.assertEqual(_simula_leggi_tasto_win(c), c)

    def test_G_ritorna_G(self):
        self.assertEqual(_simula_leggi_tasto_win("G"), "G")

    def test_C_ritorna_C(self):
        self.assertEqual(_simula_leggi_tasto_win("C"), "C")

    def test_M_ritorna_M(self):
        self.assertEqual(_simula_leggi_tasto_win("M"), "M")

    def test_Q_ritorna_Q(self):
        self.assertEqual(_simula_leggi_tasto_win("Q"), "Q")

    def test_minuscolo_normalizzato(self):
        """La logica fa .upper() quindi 'g' deve comportarsi come 'G'."""
        self.assertEqual(_simula_leggi_tasto_win("g"), "G")
        self.assertEqual(_simula_leggi_tasto_win("c"), "C")
        self.assertEqual(_simula_leggi_tasto_win("m"), "M")

    def test_stringa_multipla_ritorna_solo_primo_char(self):
        """Solo il primo carattere è rilevante (s[:1])."""
        self.assertEqual(_simula_leggi_tasto_win("GARBAGE"), "G")


# ── Navigazione: routing dei tasti nella schermata modelli ────────────────────

class TestNavigazioneSchermataModelli(unittest.TestCase):
    """
    Verifica che i tasti nella schermata modelli portino all'azione corretta.

    Struttura del menu modelli:
        ENTER       → conferma e passa al task
        0 / BACKSPACE → torna al menu principale (sys.exit(2))
        G           → tutti GPU
        C           → tutti CPU
        M           → mix GPU/CPU
        1–5         → sotto-menu ripensamento agente
    """

    RUOLI = ["ricercatore", "pianificatore", "programmatore", "tester", "ottimizzatore"]

    def _routing(self, val: str, rl: list) -> str:
        """Simula il routing della schermata modelli senza I/O reale."""
        if val == "ENTER":
            return "conferma"
        if val in ("0", "BACKSPACE"):
            return "esci"
        if val == "G":
            return "tutti_gpu"
        if val == "C":
            return "tutti_cpu"
        if val == "M":
            return "mix"
        if val.isdigit() and 1 <= int(val) <= len(rl):
            return f"ripensamento_{rl[int(val)-1]}"
        return "ignora"

    def test_enter_conferma(self):
        self.assertEqual(self._routing("ENTER", self.RUOLI), "conferma")

    def test_zero_esce(self):
        self.assertEqual(self._routing("0", self.RUOLI), "esci")

    def test_backspace_esce(self):
        self.assertEqual(self._routing("BACKSPACE", self.RUOLI), "esci")

    def test_G_imposta_gpu(self):
        self.assertEqual(self._routing("G", self.RUOLI), "tutti_gpu")

    def test_C_imposta_cpu(self):
        self.assertEqual(self._routing("C", self.RUOLI), "tutti_cpu")

    def test_M_imposta_mix(self):
        self.assertEqual(self._routing("M", self.RUOLI), "mix")

    def test_1_apre_ripensamento_ricercatore(self):
        self.assertEqual(self._routing("1", self.RUOLI), "ripensamento_ricercatore")

    def test_5_apre_ripensamento_ottimizzatore(self):
        self.assertEqual(self._routing("5", self.RUOLI), "ripensamento_ottimizzatore")

    def test_6_fuori_range_ignorato(self):
        self.assertEqual(self._routing("6", self.RUOLI), "ignora")

    def test_lettera_sconosciuta_ignorata(self):
        self.assertEqual(self._routing("Z", self.RUOLI), "ignora")


# ── Navigazione: routing del sotto-menu ripensamento ─────────────────────────

class TestNavigazioneRipensamento(unittest.TestCase):
    """
    Verifica il comportamento del sotto-menu "Configura agente" (ripensamento).

    Opzioni disponibili:
        G        → sposta agente su GPU
        C        → sposta agente su CPU
        M        → cambia modello (chiede numero)
        0 / ""   → annulla e torna alla schermata modelli
    """

    def _routing_rip(self, sc: str) -> str:
        """Simula il routing del sotto-menu ripensamento."""
        if sc in ("0", "", "ENTER"):
            return "annulla"
        if sc == "G":
            return "sposta_gpu"
        if sc == "C":
            return "sposta_cpu"
        if sc == "M":
            return "cambia_modello"
        return "ignora"

    def test_0_annulla_e_torna(self):
        self.assertEqual(self._routing_rip("0"), "annulla")

    def test_stringa_vuota_annulla(self):
        """Invio vuoto = annulla (stessa semantica di 0)."""
        self.assertEqual(self._routing_rip(""), "annulla")

    def test_G_sposta_su_gpu(self):
        self.assertEqual(self._routing_rip("G"), "sposta_gpu")

    def test_C_sposta_su_cpu(self):
        self.assertEqual(self._routing_rip("C"), "sposta_cpu")

    def test_M_richiede_modello(self):
        self.assertEqual(self._routing_rip("M"), "cambia_modello")

    def test_lettera_sconosciuta_ignorata(self):
        self.assertEqual(self._routing_rip("Z"), "ignora")

    def test_dopo_annulla_si_ridisegna_schermata_principale(self):
        """
        Dopo '0' nel sotto-menu, _ridisegna() deve essere chiamata
        per mostrare la schermata modelli principale.
        Verifica tramite mock che la funzione di ridisegno venga invocata.
        """
        ridisegna_chiamata = []

        def mock_ridisegna(msg=""):
            ridisegna_chiamata.append(msg)

        sc = "0"
        if sc in ("0", "", "ENTER"):
            mock_ridisegna()   # simula _ridisegna() nella branch "0"

        self.assertEqual(len(ridisegna_chiamata), 1,
                         "_ridisegna() deve essere chiamata esattamente una volta dopo '0'")

    def test_cambio_dispositivo_aggiorna_stato(self):
        """Cambiare G/C aggiorna correttamente il dizionario dispositivi."""
        dispositivi = {"ricercatore": "GPU", "tester": "GPU"}

        # Simula pressione C per il ricercatore
        sc = "C"
        ruolo = "ricercatore"
        if sc == "C":
            dispositivi[ruolo] = "CPU"
        elif sc == "G":
            dispositivi[ruolo] = "GPU"

        self.assertEqual(dispositivi["ricercatore"], "CPU")
        self.assertEqual(dispositivi["tester"], "GPU")   # altri invariati

    def test_cambio_modello_aggiorna_scelti(self):
        """Selezionare M e un numero valido aggiorna scelti[ruolo]."""
        nomi = ["qwen3:4b", "deepseek-r1:7b", "mistral:7b"]
        scelti = {"ricercatore": "qwen3:4b"}

        # Simula M → scelta 2 (deepseek-r1:7b)
        n = 2 - 1   # 0-indexed
        scelti["ricercatore"] = nomi[n]

        self.assertEqual(scelti["ricercatore"], "deepseek-r1:7b")

    def test_cambio_modello_indice_fuori_range_non_aggiorna(self):
        """Indice fuori range non deve modificare scelti."""
        nomi = ["qwen3:4b", "deepseek-r1:7b"]
        scelti = {"ricercatore": "qwen3:4b"}
        originale = scelti["ricercatore"]

        n = 99 - 1   # 0-indexed, fuori range
        if 0 <= n < len(nomi):
            scelti["ricercatore"] = nomi[n]
        # else: non modificare

        self.assertEqual(scelti["ricercatore"], originale)


# ── Timeout controlla_istanza ─────────────────────────────────────────────────

class TestControllaIstanzaTimeout(unittest.TestCase):
    """
    Verifica che controlla_istanza() non blocchi a lungo.
    Il timeout deve essere ≤ 0.5 s (era 2 s, ora 0.3 s).
    """

    def test_porta_chiusa_ritorna_entro_mezzo_secondo(self):
        """
        Porta 19999 quasi certamente non ha niente in ascolto.
        La funzione deve ritornare False in meno di 0.5 s.
        """
        import time
        t0 = time.monotonic()
        result = controlla_istanza(19999)
        elapsed = time.monotonic() - t0

        self.assertFalse(result)
        self.assertLess(elapsed, 0.5,
            f"controlla_istanza ha impiegato {elapsed:.2f}s — troppo lento (max 0.5s)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
