"""Personal ChatGPT coaching bridge. Python 3 standard library only.

Uses the player's own Codex ChatGPT login; never reads credentials or uses an
API key. The caller owns a private temporary directory and process group.
"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def ask_chatgpt(prompt, directory):
    codex = shutil.which("codex")
    if not codex:
        candidate = Path.home() / ".local/bin/codex"
        if candidate.is_file():
            codex = str(candidate)
    if not codex:
        return "Codex is not installed. Install Codex and run 'codex login', " \
            "or press B to use ChatGPT in your browser."

    # Keep this prototype on ChatGPT plan usage, even when API keys are set.
    environment = os.environ.copy()
    for key in ("OPENAI_API_KEY", "CODEX_API_KEY", "OPENAI_BASE_URL"):
        environment.pop(key, None)
    login = subprocess.run(
        [codex, "login", "status"], capture_output=True, text=True,
        timeout=10, env=environment, cwd=directory)
    if login.returncode or "Logged in using ChatGPT" not in login.stdout + login.stderr:
        return "ChatGPT sign-in is needed for in-game coaching. Run 'codex login' " \
            "in a terminal, then try again. Or press B to open ChatGPT and paste " \
            "the dump. Browser access depends on your account's available usage. " \
            "The game cannot supply anonymous free API tokens."

    answer = Path(directory) / "answer.txt"
    command = [
        codex, "exec", "--ignore-user-config", "--ignore-rules",
        "--ephemeral", "--skip-git-repo-check", "--sandbox", "read-only",
        "--color", "never", "-c", 'web_search="disabled"',
        "--disable", "shell_tool", "--disable", "unified_exec",
        "--disable", "plugins", "--disable", "multi_agent",
        "--disable", "view_image", "-o", str(answer), "-",
    ]
    # No shell, repository context, user config, tool connections, or API billing.
    # stdout/stderr stay local and are never shown (may contain account metadata).
    completed = subprocess.run(
        command, input=prompt, capture_output=True, text=True,
        timeout=100, env=environment, cwd=directory)
    if completed.returncode:
        return "ChatGPT could not provide advice. Your account may have reached " \
            "its usage limit, or the connection failed. Try again later, or " \
            "press B to use the browser. No API-key billing was attempted."
    if not answer.is_file() or answer.stat().st_size > 24000:
        return "The coach returned no usable advice. Try again or use the browser."
    advice = answer.read_text(encoding="utf-8").strip()
    return advice or "The coach returned no advice. Try again or use the browser."


def main():
    request, response = map(Path, sys.argv[1:3])
    try:
        if request.stat().st_size > 1024 * 1024:
            message = "The character dump is too large for this prototype. Use the browser."
        else:
            message = ask_chatgpt(request.read_text(encoding="utf-8"), request.parent)
    except subprocess.TimeoutExpired:
        message = "ChatGPT took too long to respond. Try again or use the browser."
    except (OSError, UnicodeError):
        message = "Could not run the ChatGPT coach. Check Python 3 and Codex, or use the browser."
    response.write_text(json.dumps({"message": message}), encoding="utf-8")


if __name__ == "__main__":
    main()
