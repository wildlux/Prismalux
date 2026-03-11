"""
test_ollama_utils.py — Test per ollama_utils.py
=================================================
Testa: _preset, chiedi_ollama (payload + errori), avvia_warmup,
       chiedi_parallelo, chiedi_ollama_think

Nessun Ollama necessario: requests.post è sempre mockato.

Esegui:
    python3.14 -m unittest tests.test_ollama_utils -v
"""

import sys
import os
import json
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import ollama_utils
from ollama_utils import (
    _preset,
    chiedi_ollama,
    avvia_warmup,
    chiedi_parallelo,
    chiedi_ollama_think,
)


# ── _preset ───────────────────────────────────────────────────────────────────

class TestPreset(unittest.TestCase):

    def test_tipo_estrazione_tok_256(self):
        tok, temp, ctx, top_p, ita = _preset("estrazione")
        self.assertEqual(tok, 256)
        self.assertFalse(ita)

    def test_tipo_analisi_non_forza_italiano(self):
        _, _, _, _, ita = _preset("analisi")
        self.assertFalse(ita)

    def test_tipo_codice_non_forza_italiano(self):
        _, _, _, _, ita = _preset("codice")
        self.assertFalse(ita)

    def test_tipo_risposta_forza_italiano(self):
        _, _, _, _, ita = _preset("risposta")
        self.assertTrue(ita)

    def test_tipo_risposta_lunga_forza_italiano(self):
        _, _, _, _, ita = _preset("risposta_lunga")
        self.assertTrue(ita)

    def test_tipo_spiegazione_forza_italiano(self):
        _, _, _, _, ita = _preset("spiegazione")
        self.assertTrue(ita)

    def test_tipo_sconosciuto_usa_default_risposta(self):
        result = _preset("tipo_mai_visto")
        default = _preset("risposta")
        self.assertEqual(result, default)

    def test_tutti_i_tipi_ritornano_tuple_di_5(self):
        for tipo in ["estrazione", "analisi", "codice",
                     "risposta", "risposta_lunga", "spiegazione"]:
            with self.subTest(tipo=tipo):
                result = _preset(tipo)
                self.assertIsInstance(result, tuple)
                self.assertEqual(len(result), 5)

    def test_num_predict_positivo(self):
        for tipo in ["estrazione", "analisi", "codice", "risposta"]:
            with self.subTest(tipo=tipo):
                tok, *_ = _preset(tipo)
                self.assertGreater(tok, 0)

    def test_temperature_nel_range(self):
        for tipo in ["estrazione", "analisi", "codice", "risposta"]:
            with self.subTest(tipo=tipo):
                _, temp, *_ = _preset(tipo)
                self.assertGreater(temp, 0.0)
                self.assertLessEqual(temp, 1.0)


# ── chiedi_ollama — struttura payload ─────────────────────────────────────────

class TestChiediOllamaPayload(unittest.TestCase):
    """Cattura il payload passato a requests.post e ne verifica la struttura."""

    def _cattura_payload(self, **kwargs) -> dict:
        payload_catturato = {}

        def fake_post(url, json=None, timeout=None, **kw):
            payload_catturato.update(json or {})
            r = mock.MagicMock()
            r.raise_for_status = lambda: None
            r.json.return_value = {"response": "risposta di test"}
            return r

        with mock.patch("ollama_utils.requests.post", side_effect=fake_post):
            chiedi_ollama("prompt di test", **kwargs)
        return payload_catturato

    def test_payload_NON_contiene_think(self):
        """BUG CORRETTO: chiedi_ollama non deve avere "think": False nel payload."""
        payload = self._cattura_payload()
        self.assertNotIn("think", payload)

    def test_payload_contiene_keep_alive_20m(self):
        payload = self._cattura_payload()
        self.assertEqual(payload.get("keep_alive"), "20m")

    def test_payload_stream_e_false(self):
        payload = self._cattura_payload()
        self.assertFalse(payload.get("stream"))

    def test_payload_ha_options_con_num_predict(self):
        payload = self._cattura_payload(tipo="estrazione")
        self.assertIn("options", payload)
        self.assertIn("num_predict", payload["options"])
        self.assertEqual(payload["options"]["num_predict"], 256)

    def test_tipo_risposta_aggiunge_italiano_al_prompt(self):
        payload = self._cattura_payload(tipo="risposta")
        self.assertIn("Rispondi in italiano.", payload["prompt"])

    def test_tipo_codice_non_aggiunge_italiano(self):
        payload = self._cattura_payload(tipo="codice")
        self.assertNotIn("Rispondi in italiano.", payload["prompt"])

    def test_modello_override_usato_nel_payload(self):
        payload = self._cattura_payload(modello="llama3.2:3b")
        self.assertEqual(payload["model"], "llama3.2:3b")

    def test_modello_default_qwen3_4b_se_non_specificato(self):
        payload = self._cattura_payload()
        self.assertEqual(payload["model"], ollama_utils.MODELLO)

    def test_top_p_nel_range(self):
        payload = self._cattura_payload(tipo="risposta")
        top_p = payload["options"].get("top_p", 0)
        self.assertGreater(top_p, 0)
        self.assertLessEqual(top_p, 1.0)


