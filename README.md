# RPG2

Multi-file version of [RPG](https://github.com/DamianRoberts432/RPG): the same code as RPG's `main.c`, split into smaller modules. Gameplay and visuals are unchanged.

Windows only (Windows GDI + XInput). Type each command below on its own line in **Command Prompt** and press Enter after each one.

## 1. Install the compiler (one time)

Install gcc (WinLibs MinGW-w64) with winget:

```
winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
```

If it says "Found an existing package already installed", gcc is already installed - that's fine.

Find where gcc was installed:

```
where /r "%LOCALAPPDATA%\Microsoft\WinGet\Packages" gcc.exe
```

On this PC it is:
& "C:\Users\livid\Desktop\mingw64\bin\gcc.exe" *.c -o game.exe -lgdi32 -lxinput -lmsimg32
.\game.exe


## 2. Get the code (one time)

```
cd C:\Users\D\Desktop\RPG2
git clone https://github.com/DamianRoberts432/RPG2.git
```

To get later updates, run `git pull` inside the `RPG2` folder the clone created.

## 3. Build

Go into the cloned folder:

```
cd C:\Users\D\Desktop\RPG2\RPG2
```

Build using the full gcc path (always works; it is one long line):

```
C:\Users\D\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\gcc.exe *.c -o game.exe -lgdi32 -lxinput -lmsimg32
```

Or, if you did the optional PATH step:

```
gcc *.c -o game.exe -lgdi32 -lxinput -lmsimg32
```

`*.c` compiles every source file. The explicit equivalent is:

```
gcc main.c globals.c character.c campfire.c inventory.c vendor.c interact.c world.c combat.c render_world.c render_entities.c ui.c physics.c input.c worldmap.c weather.c -o game.exe -lgdi32 -lxinput -lmsimg32
```

No output means the build succeeded.

## 4. Run

```
game.exe
```

## Troubleshooting

| Problem | Fix |
|---|---|
| `'gcc' is not recognized...` | Use the full-path build line from step 3, or do the optional PATH step and open a **new** Command Prompt window. |
| `Access is denied` or "This app can't run on your PC" when running gcc | That gcc install is damaged or blocked by Windows Security. Install gcc with winget (step 1) and use that one. |
| `'game.exe' is not recognized...` | The build failed or you are in the wrong folder. Make sure you are in the folder with the `.c` files and the build printed no errors. |
| "The filename, directory name, or volume label syntax is incorrect" | Several commands were pasted onto one line. Run each command on its own line. |
| "XINPUT1_3.dll was not found" when starting the game | Rebuild with `-lxinput9_1_0` instead of `-lxinput`. |
| "Windows protected your PC" (SmartScreen) | Click **More info** -> **Run anyway** (you built the exe yourself). |

## Controls

| Hardware Input | Character Creation Menu | Live Game Simulation Path |
| :--- | :--- | :--- |
| **Arrow Keys / DPAD** | Cycle Fields / Class / Race | Move Hero Vector Orientations |
| **A-Z Keys (while Name field selected)** | Type the Hero's Name (Backspace supported) | — |
| **Enter Key / (A) Button** | Finalize Character & Enter the World | — |
| **Space Bar / (A) Button** | — (types a space into the Name field if selected) | Swing Equipped Weapon (Axe by default) / Melee Attack |
| **'X' / 'C' Key / (X) Button** | — | Contextual Interact: Pick up loot, gold or a dropped item (nothing is ever auto-grabbed) / Chop Tree / Mine Rock / Talk / Fish. Next to a campfire it **adds 1 Wood** (a fire holds up to 10; each log burns 4x longer than before). Next to the bedroll it sleeps to the next phase: by day you wake at 9 PM, by night at 6 AM. Never lights a new campfire. |
| **'F' / 'R' Key / RT (Right Trigger)** | — | Bow Charge (Tap for Quick Shot / Hold to Charge; holding the draw drains stamina every frame). With a **Spear** equipped and no Bow, it **throws the spear**: it lands on the ground, stand on it and press X to pick it up. The Bow is a **secondary** weapon: equip it alongside your sword/axe. Arrows fly the way you face (they only hit a monster you are facing), stop at trees and rocks, and vanish at the screen edge. |
| **'Y' Button / Key** | — | Class Special Ability (gameplay-only, debounced to fire once per press, then on cooldown): Necromancer summons (Zombie from level 1 - poison bite, Skeleton from level 3 - tough, Ghost from level 5 - fast and can't be hit; each press raises the next one), Wizard ranged spell bolt, Knight/Warrior defensive backstep + counter-strike, Rogue sneak (breaks monster detection). |
| **'M' Key / Start Button** | — | Open Inventory & Route Display |
| **'1' Key / (A) in Inventory Tab** | — | Equip/Unequip the selected inventory item. One melee weapon and one Bow can be equipped at the same time. |
| **'3' Key / (X) in Inventory Tab** | — | Light a Campfire — the ONLY way to place one. Uses 2 Wood + 1 Match and needs a free campfire slot; nothing is consumed unless all are available. |
| **'4' Key / (Y) in Inventory Tab** | — | Drop the selected inventory item on the ground in front of the hero. Stand on it and press X to pick it up again (no auto-pickup). Ordinary drops vanish after 5 minutes. |
| **'5' Key / Left Stick click in Inventory Tab** | — | Sort the inventory by type (weapons, armor, shields, tools, food, materials), equipped gear first. |
| **'6' Key in Inventory Tab** | — | Build a **storage chest** from 4 Wood in front of you (up to 8 in the world). Press X next to it to store or take items. A chest out in the open (fewer than 3 trees around it) may be found broken into, with half its items gone. Chests are saved with your hero. |
| **X facing water (Fishing Pole in pack)** | — | Cast. A bobber floats out; when it dips the controller rumbles gently - press X then to catch a fish (60%, sometimes 70%, plus 3% per LCK point, up to 90%). Walk away to reel in. |
| **Mouse left click** | — | Walk to that spot, or walk up to a clicked monster and attack it. Clicking a belt slot uses it. |
| **Mouse right click** | — | Class ability (same as Y). |
| **Mouse wheel / side buttons** | — | Wheel picks a belt slot; back button taps the bow, forward button uses the picked belt item. |
| **'1'-'0' Keys (no menu open)** | — | Use belt slots 1-10: the first ten weapons, food and tools in your pack, shown at the bottom of the screen. |
| **'9' Key / Back in Options Tab** | — | Delete the save file (press twice within 3 seconds). Saving stays off until you restart. |
| **LB / 'Q' Key (speaking to a merchant)** | — | Switch the shop UI to the **Buy** tab (merchant's items for sale) |
| **RB / 'E' Key (speaking to a merchant)** | — | Switch the shop UI to the **Sell** tab (your inventory items available to sell) |
| **Up/Down Arrows / '[' / ']' Keys / DPAD Up-Down (shop open)** | — | Cycle the highlighted row within the active Buy/Sell tab |
| **Space Bar / (A) Button (shop open)** | — | Confirm the buy or sell on the highlighted row |
| **SPACE / A (shop open, Buy tab)** | — | Opens a Browse / Buy / Cancel menu; pick with up/down. There is no stealing. |
| **Escape / (B) Button (shop open)** | — | Close the shop menu |
| **X (next to a blacksmith, card shop, pet shop, king, banker, beggar or altar)** | X | Step inside: a side-view room with a menu. Up/Down picks a row, SPACE / A selects, ESC / B leaves. |

## Town buildings

Talking to a shopkeeper takes you inside their building (merchant included).

- **Blacksmith** (every town): repair any worn item, over-forge your weapon (+1 damage per forge up to +10, each forge costs 10% of its max durability), buy rare weapons (Katana, War Hammer, Great Axe) and recycle spare gear into Iron, Wood, Leather, String and Rope.
- **Card shop** (some days): ten random cards, plus 3-card (20,000), 7-card (40,000) and 60-card (350,000) packs. Selecting a card shows its set, rarity, power/toughness and what its ability does.
  - Only three sets are in print for now: **Wild Creatures** (the bestiary monsters plus bluebird, squirrel, owl, bat), **Angels** and **Goetia Demons** - 37 cards.
  - Every card is a 1/1, 2/2 or 3/3. Abilities make the difference: **First Strike** hits before a card without it; **Mighty** (Yeti, Bigfoot) is +3/+3, so they fight as 6/6; a Demon 1/1's **death curse** gives the next enemy card -2/-2; an Angel 1/1's **death blessing** gives your next card +1/+1, +2/+0 or +0/+2.
  - The kid at the corner table plays up to 10 practice hands per visit (no ante). You get three cards from the counter and pick the order to lay them; he leads with his left to right. The practice table on the right shows your three cards and his with their text, then the play-by-play: both cards deal damage at the same time unless one has First Strike, and death marks land on the next card in.
- **Pet shop** (some days): familiars from level 3. Day familiars sleep in your pack from 9 PM to 6 AM, night ones by day.
- **Castle towns** also have the **King** (Legendary quests from level 25: slay a mythic beast or Dracula, then report back for 10,000 gold), a **Bank** vault, a **beggar** and an **altar**. Villages have an altar and sometimes a beggar.
- **Beggars**: giving gold or food raises Soul and lowers Town Heat; refusing does the opposite.
- **Altars** belong to one of four gods: Solara (Light, Incense heals you), Nyx (Dark, a Chaos Relic gives +50% damage for 5 minutes), Baron Samedi (Voodoo, Tarot Card + Rum Bottle + Cigar gives Ghost Whisperer, +25% damage) or the Drowned One (Eldritch, offer a Fish). The merchant sells the offerings. Pray once a day for favor.

## World map and saving

The **Hero** tab also shows your **Karma** (Soul, Miasma from dark magic, Town Heat from crimes, Grapevine fame, and a life mark for each boss type slain), your **card collection** (each kill, cave treasure and fish catch has a 2% chance to give a card; monsters usually drop their own card), and the **bestiary** kill count per monster. These are saved with your hero. Food, materials and matches stack up to 99 per slot. The merchant now sells a **Spear** and an **A&D Strategy Guide** (use it from the inventory for a 2-second peek at secret card combos). A **Yeti** stays calm until you get within 3 tiles or hit it - then it charges. Races give small stat bonuses, and natural pairings (Elf Wizard, Human Knight, Dwarf Warrior, Halfling Rogue, Gnome Necromancer) get +1 to their key stat.

The **World Map** tab (M / Start, then tab 2) shows every screen you have explored: biome colours, brown squares for village homes, grey squares for castles, blue for rivers, lakes and sea, and a gold square for cave treasure you have not claimed yet. Each town you have found gets a number, and the **TOWNS** list beside the map gives its name, its patron god (Light, Dark, Voodoo or Eldritch) and whether its card shop and pet shop are open today. The game auto-saves the map to `rpg2_save.dat` (next to `game.exe`) every time you change screens, and loads it on start. Delete that file to start with a blank map.

## Seasons and weather

One real second is one game minute, so a day lasts 24 real minutes and each 30-day season about 12 hours. The game starts in the season and time of day from your PC's clock (October = fall). Weather is the same everywhere in the world and changes gradually between fair, rain and snow; snow builds up on the ground during winter, leaves blow in fall, and occasional wind gusts (most often in fall) add to the normal breeze.

**Optional real weather:** copy `weather.ini.example` to `weather.ini` next to `game.exe` and put your city (or latitude/longitude) in it. On start the game asks Open-Meteo (free, no API key) for your current weather, so if it is snowing outside the game starts snowing too. If there is no `weather.ini` or no internet, it just uses the clock.

**Weather patterns:** fair spells last up to a week (sometimes partly cloudy), a windy half-day comes before a storm, then overcast, 1-3 days of rain or snow, and overcast again. Rain slants with the wind. Patches of fog drift in at dawn and dusk after rain in fields, forests and swamps (rarely deserts). Deserts are yellow sand with dunes, big saguaro cacti and blowing dust. New screens favour tundra near the north and south of the world and desert near the middle.

## World details

- **Villages** have a campfire and torches. Monsters seldom spawn near them and usually turn away from the light; villagers and guards gang up on any that get through.
- **Loot preview:** stand near a drop to see what it holds. Grey = common, blue = forged, green = uncommon, yellow = rare, purple = legendary.
- **Caves** only have bats. Outside, owls and bats only come out at night.
- **Musashi's Blade** never breaks and is never dropped by bosses: it only washes up on a beach in the easternmost column of the map on summer day 13.
- **Zombies and skeletons** are sized by race height (yours, for summons), hit a little harder when bigger, and summons sometimes poison.
- **Sound:** the game runs with no sound files yet; Saturn-style synth voices and samples come later.

## Layout

| File | Contents |
|---|---|
| game.h | Includes, #defines, types/enums, extern declarations for every global, includes all module headers |
| globals.c | Definitions of every global variable and table (weapon/merchant catalogs, dialogue lines) |
| main.c | WindowProc, WinMain (main loop and scene composition) |
| character.c | Name validation, class/race stats, auto-save hook |
| campfire.c | Campfire placement, fuel, lighting |
| inventory.c | Inventory, equipment, weight, ground loot, dropped items, food |
| vendor.c | Merchant restock, buying, selling |
| town.c | Blacksmith, card shop, pet shop, king, bank, beggars, altars, interiors, familiars |
| interact.c | Proximity checks and contextual interact (chop/mine/fish/talk) |
| world.c | Biomes, procedural screens, caves, villages, NPCs, critters |
| combat.c | Melee, enemy damage/respawn, class abilities, blood/debris |
| physics.c | Movement, collision, stamina, day/night, weather (per-frame update) |
| input.c | Keyboard and XInput gamepad |
| render_world.c | Isometric helpers, tiles, trees, fires, weather, loot drawing |
| render_entities.c | Hero, projectiles, enemies, effects, merchant, village drawing |
| ui.c | HUD, inventory/map/options menu, vendor shop overlay |
| worldmap.c | Explored-world map (biomes, villages, castles, water, treasure) and the save file |
| weather.c | Seasons, world-wide weather, wind gusts, snow, fall leaves, optional real-world weather |

Each module has a matching .h with its function prototypes.
