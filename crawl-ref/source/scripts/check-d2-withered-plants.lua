-- Run with: ./crawl -script check-d2-withered-plants
crawl_require("dlua/test.lua")

local withered = 0
for attempt = 1, 5 do
    test.regenerate_level("D:2", true)
    for mon in test.level_monster_iterator() do
        assert(mon.name ~= "plant", "D:2 generated an ordinary plant")
        if mon.name == "withered plant" then
            withered = withered + 1
        end
    end
end
assert(withered > 0, "D:2 must still generate decorative plants")

-- Placement outside level generation must keep ordinary plants, both on
-- D:2 and elsewhere (e.g. player-created plants).
for _, depth in ipairs({1, 2, 3}) do
    test.regenerate_level("D:" .. depth, true)
    local points = dgn.find_points(function(p)
        return dgn.grid(p.x, p.y) == dgn.fnum("floor")
           and not dgn.mons_at(p.x, p.y)
           and (p.x ~= you.x_pos() or p.y ~= you.y_pos())
    end)
    assert(#points > 0, "No free floor for placement control")
    local p = points[1]
    local plant = dgn.create_monster(p.x, p.y, "plant")
    assert(plant and plant.name == "plant", "Ordinary placement must not change")
end
crawl.stderr("D:2 withered plants passed five generations and placement controls.")
