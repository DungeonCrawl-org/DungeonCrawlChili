"""Find archived death checkpoints for the Webtiles lobby."""

import os
import re
import fcntl
import json
import tempfile

_SPECIES_NAMES = sorted((
    "Armataur", "Barachi", "Black Draconian", "Coglin", "Centaur",
    "Deep Dwarf", "Deep Elf", "Demigod", "Demonspawn", "Djinni",
    "Felid", "Formicid", "Gale Centaur", "Gargoyle", "Ghoul", "Gnoll",
    "Green Draconian", "Grey Draconian", "Halfling", "Hill Orc", "High Elf",
    "Human", "Kobold", "Lava Orc", "Mayflytaur", "Merfolk", "Meteoran",
    "Minotaur", "Mottled Draconian", "Mountain Dwarf", "Mummy", "Naga",
    "Octopode", "Oni", "Pale Draconian", "Poltergeist", "Purple Draconian",
    "Red Draconian", "Revenant", "Silent Spectre", "Sludge Elf", "Spriggan",
    "Tengu", "Troll", "Vampire", "Vine Stalker", "White Draconian",
    "Yellow Draconian", "Draconian",
), key=len, reverse=True)


def _species_and_background(text):
    began = re.match(r"^Began as an? (.+?) on ", text)
    if not began:
        return "", ""
    character = began.group(1)
    for species in _SPECIES_NAMES:
        prefix = species + " "
        if character.casefold().startswith(prefix.casefold()):
            return species, character[len(prefix):]
    return "", character


def morgue_directory(game, username):
    path = game.templated("morgue_path", username=username)
    if path and not os.path.isabs(path):
        cwd = game.templated("cwd", username=username)
        if cwd:
            path = os.path.join(cwd, path)
    return path


def _list_player_replays(morgue_dir, player, archive_dir):
    if not morgue_dir or not player:
        return []
    pattern = re.compile(r"morgue-" + re.escape(player)
                         + r"-(\d{8}-\d{6})\.txt\Z")
    try:
        files = os.listdir(morgue_dir)
    except OSError:
        return []
    entries = []
    for filename in files:
        match = pattern.fullmatch(filename)
        if not match:
            continue
        stem = filename[:-4]
        # New checkpoints live in one shared archive for all DCF players;
        # also recognize the earlier per-player and adjacent layouts.
        if not any(os.path.isfile(os.path.join(checkpoint_dir,
                   stem + ".replay-" + str(slot) + ".cs"))
                   for checkpoint_dir in (archive_dir, morgue_dir,
                                          os.path.join(morgue_dir, "death-replay"))
                   for slot in range(2)):
            continue
        entries.append((match.group(1), filename))
    entries.sort(reverse=True)
    result = []
    for date, filename in entries:
        detail = "Death"
        score = None
        level = None
        location = "Unknown"
        species = ""
        background = ""
        try:
            with open(os.path.join(morgue_dir, filename),
                      encoding="utf-8", errors="replace") as morgue:
                for _ in range(256):
                    line = morgue.readline()
                    if not line:
                        break
                    text = line.strip()
                    scored = re.match(r"^(\d+) .+ \(level (\d+),", text)
                    if scored:
                        score = int(scored.group(1))
                        level = int(scored.group(2))
                    if text.startswith("Began as "):
                        species, background = _species_and_background(text)
                    if text.startswith(("Slain by ", "Mangled by ",
                                        "Killed by ", "Splashed by ",
                                        "Blasted by ", "Succumbed to ")):
                        detail = text[:180]
                    found_location = re.match(
                        r"^Final location: ([A-Za-z]+:[0-9]+), x=(\d+), y=(\d+)",
                        text)
                    if found_location:
                        location = "{} (x={}, y={})".format(
                            found_location.group(1), found_location.group(2),
                            found_location.group(3))
        except OSError:
            continue
        result.append({"date": date, "file": filename, "player": player,
                       "detail": detail, "score": score, "level": level,
                       "location": location, "species": species,
                       "background": background})
    return result


def _read_replay_counts(archive_dir):
    try:
        with open(os.path.join(archive_dir, "replay-counts.json"),
                  encoding="utf-8") as counts_file:
            counts = json.load(counts_file)
        if isinstance(counts, dict):
            return {key: value for key, value in counts.items()
                    if isinstance(key, str) and type(value) is int and value >= 0}
    except (OSError, ValueError):
        pass
    return {}


