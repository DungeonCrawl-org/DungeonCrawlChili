-- Check lesson 3 through the full builder, including tutorial map selection.
-- Run with: ./crawl -tutorial -script check-tutorial-lesson3
-- The -tutorial flag also exercises startup from the online Tutorial entry.

local function place_lesson(name, depth, lesson)
  debug.goto_place("D:" .. depth)
  dgn.reset_level()
  debug.generate_level(true, lesson or "tutorial_lesson3")
  local maps = dgn.maps_used_here()
  assert(#maps == 1, "Tutorial must use only its lesson map")
  assert(dgn.name(maps[1]:map()) == name,
         "D:" .. depth .. " selected a regular map instead of " .. name)
end

local function find_item(pattern)
  for y = 1, dgn.GYM - 2 do
    for x = 1, dgn.GXM - 2 do
      for _, item in ipairs(dgn.items_at(x, y)) do
        if item.name():find(pattern, 1, true) then
          return item
        end
      end
    end
  end
  error("Missing lesson item: " .. pattern)
end

for attempt = 1, 5 do
  debug.reset_player_data()
  place_lesson("tutorial_lesson3", 1)
  local flail = find_item("flail")
  assert(flail.is_identified, "D:1 weapon must be identified")
  assert(flail.plus == 2, "D:1 weapon must demonstrate +2 enchantment")
  assert(not flail.branded, "D:1 exercise must not depend on a weapon brand")
  assert(find_item("scroll of fog").is_identified)
  assert(find_item("scroll of fear").is_identified)

  place_lesson("tutorial_lesson3_level2", 2)
  flail = find_item("flail")
  assert(flail.is_identified, "D:2 brand must be known before combat")
  assert(flail.ego_type == "venom", "D:2 must supply the poison exercise")
  assert(find_item("potion of curing").is_identified)
  assert(find_item("cloak of Starlight").is_identified)
  assert(find_item("wand of flame").is_identified)
  assert(find_item("wand of digging").is_identified)
  assert(find_item("amulet of faith").is_identified)
  place_lesson("tutorial_lesson3_level3", 3)
  assert(test.find_feature("enter_shop"), "Shopping lesson needs a shop")
end

-- The shared selector also handles the other lessons' follow-up floors.
for _, number in ipairs({1, 2, 4}) do
  debug.reset_player_data()
  local lesson = "tutorial_lesson" .. number
  place_lesson(lesson, 1, lesson)
  place_lesson(lesson .. "_level2", 2, lesson)
end

-- The scoped tutorial mode must not affect ordinary level generation.
debug.reset_player_data()
debug.goto_place("D:3")
dgn.reset_level()
debug.generate_level(true)
for _, vault in ipairs(dgn.maps_used_here()) do
  assert(not dgn.name(vault:map()):find("tutorial", 1, true),
         "Regular Dungeon generation selected a tutorial map")
end
crawl.stderr("Lesson 3 full generation checks passed (five runs of D:1-3).")
