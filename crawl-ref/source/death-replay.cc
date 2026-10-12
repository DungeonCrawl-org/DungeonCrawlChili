#include "AppHdr.h"
#include "death-replay.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>

#include "files.h"
#include "package.h"
#include "player.h"
#include "store.h"
#include "stringutil.h"

static const char *const REPLAY_KEY = "death_replay_source";
static const char *const LAST_KEY = "death_replay_last_checkpoint";
static const char *const META_KEY = "death-replay";
static const int CHECKPOINT_INTERVAL = 100;

bool is_death_replay()
{
    return you.props.exists(REPLAY_KEY);
}

bool death_replay_checkpoint_due(int turn, int last)
{
    return turn >= 0 && turn != last && (last < 0 || turn % CHECKPOINT_INTERVAL == 0);
}

bool parse_death_replay(const string &text, death_replay_data &data, string &error)
{
    data = death_replay_data();
    error.clear();
    if (text.size() > 2 * 1024 * 1024)
    {
        error = "Morgue exceeds the prototype's 2 MiB limit.";
        return false;
    }
    bool recap = false, location = false, seed = false;
    std::smatch match;
    try
    {
        for (const string &line : split_string("\n", text, false, true))
        {
            if (starts_with(line, "Death recap")) recap = true;
            else if (line == "Message History") recap = false;
            if (std::regex_search(line, match, std::regex("Game seed: ([0-9]+)")))
            {
                data.seed = std::stoull(match[1]);
                seed = true;
            }
            if (std::regex_search(line, match, std::regex("^[0-9]+ (.+?) the .*\\(level [0-9]+,")))
                data.name = match[1];
            if (std::regex_search(line, match, std::regex("Turns: *([0-9]+)")))
                data.turns = std::stoi(match[1]);
            if (recap && std::regex_search(line, match,
                    std::regex("Final location: ([A-Za-z]+:[0-9]+), x=([0-9]+), y=([0-9]+)")))
            {
                data.level = level_id::parse_level_id(match[1]);
                data.position = coord_def(std::stoi(match[2]), std::stoi(match[3]));
                location = true;
                if (std::regex_search(line, match, std::regex("turn ([0-9]+)")))
                    data.turns = std::stoi(match[1]);
            }
        }
    }
    catch (const std::exception &)
    {
        error = "Invalid seed, level, or coordinates in the morgue.";
        return false;
    }
    if (!location)
        error = "Death Replay requires a Final location entry in the morgue's Death Recap section. Older morgues cannot be used by this prototype.";
    else if (!seed || data.name.empty() || data.turns < 0 || !data.level.is_valid()
        || data.position.x < 1 || data.position.x >= GXM - 1
        || data.position.y < 1 || data.position.y >= GYM - 1)
        error = "The morgue has missing or invalid replay metadata.";
    return error.empty();
}

bool read_replay_checkpoint(const string &path, replay_checkpoint &checkpoint)
{
    try
    {
        package save(path.c_str(), false);
        if (!save.has_chunk(META_KEY))
            return false;
        std::unique_ptr<chunk_reader> reader(save.reader(META_KEY));
        char raw[2048];
        const size_t size = reader->read(raw, sizeof raw);
        if (size == sizeof raw)
            return false;
        std::istringstream input(string(raw, size));
        string magic, turn, seed, birth, name;
        if (!std::getline(input, magic) || magic != "ChiliDeathReplay1"
            || !std::getline(input, turn) || !std::getline(input, seed)
            || !std::getline(input, birth) || !std::getline(input, name))
            return false;
        checkpoint = replay_checkpoint();
        checkpoint.path = path;
        checkpoint.turn = std::stoi(turn);
        checkpoint.seed = std::stoull(seed);
        checkpoint.birth = std::stoll(birth);
        checkpoint.name = name;
        return checkpoint.turn >= 0 && !name.empty();
    }
    catch (...)
    {
        return false;
    }
}

#ifdef USE_TILE_LOCAL
#include "chardump.h"
#include "format.h"
#include "hiscores.h"
#include "message.h"
#include "newgame-def.h"
#include "outer-menu.h"
#include "scroller.h"
#include "state.h"
#include "unwind.h"

static std::unique_ptr<death_replay_data> pending;
static replay_checkpoint selected;

static bool _copy_save(const string &source, const string &target)
{
    if (file_exists(target))
        return false;
    std::ifstream input(source, std::ios::binary);
    if (!input) return false;
    std::ofstream output(target, std::ios::binary);
    if (!output) return false;
    output << input.rdbuf();
    output.close();
    if (!input.bad() && output) return true;
    std::remove(target.c_str());
    return false;
}

