#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "branch.h"
#include "player.h"
#include "stairs.h"
#include "terrain.h"
#include "unwind.h"

TEST_CASE("Priest chapel exits lead to D:1", "[single-file]")
{
    unwind_var<branch_type> branch(you.where_are_you, BRANCH_START_TEMPLE);
    unwind_var<int> depth(you.depth, 1);

    REQUIRE(feat_stair_direction(DNGN_EXIT_BAZAAR) == CMD_GO_DOWNSTAIRS);
    REQUIRE(stair_destination(DNGN_STONE_STAIRS_DOWN_I, "", false)
            == level_id(BRANCH_DUNGEON, 1));
    REQUIRE(stair_destination(DNGN_EXIT_BAZAAR, "", false)
            == level_id(BRANCH_DUNGEON, 1));
}

TEST_CASE( "is_random_subbranch_test", "[single-file]" ) {

    for (branch_iterator it; it; ++it)
    {
        branch_type branch = it->id;

        CAPTURE(branch);

        bool old_is_random_subbranch = parent_branch(branch) == BRANCH_LAIR
                                        && branch != BRANCH_SLIME;

        REQUIRE(old_is_random_subbranch == is_random_subbranch(branch));
    }
}

TEST_CASE( "branch exits stay within the parent branch", "[single-file]" )
{
    const level_id old_entry = brentry[BRANCH_DEPTHS];
    const int old_dungeon_depth = brdepth[BRANCH_DUNGEON];

    brentry[BRANCH_DEPTHS] = level_id(BRANCH_DUNGEON, 15);
    brdepth[BRANCH_DUNGEON] = 14;

    const level_id destination =
        branch_exit_destination(DNGN_EXIT_DEPTHS,
                                level_id(BRANCH_DEPTHS, 1));

    brentry[BRANCH_DEPTHS] = old_entry;
    brdepth[BRANCH_DUNGEON] = old_dungeon_depth;

    REQUIRE(destination == level_id(BRANCH_DUNGEON, 14));
}

TEST_CASE( "branch entry ranges stay within their parent branch", "[single-file]" )
{
    for (branch_iterator it; it; ++it)
    {
        const Branch &branch = **it;
        if (branch.parent_branch >= NUM_BRANCHES || branch.mindepth < 1)
            continue;

        CAPTURE(branch.id, branch.parent_branch);
        REQUIRE(branch.mindepth <= branch.maxdepth);
        REQUIRE(branch.maxdepth <= branches[branch.parent_branch].numlevels);
    }
}