def _read_recommendation_counts(archive_dir):
    try:
        with open(os.path.join(archive_dir, "recommendation-counts.json"),
                  encoding="utf-8") as counts_file:
            counts = json.load(counts_file)
        if isinstance(counts, dict):
            return {key: value for key, value in counts.items()
                    if isinstance(key, str) and type(value) is int and value >= 0}
    except (OSError, ValueError):
        pass
    return {}


def record_replay(root, player, filename, entries=None):
    """Atomically increment the play count for a validated archived death."""
    root = os.path.normpath(root)
    entries = list_replays(root) if entries is None else entries
    if not any(entry["player"] == player and entry["file"] == filename
               for entry in entries):
        return False

    archive_dir = os.path.join(root, "death-replay")
    os.makedirs(archive_dir, exist_ok=True)
    key = player + "/" + filename
    lock_path = os.path.join(archive_dir, "replay-counts.lock")
    counts_path = os.path.join(archive_dir, "replay-counts.json")
    with open(lock_path, "a", encoding="utf-8") as lock_file:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX)
        counts = _read_replay_counts(archive_dir)
        # Drop counters for deaths that have already been pruned from the
        # server-wide archive.
        live_keys = {entry["player"] + "/" + entry["file"] for entry in entries}
        counts = {old_key: value for old_key, value in counts.items()
                  if old_key in live_keys}
        counts[key] = counts.get(key, 0) + 1
        temp_path = None
        try:
            with tempfile.NamedTemporaryFile(
                    mode="w", encoding="utf-8", dir=archive_dir,
                    prefix=".replay-counts-", delete=False) as temp_file:
                temp_path = temp_file.name
                json.dump(counts, temp_file, sort_keys=True)
                temp_file.write("\n")
                temp_file.flush()
                os.fsync(temp_file.fileno())
            os.replace(temp_path, counts_path)
        finally:
            if temp_path and os.path.exists(temp_path):
                os.unlink(temp_path)
        return True


def record_recommendation(root, player, filename, entries=None):
    """Atomically increment the recommendation count for a validated death."""
    root = os.path.normpath(root)
    entries = list_replays(root) if entries is None else entries
    if not any(entry["player"] == player and entry["file"] == filename
               for entry in entries):
        return False

    archive_dir = os.path.join(root, "death-replay")
    os.makedirs(archive_dir, exist_ok=True)
    key = player + "/" + filename
    lock_path = os.path.join(archive_dir, "replay-counts.lock")
    counts_path = os.path.join(archive_dir, "recommendation-counts.json")
    with open(lock_path, "a", encoding="utf-8") as lock_file:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX)
        counts = _read_recommendation_counts(archive_dir)
        live_keys = {entry["player"] + "/" + entry["file"] for entry in entries}
        counts = {old_key: value for old_key, value in counts.items()
                  if old_key in live_keys}
        counts[key] = counts.get(key, 0) + 1
        temp_path = None
        try:
            with tempfile.NamedTemporaryFile(
                    mode="w", encoding="utf-8", dir=archive_dir,
                    prefix=".recommendation-counts-", delete=False) as temp_file:
                temp_path = temp_file.name
                json.dump(counts, temp_file, sort_keys=True)
                temp_file.write("\n")
                temp_file.flush()
                os.fsync(temp_file.fileno())
            os.replace(temp_path, counts_path)
        finally:
            if temp_path and os.path.exists(temp_path):
                os.unlink(temp_path)
        return True


def list_replays(morgue_dir, username=None):
    """List checkpointed deaths for one player, or all player folders."""
    if not morgue_dir:
        return []
    morgue_dir = os.path.normpath(morgue_dir)
    root = os.path.dirname(morgue_dir) if username else morgue_dir
    archive_dir = os.path.join(root, "death-replay")
    players = []
    if username:
        players.append((username, morgue_dir))
    else:
        try:
            with os.scandir(morgue_dir) as directories:
                for directory in directories:
                    if directory.is_dir(follow_symlinks=False):
                        players.append((directory.name, directory.path))
        except OSError:
            return []
    result = []
    for player, directory in players:
        result.extend(_list_player_replays(directory, player, archive_dir))
    result.sort(key=lambda entry: (entry["date"], entry["player"]), reverse=True)
    counts = _read_replay_counts(archive_dir)
    recommendations = _read_recommendation_counts(archive_dir)
    for entry in result:
        key = entry["player"] + "/" + entry["file"]
        entry["replayed"] = counts.get(key, 0)
        entry["recommended"] = recommendations.get(key, 0)
    return result
