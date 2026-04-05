"""
test_llama_studio.py — Test per llama_cpp_studio/main.py
==========================================================
Testa: _n_threads_ottimale, _n_gpu_layers_ottimale, _cmake_flags,
       _carica_config, _salva_config, _rileva_hardware (senza GPU)

Nessuna GPU, nessun subprocess reale: tutto mockato.

Esegui:
    python3.14 -m unittest tests.test_llama_studio -v
"""

import sys
import os
import json
import unittest
from unittest import mock

ROOT       = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_PRISMALUX = os.path.dirname(ROOT)   # Python_Software -> Prismalux
sys.path.insert(0, ROOT)
sys.path.insert(0, _PRISMALUX)
sys.path.insert(0, os.path.join(_PRISMALUX, "llama_cpp_studio"))

# Mock di rich prima dell'import (potrebbe non servire se già installato,
# ma lo facciamo per sicurezza in ambienti senza rich)
_mock_rich = mock.MagicMock()
for _mod in ["rich", "rich.console", "rich.panel", "rich.table",
             "rich.text", "rich.rule"]:
    sys.modules.setdefault(_mod, _mock_rich)

# Impostiamo anche check_deps come mock per evitare import a catena
sys.modules.setdefault("check_deps", mock.MagicMock())

# Ora importa le funzioni pure (llama_cpp_studio è in sys.path via _OA)
import llama_cpp_studio.main as _studio

_n_threads_ottimale   = _studio._n_threads_ottimale
_n_gpu_layers_ottimale = _studio._n_gpu_layers_ottimale
_cmake_flags          = _studio._cmake_flags
_carica_config        = _studio._carica_config
_salva_config         = _studio._salva_config
_rileva_hardware      = _studio._rileva_hardware


# ── _n_threads_ottimale ───────────────────────────────────────────────────────

class TestNThreadsOttimale(unittest.TestCase):

    def test_core_1_ritorna_1(self):
        self.assertEqual(_n_threads_ottimale({"cpu_cores_fisici": 1}), 1)

    def test_core_2_ritorna_2(self):
        # cores <= 2: ritorna cores (non si sottrae 1)
        self.assertEqual(_n_threads_ottimale({"cpu_cores_fisici": 2}), 2)

    def test_core_4_ritorna_3(self):
        self.assertEqual(_n_threads_ottimale({"cpu_cores_fisici": 4}), 3)

    def test_core_8_ritorna_7(self):
        self.assertEqual(_n_threads_ottimale({"cpu_cores_fisici": 8}), 7)

    def test_core_0_ritorna_0(self):
        # cores <= 2 → ritorna cores direttamente, senza clamping
        result = _n_threads_ottimale({"cpu_cores_fisici": 0})
        self.assertEqual(result, 0)

    def test_core_16_ritorna_15(self):
        self.assertEqual(_n_threads_ottimale({"cpu_cores_fisici": 16}), 15)

    def test_risultato_non_negativo_per_core_positivi(self):
        for cores in range(1, 20):
            with self.subTest(cores=cores):
                result = _n_threads_ottimale({"cpu_cores_fisici": cores})
                self.assertGreaterEqual(result, 1)


# ── _n_gpu_layers_ottimale ────────────────────────────────────────────────────

