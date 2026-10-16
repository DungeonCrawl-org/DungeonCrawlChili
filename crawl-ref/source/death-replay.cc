#include "AppHdr.h"
#include "death-replay.h"

#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <regex>
#include <sstream>

#include "files.h"
#include "package.h"
#include "player.h"
#include "store.h"
#include "stringutil.h"
#include "tiles-build-specific.h"
#include "version.h"

static const char *const REPLAY_KEY = "death_replay_source";
static const char *const LAST_KEY = "death_replay_last_checkpoint";
static const char *const META_KEY = "death-replay";
static const char *const DEATH_BRANCH_KEY = "death_replay_death_branch";
static const char *const DEATH_DEPTH_KEY = "death_replay_death_depth";
static const char *const DEATH_X_KEY = "death_replay_death_x";
static const char *const DEATH_Y_KEY = "death_replay_death_y";
static const int CHECKPOINT_INTERVAL = 50;
static int recommendation_vote = 0;

bool is_death_replay()
{
    return you.props.exists(REPLAY_KEY);
}

void set_death_replay_recommendation(bool recommend)
{
    recommendation_vote = recommend ? 1 : -1;
}

int death_replay_recommendation_vote()
{
    return recommendation_vote;
}

bool death_replay_marker_position(coord_def &position)
{
    if (!is_death_replay() || !you.props.exists(DEATH_BRANCH_KEY)
        || !you.props.exists(DEATH_DEPTH_KEY) || !you.props.exists(DEATH_X_KEY)
        || !you.props.exists(DEATH_Y_KEY))
    {
        return false;
    }

    const level_id death_level(
        static_cast<branch_type>(you.props[DEATH_BRANCH_KEY].get_int()),
        you.props[DEATH_DEPTH_KEY].get_int());
    if (!death_level.is_valid() || level_id::current() != death_level)
        return false;

    position = coord_def(you.props[DEATH_X_KEY].get_int(),
                         you.props[DEATH_Y_KEY].get_int());
    return position.x > 0 && position.x < GXM - 1
        && position.y > 0 && position.y < GYM - 1;
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
                    std::regex("Final location: ([A-Za-z]+(?::[0-9]+)?), x=([0-9]+), y=([0-9]+)")))
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
        {
            return false;
        }
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

int select_death_replay_checkpoint(const vector<replay_checkpoint> &checkpoints,
                                   int death_turn)
{
    constexpr int target_rewind = 40;
    constexpr int minimum_rewind = 20;
    int selected_index = -1;
    int best_distance = std::numeric_limits<int>::max();
    int best_rewind = -1;
    for (size_t i = 0; i < checkpoints.size(); ++i)
    {
        const int rewind = death_turn - checkpoints[i].turn;
        if (rewind < minimum_rewind)
            continue;
        const int distance = rewind > target_rewind
            ? rewind - target_rewind : target_rewind - rewind;
        // On an exact tie, prefer the earlier checkpoint for more practice.
        if (distance < best_distance
            || (distance == best_distance && rewind > best_rewind))
        {
            selected_index = static_cast<int>(i);
            best_distance = distance;
            best_rewind = rewind;
        }
    }
    return selected_index;
}

#if defined(USE_TILE) && defined(CRAWL_HAS_DEATH_REPLAY)
#include "chardump.h"
#include "format.h"
#include "hiscores.h"
#include "message.h"
#include "newgame-def.h"
#include "outer-menu.h"
#include "scroller.h"
#include "state.h"
#include "syscalls.h"
#include "unwind.h"

static std::unique_ptr<death_replay_data> pending;
static replay_checkpoint selected;
static string queued_morgue;
static const size_t MAX_ARCHIVED_DEATHS = 100;

static string _parent_directory(string path)
{
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();
    const size_t separator = path.find_last_of("/\\");
    return separator == string::npos ? "" : path.substr(0, separator);
}

static bool _checkpoint_stem(const string &filename, string &stem)
{
    static const std::regex checkpoint_name(
        "(morgue-.+-[0-9]{8}-[0-9]{6})\\.replay-[01]\\.cs");
    std::smatch match;
    if (!std::regex_match(filename, match, checkpoint_name))
        return false;
    stem = match[1];
    return true;
}

static void _prune_replay_archive(const string &archive_dir)
{
    vector<string> deaths;
    for (const string &filename : get_dir_files(archive_dir))
    {
        string stem;
        if (_checkpoint_stem(filename, stem)
            && std::find(deaths.begin(), deaths.end(), stem) == deaths.end())
            deaths.push_back(stem);
    }
    // Player names precede timestamps in morgue filenames, so sort by the
    // timestamp suffix to retain the newest deaths across the whole server.
    sort(deaths.begin(), deaths.end(), [](const string &a, const string &b) {
        const string date_a = a.substr(a.size() - 15);
        const string date_b = b.substr(b.size() - 15);
        return date_a == date_b ? a > b : date_a > date_b;
    });
    for (size_t i = MAX_ARCHIVED_DEATHS; i < deaths.size(); ++i)
        for (int slot = 0; slot < 2; ++slot)
        {
            const string old_file = catpath(archive_dir,
                deaths[i] + ".replay-" + std::to_string(slot) + ".cs");
            std::remove(old_file.c_str());
        }
}

