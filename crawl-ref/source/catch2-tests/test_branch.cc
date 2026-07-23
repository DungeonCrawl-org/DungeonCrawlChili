#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "branch.h"
#include "stairs.h"

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
