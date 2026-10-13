import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from webtiles.death_replay import list_replays, morgue_directory


class DeathReplayListTests(unittest.TestCase):
    def test_only_own_checkpointed_deaths_are_listed_newest_first(self):
        with tempfile.TemporaryDirectory() as folder:
            def death(name, date, checkpoint):
                stem = "morgue-{}-{}".format(name, date)
                Path(folder, stem + ".txt").write_text(
                    "Character file\n399 Player the Fighter (level 9, -1/50 HPs)\n"
                    "Began as a Kobold Artificer on Oct 12, 2026.\n"
                    " Slain by an ogre\n")
                if checkpoint:
                    Path(folder, stem + ".replay-0.cs").touch()

            death("Player", "20261011-120000", True)
            death("Player", "20261012-120000", True)
            death("Player", "20261013-120000", False)
            death("Other", "20261014-120000", True)
            entries = list_replays(folder, "Player")
            self.assertEqual([entry["date"] for entry in entries],
                             ["20261012-120000", "20261011-120000"])
            self.assertTrue(all(entry["detail"] == "Slain by an ogre"
                                for entry in entries))
            self.assertEqual(entries[0]["score"], 399)
            self.assertEqual(entries[0]["level"], 9)
            self.assertEqual(entries[0]["species"], "Kobold")
            self.assertEqual(entries[0]["background"], "Artificer")
            self.assertEqual(list_replays(folder, "Other")[0]["date"],
                             "20261014-120000")

    def test_relative_morgue_path_uses_game_directory(self):
        class Game:
            def templated(self, key, username):
                return {"morgue_path": "rcs/" + username,
                        "cwd": "/crawl"}[key]
        self.assertEqual(morgue_directory(Game(), "Player"),
                         os.path.join("/crawl", "rcs", "Player"))


if __name__ == "__main__":
    unittest.main()
