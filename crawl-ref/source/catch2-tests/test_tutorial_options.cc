#include "AppHdr.h"
#include "catch_amalgamated.hpp"

#include <fstream>
#include <unistd.h>

#include "clua.h"
#include "initfile.h"
#include "item-name.h"
#include "item-prop.h"
#include "libutil.h"
#include "macro.h"
#include "options.h"
#include "state.h"
#include "unwind.h"

TEST_CASE("Tutorial ignores player rc settings and Lua", "[tutorial][options]")
{
    struct temporary_rc
    {
        char path[64] = "/tmp/crawl-tutorial-options-XXXXXX";
        temporary_rc()
        {
            const int fd = mkstemp(path);
            REQUIRE(fd >= 0);
            close(fd);
            std::ofstream rc(path);
            rc << "default_autopickup = false\n"
                  "bindkey = [.] CMD_MOVE_LEFT\n"
                  "{ tutorial_rc_loaded = true }\n";
        }
        ~temporary_rc() { unlink(path); }
    } rc;

    unwind_var<string> rc_path(SysEnv.crawl_rc, rc.path);
    unwind_var<string> data_path(SysEnv.crawl_dir, ".");
    unwind_var<game_type> tutorial_type(crawl_state.type, GAME_TYPE_NORMAL);
    init_properties();
    init_item_name_cache();
    clua.init_libraries();
    reset_keybindings();

    // Normal startup still reads the player's preferences and Lua.
    read_init_file(true);
    CHECK_FALSE(Options.autopickup_on);
    CHECK(key_to_command('.', KMC_DEFAULT) == CMD_MOVE_LEFT);
    REQUIRE(clua.execstring("assert(tutorial_rc_loaded); tutorial_rc_loaded = nil") == 0);

    // Selecting Tutorial after that startup must discard those preferences.
    crawl_state.type = GAME_TYPE_TUTORIAL;
    read_init_file();
    CHECK(Options.autopickup_on);
    CHECK(key_to_command('.', KMC_DEFAULT) == CMD_WAIT);
    read_init_file(true);
    CHECK(Options.autopickup_on);
    REQUIRE(clua.execstring("assert(tutorial_rc_loaded == nil)") == 0);

    // Server-provided options still apply without reading the player rc.
    SysEnv.extra_opts_last.push_back("default_autopickup = false");
    read_init_file();
    CHECK_FALSE(Options.autopickup_on);
    SysEnv.extra_opts_last.pop_back();

    // Returning to ordinary play restores the player's options.
    crawl_state.type = GAME_TYPE_NORMAL;
    read_init_file(true);
    CHECK_FALSE(Options.autopickup_on);
    CHECK(key_to_command('.', KMC_DEFAULT) == CMD_MOVE_LEFT);
    reset_keybindings();
    Options.reset_options();
}
