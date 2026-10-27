#include "AppHdr.h"
#include "catch_amalgamated.hpp"
#include <cstdlib>

#include "death-replay.h"
#include "player.h"
#include "store.h"
#include "test_player_fixture.h"
#include "unwind.h"

#if defined(__linux__) && defined(USE_TILE)
#include <cerrno>
#include <fstream>
#include <sys/inotify.h>
#include <unistd.h>

#include "files.h"
#include "hiscores.h"
#include "options.h"
#include "package.h"
#include "state.h"
#include "syscalls.h"
#endif

#ifdef USE_TILE_WEB
static void set_replay_server_flag(const char *value)
{
#ifdef TARGET_OS_WINDOWS
    _putenv_s("DCCHILI_DEATH_REPLAY", value ? value : "");
#else
    if (value)
        setenv("DCCHILI_DEATH_REPLAY", value, 1);
    else
        unsetenv("DCCHILI_DEATH_REPLAY");
#endif
}

class replay_server_scope
{
public:
    explicit replay_server_scope(const char *value)
        : was_set(std::getenv("DCCHILI_DEATH_REPLAY") != nullptr),
          previous(was_set ? std::getenv("DCCHILI_DEATH_REPLAY") : "")
    {
        set_replay_server_flag(value);
    }

    ~replay_server_scope()
    {
        set_replay_server_flag(was_set ? previous.c_str() : nullptr);
    }

private:
    bool was_set;
    string previous;
};

TEST_CASE("WebTiles Death Replay requires a server opt-in", "[death-replay]")
{
    replay_server_scope server(nullptr);
    CHECK_FALSE(death_replay_enabled());
    for (const char *value : {"", "0", "false", "yes", "true", "11"})
    {
        set_replay_server_flag(value);
        CHECK_FALSE(death_replay_enabled());
    }
    set_replay_server_flag("1");
    CHECK(death_replay_enabled());
}
#endif

static string replay_morgue()
{
    return "Dungeon Crawl Chili (tiles) character file.\n\n"
        "Game seed: 18446744073709551615\n\n"
        "399 KoSt the Ducker (level 9, -5/50 HPs)\n"
        "Began as a Human Fighter on Oct 12, 2026.\n"
        "Slain by an ogre (26 damage)\n"
        "KoSt the Ducker (HuFi) Turns: 4193, Time: 00:15:28\n"
        "Health: -5/50 AC: 6 Str: 16 XL: 9\n"
        "Magic: 6/6 Int: 12 God: Okawaru [***...]\n"
        "Gold: 241 Dex: 18\n\n"
        "Inventory:\n\nHand Weapons\n a - a +3 dagger of venom (weapon)\n"
        "Potions\n b - 2 potions of heal wounds\n\n"
        "   Skills:\n + Level 7.5 Fighting\n + Level 4.2 Dodging\n\n"
        " Your Spells              Type           Power      Damage    Failure   Level\n"
        "a - Blink                 Tloc           6%         N/A       45%         2\n\n"
        "Your spell library contained the following spells:\n\n"
        " Spells                   Type           Power      Damage    Failure   Level\n"
        " Slow                     Hex            48%        N/A       13%         1\n\n"
        "Death recap (last three turns)\n"
        "  Final location: D:8, x=23, y=17 (0-based map coordinates), turn 4193\n"
        "  HP by turn: 38 -> 46 -> 21 -> 0\n\n"
        "Message History\n\nThe ogre hits you!\n\n"
        "#####\n#.@O#\n#####\n\nYou could see an ogre.\n\n"
        "Notes\nTurn | Place | Note\n4193 | D:8 | Slain by an ogre\n";
}

TEST_CASE("Death Replay parses seed, character and final position", "[death-replay]")
{
    death_replay_data data;
    string error;
    REQUIRE(parse_death_replay(replay_morgue(), data, error));
    CHECK(data.seed == UINT64_MAX);
    CHECK(data.name == "KoSt");
    CHECK(data.level == level_id(BRANCH_DUNGEON, 8));
    CHECK(data.position == coord_def(23, 17));
    CHECK(data.turns == 4193);
}

