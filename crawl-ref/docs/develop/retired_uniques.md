# Retired uniques in the Necropolis

Nine retired DCSS uniques have been restored as Necropolis-only encounters.
The map epilogue selects one unused unique for the entrance difficulty and
places it awake in a corridor connected to the exit without opening doors.
Normal unique tracking prevents another appearance in the same game.
They retain their original living or undead nature and are not player ghosts.

| Entrance tier | Eligible uniques |
| --- | --- |
| Dungeon before D:8 | Michael |
| Dungeon D:8-11, Lair, Orc | Michael, Norbert, Jozef |
| Dungeon D:12+ | Norbert, Jozef, Maud, Francis |
| Lair rune branches | Duane, Francis, Maud, Terpsichore |
| Vaults, Elf, Depths, Crypt | Duane, Francis, Maud, Terpsichore, Pan, Lamia |

Pan arrives with two ordinary faun archers. Terpsichore uses the modern dancing
weapon machinery to animate up to two visible floor weapons; real equipment
is returned when the animation ends. If there are no weapons and she has no
active dancers, she can conjure one temporary dancing weapon. Haste replaces
her old Swiftness and Scorch replaces Michael's obsolete fire projectile.
Blink replaces Lamia's obsolete self-teleport spell.
Jozef retains poison, slowing, displacement, healing and a sling. Lamia retains
her fast actions, constriction and powerful conjurations. HP, HD and melee
profiles follow the historical definitions; XP and Will use modern values.

Historical reference sources:

- [Michael, DCSS 0.5.2](https://github.com/crawl/crawl/blob/0.5.2/crawl-ref/source/mon-data.h)
- [Duane, Francis and Norbert, DCSS 0.7.2](https://github.com/crawl/crawl/blob/0.7.2/crawl-ref/source/mon-data.h)
- [Terpsichore and Jozef, DCSS 0.10.3](https://github.com/crawl/crawl/blob/0.10.3/crawl-ref/source/mon-data.h)
- [Lamia and Maud, DCSS 0.13.2](https://github.com/crawl/crawl/blob/0.13.2/crawl-ref/source/mon-data.h)
- [Pan's introduction](https://github.com/crawl/crawl/commit/72e30d1aab31c81ac58a83c152515a598cc15eed)

Michael, Duane, Francis, Norbert, Terpsichore and Jozef use their historical
DCSS sprites. Maud and Lamia use the retained sprites in Chili; Pan uses the
satyr sprite, matching his original species. Existing monster enum values are
preserved: the six missing IDs are appended and the other three are reused.