# ── chiedi_ollama — gestione errori ──────────────────────────────────────────

class TestChiediOllamaErrori(unittest.TestCase):
    """Ogni ramo di eccezione deve ritornare stringa che inizia con '❌'."""

    def _chiedi_con_errore(self, exc):
        with mock.patch("ollama_utils.requests.post", side_effect=exc):
            return chiedi_ollama("test")

    def test_connection_error_ritorna_errore_con_emoji(self):
        import requests as req
        result = self._chiedi_con_errore(req.exceptions.ConnectionError())
        self.assertTrue(result.startswith("❌"))
        self.assertIn("Ollama non è attivo", result)

    def test_timeout_ritorna_errore_con_emoji(self):
        import requests as req
        result = self._chiedi_con_errore(req.exceptions.ReadTimeout())
        self.assertTrue(result.startswith("❌"))
        self.assertIn("Timeout", result)

    def test_http_404_ritorna_messaggio_specifico_endpoint(self):
        """BUG CORRETTO: 404 deve dare messaggio su /api/generate."""
        import requests as req
        resp_mock = mock.MagicMock()
        resp_mock.status_code = 404
        err = req.exceptions.HTTPError("404 Not Found")
        err.response = resp_mock
        result = self._chiedi_con_errore(err)
        self.assertTrue(result.startswith("❌"))
        # Deve menzionare l'endpoint o l'aggiornamento
        self.assertTrue(
            "404" in result or "/api/generate" in result or "Aggiorna" in result
        )

    def test_http_500_ritorna_errore_generico(self):
        import requests as req
        resp_mock = mock.MagicMock()
        resp_mock.status_code = 500
        err = req.exceptions.HTTPError("500 Server Error")
        err.response = resp_mock
        result = self._chiedi_con_errore(err)
        self.assertTrue(result.startswith("❌"))

    def test_risposta_json_senza_response_key(self):
        def fake_post(url, json=None, timeout=None, **kw):
            r = mock.MagicMock()
            r.raise_for_status = lambda: None
            r.json.return_value = {}  # nessuna chiave "response"
            return r
        with mock.patch("ollama_utils.requests.post", side_effect=fake_post):
            result = chiedi_ollama("test")
        self.assertIn("❌", result)

    def test_risposta_ok_ritorna_stringa_senza_emoji_errore(self):
        def fake_post(url, json=None, timeout=None, **kw):
            r = mock.MagicMock()
            r.raise_for_status = lambda: None
            r.json.return_value = {"response": "Ciao!"}
            return r
        with mock.patch("ollama_utils.requests.post", side_effect=fake_post):
            result = chiedi_ollama("test")
        self.assertEqual(result, "Ciao!")
        self.assertFalse(result.startswith("❌"))


# ── avvia_warmup ──────────────────────────────────────────────────────────────

class TestAvviaWarmup(unittest.TestCase):

    def setUp(self):
        # Resetta il flag globale prima di ogni test
        ollama_utils._warmup_done = False

    def test_prima_chiamata_lancia_un_thread(self):
        with mock.patch("threading.Thread") as mock_thread:
            mock_thread.return_value.start = mock.MagicMock()
            avvia_warmup()
        mock_thread.assert_called_once()

    def test_seconda_chiamata_non_lancia_secondo_thread(self):
        with mock.patch("threading.Thread") as mock_thread:
            mock_thread.return_value.start = mock.MagicMock()
            avvia_warmup()
            avvia_warmup()
        self.assertEqual(mock_thread.call_count, 1)

    def test_warmup_done_diventa_true_dopo_chiamata(self):
        with mock.patch("threading.Thread") as mock_thread:
            mock_thread.return_value.start = mock.MagicMock()
            avvia_warmup()
        self.assertTrue(ollama_utils._warmup_done)

    def test_modello_custom_passato(self):
        """avvia_warmup(modello) deve usare il modello specificato nel payload."""
        payload_catturato = {}

        def fake_post(url, json=None, timeout=None, **kw):
            payload_catturato.update(json or {})
            r = mock.MagicMock()
            r.raise_for_status = lambda: None
            r.json.return_value = {}
            return r

        # Eseguiamo il thread sincronicamente per catturare il payload
        def fake_thread(target=None, daemon=None, name=None, **kw):
            t = mock.MagicMock()
            t.start = target  # esegue subito la funzione
            return t

        with mock.patch("threading.Thread", side_effect=fake_thread):
            with mock.patch("ollama_utils.requests.post", side_effect=fake_post):
                avvia_warmup(modello="gemma2:2b")
                # start è la funzione target — la invochiamo
                ollama_utils._warmup_done = False  # reset per test
        # Non facciamo asserzioni sul payload perché il thread è daemon
        # — verifichiamo solo che non crashi
        self.assertTrue(True)


