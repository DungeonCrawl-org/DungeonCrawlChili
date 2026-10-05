# Basic keys and tutorial coverage

The **Basic keys (tutorial lessons)** help page is available from `?>`, or
by pressing `>` in the full `??` command reference. It is available during
normal play as well as tutorials. Keys on the page use the player's current
bindings, like the full reference.

The page is defined in `_add_formatted_tutorial_keyhelp` in `source/command.cc`.
Its scope is commands already taught in `source/dat/descript/tutorial.txt`.
When changing a lesson, update the page and this coverage list together.
Commands mentioned only in a lesson's summary count as covered; additions
should preferably include practice as well as a summary.

## Commands covered today

| Lesson | Commands and interactions |
| --- | --- |
| Introduction | Replay messages; clear `--more--` with Space. |
| 1: Movement | Eight movement directions; Shift + direction to run; opening doors by movement; closing doors; upstairs/downstairs; level map and Enter to travel; map stair shortcuts; Escape to leave the map; autoexplore. |
| 2: Combat | Attack by moving into an enemy; autofight (`Tab`) practised at the rat room entrance, including movement and manual retreat; pickup; equip; examining monsters and reading descriptions; targeting and cycling targets; firing the held weapon; firing the quiver; firing at the closest enemy (`p` / Shift-Tab); quivering; waiting; resting. |
| 3: Items | Inventory and item descriptions; pickup/drop; equip/unequip; potions; scrolls; wand evocation; searching known items/features; command help. |
| 4: Magic | Memorisation; spell list and failure rates; casting and `?` to list spells; quivering spells and firing them; ally orders; waiting/resting; unequipping. |
| 5: Religion | Dungeon overview; praying at an altar; religion screen and `!` for details; divine abilities; waiting/resting; description lookup (in the summary). |

The basic page groups repeated commands under one lesson. It includes the
main commands, movement diagram and targeting controls, rather than every
alternative binding or confirmation used in the lessons. For example, map
stair shortcuts and the religion screen's detail toggle are covered in the
lessons but omitted from the compact page.

## Candidates for future lessons

These commands are useful for beginners but have no explicit keyboard
instruction in the current five lessons. They are deliberately excluded
from the basic page until a lesson teaches them. The keys below are defaults;
use `$cmd[...]` placeholders when writing lesson text.

| Default key | Command | Possible lesson addition |
| --- | --- | --- |
| `S` / Ctrl-S | Save and exit | End lesson 1 with a save/resume explanation. |
| `%` | Character overview and resistances | Explain resistances before introducing a dangerous elemental attack. |
| `@` | Character status | Teach how to inspect temporary conditions during combat. |
| `m` | Skill training | Add a small training exercise to the combat or magic lesson. |
| `G` | Travel between levels | Extend lesson 1 after teaching the level map. |
| `;` | Inspect items underfoot | Teach inspecting a pile before picking up items in lesson 3. |
| `A` | Mutations | Explain inspecting mutations if a future lesson introduces them. |

Equipment-specific commands (`w`, `W`, `T`, `P`, `R`) need not all be added:
the lessons already teach the general equip/unequip commands (`e`, `c`).
Prioritise actions that introduce a useful capability over extra aliases.
