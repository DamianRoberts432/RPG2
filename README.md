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
| **Shift / LT (Left Trigger), held while moving** | — | Run: 1.6x speed, drains stamina. Plain walking barely tires you, and carry weight only slows you past 70% of your limit. |
| **'J' Key / LB (Left Bumper)** | — | Quick hop: a short, fast hop (costs 5 stamina). Monsters can't hit you mid-hop. |
| **'M' Key / Start Button** | — | Open Inventory & Route Display |
| **'1' Key / (A) in Inventory Tab** | — | Equip/Unequip the selected inventory item. One melee weapon and one Bow can be equipped at the same time. |
| **'3' Key / (X) in Inventory Tab** | — | Light a Campfire — the ONLY way to place one. Uses 2 Wood + 1 Match and needs a free campfire slot; nothing is consumed unless all are available. |
| **'4' Key / (Y) in Inventory Tab** | — | Drop the selected inventory item on the ground in front of the hero. Stand on it and press X to pick it up again (no auto-pickup). Ordinary drops vanish after 5 minutes. |
| **'5' Key / Left Stick click in Inventory Tab** | — | Sort the inventory by type (weapons, armor, shields, tools, food, materials), equipped gear first. |
| **'6' Key in Inventory Tab** | — | Build a **storage chest** from 4 Wood in front of you (up to 8 in the world). Press X next to it to store or take items. A chest out in the open (fewer than 3 trees around it) may be found broken into, with half its items gone. Chests are saved with your hero. |
| **X facing water (Fishing Pole in pack)** | — | Cast. A bobber floats out; when it dips the controller rumbles gently - press X then to catch a fish (60%, sometimes 70%, plus 3% per LCK point, up to 90%). If nothing bites the line stays out (with a notice) until you press X to reel in or walk away. |
| **Mouse left click** | — | Walk to that spot, or walk up to a clicked monster and attack it. Click loot or a villager to walk over and pick it up / talk. In a shop, click a row to pick it and click it again to buy, sell or confirm; click the Buy/Sell tabs to switch; the wheel scrolls a shop menu. On the World Map tab, click a tile to put the white cursor on it. Clicking a belt slot uses it. |
| **Mouse right click** | — | Class ability (same as Y). In a shop it backs out. |
| **'V' Key / Right Stick click** | — | Step the camera through three zoom levels: wide, normal, close. |
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
- **Card shop** (some days): ten random cards, plus a 3-card booster, a 60-card box and the complete set for **each set separately**. Pulls never mix sets, so a Starter booster only holds Starter cards. Packs and card drops favor small cards: 70% 1/1, 25% 2/2, 5% 3/3. Enemies drop cards 1.5% of the time. Selecting a card shows its set, rarity, power/toughness and what its ability does.
  - Sets in print: the **Starter Set** (the bestiary monsters plus bluebird, squirrel, owl and bat, 13 cards; booster 2,000, box 30,000) and **Angels & Goetia** (the 72 angels of the Shem HaMephorash, the 72 Goetia demons and the 22 Major Arcana, 166 cards; booster 20,000, box 350,000). A complete set costs 3 boxes.
  - Every card is a 1/1 (common), 2/2 (uncommon) or 3/3 (rare). Only 2/2s and 3/3s have abilities; the 1/1s are plain pawns. **First Strike** hits before a card without it. **Mighty** (Yeti, Bigfoot) is +3/+3, so they fight as 6/6. A **death curse** (demons) gives the next enemy card -2/-2. A **death blessing** (angels) gives your next card +1/+1, +2/+0 or +0/+2.
  - **Releasing a new set:** nine mythology sets of 50 cards each (Greek, Norse, Egyptian, Celtic, Japanese, Hindu, Aztec, Slavic, Chinese) are already written in `cards_future.h`. To release one, uncomment its row in `card_set_table` in `cards_data.c`. The card shop, the Card Game Strategy Guide's SETS IN PRINT list and the card kid all pick it up automatically, and the kid hints at the next set that isn't out yet.
  - The kid at the corner table plays up to 10 practice hands per visit (no ante). You get three cards from the counter and pick the order to lay them; he leads with his left to right. The practice table on the right shows your three cards and his with their text, then the play-by-play: both cards deal damage at the same time unless one has First Strike, and death marks land on the next card in.
