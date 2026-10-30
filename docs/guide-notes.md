# Player's strategy guide -- notes for when it is written

Decided with the owner, 2026-10-30. Nothing is written yet; this is the brief.
It comes last, after the items and selling, the economy rerun and (possibly)
the post-game battle league, so it describes the finished game.

## What it is

A strategy guide in the style of the 90s/2000s magazine guides: walkthrough
chapters, tables, "PRO TIP" sidebars. Players choose whether to look things
up. Published as a page the installer links to.

## Generated, not typed

Every table comes from the game's own data through a script, so the guide
cannot drift from the firmware (CLAUDE.md: derive, never restate):

- gym and Elite Four rosters per region and difficulty (`trainers.h`), with
  suggested counters by type
- Explore areas: species, odds, levels, time of day, weather, swarms, the
  badge each area needs (`wild_areas.h`, `tools/emu/odds_report.cpp`)
- legendaries and one-offs: home, gate, level, the guaranteed first meeting
- every evolution route: level, stone, Linking Cord (the trade stand-in),
  branches, gender, Meltan's steps, Shedinja
- the Mart, the Game Corners' prizes and odds, item prices and sell prices

## Chapters to write by hand

- getting started: care, sleep and naps, training games, the farewell
- the economy: steps, gym prizes, selling, what is worth buying
- building a hard-mode team: raise and train stored creatures, vitamins on
  stored creatures, X items, PP Ups, moves and TMs, IVs set the ceiling
- evolutions that hide behind knowledge, said plainly: SLOWKING is a Linking
  Cord on a SLOWPOKE (otherwise SLOWBRO at 37); KLEAVOR is the Black Augurite
  on SCYTHER before any cord; MELMETAL is 10,000 steps walked as the live pet

## At the back, NOT in the table of contents

Small and unobtrusive, as the magazines did:

- **Spoilers / secrets**: the best-rod gate, the 8-hour care window, the
  legendary safety net, the farming lockout, Surfing Pikachu's conditions,
  Galar's water-mode gate, Necrozma's Ultra Beasts, the Paldea story gates.
- **Codes**: a small box with the Memory-game codes (Celadon, Goldenrod, Surf,
  SETTINGS, CHEATS) and what the hidden settings and cheats do, including the
  permanent marks they leave on the player card and on creatures.

These stay out of the README, release notes and installer page as before; the
guide is the one place a player can find them.
