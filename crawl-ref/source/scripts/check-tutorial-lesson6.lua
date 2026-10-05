-- Run with: ./crawl -script check-tutorial-lesson6
-- Exercise the actual builder, including D:1 item restrictions and stairs.
crawl_require("dlua/test.lua")

local function generate(depth)
    debug.goto_place("D:" .. depth)
    dgn.reset_level()
    debug.generate_level(true, "tutorial_lesson6")
    local maps = dgn.maps_used_here()
    assert(#maps == 1, "Survival lesson must not include regular vaults")
    local name = depth == 1 and "tutorial_lesson6" or "tutorial_lesson6_level2"
    assert(dgn.name(maps[1]:map()) == name, "Wrong lesson map selected")
end

local function find_item(name, quantity)
    for y = 1, dgn.GYM - 2 do
        for x = 1, dgn.GXM - 2 do
            for _, item in ipairs(dgn.items_at(x, y)) do
                local singular = item.name():gsub("potions", "potion")
                                            :gsub("scrolls", "scroll")
                if singular:find(name, 1, true) then
                    assert(item.is_identified, "Supplies must be identified")
                    if quantity then
                        assert(item.quantity == quantity, "Wrong supply quantity")
                    end
                    return item
                end
            end
        end
    end
    error("Missing survival supply: " .. name)
end

local function reachable(start, goal)
    local passable = dgn.feature_set_fn("floor", "closed_door", "open_door",
        "stone_stairs_up_i", "stone_stairs_up_ii", "stone_stairs_up_iii",
        "stone_stairs_down_i", "stone_stairs_down_ii", "stone_stairs_down_iii",
        "exit_dungeon")
    local queue, seen = {start}, {}
    local index = 1
    seen[start.x .. "," .. start.y] = true
    while index <= #queue do
        local p = queue[index]
        index = index + 1
        if p.x == goal.x and p.y == goal.y then return true end
        for dy = -1, 1 do
            for dx = -1, 1 do
                local x, y = p.x + dx, p.y + dy
                local key = x .. "," .. y
                if not seen[key] and dgn.in_bounds(x, y)
                   and passable(dgn.grid(x, y)) then
                    seen[key] = true
                    queue[#queue + 1] = {x=x, y=y}
                end
            end
        end
    end
    return false
end

local function check_route(depth)
    local up = dgn.find_points(function(p)
        return dgn.feature_set_fn("stone_stairs_up_i", "stone_stairs_up_ii",
            "stone_stairs_up_iii", "exit_dungeon")(dgn.grid(p.x, p.y))
    end)
    local down = dgn.find_points(function(p)
        return test.is_down_stair(dgn.grid(p.x, p.y))
    end)
    assert(#up == (depth == 1 and 1 or 2), "D:" .. depth .. " has " .. #up .. " upstairs and " .. #down .. " downstairs")
    assert(#down == (depth == 1 and 1 or 0), "Wrong downward stairs")
    local start = up[1]
    local goal = depth == 1 and down[1] or up[2]
    assert(not reachable(start, goal), "Digging exercise can be bypassed on foot")
    local vault = dgn.maps_used_here()[1]
    local origin, size = vault:pos(), vault:size()
    if depth == 2 then
        for mons in test.level_monster_iterator() do
            if mons.type_name == "gnoll" then
                local px, py = you.pos()
                local gnoll = {x=px + mons.x, y=py + mons.y}
                assert(not reachable(start, gnoll),
                       "Gnoll can reach the fog corridor before the barrier is dug")
                assert(los.cell_see_cell(origin.x + 22, origin.y + 13,
                                        gnoll.x, gnoll.y) == 0,
                       "Gnoll must be hidden from the fog teaching tile")
            end
        end
    end
    local rocks = dgn.find_points(function(p)
        return p.x >= origin.x and p.x < origin.x + size.x
           and p.y >= origin.y and p.y < origin.y + size.y
           and dgn.grid(p.x, p.y) == dgn.fnum("rock_wall")
    end)
    assert(#rocks == 3, "Each floor needs exactly three diggable barrier tiles")
    for _, p in ipairs(rocks) do dgn.grid(p.x, p.y, "floor") end
    assert(reachable(start, goal), "Escape remains blocked after digging")
end

for attempt = 1, 5 do
    debug.reset_player_data()
    generate(1)
    local ogres = 0
    for mons in test.level_monster_iterator() do
        if mons.type_name == "ogre" then
            ogres = ogres + 1
            assert(mons.get_info():threat() == 3,
                   "Caged ogre must be a red threat at the lesson's starting XL")
        end
    end
    assert(ogres == 1, "Practice floor must contain its caged ogre")
    local staff = find_item("quarterstaff")
    assert(staff.plus == 2 and not staff.branded, "D:1 weapon must be +2 unbranded")
    find_item("potion of haste", 2)
    find_item("potion of curing", 2)
    find_item("potion of heal wounds", 2)
    find_item("scroll of fog", 3)
    find_item("scroll of teleportation", 2)
    find_item("wand of digging")

    local destinations = dgn.find_points(function(p)
        return dgn.grid(p.x, p.y) == dgn.fnum("floor")
           and not dgn.fprop_at(p.x, p.y, "no_tele_into")
    end)
    assert(#destinations > 1, "Teleport drill has no valid destinations")
    for _, p in ipairs(destinations) do
        for _, q in ipairs(destinations) do
            assert(math.abs(p.x - q.x) <= 13 and math.abs(p.y - q.y) <= 4,
                   "Teleport destinations: " .. p.x .. "," .. p.y .. " and " .. q.x .. "," .. q.y)
        end
    end
    check_route(1)
    generate(2)
    local monsters = 0
    for mons in test.level_monster_iterator() do
        monsters = monsters + 1
        assert(mons.experience == 1, "Challenge should not trigger level-up prompts")
    end
    assert(monsters == 4, "Wrong number of challenge enemies")
    check_route(2)
end
crawl.stderr("Lesson 6 supplies, teleport drill and escape routes passed five generations.")
