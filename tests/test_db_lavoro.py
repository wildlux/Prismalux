"""
test_db_lavoro.py — Test per strumento_pratico/db_lavoro.py
=============================================================
Testa: _prossimo_id, valuta_offerta, cerca_in_lista,
       aggiungi, rimuovi, BLACKLIST_DEFAULT

Nessun I/O su disco: _carica e _salva sono sempre mockati.

Esegui:
    python3.14 -m unittest tests.test_db_lavoro -v
"""

import sys
import os
import unittest
from unittest import mock

# Root del progetto
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, "strumento_pratico"))

# Mock di rich PRIMA di importare db_lavoro (che lo richiede a livello di modulo)
_mock_rich = mock.MagicMock()
for _mod in ["rich", "rich.console", "rich.panel", "rich.table"]:
    sys.modules.setdefault(_mod, _mock_rich)

from db_lavoro import (
    _prossimo_id,
    valuta_offerta,
    cerca_in_lista,
    aggiungi,
    rimuovi,
    BLACKLIST_DEFAULT,
)
import db_lavoro


# ── _prossimo_id ──────────────────────────────────────────────────────────────

class TestProssimoId(unittest.TestCase):

    def test_lista_vuota_ritorna_1(self):
        self.assertEqual(_prossimo_id([]), 1)

    def test_ritorna_max_id_piu_1(self):
        lista = [{"id": 1}, {"id": 5}, {"id": 3}]
        self.assertEqual(_prossimo_id(lista), 6)

    def test_elementi_senza_id_ritorna_1(self):
        lista = [{"valore": "x"}, {"valore": "y"}]
        self.assertEqual(_prossimo_id(lista), 1)

    def test_singolo_elemento(self):
        self.assertEqual(_prossimo_id([{"id": 10}]), 11)

    def test_id_non_sequenziali(self):
        lista = [{"id": 100}, {"id": 1}, {"id": 50}]
        self.assertEqual(_prossimo_id(lista), 101)


# ── valuta_offerta ────────────────────────────────────────────────────────────

class TestValutaOfferta(unittest.TestCase):

    def _mock_liste(self, white=None, black=None):
        """Helper: simula le liste senza toccare il filesystem."""
        white = white or []
        black = black or []

        def fake_cerca(lista, testo):
            source = white if lista == "white" else black
            testo_low = testo.lower()
            return [r for r in source if r["valore"].lower() in testo_low]

        return mock.patch("db_lavoro.cerca_in_lista", side_effect=fake_cerca)

    def test_blacklist_match_semaforo_rosso(self):
        bl = [{"valore": "investimento iniziale", "motivo": "truffa"}]
        with self._mock_liste(black=bl):
            result = valuta_offerta("Richiediamo un investimento iniziale")
        self.assertEqual(result["semaforo"], "rosso")
        self.assertEqual(len(result["black"]), 1)

    def test_whitelist_match_semaforo_verde(self):
        wl = [{"valore": "google", "nota": "affidabile"}]
        with self._mock_liste(white=wl):
            result = valuta_offerta("Posizione aperta in Google Italia")
        self.assertEqual(result["semaforo"], "verde")
        self.assertEqual(len(result["white"]), 1)

    def test_nessun_match_semaforo_giallo(self):
        with self._mock_liste():
            result = valuta_offerta("Impiegato amministrativo part-time")
        self.assertEqual(result["semaforo"], "giallo")
        self.assertEqual(result["white"], [])
        self.assertEqual(result["black"], [])

    def test_blacklist_ha_precedenza_su_whitelist(self):
        wl = [{"valore": "buona azienda", "nota": "ok"}]
        bl = [{"valore": "investimento iniziale", "motivo": "truffa"}]
        with self._mock_liste(white=wl, black=bl):
            result = valuta_offerta("Buona azienda, richiesto investimento iniziale")
        self.assertEqual(result["semaforo"], "rosso")

    def test_struttura_risultato_ha_tre_chiavi(self):
        with self._mock_liste():
            result = valuta_offerta("qualsiasi testo")
        self.assertIn("white", result)
        self.assertIn("black", result)
        self.assertIn("semaforo", result)

    def test_semaforo_ha_valore_valido(self):
        with self._mock_liste():
            result = valuta_offerta("testo neutro")
        self.assertIn(result["semaforo"], {"verde", "giallo", "rosso"})


