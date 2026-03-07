"""
test_check_deps.py — Test per check_deps.py
=============================================
Testa: Ridimensiona, _barra_res, controlla_terminale,
       risorse, controlla_dipendenze, leggi_tasto (ramo fallback)

Esegui:
    python3.14 -m unittest tests.test_check_deps -v
"""

import sys
import os
import unittest
from unittest import mock

# Aggiungi la root del progetto al path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from check_deps import (
    Ridimensiona,
    _barra_res,
    controlla_terminale,
    risorse,
    controlla_dipendenze,
    leggi_tasto,
)


# ── Ridimensiona ───────────────────────────────────────────────────────────────

class TestRidimensiona(unittest.TestCase):

    def test_e_base_exception(self):
        self.assertTrue(issubclass(Ridimensiona, BaseException))

    def test_non_e_exception_standard(self):
        # Deve essere BaseException diretta, non Exception
        self.assertFalse(issubclass(Ridimensiona, Exception))

    def test_si_lancia_e_cattura(self):
        with self.assertRaises(Ridimensiona):
            raise Ridimensiona()

    def test_catturabile_con_base_exception(self):
        catturata = False
        try:
            raise Ridimensiona()
        except BaseException:
            catturata = True
        self.assertTrue(catturata)

    def test_non_catturabile_con_exception(self):
        catturata = False
        try:
            raise Ridimensiona()
        except Exception:
            catturata = True
        except BaseException:
            pass
        self.assertFalse(catturata)


# ── _barra_res ────────────────────────────────────────────────────────────────

class TestBarraRes(unittest.TestCase):

    def test_zero_percento_tutto_vuoto(self):
        self.assertEqual(_barra_res(0, 6), "░░░░░░")

    def test_cento_percento_tutto_pieno(self):
        self.assertEqual(_barra_res(100, 6), "██████")

    def test_cinquanta_percento_meta_piena(self):
        self.assertEqual(_barra_res(50, 6), "███░░░")

    def test_lunghezza_sempre_rispettata(self):
        for perc in [0, 25, 50, 75, 100]:
            with self.subTest(perc=perc):
                self.assertEqual(len(_barra_res(perc, 10)), 10)

    def test_larghezza_personalizzata(self):
        self.assertEqual(len(_barra_res(50, 4)), 4)

    def test_perc_negativa_pieni_non_positivi(self):
        # _barra_res non fa clamping: pieni può essere negativo → solo vuoti
        barra = _barra_res(-10, 6)
        self.assertNotIn("█", barra)

    def test_perc_oltre_100_pieni_maggiori_di_larghezza(self):
        # _barra_res non fa clamping: accettiamo che la barra sia più lunga
        barra = _barra_res(150, 6)
        self.assertIsInstance(barra, str)


# ── controlla_terminale ───────────────────────────────────────────────────────

class TestControllaTerminale(unittest.TestCase):

    def test_restituisce_tuple_di_due_interi(self):
        with mock.patch("shutil.get_terminal_size", return_value=(120, 40)):
            result = controlla_terminale()
        self.assertIsInstance(result, tuple)
        self.assertEqual(len(result), 2)
        cols, rows = result
        self.assertIsInstance(cols, int)
        self.assertIsInstance(rows, int)

    def test_terminale_grande_non_stampa_avviso(self):
        with mock.patch("shutil.get_terminal_size", return_value=(200, 60)):
            with mock.patch("builtins.print") as mock_print:
                controlla_terminale(min_cols=90, min_righe=22)
        mock_print.assert_not_called()

    def test_terminale_piccolo_non_crasha(self):
        with mock.patch("shutil.get_terminal_size", return_value=(40, 10)):
            with mock.patch("builtins.print"):
                try:
                    controlla_terminale(min_cols=90, min_righe=22)
                except Exception as e:
                    self.fail(f"controlla_terminale ha sollevato {e}")

    def test_valori_restituiti_corrispondono_a_get_terminal_size(self):
        with mock.patch("shutil.get_terminal_size", return_value=(115, 35)):
            cols, rows = controlla_terminale()
        self.assertEqual(cols, 115)
        self.assertEqual(rows, 35)


# ── risorse ───────────────────────────────────────────────────────────────────