static string _checkpoint_path(int slot)
{
    const string normal = get_savedir_filename(you.your_name);
    return normal + "." + std::to_string(you.birth_time) + ".replay-" + std::to_string(slot);
}

void checkpoint_death_replay()
{
    if (!crawl_state.game_started || crawl_state.generating_level || is_death_replay()
        || Options.no_save || !you.save || you.hp <= 0 || !you.on_current_level)
        return;
    const int last = you.props.exists(LAST_KEY) ? you.props[LAST_KEY].get_int() : -1;
    if (!death_replay_checkpoint_due(you.num_turns, last))
        return;
    you.props[LAST_KEY] = you.num_turns;
    const string target = _checkpoint_path((you.num_turns / CHECKPOINT_INTERVAL) % 2);
    const string temporary = target + ".tmp";
    std::remove(temporary.c_str());
    you.save->commit();
    if (!_copy_save(you.save->get_filename(), temporary))
    {
        mprf(MSGCH_ERROR, "Could not create Death Replay checkpoint.");
        return;
    }
    {
        // Serialize into a cloned package. The ordinary save is never marked
        // as a replay and the live character keeps its normal game state.
        std::unique_ptr<package> copy(new package(temporary.c_str(), true));
        unwind_var<package *> save(you.save, copy.get());
        unwind_var<bool> wizard(you.wizard, true);
        struct marker_scope
        {
            ~marker_scope() { you.props.erase(REPLAY_KEY); }
        } marker;
        you.props[REPLAY_KEY] = you.your_name;
        save_game_state();
        const string metadata = "ChiliDeathReplay1\n" + std::to_string(you.num_turns)
            + "\n" + std::to_string(you.game_seed) + "\n" + std::to_string(you.birth_time)
            + "\n" + you.your_name + "\n";
        {
            std::unique_ptr<chunk_writer> writer(copy->writer(META_KEY));
            writer->write(metadata.data(), metadata.size());
        }
        copy->commit();
    }
    if (std::rename(temporary.c_str(), target.c_str()) != 0)
    {
        std::remove(temporary.c_str());
        mprf(MSGCH_ERROR, "Could not rotate Death Replay checkpoints.");
    }
}

void archive_death_replay(const scorefile_entry &score)
{
    if (is_death_replay() || Options.no_save)
        return;
    string directory = morgue_directory();
    if (!check_mkdir("morgue", &directory, true)) return;
    const string base = catpath(directory,
        strip_filename_unsafe_chars(morgue_name(score.get_name(), score.get_death_time())));
    for (int slot = 0; slot < 2; ++slot)
    {
        const string source = _checkpoint_path(slot);
        replay_checkpoint info;
        if (!read_replay_checkpoint(source, info) || info.name != you.your_name
            || info.seed != you.game_seed || info.birth != you.birth_time
            || info.turn > you.num_turns)
            continue;
        const string target = base + ".replay-" + std::to_string(slot) + ".cs";
        if (_copy_save(source, target))
            std::remove(source.c_str());
        else
            mprf(MSGCH_ERROR, "Could not preserve Death Replay checkpoint %d.", slot);
    }
}

bool death_replay_pending() { return bool(pending); }

static void _notice(const string &text)
{
    formatted_scroller screen;
    screen.set_title(formatted_string("Death Replay", WHITE));
    screen.add_raw_text(text);
    screen.show();
}

