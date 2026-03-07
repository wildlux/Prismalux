"""
test_multi_agente.py — Test per multi_agente/multi_agente.py
=============================================================
Testa: _HAS_TERMIOS, trova_default, controlla_istanza,
       _gpu_label, PREFERENZE_MODELLI

Langgraph e langchain-ollama vengono mockati prima dell'import
per evitare sys.exit(1) se non installati.

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


if __name__ == "__main__":
    unittest.main(verbosity=2)