# ── cerca_in_lista ────────────────────────────────────────────────────────────

class TestCercaInLista(unittest.TestCase):

    def test_trova_keyword_presente_case_insensitive(self):
        righe = [{"valore": "herbalife", "motivo": "MLM"}]
        with mock.patch("db_lavoro.leggi_lista", return_value=righe):
            result = cerca_in_lista("black", "Lavoro con Herbalife")
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["valore"], "herbalife")

    def test_nessuna_corrispondenza_ritorna_lista_vuota(self):
        righe = [{"valore": "herbalife", "motivo": "MLM"}]
        with mock.patch("db_lavoro.leggi_lista", return_value=righe):
            result = cerca_in_lista("black", "Offerta normalissima")
        self.assertEqual(result, [])

    def test_lista_vuota_ritorna_lista_vuota(self):
        with mock.patch("db_lavoro.leggi_lista", return_value=[]):
            result = cerca_in_lista("black", "qualsiasi testo")
        self.assertEqual(result, [])

    def test_piu_match_ritornati(self):
        righe = [
            {"valore": "herbalife", "motivo": "MLM"},
            {"valore": "amway", "motivo": "MLM"},
        ]
        with mock.patch("db_lavoro.leggi_lista", return_value=righe):
            result = cerca_in_lista("black", "lavoro con herbalife e amway")
        self.assertEqual(len(result), 2)


# ── aggiungi ──────────────────────────────────────────────────────────────────

class TestAggiungi(unittest.TestCase):

    def test_aggiunge_elemento_nuovo_ritorna_true(self):
        with mock.patch("db_lavoro.leggi_lista", return_value=[]):
            with mock.patch("db_lavoro._salva") as mock_salva:
                result = aggiungi("black", "keyword", "nuova truffa", "motivo test")
        self.assertTrue(result)
        mock_salva.assert_called_once()

    def test_duplicato_case_insensitive_non_aggiunto(self):
        esistente = [{"id": 1, "valore": "herbalife", "tipo": "azienda",
                      "motivo": "MLM", "data": "2025-01-01"}]
        with mock.patch("db_lavoro.leggi_lista", return_value=esistente):
            with mock.patch("db_lavoro._salva") as mock_salva:
                result = aggiungi("black", "azienda", "HERBALIFE", "test")
        self.assertFalse(result)
        mock_salva.assert_not_called()

    def test_whitelist_usa_campo_nota(self):
        dati_salvati = {}

        def fake_salva(percorso, dati):
            dati_salvati["dati"] = dati

        with mock.patch("db_lavoro.leggi_lista", return_value=[]):
            with mock.patch("db_lavoro._salva", side_effect=fake_salva):
                aggiungi("white", "azienda", "Google", "affidabile")

        self.assertIn("dati", dati_salvati)
        elem = dati_salvati["dati"][0]
        self.assertIn("nota", elem)
        self.assertNotIn("motivo", elem)

    def test_blacklist_usa_campo_motivo(self):
        dati_salvati = {}

        def fake_salva(percorso, dati):
            dati_salvati["dati"] = dati

        with mock.patch("db_lavoro.leggi_lista", return_value=[]):
            with mock.patch("db_lavoro._salva", side_effect=fake_salva):
                aggiungi("black", "keyword", "truffa", "motivo x")

        elem = dati_salvati["dati"][0]
        self.assertIn("motivo", elem)
        self.assertNotIn("nota", elem)

    def test_id_viene_assegnato(self):
        dati_salvati = {}

        def fake_salva(percorso, dati):
            dati_salvati["dati"] = dati

        esistente = [{"id": 5, "valore": "altro"}]
        with mock.patch("db_lavoro.leggi_lista", return_value=esistente):
            with mock.patch("db_lavoro._salva", side_effect=fake_salva):
                aggiungi("black", "keyword", "nuovo", "motivo")

        nuovo = dati_salvati["dati"][-1]
        self.assertEqual(nuovo["id"], 6)

    def test_valore_viene_strippato(self):
        dati_salvati = {}

        def fake_salva(percorso, dati):
            dati_salvati["dati"] = dati

        with mock.patch("db_lavoro.leggi_lista", return_value=[]):
            with mock.patch("db_lavoro._salva", side_effect=fake_salva):
                aggiungi("black", "keyword", "  spazi  ", "motivo")

        nuovo = dati_salvati["dati"][0]
        self.assertEqual(nuovo["valore"], "spazi")


