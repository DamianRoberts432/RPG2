# RPG2

Multi-file version of [RPG](https://github.com/DamianRoberts432/RPG): the same code as RPG's `main.c`, split into smaller modules. Gameplay and visuals are unchanged.

## Build (run inside this folder)

    C:\Users\D\Desktop\mingw64\bin\gcc.exe main.c globals.c character.c campfire.c inventory.c vendor.c interact.c world.c combat.c render_world.c render_entities.c ui.c physics.c input.c -o game.exe -lgdi32 -lxinput -lmsimg32

Or the shorter version: `gcc.exe *.c -o game.exe -lgdi32 -lxinput -lmsimg32`

If you get "XINPUT1_3.dll was not found", use `-lxinput9_1_0` instead of `-lxinput`.

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
