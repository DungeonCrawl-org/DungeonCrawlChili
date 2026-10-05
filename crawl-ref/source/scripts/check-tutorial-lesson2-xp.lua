-- Run with: ./crawl -script check-tutorial-lesson2-xp
-- Even killing the comparison wyvern must leave the player below XL 3.
crawl_require("dlua/test.lua")

local total_xp = 0
local maximum_xp = 0
for _, name in ipairs({"tutorial_lesson2", "tutorial_lesson2_level2"}) do
    debug.reset_player_data()
    dgn.reset_level()
    local map = dgn.map_by_name(name)
    assert(map, "Missing map: " .. name)
    assert(dgn.place_map(map, false, true), "Cannot place map: " .. name)
    local count = 0
    for mons in test.level_monster_iterator() do
        local xp = mons.experience
        assert(xp == 1, name .. ": unexpected monster XP: " .. xp)
        count = count + 1
        total_xp = total_xp + xp
        -- gain_exp rounds the 10% bonus randomly for each kill.
        maximum_xp = maximum_xp + xp + math.ceil(xp / 10)
    end
    assert(count == 7, name .. ": expected seven monsters, found " .. count)
end

assert(total_xp >= you.exp_needed(2), "Lesson no longer teaches leveling up")
assert(maximum_xp < you.exp_needed(3), "Lesson can reach XL 3")
crawl.stderr("Lesson 2 XP: " .. total_xp .. " base, " .. maximum_xp
             .. " maximum with bonus; XL 3 requires " .. you.exp_needed(3))
