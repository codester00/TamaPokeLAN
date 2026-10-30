#pragma once
#include <Arduino.h>

// Idiomas soportados. La fuente del firmware no tiene acentos: ambos textos van
// sin tildes ni enes (igual que ya iba el espanol).
enum Lang : uint8_t { LANG_ES = 0, LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_PT, LANG_COUNT };
#define LANG_DEFAULT LANG_EN  // idioma por defecto: ingles

extern Lang gLang;  // idioma activo (definido en i18n.cpp)

// IDs de cadena. El orden debe coincidir con la tabla STRINGS de i18n.cpp.
enum StrId : uint16_t {
  // estado del bicho (statusMsg)
  S_EVOLVING, S_EATING, S_LIKES, S_HUNGRY, S_NEEDS_BATH,
  S_EXHAUSTED, S_SAD, S_CHUBBY, S_IS_SHINY, S_HAPPY,
  // ceremonias de despedida
  S_FAREWELL, S_RUNAWAY, S_GOODBYE,
  // huevo
  S_EGG_HDR, S_EGG_LEGEND, S_EGG_RARE, S_EGG_TOUCH, S_EGG_MOVES, S_EGG_ALMOST,
  // formatos compartidos
  S_POKEDEX_FMT,   // "POKEDEX %u/151"
  S_NAME_FMT,      // "%s%s Nv.%u"
  // dialogo soltar
  S_RELEASE_FMT, S_YES, S_NO,
  // minijuego y saco
  S_HITS_FMT, S_STR_GAIN_FMT, S_NEW_RECORD, S_RECORD_FMT, S_HIT_FAST,
  S_SCORE_FMT, S_PLUS_JOY,
  // reloj / ajustes
  S_SET_TIME, S_HOUR, S_MIN, S_CLOCK_CANCEL, S_LANG_LABEL,
  // celebracion
  S_MEDAL_BANNER, S_GREAT, S_STREAK_DAYS_FMT,
  // ficha: perfil
  S_STREAK_FMT, S_VIN, S_BERRY_UNK, S_BERRY_RED, S_BERRY_BLUE, S_BERRY_GREEN,
  S_INFO_FMT, S_RENAME_HINT,
  // ficha: combate
  S_BATTLE, S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_FIT, S_TRAIN_STR,
  // ficha: medallas
  S_MEDALS_FMT, S_BACK,
  // teclado y galeria
  S_NAME, S_DETAIL_BACK,
  // barras
  S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG,
  // marcador en vivo del minijuego
  S_REC_FMT,
  // ficha: pagina de progreso
  S_PROGRESS, S_LVL_FMT, S_NEXT_LVL_FMT, S_EVO_LABEL, S_FINAL_FORM,
  S_EVO_READY, S_EVO_BLOCKED, S_EVO_IN_FMT, S_MISTAKES_FMT,
  // interruptor de sonido (ajustes)
  S_SND_ON, S_SND_OFF,
  S_EVO_TAP,        // texto del boton de evolucion
  S_FAREWELL_BTN,   // texto del boton de despedida (lleva el nombre: "%s ...")
  S_RUNAWAY_BTN,    // texto del boton de escapada por abandono (final triste)
  // dialogos de decision (evolucionar/mantener, despedirse/quedaros)
  S_EVO_Q, S_EVO_KEEP, S_FAR_Q, S_FAR_GO, S_FAR_STAY,
  S_CHOOSE_STARTER,  // titulo de la eleccion del inicial (primera vez)
  S_NO_SPRITES, S_LOAD_SPRITES,  // aviso cuando falta el sprite en la SD
  S_STAT_VIT,   // vitalidad (abreviatura de PS/HP en cada idioma)
  S_IV_FMT,     // valor individual junto a cada stat ("IV %u")
  // menu overlay (tap the name on the main screen) and the party
  S_MENU_TITLE, S_SETTINGS, S_CLOSE,
  S_PARTY_FMT,      // "PARTY %u/6"
  S_PARTY,          // short navigation label
  S_PARTY_EMPTY,    // empty slot marker
  S_PARTY_JOINED,   // "%s joined the party!"
  S_PARTY_FULL,     // prompt: pick someone to replace
  S_PARTY_LETGO,    // decline the swap
  // stats shortcut in the menu, and the training submenu behind the 5th icon
  S_STATS,          // menu row: jump straight to the stats card page
  S_TRAIN,          // training submenu title
  S_TR_ATK,         // row: train strength (the punching bag)
  S_TR_SPE,         // row: train speed (the ball game)
  S_TR_DEF,         // row: defence -- passive, shown for information only
  S_TR_DEF_HINT,    // why the defence row does nothing when tapped
  // moves card page + the move picker behind it
  S_MOVES,          // card page title
  S_MOVE_EMPTY,     // an unused move slot
  S_MOVE_PICK,      // picker title: choose what goes in this slot
  S_MOVE_TAP,       // hint on the moves page: tap a slot to change it
  S_MOVE_PWR,       // "PWR %u" next to an attacking move
  S_MOVE_STATUS,    // shown instead of power for a status move
  // level-up learn prompt
  S_LEARN_Q,        // "%s wants to learn" (name of the creature)
  S_LEARN_SKIP,     // decline the new move
  // battle
  S_BTL_USED,       // "%s used %s"
  S_BTL_SUPER, S_BTL_WEAK, S_BTL_IMMUNE,
  S_BTL_MISS, S_BTL_CRIT, S_BTL_FAINT,
  S_BTL_HURTSELF,   // confusion
  S_BTL_STATUS,     // "%s was %s" -- ailment name comes from S_AIL_*
  S_AIL_PARA, S_AIL_BURN, S_AIL_POISON, S_AIL_SLEEP, S_AIL_FREEZE, S_AIL_CONFUSE,
  S_BTL_WIN, S_BTL_LOSE,
  S_BTL_SENDS,      // "%s sends out %s"
  S_BTL_GO,         // "Go, %s!"
  S_GYMS,           // gym list title
  S_BADGES_FMT,     // "BADGES %u/8"
  S_TRAINER,        // player card title
  S_SPD_GAIN_FMT,   // "SPEED +%u" after the reaction test
  S_AVATAR_HINT,    // "tap: change avatar"
  S_MEDALS_TOTAL_FMT,  // "%u earned in all"
  S_EASY, S_HARD,   // gym ladder difficulty toggle
  S_PICK_FMT,       // "CHOSEN %u/%u"
  S_FIGHT,          // start the battle
  S_LOCKED,         // a gym not yet unlocked
  S_BTL_SWITCH,     // battle menu: bring on another creature
  S_BTL_BEAT,       // "%s defeated!" on the win screen
  S_BTL_NEWBADGE,   // a badge earned for the first time
  S_VOL_FMT,        // volume level, e.g. "VOL 7"
  S_BOX_FMT,        // "BOX %u/%u"
  S_BOX_SWAP,       // "swap with %s: pick a slot"
  S_BOX_BTN,        // the button on the party screen
  S_REVIVE,         // bring a banked creature back as a companion
  S_REVIVE_EGG,     // why it is unavailable right now
  // LAN battle
  S_LAN, S_LAN_HOST, S_LAN_JOIN, S_LAN_WAIT, S_LAN_READY, S_LAN_REFUSED,
  S_LAN_PICK, S_LAN_VS,
  S_LAN_GONE, S_LAN_WAITFOE, S_LAN_REMATCH,
  S_BTL_RUN,        // the third option in a fight: leave it
  S_EGG_REGION,     // hint under the egg's region pill
  S_WIN_TRAIN_FMT,  // "%s +%u" -- the training a gym win is worth
  S_WIN_MAXED,      // ... or that there is nowhere left to train
  S_CHOOSE_REGION,  // first boot: pick the region your starter comes from
  S_RETIRE,         // menu row: retire the creature on demand
  S_RETIRE_Q,       // ...and the confirmation
  S_RETIRE_COST,    // what retiring early costs the next creature
  S_EVO_SLOW,       // shown on the card while a creature carries that debt
  S_NEED_PACK,      // region chooser: this region's sprite pack is not on the SD
  // Letting a BANKED creature go for good, from the party or box detail sheet.
  // The confirmation itself reuses S_RELEASE_FMT / S_YES / S_NO, which the live
  // pet's own release dialog already owns -- the question is the same question.
  S_RELEASE_BTN,    // the button on the detail sheet
  S_RELEASE_GONE,   // ...and the warning under it: this one does not come back
  S_BOX_TAKE,       // box detail sheet: move this creature into the party
  S_RETIRE_GONE,    // retire confirm: an early retire is not banked either
  // Wild encounters, the bag, and choosing which creature you raise.
  S_BAG,            // menu row, and the battle menu's fourth option
  S_BAG_EMPTY,      // ...with nothing in it
  S_EXPLORE,        // gym screen: go looking for a wild creature
  S_WILD_MET,       // "A wild %s appeared!"
  S_WILD_FLED,      // "%s fled!"
  S_GOT_AWAY,       // running from a wild fight succeeded
  S_NO_ESCAPE,      // ...and failed, which costs the turn
  S_THREW,          // "You threw a %s"
  S_CAUGHT,         // "%s was caught!"
  S_BROKE_FREE,     // the ball failed
  S_ITEM_USED,      // "Used %s"
  S_ITEM_NOUSE,     // the item was valid but had nothing to do
  S_FOUND_ITEM,     // a wild win's drop
  S_FOCUS,          // party/box sheet: raise this one instead of the live pet
  S_FOCUS_NOW,      // ...and the label once it already is the one being raised
  // A way out of "waiting for the rival" that does not require the rival's
  // radio to ever answer. S_YES/S_NO on the confirm are reused, same as the
  // release dialog does.
  S_LAN_QUIT,       // the button, once the wait has run long enough to show it
  S_LAN_QUIT_Q,     // ...and the confirmation
  // The rival record on the LAN pairing screen. Two variants each because the
  // table has no pluralization rule of its own -- some languages need a
  // different word at exactly 1 (English "time"/"times"), some do not
  // (French "fois", German "Mal" do not change), so each language just
  // writes both and the render code picks by count == 1.
  S_LAN_DEFEATED_1, S_LAN_DEFEATED_N,
  S_LAN_LOST_1, S_LAN_LOST_N,
  // the Poke Mart
  S_MART,           // screen title
  S_MART_RATE,      // the exchange rate, e.g. "1 pedometer step = $1"
  S_MART_STEPS_FMT, // "your lifetime step count: %lu"
  S_WALLET_FMT,     // "Wallet: $%lu"
  S_MART_BUY_Q,     // buy confirm: "Buy %s?"
  S_MART_PRICE_FMT, // buy confirm sub-line: "COST: $%lu"
  S_EVO_TRADE,      // card: this species evolves only by being traded
  S_CORD_PICK,      // Linking Cord on Karrablast/Shelmet: pick the partner
  // Trading (trade.h), appended in one block
  S_TRADE, // LAN screen button and trade title
  S_TRADE_LOOK,
  S_TRADE_LOOK2,
  S_TRADE_NOBODY,
  S_TRADE_WITH, // %s = the other trainer
  S_TRADE_GIVE,
  S_TRADE_GET,
  S_TRADE_CHOOSE,
  S_TRADE_WAIT, // their offer has not arrived
  S_TRADE_WHY_BAD,
  S_TRADE_WHY_PACK,
  S_TRADE_WHY_HAVE,
  S_TRADE_ACCEPT,
  S_TRADE_ACCEPTED, // the button once pressed; tap again to take it back
  S_TRADE_THEYOK,
  S_TRADE_BUSY,
  S_TRADE_KEEPON,
  S_TRADE_DONE,
  S_TRADE_GOT,
  S_TRADE_EVOHINT, // a received trade evolver: hinted, not spoiled
  S_TRADE_CANCELLED,
  S_TRADE_VERSION,
  S_TRADE_SELF,
  S_TRADE_LOGFULL,
  S_TRADE_NORADIO,
  S_TRADE_LOST, // stuck screen, the owner's wording
  S_TRADE_LEFT, // minutes:seconds until the player is asked
  S_TRADE_INCOMPLETE,
  S_TRADE_UNCONF,
  S_TRADE_YOUHAVE,
  S_TRADE_ASK1,
  S_TRADE_ASK2,
  S_TRADE_ASK3, // %s = S_TRADE_KEEP, then S_TRADE_REVERT
  S_TRADE_REVERT,
  S_TRADE_KEEP,
  S_TRADE_REVERT_SUB,
  S_TRADE_KEEP_SUB,
  S_TRADE_SURE,
  S_TRADE_SURE2,
  S_TRADE_PICK,
  S_TRADE_PARTYLBL, // picker hub: showing the party
  S_TRADE_HUBHINT, // picker hub: tap cycles party / box pages
  S_GAME_BAG, // training menu: the strength game
  S_GAME_REACT, // the speed game
  S_GAME_BALL, // the defence game
  S_GAME_BERRY, // the joy/bond game
  S_ST_JOY, // what a game raises
  S_ST_BOND,
  S_ST_MAX, // a stat at its ceiling
  S_CATCH_FMT,
  S_CATCH_GAIN_FMT, // joy, then bond
  S_CATCH_HINT,
  S_GAME_ROCK, // hybrid: strength + speed
  S_GAME_BLOCK, // hybrid: defence + speed
  S_GAME_TILT, // hybrid: speed + defence
  S_ROCK_HINT,
  S_BLOCK_HINT,
  S_TILT_HINT,
  S_PERFECT, // rhythm grade
  S_GOOD, // rhythm grade
  S_MISS, // rhythm grade
  S_GAIN2_FMT, // two stats gained: +n NAME  +n NAME
  S_BLOCKED_FMT,
  S_SMASH_FMT,
  S_TILT_DONE_FMT,
  S_TILT_NOSENSOR,
  S_GAME_PUSH, // hybrid: strength + defence
  S_GAME_SIMON, // joy + bond
  S_GAME_SEEK, // joy + bond
  S_PUSH_HINT,
  S_SIMON_WATCH,
  S_SIMON_GO,
  S_SEEK_HINT,
  S_SEEK_PICK,
  S_PUSHED_FMT,
  S_SIMON_FMT,
  S_SEEK_FMT,
  S_FOUND,
  S_NOT_HERE,
  S_PLUS_FMT, // a gain on a result screen: +n NAME
  S_MAX_TODAY, // bond's daily allowance is used up
  S_AVATAR_PICK, // avatar picker title
  S_BLACKOUT_FMT, // after losing a battle: the trainer
  S_BLACKOUT_SUB, // why
  S_MOM_1, // Mom, first line
  S_MOM_2, // Mom, second line
  S_AREA_REQUIRES,  // a locked Explore area: "REQUIRES %s" (a badge name)
  S_AREA_REQ_CHAMP, // a locked Explore area opened by the champion
  S_AREA_LV_FMT,    // an Explore area's level band, "Lv.%u-%u"
  S_AREA_WHOLE,     // Explore in a region that has no areas yet
  S_AREA_NO_SPRITES, // an open Explore area with nothing that can be drawn to meet (yet)
  // Today's weather in a Galar Wild Area zone, on Explore's area row, in
  // WILD_WX_* order (wild.cpp static_asserts the count). 11 characters at most:
  // the row fits "Lv.60-70 " and the name at text size 2.
  S_WX_CLEAR, S_WX_OVERCAST, S_WX_RAIN, S_WX_THUNDERSTORM, S_WX_SNOW,
  S_WX_SNOWSTORM, S_WX_HARSH_SUN, S_WX_SANDSTORM, S_WX_FOG,
  // A stat stage changing in battle (v3.50): "<creature>'s <stat> rose!".
  // Creature name first, then the stat, in every language -- printf has no
  // reordering, so the sentence is built around that order. 39 chars at most
  // with a 12-character name, the battle message buffer.
  S_BTL_STAT_UP, S_BTL_STAT_UP2, S_BTL_STAT_DOWN, S_BTL_STAT_DOWN2,
  S_ST_ATTACK, S_ST_DEFENSE, S_ST_SPATK, S_ST_SPDEF, S_ST_SPEED, S_ST_STATS,
  // PP (v3.52): the label (German says AP), a move with none left, and a
  // creature with no moves left at all, which then STRUGGLEs.
  S_PP, S_BTL_NO_PP, S_BTL_STRUGGLE,
  // The Game Corners (v3.53). Hall names are proper nouns and stay English.
  S_GC_SLOTS, S_GC_CARDS, S_GC_PRIZES, S_GC_BUY, S_GC_COINS_FMT, S_GC_BUY_Q, S_GC_BET_FMT, S_GC_SPIN, S_GC_STOP, S_GC_FLIP, S_GC_WIN_FMT, S_GC_NOLUCK, S_GC_NEED_COINS, S_GC_ANY, S_GC_PAYS_FMT, S_GC_LEFT_FMT, S_GC_PRIZE_Q, S_GC_PRICE_FMT, S_GC_NO_ROOM, S_GC_OWNED, S_GC_GOT_FMT, S_GC_COIN_CAP, S_GC_GUESS,
  // Surfing Pikachu (v3.54)
  S_SURF_SCORE_FMT, S_SURF_BEST_FMT, S_SURF_TIME_FMT, S_SURF_TILT, S_SURF_OHNO, S_SURF_GOAL, S_SURF_NEWBEST,
  // The hidden SETTINGS (v3.59). S_SET_FIRST + bit, in settings.h's bit order:
  // APPEND-ONLY, as the bits are.
  S_CODE_SETTINGS, S_SET_ON, S_SET_OFF, S_RULES_MARK,
  S_SET_FIRST, S_SET_1, S_SET_2, S_SET_3, S_SET_4, S_SET_5, S_SET_6, S_SET_7, S_SET_8, S_SET_9, S_SET_10, S_SET_11, S_SET_LAST,
  // The hidden CHEATS (v3.60). S_CHEAT_FIRST + CheatId: APPEND-ONLY.
  S_CODE_CHEATS, S_CHEAT_USE, S_CHEATS_MARK, S_CHEATED,
  S_CHEAT_FIRST, S_CHEAT_1, S_CHEAT_2, S_CHEAT_3, S_CHEAT_4, S_CHEAT_5, S_CHEAT_6, S_CHEAT_7, S_CHEAT_8, S_CHEAT_9, S_CHEAT_10, S_CHEAT_LAST,
  // Bill's PC screens (v3.62) and the Pokedex's OWNED, GO TO # and filter (v3.63)
  S_PC_TITLE, S_PC_BTN, S_PC_MORE_FMT, S_PC_SEVENTH, S_OWNED_FMT, S_GOTO, S_MISSING_ONLY, S_SHOW_ALL,
  // end-of-turn damage, narrated (v3.64): the bar drains with its own line
  S_BTL_HURT_PSN, S_BTL_HURT_BRN,
  S_SURF_PLUS_FMT, S_SURF_TRICKS_FMT, S_GC_PICK_MON, S_GC_PICK_NUM,   // v3.67
  S_BTL_SKETCHED, S_BTL_FAILED,   // Sketch (v3.69)
  S_MAX_LEVEL,                    // the level bar at 100 (v3.72)
  S_SAFARI_BALL, S_SAFARI_BALL_FMT, S_SAFARI_BAIT, S_SAFARI_ROCK, S_SAFARI_THREW_BAIT, S_SAFARI_THREW_ROCK, S_SAFARI_EATING, S_SAFARI_ANGRY, S_SAFARI_WATCHING, S_SAFARI_OUT, S_SAFARI_LEFT_FMT,  // Safari encounters (v3.75)
  S_SAFARI_BLOCK, S_SAFARI_GO_NEAR, S_SAFARI_MUD, S_SAFARI_THREW_BLOCK, S_SAFARI_THREW_MUD, S_SAFARI_CURIOUS, S_SAFARI_CLOSER, S_SAFARI_NO_CLOSER,  // Hoenn's and Sinnoh's Safari (v3.76)
  S_SWARM,  // an area's swarm on its status line (v3.78)
  S_WIN_PRIZE_FMT,  // a trainer win's prize money (v3.79)
  S_EVO_ITEM_FMT,  // the card's line for a stone evolution (v3.81)
  S_EVO_EEVEE,  // Eevee's card line: stones, or bond and two days (v3.81)
  S_EVO_INTO_FMT, S_EVO_PICK,  // the evolution dialog names its target; Applin's and Charcadet's choice (v3.82)
  S_EVO_STEPS_FMT,  // Meltan's card line (v3.83)
  S_BTN_BACK,  // the Pokedex's BACK button (v3.88)
  S_MART_TAB_BUY, S_MART_TAB_SELL, S_MART_SELL_Q, S_MART_PAYS_FMT, S_MART_NOTHING,  // selling (v3.90)
  S_VIT_PICK, S_VIT_LIVE,  // a vitamin's target, on the party ring (v3.91)
  S_PPUP_PICK,  // PP UP's move (v3.92)
  STR_COUNT
};

const char *T(StrId id);       // texto en el idioma activo
const char *moveName(uint8_t move);  // localized display name; move ID stays stable
const char *medalName(int i);  // banner de medalla (MED_COUNT)
const char *medalLabel(int i); // etiqueta corta de medalla
const char *medalDesc(int i);  // descripcion larga de medalla

void loadLang();             // lee el idioma de NVS (llamar en setup)
void setLang(Lang l);        // cambia y persiste el idioma