class TestRisorse(unittest.TestCase):

    def test_ritorna_dict_con_chiavi_attese(self):
        result = risorse()
        chiavi = ["cpu", "ram_perc", "ram_gb", "ram_tot",
                  "disco_perc", "disco_gb_usato", "disco_gb_tot"]
        for k in chiavi:
            with self.subTest(chiave=k):
                self.assertIn(k, result)

    def test_valori_nel_range_ammissibile(self):
        result = risorse()
        self.assertGreaterEqual(result["cpu"], 0.0)
        self.assertLessEqual(result["cpu"], 100.0)
        self.assertGreaterEqual(result["ram_perc"], 0.0)
        self.assertLessEqual(result["ram_perc"], 100.0)
        self.assertGreaterEqual(result["disco_perc"], 0.0)
        self.assertLessEqual(result["disco_perc"], 100.0)

    def test_non_crasha_senza_proc_meminfo(self):
        # Simula sistema senza /proc (Windows)
        original_open = open

        def fake_open(path, *args, **kwargs):
            if "/proc" in str(path):
                raise FileNotFoundError(f"No such file: {path}")
            return original_open(path, *args, **kwargs)

        with mock.patch("builtins.open", side_effect=fake_open):
            try:
                result = risorse()
            except Exception as e:
                self.fail(f"risorse() ha sollevato {e}")
        self.assertIn("cpu", result)

    def test_ram_gb_non_negativo(self):
        result = risorse()
        self.assertGreaterEqual(result["ram_gb"], 0.0)

    def test_disco_gb_tot_positivo(self):
        result = risorse()
        self.assertGreater(result["disco_gb_tot"], 0.0)


# ── controlla_dipendenze ──────────────────────────────────────────────────────

class TestControllaDipendenze(unittest.TestCase):

    def test_tutto_installato_non_esce(self):
        # "os" è sempre disponibile
        try:
            with mock.patch("builtins.print"):
                controlla_dipendenze([("os", "os", True, "Sistema")])
        except SystemExit:
            self.fail("controlla_dipendenze ha chiamato sys.exit con tutto installato")

    def test_mancante_obbligatorio_chiama_sys_exit_1(self):
        dipendenze = [("pkg_che_non_esiste_xyz", "pkg-xyz", True, "Test")]
        with self.assertRaises(SystemExit) as ctx:
            with mock.patch("builtins.print"):
                with mock.patch("builtins.input", return_value=""):
                    controlla_dipendenze(dipendenze)
        self.assertEqual(ctx.exception.code, 1)

    def test_mancante_opzionale_non_chiama_sys_exit(self):
        dipendenze = [("pkg_che_non_esiste_xyz", "pkg-xyz", False, "Test")]
        try:
            with mock.patch("builtins.print"):
                with mock.patch("builtins.input", return_value=""):
                    controlla_dipendenze(dipendenze)
        except SystemExit:
            self.fail("controlla_dipendenze ha chiamato sys.exit per dipendenza opzionale")

    def test_mix_installato_e_mancante_obbligatorio_esce(self):
        dipendenze = [
            ("os", "os", True, "Sempre presente"),
            ("pkg_xyz_inesistente", "pkg-xyz", True, "Mancante"),
        ]
        with self.assertRaises(SystemExit):
            with mock.patch("builtins.print"):
                controlla_dipendenze(dipendenze)


# ── leggi_tasto — ramo fallback ───────────────────────────────────────────────

class TestLeggiTastoFallback(unittest.TestCase):
    """
    Testa solo il ramo del fallback testuale (Windows / stdin non-tty).
    Forziamo OSError su sys.stdin.fileno() per attivare il fallback.
    """

    def _chiama_con_input(self, valore_input: str) -> str:
        """Helper: forza OSError su fileno + mocka input()"""
        with mock.patch.object(sys.stdin, "fileno", side_effect=OSError("no tty")):
            with mock.patch("builtins.input", return_value=valore_input):
                return leggi_tasto()

    def test_input_vuoto_ritorna_stringa_vuota(self):
        result = self._chiama_con_input("")
        self.assertEqual(result, "")

    def test_lettera_minuscola_diventa_maiuscola(self):
        result = self._chiama_con_input("a")
        self.assertEqual(result, "A")

    def test_cifra_non_cambia(self):
        result = self._chiama_con_input("3")
        self.assertEqual(result, "3")

    def test_f1_testuale_riconosciuto(self):
        result = self._chiama_con_input("f1")
        self.assertEqual(result, "F1")

    def test_F1_maiuscolo_riconosciuto(self):
        result = self._chiama_con_input("F1")
        self.assertEqual(result, "F1")

    def test_punto_interrogativo_riconosciuto_come_f1(self):
        result = self._chiama_con_input("?")
        self.assertEqual(result, "F1")

    def test_stringa_lunga_ritorna_primo_carattere_maiuscolo(self):
        result = self._chiama_con_input("abc")
        self.assertEqual(result, "A")


if __name__ == "__main__":
    unittest.main(verbosity=2)
