"""WebTiles replay requires a server opt-in, independent of player RCs."""

from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

import tornado.template

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from webtiles import config
from webtiles.process_handler import _crawl_environment
from webtiles.ws_handler import CrawlWebSocket


class ReplayPolicyTests(unittest.TestCase):
    def setUp(self):
        configuration = patch.object(config, "server_config", {})
        configuration.start()
        self.addCleanup(configuration.stop)

    def game(self, env=None):
        return SimpleNamespace(templated=lambda *args, **kwargs: env or {})

    def test_webtiles_servers_default_to_no_replays(self):
        self.assertFalse(config.get("enable_death_replay"))
        self.assertEqual(_crawl_environment(self.game(), "Player")["DCCHILI_DEATH_REPLAY"], "0")

    def test_disabled_server_overrides_a_game_environment_flag(self):
        original = {"DCCHILI_DEATH_REPLAY": "1", "EXTRA_TEST_VALUE": "kept"}
        result = _crawl_environment(self.game(original), "Player")
        self.assertEqual(result["DCCHILI_DEATH_REPLAY"], "0")
        self.assertEqual(result["EXTRA_TEST_VALUE"], "kept")
        self.assertEqual(original["DCCHILI_DEATH_REPLAY"], "1")

    def test_dcf_opt_in_reaches_the_game_process(self):
        config.server_config["enable_death_replay"] = True
        self.assertEqual(_crawl_environment(self.game(), "Player")["DCCHILI_DEATH_REPLAY"], "1")

    def test_disabled_catalogue_request_does_not_scan_morgues(self):
        socket = SimpleNamespace(username="Player", game_id_allowed=lambda key: True,
                                 send_message=Mock())
        with patch("webtiles.death_replay.list_replays") as listing:
            CrawlWebSocket.list_death_replays(socket, "chili")
        listing.assert_not_called()
        socket.send_message.assert_not_called()

    def test_disabled_replay_start_is_rejected_before_opening_files(self):
        socket = SimpleNamespace(go_lobby=Mock())
        CrawlWebSocket.start_crawl(socket, "chili", "morgue-Player.txt", "Player")
        socket.go_lobby.assert_called_once_with()

    def links(self):
        root = Path(__file__).resolve().parents[1] / "templates"
        template = tornado.template.Loader(str(root)).load("game_links.html")
        return template.generate(config=config, games={"chili": {
            "name": "Dungeon Crawl Chili", "rcfile_path": "/rc/chili"}},
            save_info={"chili": None}, disabled=lambda save: False)

    def test_disabled_lobby_has_no_death_replay_link(self):
        self.assertNotIn(b"Death Replay", self.links())

    def test_opted_in_lobby_keeps_its_death_replay_link(self):
        config.server_config["enable_death_replay"] = True
        self.assertIn(b"Death Replay", self.links())


if __name__ == "__main__":
    unittest.main()
