-- Sanity checks that should be run just before the game starts.

local function assert_place_has_map(place)
  assert(dgn.map_by_place(place), "No map found for " .. place)
end

local function sanity_checks()
  -- Tutorials restrict place-based selection to the selected lesson. Regular
  -- branch-ending maps are therefore unavailable, even though they are loaded.
  -- Check the tutorial maps by name before the player chooses a lesson instead.
  if crawl.hints_type() == "tutorial" then
    local tutorials = {
      "tutorial_lesson1", "tutorial_lesson1_level2",
      "tutorial_lesson2", "tutorial_lesson2_level2",
      "tutorial_lesson3", "tutorial_lesson3_level2", "tutorial_lesson3_level3",
      "tutorial_lesson4", "tutorial_lesson4_level2",
      "tutorial_lesson5", "tutorial_lesson6", "tutorial_lesson6_level2"
    }
    for _, name in ipairs(tutorials) do
      assert(dgn.map_by_name(name), "No tutorial map found for " .. name)
    end
    return
  end

  local places = {
    "Zot:$", "Snake:$", "Swamp:$", "Spider:$", "Slime:$", "Elf:$",
    "Vaults:$", "Tomb:$", "Coc:$", "Tar:$", "Dis:$", "Geh:$"
  }
  for _, place in ipairs(places) do
    assert_place_has_map(place)
  end
end

sanity_checks()
