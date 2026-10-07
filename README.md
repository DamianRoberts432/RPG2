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

Use that path in the home PC build line in step 4 if `gcc` alone is not recognized.

## 2. Get the code (one time only)

Do this **once per computer**. Never clone again after that - updates happen in the same folder (step 3).

```
cd %USERPROFILE%\Desktop
```

```
git clone https://github.com/DamianRoberts432/RPG2.git
```

This creates `Desktop\RPG2`. If it says "already exists", you already have it - skip to step 3.

## 3. Update to the latest main (every time)

Go into your RPG2 folder. Use the line for the computer you're on:

Home PC:

```
cd C:\Users\D\Desktop\RPG2\RPG2
```

Work laptop:

```
cd C:\Users\livid\Desktop\RPG2_new
```

Then get the latest code from GitHub:

```
git checkout main
```

```
git pull
```

If `git checkout main` says local changes would be overwritten, run this, then run the two lines above again:

```
git checkout -- .
```

If `git pull` says "not a git repository", that folder came from a ZIP download and can't update. Do step 2 instead.

## 4. Build (every time after updating)

Home PC (Command Prompt):

```
gcc *.c -o game.exe -lgdi32 -lxinput -lmsimg32
```

If that says `'gcc' is not recognized`, use the full path instead (one long line):

```
C:\Users\D\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\gcc.exe *.c -o game.exe -lgdi32 -lxinput -lmsimg32
```

Work laptop (PowerShell):

```
& "C:\Users\livid\Desktop\mingw64\bin\gcc.exe" *.c -o game.exe -lgdi32 -lxinput -lmsimg32
```

`*.c` compiles every source file. No output means the build succeeded.

## 5. Run

Command Prompt:

```
game.exe
```

PowerShell:

```
.\game.exe
```

## Troubleshooting

| Problem | Fix |
|---|---|
| `'gcc' is not recognized...` | Use the full-path build line from step 4. |
| `Access is denied` or "This app can't run on your PC" when running gcc | That gcc install is damaged or blocked by Windows Security. Install gcc with winget (step 1) and use that one. |
| `'game.exe' is not recognized...` | In PowerShell, type `.\game.exe`. Otherwise the build failed or you are in the wrong folder. Make sure you are in the folder with the `.c` files and the build printed no errors. |
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
| **'X' / 'C' Key / (X) Button** | — | Contextual Interact: Pick up a dropped item / Chop Tree / Mine Rock / Talk / Fish. Next to a campfire it **adds 1 Wood** (a fire holds up to 10). Never lights a new campfire. |
| **'F' / 'R' Key / RT (Right Trigger)** | — | Bow Charge (Tap for Quick Shot / Hold to Charge). The Bow is a **secondary** weapon: equip it alongside your sword/axe. Arrows fly the way you face (they only hit a monster you are facing), stop at trees and rocks, and vanish at the screen edge. |
| **'Y' Button / Key** | — | Class Special Ability (gameplay-only, debounced to fire once per press, then on cooldown): Necromancer/Wizard ranged spell bolt, Knight/Warrior defensive backstep + counter-strike, Rogue sneak (breaks monster detection). |
| **'M' Key / Start Button** | — | Open Inventory & Route Display |
| **'1' Key / (A) in Inventory Tab** | — | Equip/Unequip the selected inventory item. One melee weapon and one Bow can be equipped at the same time. |
| **'3' Key / (X) in Inventory Tab** | — | Light a Campfire — the ONLY way to place one. Uses 2 Wood + 1 Match and needs a free campfire slot; nothing is consumed unless all are available. |
| **'4' Key / (Y) in Inventory Tab** | — | Drop the selected inventory item on the ground in front of the hero. Stand on it and press X to pick it up again (no auto-pickup). Ordinary drops vanish after 5 minutes. |
| **LB / 'Q' Key (speaking to a merchant)** | — | Switch the shop UI to the **Buy** tab (merchant's items for sale) |
| **RB / 'E' Key (speaking to a merchant)** | — | Switch the shop UI to the **Sell** tab (your inventory items available to sell) |
| **Up/Down Arrows / '[' / ']' Keys / DPAD Up-Down (shop open)** | — | Cycle the highlighted row within the active Buy/Sell tab |
| **Space Bar / (A) Button (shop open)** | — | Confirm the buy or sell on the highlighted row |
| **Escape / (B) Button (shop open)** | — | Close the shop menu |

## World map and saving

The **World Map** tab (M / Start, then tab 2) shows every screen you have explored: biome colours, brown squares for village homes, grey squares for castles, blue for rivers, lakes and sea, and a gold square for cave treasure you have not claimed yet. The game auto-saves the map to `rpg2_save.dat` (next to `game.exe`) every time you change screens, and loads it on start. Delete that file to start with a blank map.

## Seasons and weather

Each season lasts 45 real minutes. The game starts in the season and time of day from your PC's clock (October = fall). Weather is the same everywhere in the world and changes gradually between fair, rain and snow; snow builds up on the ground during winter, leaves blow in fall, and occasional wind gusts (most often in fall) add to the normal breeze.

**Optional real weather:** copy `weather.ini.example` to `weather.ini` next to `game.exe` and put your city (or latitude/longitude) in it. On start the game asks Open-Meteo (free, no API key) for your current weather, so if it is snowing outside the game starts snowing too. If there is no `weather.ini` or no internet, it just uses the clock.

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