- **Pet shop** (some days): familiars from level 3. Day familiars sleep in your pack from 9 PM to 6 AM, night ones by day.
- **Castle towns** also have the **King** (Legendary quests from level 25: slay a mythic beast or a Vampire, then report back for 10,000 gold), a **Bank** vault, a **beggar** and an **altar**. Villages have an altar and sometimes a beggar.
- **Beggars**: giving gold or food raises Soul and lowers Town Heat; refusing does the opposite.
- **Altars** belong to one of four gods: Solara (Light, Incense heals you), Nyx (Dark, a Chaos Relic gives +50% damage for 5 minutes), Baron Samedi (Voodoo, Tarot Card + Rum Bottle + Cigar gives Ghost Whisperer, +25% damage) or the Drowned One (Eldritch, offer a Fish). The merchant sells the offerings. Pray once a day for favor. A blessing shows on screen: Solara pours a sunbeam on you, Nyx draws a veil of stars, Baron Samedi rises in the corner and grins, the Drowned One raises deep water and bubbles.

## Homes, clothes and world memory

- **Walk-in homes**: press X at a village cottage door to step inside a polygon room. X by the bed sleeps; walk out through the door to leave. Shop, castle and altar rooms use the same polygon walls and floors.
- **Your own house**: buy **House Supplies** from the merchant (or get them as thanks for a Wood or Stone job). Press X on open ground to plan the house, and X again to cycle through 12 fronts (4 roofs x 3 walls). Walk away and it gets built. The merchant sells a Bookshelf, Armchair, Potted Plant, Painting, Wardrobe and Lantern, and they go into the house when you go inside. A tiny picture of it marks its tile on the World Map.
- **Clothes**: any class can wear Head, Cloak and Clothes items from the merchant. A Cloth Hood hides the face.
- **Start screen looks**: Body, Beard, Eyes and Hair (the colour shows age). These can only be changed on the start screen.
- **World memory**: trees you cut and rocks you mine stay gone (trees regrow after 3 game days, rocks after 10, never on tilled soil). A cleared screen fills back up with monsters slowly over 6 game hours. Angry monsters close behind you follow you to the next screen. Snow melted by a campfire fills back in only while it snows.

## Quests

