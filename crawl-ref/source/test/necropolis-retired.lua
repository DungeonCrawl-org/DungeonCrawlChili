-- Exercise the actual Necropolis layouts and their map epilogues.
local difficulties = {
  {"pre_temple_d", {Michael = true}},
  {"pre_lair_d", {Michael = true, Norbert = true, Jozef = true}},
  {"post_lair_d", {Norbert = true, Jozef = true, Maud = true, Francis = true}},
  {"snake", {Duane = true, Francis = true, Maud = true, Terpsichore = true}},
  {"depths_crypt", {Duane = true, Francis = true, Maud = true,
                    Terpsichore = true, Pan = true, Lamia = true}}
}
local retired = {Michael = true, Duane = true, Francis = true, Norbert = true,
                 Terpsichore = true, Jozef = true, Pan = true, Lamia = true,
                 Maud = true}
local layouts = {}

for _, difficulty in ipairs(difficulties) do
  for iteration = 1, 4 do
    debug.reset_player_data()
    debug.goto_place("Necropolis:1")
    dgn.persist.necropolis_difficulty = difficulty[1]
    debug.generate_level()
    debug.run_map_epilogues()
    for _, vault in ipairs(dgn.maps_used_here()) do
      if vault:orient() == "encompass" then
        local name = dgn.name(vault:map())
        layouts[name] = (layouts[name] or 0) + 1
      end
    end
    local count = 0
    for mon in test.level_monster_iterator() do
      if retired[mon.name] then
        count = count + 1
        test.map_assert(difficulty[2][mon.name],
                        mon.name .. " misplaced in " .. difficulty[1])
        test.map_assert(you.uniques(mon.name), "Unique tracking was not set")
      end
    end
    test.map_assert(count == 1, "Expected one retired unique in " .. difficulty[1]
                               .. ", found " .. count)
    assert(not dgn.place_retired_unique(4), "A second retired unique was placed")
  end
end

for name, count in pairs(layouts) do
  crawl.stderr(name .. ": " .. count .. " successful generations")
end
crawl.stderr("Retired unique placement passed all five entrance tiers.")