TEST_CASE("Death Replay parses branch-only final locations", "[death-replay]")
{
    string text = replay_morgue();
    const size_t location = text.find("D:8, x=23, y=17");
    REQUIRE(location != string::npos);
    text.replace(location, string("D:8").size(), "Gulch");

    death_replay_data data;
    string error;
    REQUIRE(parse_death_replay(text, data, error));
    CHECK(data.level == level_id(BRANCH_GULCH, 1));
    CHECK(data.position == coord_def(23, 17));
}

TEST_CASE("Death Replay rejects malformed replay metadata", "[death-replay]")
{
    death_replay_data data;
    string error;
    CHECK_FALSE(parse_death_replay("not a morgue", data, error));
    CHECK_FALSE(error.empty());
    const string original = replay_morgue();
    for (const auto &replacement : vector<pair<string, string>>{
        {"18446744073709551615", "18446744073709551616"},
        {"x=23", "x=99999"}, {"D:8,", "Unknown:8,"}})
    {
        string text = original;
        text.replace(text.find(replacement.first), replacement.first.size(), replacement.second);
        CHECK_FALSE(parse_death_replay(text, data, error));
    }
    CHECK_FALSE(parse_death_replay(string(2 * 1024 * 1024 + 1, 'x'), data, error));
}

TEST_CASE("Death Replay checkpoints at 50-turn boundaries without duplicate writes", "[death-replay]")
{
    CHECK(death_replay_checkpoint_due(0, -1));
    CHECK_FALSE(death_replay_checkpoint_due(0, 0));
    // Resuming an older game establishes a checkpoint immediately.
    CHECK(death_replay_checkpoint_due(17, -1));
    CHECK(death_replay_checkpoint_due(50, 0));
    CHECK_FALSE(death_replay_checkpoint_due(50, 50));
    CHECK_FALSE(death_replay_checkpoint_due(99, 50));
    CHECK(death_replay_checkpoint_due(100, 50));
    CHECK_FALSE(death_replay_checkpoint_due(100, 100));
    CHECK_FALSE(death_replay_checkpoint_due(125, 100));
    CHECK(death_replay_checkpoint_due(150, 100));
    CHECK(death_replay_checkpoint_due(200, 150));
    CHECK_FALSE(death_replay_checkpoint_due(200, 200));
    CHECK_FALSE(death_replay_checkpoint_due(-1, -1));
}

TEST_CASE("Webtiles Death Replay selects the checkpoint closest to 40 turns before death",
          "[death-replay]")
{
    vector<replay_checkpoint> checkpoints(2);
    checkpoints[0].turn = 100;
    checkpoints[1].turn = 150;

    CHECK(select_death_replay_checkpoint(checkpoints, 190) == 1);
    // The checkpoint 20 turns before death is eligible; the one 15 turns
    // before death is not, even though it is closer to the death.
    CHECK(select_death_replay_checkpoint(checkpoints, 170) == 1);
    CHECK(select_death_replay_checkpoint(checkpoints, 165) == 0);
    CHECK(select_death_replay_checkpoint(checkpoints, 160) == 0);

    checkpoints[0].turn = 0;
    CHECK(select_death_replay_checkpoint(checkpoints, 15) == -1);
}

TEST_CASE("Death Replay requires coordinates inside the Death Recap section", "[death-replay]")
{
    string text = replay_morgue();
    const size_t start = text.find("  Final location:");
    text.erase(start, text.find('\n', start) + 1 - start);
    death_replay_data data;
    string error;
    CHECK_FALSE(parse_death_replay(text, data, error));
    CHECK(error.find("Final location entry") != string::npos);
    text += "Final location: D:8, x=23, y=17\n";
    CHECK_FALSE(parse_death_replay(text, data, error));
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture, "Death Replay marker survives saved-player properties", "[death-replay]")
{
#ifdef USE_TILE_WEB
    replay_server_scope server("1");
#endif
    you.props.erase("death_replay_source");
    CHECK_FALSE(is_death_replay());
    you.props["death_replay_source"] = "Original character";
    CHECK(is_death_replay() == death_replay_enabled());
    CHECK(you.props["death_replay_source"].get_string() == "Original character");
}