void queue_death_replay(const string &morgue_file)
{
    queued_morgue = morgue_file;
}

bool start_queued_death_replay(const string &player_name)
{
    if (queued_morgue.empty())
        return true;
    const string file = queued_morgue;
    queued_morgue.clear();
    // Webtiles sends an absolute, server-validated path when the selected
    // death belongs to another account. Keep the filename and containing
    // player directory tied together before opening it.
    if (!file.empty() && file[0] == '/')
    {
        const size_t slash = file.find_last_of('/');
        const string directory = slash == string::npos ? "" : file.substr(0, slash);
        string directory_player = directory.substr(directory.find_last_of('/') == string::npos
            ? 0 : directory.find_last_of('/') + 1);
        if (directory_player == "death-replay")
        {
            const string account_dir = directory.substr(0,
                directory.find_last_of('/'));
            directory_player = account_dir.substr(
                account_dir.find_last_of('/') == string::npos
                ? 0 : account_dir.find_last_of('/') + 1);
        }
        const string filename = slash == string::npos ? file : file.substr(slash + 1);
        const string prefix = "morgue-";
        const size_t date_start = filename.size() >= 20 ? filename.size() - 20 : 0;
        if (filename.size() != date_start + 20 || date_start <= prefix.size()
            || !starts_with(filename, prefix)
            || filename.substr(date_start, 1) != "-"
            || filename.substr(filename.size() - 4) != ".txt")
            return false;
        const string archive_player = filename.substr(prefix.size(),
                                                       date_start - prefix.size());
        const string date = filename.substr(date_start + 1, 15);
        if (archive_player.empty() || archive_player != directory_player
            || !std::all_of(date.begin(), date.end(), [](char c) {
                return c == '-' || (c >= '0' && c <= '9');
            }))
            return false;
        return prepare_death_replay(file);
    }
    const string prefix = "morgue-" + player_name + "-";
    if (!starts_with(file, prefix) || file.size() != prefix.size() + 19
        || file.substr(file.size() - 4) != ".txt"
        || !std::all_of(file.begin() + prefix.size(), file.end() - 4,
            [](char c) { return c == '-' || (c >= '0' && c <= '9'); }))
    {
        return false;
    }
    return prepare_death_replay(catpath(morgue_directory(), file));
}

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
        || Options.no_save || Options.opt_out_death_replay || !you.save
        || you.hp <= 0 || !you.on_current_level)
    {
        return;
    }
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
        // save_game(false) does not normally persist the current level on
        // desktop builds. A replay checkpoint must include current map
        // knowledge and level contents, rather than the level chunk from
        // when it was last entered or left.
        save_level(level_id::current());
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
    if (Options.opt_out_death_replay)
    {
        for (int slot = 0; slot < 2; ++slot)
            std::remove(_checkpoint_path(slot).c_str());
        return;
    }
    string directory = morgue_directory();
    if (!check_mkdir("morgue", &directory, true)) return;
    const string morgue_root = _parent_directory(directory);
    string archive_dir = catpath(morgue_root, "death-replay");
    if (!check_mkdir("Death Replay directory", &archive_dir, true)) return;

    // Consolidate checkpoints written by earlier prototype versions into the
    // shared archive before applying the server-wide retention limit.
    for (const string &player_dir_name : get_dir_files(morgue_root))
    {
        const string player_dir = catpath(morgue_root, player_dir_name);
        if (player_dir_name == "death-replay" || !dir_exists(player_dir))
            continue;
        for (const string &filename : get_dir_files(player_dir))
        {
            string stem;
            if (!_checkpoint_stem(filename, stem))
                continue;
            const string source = catpath(player_dir, filename);
            const string target = catpath(archive_dir, filename);
            if (file_exists(target))
                std::remove(source.c_str());
            else
                rename_u(source.c_str(), target.c_str());
        }
    }

    const string base = strip_filename_unsafe_chars(
        morgue_name(score.get_name(), score.get_death_time()));
    for (int slot = 0; slot < 2; ++slot)
    {
        const string source = _checkpoint_path(slot);
        replay_checkpoint info;
        if (!read_replay_checkpoint(source, info) || info.name != you.your_name
            || info.seed != you.game_seed || info.birth != you.birth_time
            || info.turn > you.num_turns)
        {
            continue;
        }
        const string target = catpath(archive_dir,
            base + ".replay-" + std::to_string(slot) + ".cs");
        if (_copy_save(source, target))
            std::remove(source.c_str());
        else
            mprf(MSGCH_ERROR, "Could not preserve Death Replay checkpoint %d.", slot);
    }
    _prune_replay_archive(archive_dir);
}

