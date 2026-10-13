"""Find archived death checkpoints for the Webtiles lobby."""

import os
import re

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
    return result