#if defined(__linux__) && defined(USE_TILE)
TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Death Replay archiving does not scan the shared morgue tree",
                 "[death-replay]")
{
#ifdef USE_TILE_WEB
    const bool enabled = GENERATE(false, true);
    replay_server_scope server(enabled ? "1" : nullptr);
#else
    const bool enabled = true;
#endif
    char temporary[] = "/tmp/chili-replay-archive-XXXXXX";
    REQUIRE(mkdtemp(temporary) != nullptr);
    const string root = temporary;
    const string morgues = catpath(root, "morgues");
    const string owner = catpath(morgues, you.your_name);
    const string other = catpath(morgues, "OtherPlayer");
    const string archive = catpath(morgues, "death-replay");
    const string saves = catpath(root, "saves");
    const string legacy = "morgue-OtherPlayer-20261027-191722.replay-0.cs";
    ON_UNWIND
    {
        unlink(catpath(root, "normal.cs").c_str());
        unlink(catpath(other, legacy).c_str());
        unlink(catpath(archive, legacy).c_str());
        rmdir(owner.c_str());
        rmdir(other.c_str());
        rmdir(archive.c_str());
        rmdir(morgues.c_str());
        rmdir(saves.c_str());
        rmdir(root.c_str());
    };
    for (const string &path : {morgues, owner, other, saves})
        REQUIRE(mkdir_u(path.c_str(), 0700) == 0);
    {
        std::ofstream checkpoint(catpath(other, legacy));
        checkpoint << "legacy checkpoint";
        REQUIRE(checkpoint.good());
    }

    const int notify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    REQUIRE(notify >= 0);
    ON_UNWIND { close(notify); };
    REQUIRE(inotify_add_watch(notify, morgues.c_str(), IN_ACCESS) >= 0);

    unwind_var<string> morgue_dir(Options.morgue_dir, owner);
    unwind_var<string> save_dir(Options.save_dir, saves);
    unwind_bool no_save(Options.no_save, false);
    unwind_bool opt_out(Options.opt_out_death_replay, false);
    you.props.erase("death_replay_source");
    scorefile_entry score;
    if (!enabled)
    {
        // Supply an active game and a real save so unrelated early returns
        // cannot mask a missing server-policy check in checkpoint creation.
        package normal(catpath(root, "normal.cs").c_str(), true, true);
        {
            chunk_writer chunk(&normal, "test");
            chunk.write("normal save", 11);
        }
        normal.commit();
        unwind_var<package *> save(you.save, &normal);
        unwind_bool started(crawl_state.game_started, true);
        unwind_bool generating(crawl_state.generating_level, false);
        unwind_bool current_level(you.on_current_level, true);
        unwind_var<int> hp(you.hp, 10);
        checkpoint_death_replay();
        CHECK_FALSE(you.props.exists("death_replay_last_checkpoint"));
        queue_death_replay(catpath(owner, "unavailable.txt"));
        CHECK(start_queued_death_replay(you.your_name));
        CHECK_FALSE(prepare_death_replay(catpath(owner, "unavailable.txt")));
        CHECK_FALSE(death_replay_pending());
    }
    archive_death_replay(score);
    CHECK(dir_exists(archive) == enabled);
    CHECK(get_dir_files(saves).empty());

    // Legacy files remain available in place; no other player's archive is
    // migrated as a side effect of this character dying.
    CHECK(file_exists(catpath(other, legacy)));
    CHECK_FALSE(file_exists(catpath(archive, legacy)));

    alignas(inotify_event) char events[4096];
    ssize_t count;
    while ((count = read(notify, events, sizeof events)) > 0)
        for (size_t offset = 0; offset < static_cast<size_t>(count);)
        {
            const auto *event = reinterpret_cast<const inotify_event *>(events + offset);
            INFO("replay enabled=" << enabled << ", mask=" << event->mask
                 << ", name=" << (event->len ? event->name : "<watched directory>"));
            CHECK(enabled);
            // Reading the shared archive or this player's directory is fine.
            // Reading the morgue root itself enumerates the server's players.
            CHECK(event->len > 0);
            if (event->len > 0)
                CHECK(string(event->name) != "OtherPlayer");
            offset += sizeof(inotify_event) + event->len;
        }
    const int read_error = errno;
    CHECK(count == -1);
    CHECK(read_error == EAGAIN);
}
#endif