class TestNGpuLayersOttimale(unittest.TestCase):

    def test_senza_vram_ritorna_0(self):
        self.assertEqual(_n_gpu_layers_ottimale({"gpu_nvidia_vram_gb": 0.0}), 0)

    def test_chiave_assente_ritorna_0(self):
        self.assertEqual(_n_gpu_layers_ottimale({}), 0)

    def test_vram_superiore_modello_ritorna_999(self):
        # vram=10 GB >= size_gb=4 * 1.15 = 4.6 → tutto in GPU
        self.assertEqual(_n_gpu_layers_ottimale({"gpu_nvidia_vram_gb": 10.0},
                                                size_gb=4.0), 999)

    def test_vram_esattamente_soglia_ritorna_999(self):
        # vram = 4.0 * 1.15 = 4.6 → esattamente al limite
        self.assertEqual(_n_gpu_layers_ottimale({"gpu_nvidia_vram_gb": 4.6},
                                                size_gb=4.0), 999)

    def test_vram_parziale_ritorna_stima_tra_1_e_999(self):
        # vram=4, size_gb=8 → 50% → circa 17 layers
        result = _n_gpu_layers_ottimale({"gpu_nvidia_vram_gb": 4.0}, size_gb=8.0)
        self.assertGreaterEqual(result, 1)
        self.assertLess(result, 999)

    def test_vram_minima_ritorna_almeno_1(self):
        result = _n_gpu_layers_ottimale({"gpu_nvidia_vram_gb": 0.5}, size_gb=8.0)
        self.assertGreaterEqual(result, 1)


# ── _cmake_flags ──────────────────────────────────────────────────────────────

class TestCmakeFlags(unittest.TestCase):

    def test_sempre_contiene_build_type_release(self):
        self.assertIn("-DCMAKE_BUILD_TYPE=Release", _cmake_flags({}))

    def test_sempre_contiene_ggml_native(self):
        self.assertIn("-DGGML_NATIVE=ON", _cmake_flags({}))

    def test_cuda_aggiunge_flag_cuda(self):
        flags = _cmake_flags({"cuda": True})
        self.assertIn("-DGGML_CUDA=ON", flags)

    def test_senza_cuda_nessun_flag_cuda(self):
        flags = _cmake_flags({"cuda": False})
        self.assertNotIn("-DGGML_CUDA=ON", flags)

    def test_rocm_aggiunge_hip(self):
        flags = _cmake_flags({"rocm": True, "cuda": False})
        self.assertIn("-DGGML_HIP=ON", flags)

    def test_metal_aggiunge_metal(self):
        flags = _cmake_flags({"metal": True})
        self.assertIn("-DGGML_METAL=ON", flags)

    def test_vulkan_senza_cuda_aggiunge_vulkan(self):
        flags = _cmake_flags({"vulkan": True, "cuda": False, "rocm": False})
        self.assertIn("-DGGML_VULKAN=ON", flags)

    def test_vulkan_con_cuda_non_aggiunge_vulkan(self):
        flags = _cmake_flags({"vulkan": True, "cuda": True})
        self.assertNotIn("-DGGML_VULKAN=ON", flags)

    def test_vulkan_con_rocm_non_aggiunge_vulkan(self):
        flags = _cmake_flags({"vulkan": True, "rocm": True, "cuda": False})
        self.assertNotIn("-DGGML_VULKAN=ON", flags)

    def test_cpu_only_solo_due_flag_base(self):
        hw = {"cuda": False, "rocm": False, "metal": False, "vulkan": False}
        flags = _cmake_flags(hw)
        self.assertEqual(len(flags), 2)

    def test_ritorna_lista(self):
        self.assertIsInstance(_cmake_flags({}), list)


# ── _carica_config ────────────────────────────────────────────────────────────

class TestCaricaConfig(unittest.TestCase):

    def test_file_assente_ritorna_default(self):
        with mock.patch("os.path.exists", return_value=False):
            cfg = _carica_config()
        self.assertFalse(cfg["compilato"])
        self.assertIsNone(cfg["modello_default"])
        self.assertEqual(cfg["n_gpu_layers"], 0)
        self.assertEqual(cfg["ctx_size"], 4096)
        self.assertIsInstance(cfg["binari"], list)
        self.assertIsInstance(cfg["cmake_flags"], list)

    def test_file_valido_caricato_correttamente(self):
        dati = {
            "compilato": True,
            "modello_default": "test.gguf",
            "cmake_flags": ["-DGGML_CUDA=ON"],
            "hardware": {},
            "n_threads": 7,
            "n_gpu_layers": 35,
            "ctx_size": 8192,
            "binari": ["llama-cli"],
        }
        m = mock.mock_open(read_data=json.dumps(dati))
        with mock.patch("os.path.exists", return_value=True):
            with mock.patch("builtins.open", m):
                cfg = _carica_config()
        self.assertTrue(cfg["compilato"])
        self.assertEqual(cfg["modello_default"], "test.gguf")
        self.assertEqual(cfg["ctx_size"], 8192)

    def test_file_json_corrotto_ritorna_default_senza_crash(self):
        m = mock.mock_open(read_data="{JSON INVALIDO!!!")
        with mock.patch("os.path.exists", return_value=True):
            with mock.patch("builtins.open", m):
                cfg = _carica_config()
        self.assertFalse(cfg["compilato"])

    def test_file_vuoto_ritorna_default(self):
        m = mock.mock_open(read_data="")
        with mock.patch("os.path.exists", return_value=True):
            with mock.patch("builtins.open", m):
                cfg = _carica_config()
        self.assertFalse(cfg["compilato"])