bool death_replay_pending() { return bool(pending); }

static void _notice(const string &text)
{
#ifdef USE_TILE_LOCAL
    formatted_scroller screen;
    screen.set_title(formatted_string("Death Replay", WHITE));
    screen.add_raw_text(text);
    screen.show();
#else
    mpr(text);
#endif
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
    const size_t separator = path.find_last_of("/\\");
    const string morgue_dir = separator == string::npos
        ? "" : path.substr(0, separator);
    const string archive_dir = catpath(_parent_directory(morgue_dir),
                                       "death-replay");
    const string base = path.substr(0, path.size() - 4);
    const size_t base_separator = base.find_last_of("/\\");
    const string base_name = base.substr(base_separator == string::npos
        ? 0 : base_separator + 1);
    for (int slot = 0; slot < 2; ++slot)
    {
        replay_checkpoint info;
        string checkpoint_path = catpath(archive_dir,
            base_name + ".replay-" + std::to_string(slot) + ".cs");
        bool checkpoint_loaded = read_replay_checkpoint(checkpoint_path, info);
        if (!checkpoint_loaded)
        {
            // Continue to recognize files archived by the previous prototype.
            checkpoint_path = base + ".replay-" + std::to_string(slot) + ".cs";
            checkpoint_loaded = read_replay_checkpoint(checkpoint_path, info);
        }
        if (checkpoint_loaded
            && info.name == data->name && info.seed == data->seed && info.turn <= data->turns)
        {
            checkpoints.push_back(info);
        }
    }
    sort(checkpoints.begin(), checkpoints.end(), [](const replay_checkpoint &a, const replay_checkpoint &b) { return a.turn < b.turn; });
    if (checkpoints.empty())
    {
        _notice("No preserved checkpoints exist for this death.\n\n"
            "This prototype requires Final location in Death Recap and a recorded checkpoint. "
            "New games save a practice checkpoint at their first input and every 50 turns, retaining the last two. "
            "A morgue alone cannot restore a playable game.");
        return false;
    }
#ifdef USE_TILE_WEB
    const int checkpoint_index =
        select_death_replay_checkpoint(checkpoints, data->turns);
    if (checkpoint_index < 0)
    {
        _notice("No preserved checkpoint is at least 20 turns before death.");
        return false;
    }
    selected = checkpoints[checkpoint_index];
    pending = std::move(data);
    return true;
#else
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
#endif
}

void configure_death_replay(newgame_def &game)
{
    ASSERT(pending);
    game = newgame_def();
    for (int suffix = 1; ; ++suffix)
    {
        // Webtiles shares one save directory across accounts. The '+' cannot
        // occur in an ordinary player name, so another account cannot load
        // this practice save by choosing the same character name.
#ifdef USE_TILE_WEB
        game.name = selected.name + "+DeathReplay" + std::to_string(suffix);
#else
        game.name = selected.name + "-DeathReplay" + std::to_string(suffix);
#endif
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
    you.props[DEATH_BRANCH_KEY] = static_cast<int>(pending->level.branch);
    you.props[DEATH_DEPTH_KEY] = pending->level.depth;
    you.props[DEATH_X_KEY] = pending->position.x;
    you.props[DEATH_Y_KEY] = pending->position.y;
    return true;
}

void start_death_replay()
{
    ASSERT(pending);
    recommendation_vote = 0;
    const int rewind = pending->turns - selected.turn;
    const string name = pending->name;
    const string death_level = pending->level.describe();
    const coord_def death_position = pending->position;
    pending.reset();
    save_game_state();
#ifdef USE_TILE
    coord_def marker_position;
    if (death_replay_marker_position(marker_position))
        tiles.update_minimap(marker_position);
#endif
    _notice(make_stringf("Loaded %s at turn %d, %d turns before death.\n"
        "Death location: %s, x=%d, y=%d (0-based map coordinates).\n\n"
        "The death occurred at the location of the red dot on the minimap.\n\n"
        "The complete checkpoint state is restored. Choose different actions and play forward. "
        "This is wizard practice: scores and ghost creation are disabled. "
        "You can only quit a Death Replay with Ctrl-Q.",
        name.c_str(), you.num_turns, rewind, death_level.c_str(),
        death_position.x, death_position.y));
}

#else
void checkpoint_death_replay() {}
void archive_death_replay(const scorefile_entry &) {}
bool prepare_death_replay(const string &) { return false; }
bool death_replay_pending() { return false; }
void configure_death_replay(newgame_def &) {}
bool restore_death_replay(const newgame_def &) { return false; }
void start_death_replay() {}
void queue_death_replay(const string &) {}
bool start_queued_death_replay(const string &) { return true; }
#endif