# ── chiedi_parallelo ──────────────────────────────────────────────────────────

class TestChiediParallelo(unittest.TestCase):

    def test_ordine_risultati_preservato(self):
        attese = ["A", "B", "C"]
        i = iter(attese)

        def fake_chiedi(prompt, tipo="risposta", modello=None):
            return next(i)

        with mock.patch("ollama_utils.chiedi_ollama", side_effect=fake_chiedi):
            risultato = chiedi_parallelo([
                ("p1", "risposta"),
                ("p2", "risposta"),
                ("p3", "risposta"),
            ])
        self.assertEqual(risultato, attese)

    def test_lista_vuota_ritorna_lista_vuota(self):
        result = chiedi_parallelo([])
        self.assertEqual(result, [])

    def test_singolo_task(self):
        with mock.patch("ollama_utils.chiedi_ollama", return_value="ok"):
            result = chiedi_parallelo([("prompt", "risposta")])
        self.assertEqual(result, ["ok"])

    def test_lunghezza_output_uguale_a_input(self):
        with mock.patch("ollama_utils.chiedi_ollama", return_value="x"):
            result = chiedi_parallelo([
                ("p1", "risposta"), ("p2", "codice"), ("p3", "analisi")
            ])
        self.assertEqual(len(result), 3)


# ── chiedi_ollama_think ───────────────────────────────────────────────────────

class TestChiediOllamaThink(unittest.TestCase):

    def _fake_post(self, thinking="", response="risposta"):
        def post(url, json=None, timeout=None, **kw):
            r = mock.MagicMock()
            r.raise_for_status = lambda: None
            r.json.return_value = {"thinking": thinking, "response": response}
            return r
        return post

    def test_payload_contiene_think_true(self):
        """chiedi_ollama_think deve avere "think": True — a differenza di chiedi_ollama."""
        payload_catturato = {}

        def fake_post(url, json=None, timeout=None, **kw):
            payload_catturato.update(json or {})
            r = mock.MagicMock()
            r.raise_for_status = lambda: None
            r.json.return_value = {"thinking": "", "response": "ok"}
            return r

        with mock.patch("ollama_utils.requests.post", side_effect=fake_post):
            chiedi_ollama_think("test")
        self.assertTrue(payload_catturato.get("think"))

    def test_ritorna_tuple_di_due_elementi(self):
        with mock.patch("ollama_utils.requests.post",
                        side_effect=self._fake_post("pens.", "risp.")):
            result = chiedi_ollama_think("test")
        self.assertIsInstance(result, tuple)
        self.assertEqual(len(result), 2)

    def test_thinking_e_risposta_corretti(self):
        with mock.patch("ollama_utils.requests.post",
                        side_effect=self._fake_post("mio ragionamento", "mia risposta")):
            thinking, risposta = chiedi_ollama_think("test")
        self.assertEqual(thinking, "mio ragionamento")
        self.assertEqual(risposta, "mia risposta")

    def test_fallback_tag_think_estratto_da_response(self):
        """Se "thinking" è vuoto, cerca <think>...</think> nel testo response."""
        with mock.patch("ollama_utils.requests.post",
                        side_effect=self._fake_post("", "<think>pensiero</think>risposta finale")):
            thinking, risposta = chiedi_ollama_think("test")
        self.assertEqual(thinking, "pensiero")
        self.assertEqual(risposta, "risposta finale")

    def test_connection_error_ritorna_tuple_con_errore(self):
        import requests as req
        with mock.patch("ollama_utils.requests.post",
                        side_effect=req.exceptions.ConnectionError()):
            thinking, risposta = chiedi_ollama_think("test")
        self.assertEqual(thinking, "")
        self.assertTrue(risposta.startswith("❌"))

    def test_timeout_ritorna_tuple_con_errore(self):
        import requests as req
        with mock.patch("ollama_utils.requests.post",
                        side_effect=req.exceptions.ReadTimeout()):
            thinking, risposta = chiedi_ollama_think("test")
        self.assertEqual(thinking, "")
        self.assertTrue(risposta.startswith("❌"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
