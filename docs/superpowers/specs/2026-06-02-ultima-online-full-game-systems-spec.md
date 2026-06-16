# Design Spec: Ultima Online Classic Sandbox Systems

This document specifies the design for the full classic sandbox systems of the Ultima Online remake. We are adding use-based skills, stats caps, a reagent-based Magery spellbook, Mining and Blacksmithing, corpse looting, merchant NPCs, and a custom UO-inspired graphical interface.

---

## 1. Core Sandbox Stats & Use-Based Skill System

UO does not use levels or experience points. Progression is strictly use-based.

### Character Stats
- **Strength (Str)**: Base health = `Str * 0.5 + 50`. Affects melee damage.
- **Dexterity (Dex)**: Base stamina = `Dex`. Affects attack speed.
- **Intelligence (Int)**: Base mana = `Int`. Affects spell damage and mana regen.
- **Limits**:
  - Max per stat: `125`
  - Total stat cap: `225`

### Skill Progression
- Each skill has a value from `0.0%` to `100.0%` (Grandmaster).
- Total skill cap: `700.0%`
- **Raise / Lower / Lock Mechanic**:
  - In the Skills menu, players toggle each skill's state: **Raise (`^`)**, **Lower (`v`)**, or **Lock (`-`)**.
  - Performing actions has a chance to raise the skill (depending on difficulty).
  - If a skill raises but the total sum of all skills is at `700.0%`, the system automatically decreases one or more skills flagged as **Lower (`v`)** to make room for the gain.

### Implemented Skills
1. **Swordsmanship**: Melee weapon accuracy and hit chance. Gained by hitting enemies with a sword.
2. **Tactics**: Scales melee damage. Gained by fighting in melee.
3. **Anatomy**: Scales melee damage and improves physical healing. Gained by fighting or bandaging.
4. **Magery**: Higher skill reduces spell fizzle rates. Gained by casting spells.
5. **Meditation**: Speeds up Mana recovery. Gained by active meditating (hotkey) or passive regen.
6. **Mining**: Digging iron ore from rocks. Gained by mining.
7. **Blacksmithing**: Crafting weapons and armor from ore. Gained by smithing.
8. **Healing**: Restoring HP using clean bandages. Gained by healing characters.

---

## 2. Reagent-Based Magery Spellbook

Spells consume Mana and specific Reagents (herbs/elements).

### Reagent Items
- **Black Pearl (BP)**, **Bloodmoss (BM)**, **Garlic (GA)**, **Ginseng (GI)**, **Mandrake Root (MR)**, **Nightshade (NS)**, **Sulfurous Ash (SA)**, **Spiders' Silk (SS)**.

### Spell list
1. **Heal** (Mana: 4, Reagents: Garlic, Ginseng). Restores 10-15 HP.
2. **Magic Arrow** (Mana: 6, Reagents: Nightshade, Sulfurous Ash). Shoots a weak magic projectile.
3. **Fireball** (Mana: 9, Reagents: Black Pearl, Sulfurous Ash). Shoots a medium fire projectile.
4. **Greater Heal** (Mana: 11, Reagents: Garlic, Ginseng, Mandrake Root, Spiders' Silk). Restores 30-40 HP.
5. **Teleport** (Mana: 11, Reagents: Bloodmoss, Mandrake Root). Instantly moves the caster to clicked coordinates (if unblocked).

### Spell Failure (Fizzle)
- Each spell belongs to a Circle (1 to 4).
- Caster's Magery skill must be sufficiently high relative to the spell's circle, otherwise the spell fails ("fizzles").
- On fizzling, the mana and reagents are consumed, a fizzle animation (smoke/sparkle) displays, and the spell has no effect.

---

## 3. Gathering & Crafting Systems

### Mining
- Requires a **Pickaxe** or **Shovel** equipped or in the inventory.
- Double-clicking the shovel and targeting a stone/mountain tile (colliders or mapped tile indices `08`, `17`, etc.) initiates a mining swing.
- If successful, mines an **Iron Ore** item. Gaining Mining skill.

### Blacksmithing
- Requires a **Smithy Hammer** and standing near a **Forge** (special static tile).
- Double-clicking the hammer opens a Blacksmithing crafting gump.
- Options:
  - **Dagger** (Cost: 3 Ore)
  - **Longsword** (Cost: 8 Ore)
  - **Iron Shield** (Cost: 6 Ore)
  - **Plate Armor** (Cost: 12 Ore)
- Gaining Blacksmithing skill. Higher skill yields higher-quality gear (adds damage or defense).

---

## 4. Looting & Container System

- When an NPC or player dies, the server deletes the entity and spawns a **Corpse** container at its location.
- Double-clicking the corpse opens a chest/loot UI showing gold coins, reagents, and equipment.
- Players drag items from the corpse container into their own Inventory.

---

## 5. NPC Merchant Shops

- Merchants (Weaponsmith, Alchemist) stand near the starting shrine.
- Click them to select **Buy** or **Sell** options.
- Opens a shop menu: buy reagents, potions, shovels, weapons, or sell gathered ore and weapons for gold coins.

---

## 6. GUI Gump Windows

The client renders draggable overlays:
1. **Status Window**: Displays Str, Dex, Int, HP, Stamina, Mana, Gold, and Weight.
2. **Inventory Grid**: 6x4 slots container holding icons of weapons, armor, ore, reagents.
3. **Skills Gump**: Scrollable list of the 8 skills with numerical percentage values and Raise/Lower/Lock toggles.
4. **Spellbook Gump**: Shows spell list with mana costs and quick-cast buttons.

---

## 7. High-Quality Pixel Art Assets

We will generate 2D modern pixel-art sprites using `generate_image`:
- `assets/terrain_ss.png`: Modern isometric tilesheet with grass, paths, stone walls, water, forge, anvil, shrine.
- `assets/player_anims.png`: 32x32 character sheet containing row 0 (Idle: 3 frames), row 1 (Walk: 8 frames), and row 2 (Attack: 5 frames).
- `assets/orc_anims.png`, `assets/ogre_anims.png`, `assets/skeleton_anims.png`, `assets/lizardman_anims.png`: Unique monster sheets matching the same row format.
- `assets/gui_elements.png`: Core frames, slots, and icons for reagents, iron ore, and weapons.