# ── rimuovi ───────────────────────────────────────────────────────────────────

class TestRimuovi(unittest.TestCase):

    def test_rimuove_elemento_esistente_ritorna_true(self):
        esistente = [{"id": 1, "valore": "herbalife"}]
        with mock.patch("db_lavoro.leggi_lista", return_value=esistente):
            with mock.patch("db_lavoro._salva"):
                result = rimuovi("black", "herbalife")
        self.assertTrue(result)

    def test_elemento_assente_ritorna_false(self):
        with mock.patch("db_lavoro.leggi_lista", return_value=[]):
            with mock.patch("db_lavoro._salva"):
                result = rimuovi("black", "inesistente")
        self.assertFalse(result)

    def test_rimozione_case_insensitive(self):
        esistente = [{"id": 1, "valore": "herbalife"}]
        with mock.patch("db_lavoro.leggi_lista", return_value=esistente):
            with mock.patch("db_lavoro._salva") as mock_salva:
                result = rimuovi("black", "HERBALIFE")
        self.assertTrue(result)
        mock_salva.assert_called_once()

    def test_rimozione_non_tocca_altri_elementi(self):
        esistente = [
            {"id": 1, "valore": "herbalife"},
            {"id": 2, "valore": "amway"},
        ]
        dati_rimasti = {}

        def fake_salva(percorso, dati):
            dati_rimasti["dati"] = dati

        with mock.patch("db_lavoro.leggi_lista", return_value=esistente):
            with mock.patch("db_lavoro._salva", side_effect=fake_salva):
                rimuovi("black", "herbalife")

        self.assertEqual(len(dati_rimasti["dati"]), 1)
        self.assertEqual(dati_rimasti["dati"][0]["valore"], "amway")

    def test_lista_vuota_ritorna_false_senza_salva(self):
        with mock.patch("db_lavoro.leggi_lista", return_value=[]):
            with mock.patch("db_lavoro._salva") as mock_salva:
                result = rimuovi("black", "qualcosa")
        self.assertFalse(result)
        mock_salva.assert_not_called()


# ── BLACKLIST_DEFAULT ─────────────────────────────────────────────────────────

class TestBlacklistDefault(unittest.TestCase):

    def test_ha_11_voci(self):
        self.assertEqual(len(BLACKLIST_DEFAULT), 11)

    def test_ogni_voce_ha_tipo_valore_motivo(self):
        for voce in BLACKLIST_DEFAULT:
            with self.subTest(voce=voce.get("valore")):
                self.assertIn("tipo", voce)
                self.assertIn("valore", voce)
                self.assertIn("motivo", voce)

    def test_tipi_sono_validi(self):
        tipi_validi = {"keyword", "azienda", "truffa"}
        for voce in BLACKLIST_DEFAULT:
            with self.subTest(voce=voce.get("valore")):
                self.assertIn(voce["tipo"], tipi_validi)

    def test_valori_non_vuoti(self):
        for voce in BLACKLIST_DEFAULT:
            with self.subTest(voce=voce.get("valore")):
                self.assertTrue(voce["valore"].strip())

    def test_contiene_herbalife_e_amway(self):
        valori = {v["valore"].lower() for v in BLACKLIST_DEFAULT}
        self.assertIn("herbalife", valori)
        self.assertIn("amway", valori)

    def test_contiene_keyword_investimento_iniziale(self):
        valori = {v["valore"].lower() for v in BLACKLIST_DEFAULT}
        self.assertIn("investimento iniziale", valori)


if __name__ == "__main__":
    unittest.main(verbosity=2)