bool prepare_death_replay(const string &path)
{
    pending.reset();
    std::ifstream input(path);
    string text;
    char buffer[4096];
    while (input.read(buffer, sizeof buffer) || input.gcount())
    {
        text.append(buffer, input.gcount());
        if (text.size() > 2 * 1024 * 1024) break;
    }
    std::unique_ptr<death_replay_data> data(new death_replay_data);
    string error;
    if (!input.eof() || !parse_death_replay(text, *data, error))
    {
        _notice(error.empty() ? "The morgue could not be read." : error);
        return false;
    }
    vector<replay_checkpoint> checkpoints;
    const string base = path.substr(0, path.size() - 4);
    for (int slot = 0; slot < 2; ++slot)
    {
        replay_checkpoint info;
        if (read_replay_checkpoint(base + ".replay-" + std::to_string(slot) + ".cs", info)
            && info.name == data->name && info.seed == data->seed && info.turn <= data->turns)
            checkpoints.push_back(info);
    }
    sort(checkpoints.begin(), checkpoints.end(), [](const replay_checkpoint &a, const replay_checkpoint &b) { return a.turn < b.turn; });
    if (checkpoints.empty())
    {
        _notice("No preserved checkpoints exist for this death.\n\n"
            "This prototype requires Final location in Death Recap and a recorded checkpoint. "
            "New local Tiles games save a practice checkpoint at their first input and every 100 turns, retaining the last two. "
            "A morgue alone cannot restore a playable game.");
        return false;
    }
    using namespace ui;
    auto box = make_shared<Box>(Widget::VERT);
    auto description = make_shared<Text>(make_stringf(
        "Death Replay prototype - wizard practice\n\n%s died on %s, x=%d, y=%d, turn %d.\n"
        "Maximum available rewind: %d turns.\n\n"
        "Load an actual saved game state, including inventory, spell library, map, monsters and RNG state. "
        "The original character and archives are preserved. Practice uses a separate save and is unscored.\n\n"
        "[R] Load oldest checkpoint   [1-2] Choose checkpoint   [Esc] Cancel",
        data->name.c_str(), data->level.describe().c_str(), data->position.x, data->position.y,
        data->turns, data->turns - checkpoints.front().turn));
    description->set_wrap_text(true);
    description->max_size().width = 700;
    box->add_child(description);
    bool done = false;
    int choice = -1;
    vector<shared_ptr<MenuButton>> buttons;
    for (size_t i = 0; i < checkpoints.size(); ++i)
    {
        auto button = make_shared<MenuButton>();
        button->set_child(make_shared<Text>(make_stringf("[%zu] Replay from turn %d (%d turns before death)",
            i + 1, checkpoints[i].turn, data->turns - checkpoints[i].turn)));
        button->on_activate_event([&, i](const ActivateEvent&) { choice = i; done = true; return true; });
        box->add_child(button);
        buttons.push_back(button);
    }
    auto popup = make_shared<ui::Popup>(box);
    popup->on_hotkey_event([&](const KeyEvent &event) {
        if (event.key() == 'r' || event.key() == 'R') { choice = 0; done = true; }
        else if (event.key() >= '1' && event.key() < '1' + static_cast<int>(checkpoints.size())) { choice = event.key() - '1'; done = true; }
        else if (key_is_escape(event.key())) done = true;
        else return false;
        return true;
    });
    run_layout(popup, done, buttons.front());
    if (choice < 0) return false;
    selected = checkpoints[choice];
    pending = std::move(data);
    return true;
}

void configure_death_replay(newgame_def &game)
{
    ASSERT(pending);
    game = newgame_def();
    for (int suffix = 1; ; ++suffix)
    {
        game.name = "DeathReplay" + std::to_string(suffix);
        if (!save_exists(get_save_filename(game.name))) break;
    }
    game.filename = get_save_filename(game.name);
    game.type = GAME_TYPE_CUSTOM_SEED;
    game.seed = pending->seed;
    Options.seed = game.seed;
}

bool restore_death_replay(const newgame_def &game)
{
    const string target = get_savedir_filename(game.name);
    if (!_copy_save(selected.path, target))
    {
        pending.reset();
        _notice("Could not create a separate replay save. The archive is unchanged.");
        return false;
    }
    if (!restore_game(game.filename))
    {
        std::remove(target.c_str());
        pending.reset();
        return false;
    }
    if (you.your_name != selected.name || you.game_seed != selected.seed
        || you.birth_time != selected.birth || you.num_turns != selected.turn)
    {
        you.save->unlink();
        delete you.save;
        you.save = nullptr;
        pending.reset();
        _notice("The checkpoint metadata does not match the saved character.");
        return false;
    }
    you.your_name = game.name;
    you.wizard = true;
    you.props[REPLAY_KEY] = selected.name;
    return true;
}

void start_death_replay()
{
    ASSERT(pending);
    const int rewind = pending->turns - selected.turn;
    const string name = pending->name;
    pending.reset();
    save_game_state();
    _notice(make_stringf("Loaded %s at turn %d, %d turns before death.\n\n"
        "The complete checkpoint state is restored. Choose different actions and play forward. "
        "This is wizard practice: scores and ghost creation are disabled. "
        "Saving creates a separate DeathReplay character; the checkpoint archive is unchanged.",
        name.c_str(), you.num_turns, rewind));
}

#else
void checkpoint_death_replay() {}
void archive_death_replay(const scorefile_entry &) {}
bool prepare_death_replay(const string &) { return false; }
bool death_replay_pending() { return false; }
void configure_death_replay(newgame_def &) {}
bool restore_death_replay(const newgame_def &) { return false; }
void start_death_replay() {}
#endif
