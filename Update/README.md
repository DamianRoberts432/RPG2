# Update folder

Put a `.txt` file in this folder (next to `game.exe`), one change per line, then start the game.
Each line is applied, saved into `applied.txt` here (so it keeps working every launch),
and the `.txt` file you dropped in is deleted. Lines the game didn't understand go to `errors.txt`.
To undo everything, delete `applied.txt`. Lines starting with `#` are ignored.

Only `.txt` files are read, so sound files (`.wav`) can live here too.

## Items

    EditItem,Garlic,GoldValue,12
    EditItem,Katana,AtkBonus,2,Durability,300,Weight,2

Use the item's name as shown in the shop (or `item_id_N`, the Nth merchant item counting from 0).
Fields: `GoldValue` (or `Price`), `Stock` (-1 = unlimited), `Weight`, and for weapons `Damage`,
`AtkBonus` (adds to damage) and `Durability`. Weapons you already carry keep their old stats.

New merchant items (Type is Food, Material or Tool; for Food, the last number is the health it restores):

    NewItem,105,Honey Cake,Food,25,1,0.3,18

Home decorations the merchant sells (Price, then Light 1 to make it glow in your house, then the Comfort it adds):

    NewDecor,201,Candle Lamp,10,1,1

## Cards

    NewCard,401,Odin Allfather,1,3,3,3,1

Fields: id (ignored, cards are added in order), name, set (0 Wild Creatures, 1 Angels, 2 Goetia Demons),
rarity (1-3), attack, health, ability (0 none, 1 First Strike, 2 Mighty +3/+3, 3 death curse,
4/5/6 death blessing both/strength/life). Stats are capped by rarity: 1/1, 2/2 or 3/3, so the
Odin above becomes a 3/3. New cards show up in packs, drops and the card shop.

## Villagers

    EditNPC,Villager,Dialogue,"Mind the wolves, friend."

Adds a line villagers say (up to 16). Quotes let a line contain commas.

    NewDialogue,5001,201,Bartholomew,"Welcome, traveler!",0

Adds "Bartholomew: Welcome, traveler!" to the same villager lines.

## Quests

    EditQuest,3,TargetEntity,Goblin,TargetCount,5,RewardGold,200
    EditQuest,5,TargetEntity,Wood,TargetCount,10

Quests 1-8: 1 Skeletons, 2 Zombies, 3 Goblins, 4 Satyrs, 5 Wood, 6 Stone, 7 Fish, 8 Apples.
`TargetEntity` is a monster (slay quest) or an item name (fetch quest). Counts and rewards
grow with your level on top of these numbers.

    NewQuest,9,questgeneric,Wolf Trouble,201,200,0
    EditQuest,9,TargetEntity,Goblin,TargetCount,4

`NewQuest` names quest slot 9-16 and sets its reward (queststory, questgeneric or questking);
villagers start offering it once `EditQuest` gives it a target and count.

## Spawners

    NewSpawner,1,Forest,Goblin,40
    NewSpawner,2,Any,Vampire,5,1

On that biome (or `Any`), the monster turns up that often (percent) instead of the usual pick.
A last `1` means nights only. Monsters: Skeleton, Zombie, Vampire (or Dracula), Goblin, Satyr,
Yeti, Sphinx, Bigfoot, Bunyip. Up to 16 spawners.

## Set pieces

    NewSetPiece,1,Prairie,30,www/w.w/www

Stamps a small hand-made patch (up to 7x7) onto that percent of the biome's screens.
Rows are split by `/`. Letters: `g` ground, `w` rock wall, `s` sand, `n` snow, `c` cactus,
`t` tree, `~` water, `.` leave as it is. Up to 12 set pieces.

## Biomes

    NewBiome,1,Dark Woods,Forest,25,60,5,5

A named variant of a base biome: that percent of its screens are laid out again with the given
percent of trees, rock walls and water (the rest is open ground). Rock is capped at 30 and the
three together at 90, so you never get boxed in. Caves and castles can't be varied. Up to 8.

## Recipes

    NewRecipe,1,Rope,2,Wood,3
    NewRecipe,2,Lantern,1,Wood,2,Stone,1

The blacksmith gets a "Craft" row: hand over the materials (up to 3 kinds), get the result.
Up to 16 recipes.

Biome names: Prairie, Swamp, Beach, Mountain, Desert, Tundra, Grasslands, Oasis, Forest,
Island, Ocean, Lake, River, Cave, Castle.
