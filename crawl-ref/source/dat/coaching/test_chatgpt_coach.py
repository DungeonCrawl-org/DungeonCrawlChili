"""Run with: python3 dat/coaching/test_chatgpt_coach.py"""

import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "coach", Path(__file__).with_name("chatgpt-coach.py"))
coach = importlib.util.module_from_spec(spec)
spec.loader.exec_module(coach)


class CoachingBridgeTests(unittest.TestCase):
    def test_signed_out_and_api_login_never_send_dump(self):
        for code, status in ((1, "Not logged in"), (0, "Logged in using an API key")):
            with self.subTest(status=status), tempfile.TemporaryDirectory() as directory:
                with patch.object(coach.shutil, "which", return_value="/fake/codex"), \
                     patch.object(coach.subprocess, "run", return_value=
                                  subprocess.CompletedProcess([], code, "", status)) as run:
                    message = coach.ask_chatgpt("Private live dump", directory)
                    self.assertIn("sign-in is needed", message)
                    self.assertEqual(run.call_count, 1)
                    self.assertNotIn("input", run.call_args.kwargs)

    def test_chatgpt_receives_exact_prompt_without_api_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            calls = []

            def run(command, **kwargs):
                calls.append((command, kwargs))
                if "status" in command:
                    return subprocess.CompletedProcess(command, 0, "Logged in using ChatGPT", "")
                Path(directory, "answer.txt").write_text("Retreat to the stairs.")
                return subprocess.CompletedProcess(command, 0, "", "")

            with patch.dict(os.environ, {"OPENAI_API_KEY": "never-send",
                                         "CODEX_API_KEY": "never-send",
                                         "OPENAI_BASE_URL": "https://other.example"}), \
                 patch.object(coach.shutil, "which", return_value="/fake/codex"), \
                 patch.object(coach.subprocess, "run", side_effect=run):
                self.assertEqual(coach.ask_chatgpt("Live dump #123", directory),
                                 "Retreat to the stairs.")
            command, options = calls[1]
            self.assertEqual(options["input"], "Live dump #123")
            self.assertEqual(options["cwd"], directory)
            for _, options in calls:
                for key in ("OPENAI_API_KEY", "CODEX_API_KEY", "OPENAI_BASE_URL"):
                    self.assertNotIn(key, options["env"])
            self.assertIn("--ignore-user-config", command)
            self.assertIn("--ephemeral", command)
            self.assertIn("read-only", command)
            self.assertNotIn("--dangerously-bypass-approvals-and-sandbox", command)

    def test_failure_does_not_expose_cli_account_details(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(coach.shutil, "which", return_value="/fake/codex"), \
                 patch.object(coach.subprocess, "run", side_effect=[
                     subprocess.CompletedProcess([], 0, "Logged in using ChatGPT", ""),
                     subprocess.CompletedProcess([], 1, "secret account data", "secret token")]):
                message = coach.ask_chatgpt("dump", directory)
                self.assertIn("could not provide advice", message)
                self.assertNotIn("secret", message)

    def test_missing_cli_offers_browser(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(coach.shutil, "which", return_value=None), \
                 patch.object(coach.Path, "home", return_value=Path(directory)):
                self.assertIn("press B", coach.ask_chatgpt("dump", directory))


if __name__ == "__main__":
    unittest.main()