- **330 quests** are built from 110 tales out of world mythology and classic RPG jobs (the Troll Under the Bridge, Rescue the Princess, the Golden Fleece, Baba Yaga's Hut, the Lost Anvil...). Each tale comes in 3 versions that are harder, higher level and rarer: Common, Uncommon, Rare and Legendary.
- Villagers, shopkeepers, guards and (from level 25) the King may offer one when you talk to them. Press X again to accept.
- Quests unfold step by step. A step may send you to someone in another town ("visit my brother in Briarville"), into a cave, to a certain land, out under the night sky, or after monsters, fish, crops or items. Later steps stay hidden as **???** until you finish the one before.
- **QUESTS tab** (menu): filters ACTIVE / READY / HERE / HUNTS / ERRANDS / DONE, sorted so the tracked quest and the ones ready to hand in come first. LEFT/RIGHT filter, UP/DOWN pick, **A or T** tracks a quest on the HUD, **Y or DEL twice** abandons it, which puts it back in the world for someone to offer again.
- **Sister and rival towns:** towns of the same god are sisters, and Light/Dark and Voodoo/Eldritch towns are rivals. Quests done for a town's sisters make its shop cheaper; quests done for its rivals make it dearer.
- **Werewolves** hunt open land on full-moon nights (every 8th day of a season). Only silver cuts through their hide.
- **Treasure Map:** building a chest gives you one, and a red X on the World Map marks each of your chests.
- **CARDS tab:** owning the Strategy Guide opens it for good. It lists every card with its set, power, ability and whether you own it.

## Asking for work

- NPCs don't push quests on you. Villagers and guards open a Chat / Ask for work / Goodbye menu, every shop menu has an "Ask for work" option, and at the merchant you press LT or Q. Ask again to accept.
- You can hold 3 quests at the start, plus 1 more every 4 levels.
- Each quest belongs to fixed towns and a named giver. A finished big quest never comes back, and an abandoned one goes back to the person who gave it.
- Small villager jobs can be repeated and grow with your level. The merchant's Wood Cart makes wood and stone weigh a quarter as much, and villagers then ask for 5x bigger hauls at 6x the pay.
- World Map dots mark towns your quests point to: purple for the King, red for a boss, yellow for a cave or treasure, light blue for water, green for everything else.

## Outlaws, legacy and caves

- **Outlaws:** crimes such as smashing barrels raise town heat. At 30 or more you are **WANTED** (shown on the HUD), and bounty hunters may track you down outside town. A town guard takes a fine (press X twice) to clear your name, and each night's sleep also cools the heat.
- **Highwaymen:** a chest within 3 tiles of a worn road gets raided about every other day, even when trees hide it.
- **Ancestral legacy:** deleting a save leaves a legacy for your next hero: 1 attribute point per 10 levels and per 25 quests done (5 at most), plus a tenth of your gold (500 at most).
- **Cave network:** every cave entrance opens into one of 8 shared chambers, each with one chest. Once a chamber's chest is found, it stays found from every entrance.

## World map and saving

The **Hero** tab also shows your **Karma** (Soul, Miasma from dark magic, Town Heat from crimes, Grapevine fame, and a life mark for each boss type slain), your **card collection** (each kill, cave treasure and fish catch has a 2% chance to give a card; monsters usually drop their own card), and the **bestiary** kill count per monster. These are saved with your hero. Food, materials and matches stack up to 99 per slot. The merchant now sells a **Spear** and an **A&D Strategy Guide** (use it from the inventory for a 2-second peek at secret card combos). A **Yeti** stays calm until you get within 3 tiles or hit it - then it charges. Races give small stat bonuses, and natural pairings (Elf Wizard, Human Knight, Dwarf Warrior, Halfling Rogue, Gnome Necromancer) get +1 to their key stat.

The **World Map** tab (M / Start, then tab 2) shows every screen you have explored: biome colours, brown squares for village homes, grey squares for castles, blue for rivers, lakes and sea, and a gold square for cave treasure you have not claimed yet. Each town you have found gets a number, and the **TOWNS** list beside the map gives its name, its patron god (Light, Dark, Voodoo or Eldritch) and whether its card shop and pet shop are open today. The game auto-saves the map to `rpg2_save.dat` (next to `game.exe`) every time you change screens, and loads it on start. Delete that file to start with a blank map.

**Map cursor:** on the World Map tab, the arrow keys, D-pad or a mouse click move a white cursor. The line under the map names the town there (castle town or village, its god, and whether its card shop and pet shop are open today) or the kind of land. Zoom in (+ / RT) far enough and town names are written on their tiles.

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
- **Wildlife:** at most 6 animals per screen.
- **Owls** perch in trees and fly to another tree if you come close. **Scorpions** scuttle about in deserts.
- **Babies:** when two birds or two squirrels shelter in the same tree, a baby can appear (at most one per screen per day, and the 6-animal cap still holds).
- **Familiars** face the way they walk. When you are AFK a dog chases squirrels and a cat chases butterflies or birds, then comes back; they never catch anything. Asleep, they curl up by a campfire if one is near, otherwise they ride in your pack and weigh 2. At the pet shop you can pick small, medium or large and one of 4 coats (this choice is not saved yet and resets when you reload).
- **Campfires** slowly refill HP, MP and stamina while you stand near one; sleeping refills all three.
- **Bow:** holding a full draw tires you out: stamina stops coming back for 10 seconds.
- **Beggars:** give as often as you like; the karma reward comes once a day.
- **Homes** sit on the ground and you can't walk through them. Sand lies flat with the ground.
- **Loot:** fewer drops, and monsters killed by villagers or town defenders drop nothing, so gold doesn't pile up while you are away.
- **Attacks:** monsters lunge at you up close, and your minions lunge each time they land a blow. Fishing shows the rod in your hand, the bobber flying out in an arc, a sagging line and the rod bending on a nibble.
- **Dark vision:** at night (and in caves) the dark closes in beyond what your race can see: Human 5.5 tiles, Halfling 6, Elf 7.5, Dwarf and Gnome 8.5, Night Vision 10. The AFK view fogs in to the same distance. Campfires and village lamps push the dark back with a warm glow on trees, the hero, villagers and monsters.
- **Campfire edge:** monsters kept out by a fire take turns lurking at the edge of the light, hiding on the dark side of a nearby tree, or stalking a nearby animal.
- **Roaming monsters:** monsters that aren't chasing you walk to a tree, the water's edge, an animal or a look-out spot, linger there a while, then move on.
- **Big trees:** old giants (many in forests) have a high crown; it turns lacy when you walk behind it so you can still see yourself. In AFK they tower over you and block part of the sky.
- **Living watercolour (AFK):** water colours bleed softly, canopies sway, butterflies drift and flowers nod.
- **NPC routines:** villagers sweep, farm, fetch water or enjoy a tree; the blacksmith hammers and quenches; the merchant checks his barrels and stares, then walks over, if you smash them (raises town heat).
- **Home decorations:** Flower Vase, Pothos Ivy, Stack of Books, Candle, a Snow Globe (winter only) and a Jack-o-Lantern (fall only) from the merchant; set them out at home for comfort, and candles light the room. Add your own with `NewDecor` lines in the Update folder. House Supplies now cost 250.
- **Card Binder:** at home, open the Card Binder to move cards between your 60-card deck and the binder; the selected card is shown face up.
- **Mountains:** cliffs along mountain edges that you walk around, with paths in on two sides; neighbouring screens show the cliff face. Some mountains are peaks with rings of crags round a snowy summit, where Yeti roam.
- **Caves:** a real cavern system to explore, with the treasure at the far end.
- **Clothes and trim:** on the start screen pick a clothes colour ([8]) and a trim colour ([9]) for the hem, belt, collar and hood edge. Under a hood your chin shows and your eyes are two glowing points.
- **AFK world:** Yeti, Bigfoot, Sphinx and Bunyip appear at their real size and shape, the Vampire wears his cape, and towns show their barrels, torches, fire pits, altar, castle stall and merchant tent.
- **Vampires** (formerly Dracula): carry **Garlic** (merchant) and they won't come for you; a **Silver Dagger** (merchant) does double damage to them.
- **Farming:** buy a **Hoe** and **Seeds** from the merchant. X with the Hoe on bare grass tills a plot (up to 48); X with Seeds plants turnips, which grow one stage a day and are ripe on day 3; X on a ripe plot harvests 2-3 **Turnips** (eat to heal) plus 3-4 Seeds per turnip, so you only buy Seeds once. Plots are saved.
- **Footpaths:** grass you walk over often turns to a dirt path, and grows back a little each day you leave it alone. Paths are saved.
- **Villager quests:** villagers sometimes ask you to slay a few monsters or bring them Wood, Stone, Fish or Apples, scaled to your level. One job at a time; any villager pays you when it's done. Talk to a villager to see your progress.
- **Hit splashes:** class abilities splash their own colour - green for Rogue venom (hits while sneaking) and zombie bites, blue for the Wizard's bolt, gold for the Knight/Warrior counter, bone-white for skeletons and pale blue for ghosts.
- **AFK view:** stand idle and the camera eases into the hero's head. You see the world through their eyes: sky (stars, the moon with its phase, shooting stars at night), ground, trees, homes, butterflies and critters, your familiar, the campfire, and your rod and bobber if you are fishing. The gaze slowly moves between nearby things. Touch any control and it snaps straight back to normal play.
- **Grass** bends aside as you walk through it, and **foam** laps at every shoreline.
- **Barrels** stand by the homes in towns and in caves. X smashes one for a little gold, an apple or wood (2% card chance). They are back the next day.
- **Zombies** near rivers and marshes rise out of the water's edge.
- **Peyote:** some desert cacti are small round peyote buttons with a pink flower. Two arrows split one open, X gathers it, and eating it melts the colours on screen for 30 seconds.
- **Update folder:** see `Update/README.md` to change items, villager lines and quests with a text file, no recompiling.
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
| farming.c | Tilled plots, planting, crop growth and harvest |
| paths.c | Worn footpaths that grow back |
| quests.c | Everyday villager kill/fetch quests |
| update.c | Reads the Update folder's text files at launch |

Each module has a matching .h with its function prototypes.

## Getting hurt

- Monsters can now hurt you (in the AFK view too). For now it's forgiving: about 2 HP a hit, 5 from big beasts like Bigfoot, a second or more apart.
- **Zombies** grab and bite, holding you in place for half a second. **Vampires** drain HP from a short distance and heal themselves; stronger ones reach further. Garlic still keeps them away.
- **Vitality (VIT)**, the old STA stat, sets your HP bar: 100 + 10 per point.
- If you're knocked out you wake in the nearest castle town you've found. A townsman says he carried you there and his son fetched your things. Nothing is lost.
- Homes, shops, altars and the castle are menus: **B / ESC** or **Leave** always takes you back outside.

### The world, highways, Elysium and prices

- **Whole world at the start:** the whole 64x64 world (biomes, 12 towns, about 45 villages and 6 dwarf holds) is laid out from a world seed when the game starts. The seed is kept in your save, so towns don't move. The map still only shows places you've been.
- **Elysium:** the hub and trading town, where every new hero starts. Depending on the world it sits in the heartland, by an ocean port, on a lakeshore, in a forest or in the desert. Elysium always has every shop open and pays 25% more for what you sell.
- **Highways:** stone roads join all the towns along the shortest network. A highway screen has a cleared road from the centre to each edge it leads to.
- **Dwarf holds:** villages up in the mountains are always dwarf holds: short, bearded folk with their own greetings.
- **Supply and demand:** what grows around a town is cheaper there (wood near forests, stone near mountains, fish near water, food near farmland), and deserts make food and fish dearer. Selling a lot of one thing in a town lowers what it pays, and that recovers by about 40% each day.

### Hero facing and sprites

- The hero turns to face all 8 directions you can move and attack in. From behind you see the back of the head with no face; side views show one eye and a nose; diagonals turn the face part-way. Weapons and the bow are held toward the side being faced, and the AFK view starts looking the same way.
- **Hand-drawn sprites:** put `hero_s.bmp`, `hero_sw.bmp`, `hero_w.bmp`, `hero_nw.bmp`, `hero_n.bmp`, `hero_ne.bmp`, `hero_e.bmp` and `hero_se.bmp` in the `sprites` folder (see `sprites/README.txt`). Magenta is see-through, feet go at the bottom edge, and missing right-side pictures are mirrored from the left-side ones. Directions without a picture use the drawn hero.

### AFK view: bodies and pets

- **Rounder bodies:** people and monsters in the AFK view have six-sided rounded torsos, heads and limbs. Legs bend at the knee as they stride and arms swing with a soft elbow. Zombies lurch slowly with their arms held out.
- **Pets:** the dog and cat have rounded bodies, necks and tails, and their legs bend as they trot.
  - The cat paws at a butterfly close by and, with nothing to chase, crouches, wiggles and pounces on leaves. It also chases birds and butterflies.
  - The dog chases squirrels, lifts its nose to sniff the air near butterflies, and wags its tail.
  - Between chases both wander playfully around you, and they never catch anything.

### Tier 1 + 2 (Saturn-minded: fixed arrays, no heap, tiny save files)
- **Town banner:** entering a town shows its name and your Town Heat.
- **Worn gear:** a belt slot flashes red when the item is under 15% durability.
- **Cacti and peyote:** any weapon breaks them. One swing or arrow fells a cactus; a swing splits peyote open (X to gather).
- **Doorway camera:** near a cottage door the camera eases in and pans to just behind you, then glides back to isometric.
- **World Map:** highways are drawn through places you've visited.
- **Town menus** scroll (up to 64 rows), so the card shop has room for more sets.
- **Hall of Records:** the Options tab lists your heroes' falls and retirements (`hall.dat`, last 16).
- **Wishing wells** on town and village squares: 10 gold once a day for +25% damage for 5 minutes.
- **Double jump:** press hop (LB / J) again on the way down.
- **3-hit combo:** sword/axe swings within 0.8 s chain overhead, wide side sweep, long thrust (1.5x).
- **Quick-use wheel:** hold BACK and tap a D-pad direction to eat or drink one of your first 4 foods/potions.
- **Barrels** stand against house sides, away from shopkeepers.
- **Caravans:** villagers in a town offer rides to the 3 nearest towns you've found, 200 gold.
- **Holidays** (from the PC date): Halloween (Oct 24-31) night XP +50%, Winter Feast (Dec 20-31) 15% off, New Year's Day double XP.
- **Left stick** moves in any direction; tilt sets speed.
- **Shadows:** trees cast soft canopy shadows on the ground, and your shadow stretches away from the sun or moon.
- **Update files:** `NewNPC,NPCID,Name,Job,Line` and `NewEconomy,EconomyID,ItemName,PricePercent`.