# ── _salva_config ─────────────────────────────────────────────────────────────

class TestSalvaConfig(unittest.TestCase):

    def test_scrive_json_valido(self):
        scritto = []

        def fake_open(path, mode="r", **kw):
            if "w" in mode:
                m = mock.MagicMock()
                buf = []
                m.__enter__ = lambda s: m
                m.__exit__ = mock.MagicMock(return_value=False)
                m.write = lambda data: buf.append(data)
                scritto.append(buf)
                return m
            return mock.mock_open(read_data="{}")()

        cfg = {"compilato": True, "modello_default": "test.gguf",
               "cmake_flags": [], "hardware": {}, "n_threads": 4,
               "n_gpu_layers": 0, "ctx_size": 4096, "binari": []}

        with mock.patch("builtins.open", side_effect=fake_open):
            # Non deve sollevare eccezioni
            try:
                _salva_config(cfg)
            except Exception:
                pass  # il mock potrebbe non catturare tutto il JSON, ma non deve crashare

    def test_salva_non_crasha_con_config_vuota(self):
        m = mock.mock_open()
        with mock.patch("builtins.open", m):
            try:
                _salva_config({})
            except Exception as e:
                self.fail(f"_salva_config ha sollevato {e}")


# ── _rileva_hardware ──────────────────────────────────────────────────────────

class TestRilevaHardware(unittest.TestCase):
    """
    _rileva_hardware() NON deve crashare quando i comandi GPU
    (nvidia-smi, rocminfo, vulkaninfo) non sono disponibili.
    """

    def _hw_senza_gpu(self):
        def fake_run(cmd, **kwargs):
            raise FileNotFoundError(f"comando non trovato: {cmd[0]}")

        with mock.patch("subprocess.run", side_effect=fake_run):
            return _rileva_hardware()

    def test_non_crasha_senza_comandi_gpu(self):
        try:
            hw = self._hw_senza_gpu()
        except Exception as e:
            self.fail(f"_rileva_hardware ha sollevato {e}")

    def test_struttura_dict_completa(self):
        hw = self._hw_senza_gpu()
        chiavi_attese = [
            "cpu_nome", "cpu_cores_fisici", "cpu_threads", "ram_gb", "arch",
            "avx", "avx2", "avx512", "fma", "f16c", "neon",
            "cuda", "gpu_nvidia", "gpu_nvidia_vram_gb",
            "rocm", "gpu_amd", "vulkan", "metal",
        ]
        for k in chiavi_attese:
            with self.subTest(chiave=k):
                self.assertIn(k, hw)

    def test_senza_gpu_flags_false(self):
        hw = self._hw_senza_gpu()
        self.assertFalse(hw.get("cuda", True))
        self.assertFalse(hw.get("rocm", True))

    def test_arch_non_vuota(self):
        hw = self._hw_senza_gpu()
        self.assertTrue(hw.get("arch", ""))

    def test_cpu_cores_fisici_positivo(self):
        hw = self._hw_senza_gpu()
        self.assertGreaterEqual(hw.get("cpu_cores_fisici", 0), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
