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

```
C:\Users\D\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\gcc.exe
```

Optional - make plain `gcc` work in every new Command Prompt window (run once, then **close and reopen** Command Prompt):

```
powershell -NoProfile -Command "[Environment]::SetEnvironmentVariable('Path', [Environment]::GetEnvironmentVariable('Path','User') + ';C:\Users\D\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin', 'User')"
```

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
gcc main.c globals.c character.c campfire.c inventory.c vendor.c interact.c world.c combat.c render_world.c render_entities.c ui.c physics.c input.c -o game.exe -lgdi32 -lxinput -lmsimg32
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
| **'X' / 'C' Key / (X) Button** | — | Contextual Harvest Interact ONLY (Chop Tree 🪓 / Mine Rock ⛏️). Never places a campfire. |
| **'F' / 'R' Key / RT (Right Trigger)** | — | Bow Charge (Tap for Quick Shot / Hold to Charge) — only while a Bow is equipped; no Bow is granted by default, so this is inactive until one is obtained and equipped from the inventory menu. |
| **'Y' Button / Key** | — | Class Special Ability (gameplay-only, debounced to fire once per press, then on cooldown): Necromancer/Wizard ranged spell bolt, Knight/Warrior defensive backstep + counter-strike, Rogue sneak (breaks monster detection). |
| **'M' Key / Start Button** | — | Open Inventory & Route Display |
| **'1' Key / (A) in Inventory Tab** | — | Equip/Unequip the selected inventory item (only items you own can become the active weapon) |
| **'3' Key / (X) in Inventory Tab** | — | Light a Campfire — the ONLY way to place one. Requires Wood + a free campfire slot + a Match; nothing is consumed unless all three are available. |
| **'4' Key / (Y) in Inventory Tab** | — | Drop the selected inventory item on the ground in front of the hero. Walk back over it to pick it up again. |
| **LB / 'Q' Key (speaking to a merchant)** | — | Switch the shop UI to the **Buy** tab (merchant's items for sale) |
| **RB / 'E' Key (speaking to a merchant)** | — | Switch the shop UI to the **Sell** tab (your inventory items available to sell) |
| **'[' / ']' Keys / DPAD Up-Down (shop open)** | — | Cycle the highlighted row within the active Buy/Sell tab |
| **Space Bar / (A) Button (shop open)** | — | Confirm the buy or sell on the highlighted row |
| **Escape / (B) Button (shop open)** | — | Close the shop menu |

## Layout

| File | Contents |
|---|---|
| game.h | Includes, #defines, types/enums, extern declarations for every global, includes all module headers |
| globals.c | Definitions of every global variable and table (weapon/merchant catalogs, dialogue lines) |
| main.c | WindowProc, WinMain (main loop and scene composition) |
| character.c | Name validation, class/race stats, save stub |
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

Each module has a matching .h with its function prototypes.
