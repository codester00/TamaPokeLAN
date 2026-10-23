#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "dex.h"
#include "trainers.h"   // GYM_REGIONS sizes the badge masks
#include "party.h"

// 1 tick = 1 minuto de juego. Baja este valor para probar mas rapido
// (p. ej. 5000UL = las estadisticas caen 12x mas rapido).
#define PET_TICK_MS 60000UL
// Minutos de juego por nivel. Con 60, CHARMANDER evoluciona a las ~16 h
// de juego con cuidado perfecto. Baja a 1 para ver evoluciones al momento.
#define MINUTES_PER_LEVEL 60
#define MAX_LEVEL 100              // reached at 4d 3h; see level()
#define EAT_ANIM_MS 2500UL
#define HEART_MS 1500UL
#define EVOLVE_ANIM_MS 5200UL              // animacion de evolucion (mas larga = mas epica)
#define CEREMONY_MS 10000UL                // duracion de la despedida en pantalla
#define FAREWELL_AGE_MIN (3UL * 24 * 60)   // se despide a los 3 dias de juego (en forma final)
// The legendary safety net (v3.56). After this many FAREWELL eggs in a row
// without a legendary, the next farewell egg is one -- as long as the egg's
// region still has an unregistered legendary that can hatch. SECRET, by the
// owner's call, like the 8-hour window: nothing on screen says it exists.
// Sized by simulation so a consistent player completes the egg-only
// legendaries in about two years (README, Game manual).
#define LEGEND_SAFETY_EGGS 10
// The farming lockout (v3.56). An egg after an early retire or a release is a
// legendary ATTEMPT at the plain 3 %. The first LEGEND_FREE_ATTEMPTS since the
// last farewell keep it -- an honest player's mistakes cost nothing -- and every
// one after that rolls legendary at 0 until a farewell clears the count. Like a
// password lockout: it stops the hatch-and-dump loop (~1,240 eggs, ~8 hours of
// tapping for every egg-only legendary) without touching a farewell egg.
#define LEGEND_FREE_ATTEMPTS 1
// Retiring a creature BEFORE it has earned its farewell costs the NEXT one a
// day's worth of evolution. Derived from MINUTES_PER_LEVEL rather than written
// as 24, so it stays "a day" if the level rate is ever retuned.
#define EVO_PENALTY_LEVELS ((uint8_t)((24UL * 60) / MINUTES_PER_LEVEL))
#define RUNAWAY_TICKS 60                   // se escapa tras 1 h con TODO a cero
// How fast needs fall while AWAKE (v3.34), and the owner's rule that sets it:
// a child can walk away from a topped-up creature for 8 hours, screen on and
// NOT put to sleep, and come back to one that is hungry, tired and asking for
// attention -- but has not been penalised. Past 8 hours, neglect counts.
//
// So from 100, food, energy and joy each lose ~10 an hour and hygiene ~7.5
// (plus 4 an hour for each poop): about 20 left at 8 h. Inside the window the
// needs stop at GRACE_FLOOR and no care mistake is recorded; after it they fall
// to 0, mistakes count again (each delays evolution a level), and an hour at
// total zero makes the runaway possible -- around 11 h untended.
//
// "Untended" is AWAKE minutes since the creature was last FULLY cared for:
// every one of food, energy, joy and hygiene at CARE_FULL_PCT or more (the
// owner's rule, v3.34). A single snack does not restart the window and does
// not raise the floor back -- only full care does (Pet::graceCheck(), the one
// place that decides). Sleep and time with the board off do not count:
// sleep has its own gentler floors, and switching off has always been gentle.
// Before v3.34 food fell 2/min and the rest 1/min -- empty in under an hour,
// which is why the creature effectively had to be kept asleep.
#define GRACE_MIN (8UL * 60)
#define GRACE_FLOOR 15                     // the same 15 the board-off catch-up uses
#define CARE_FULL_PCT 90                   // every need at least this = fully cared for
#define BOND_DAILY_MAX 20                  // bond any one day can add; it is not farmable
#define DRAIN_FOOD_MIN 6                   // -1 every 6 min awake: 10/h
#define DRAIN_ENERGY_MIN 6                 // 10/h (twice that when overweight)
#define DRAIN_JOY_MIN 6                    // 10/h
#define DRAIN_HYG_MIN 8                    // 7.5/h
#define DRAIN_POOP_MIN 15                  // each poop on the floor: another 4/h
#define DRAIN_SAD_MIN 12                   // hungry (<30) or dirty (<30): joy 5/h more, each
#define POOP_CHANCE_PCT 1                  // per awake minute while fed (>40): ~one every 100 min
#define SLEEP_DRAIN_MIN 18                 // asleep: food and joy 3.3/h -- gentler than awake
#define SLEEP_HYG_MIN 24                   // asleep: hygiene 2.5/h
// Petting wears off (the owner's design): each tap gives the next of these,
// ending at 0, until a joy/bond minigame (playJoyGame) makes it fresh again.
// Bond fades with it: +1 per tap while the joy is still above 0.
#define PET_JOY_STEPS { 10, 8, 6, 4, 2, 0 }
#define PET_JOY_STEP_COUNT 6
#define JOYGAME_JOY_MAX 30                 // a good round of a joy game
#define JOYGAME_BOND_MAX 3
#define TRAIN_SESSION_MAX 18               // one session's training points, however split
#define TR_STR 0x01
#define TR_DEF 0x02
#define TR_SPE 0x04
// How long a FAILED checkpoint write waits before the idle-flush path in
// loop() is allowed to retry it. See Pet::retryDue()'s comment -- without
// this, a failing NVS write retried on every single loop iteration while the
// screen was dimmed or the pet was asleep.
#define SAVE_RETRY_COOLDOWN_MS 5000UL
// Night, by the RTC: midnight to 06:00. Auto-sleep needs BOTH: the screen off
// AND this window.
// The screen alone would pause the game every time you put the device in a
// pocket -- the creature is supposed to get hungry during the day. The hour
// alone would send it to bed while you were playing. Set the clock in SETTINGS
// or over the console with RTCSET; an unset board reads months out and simply
// never auto-sleeps, which fails in the safe direction (it still drains, and
// the light button still works by hand).
#define NIGHT_START 0
#define NIGHT_END 6
// A NAP (v3.34, the owner's rule): put to bed by hand between NAP_START and
// NAP_END, the creature wakes by itself once fully rested, so a child can
// send a tired creature to bed and go to school. If it was fed, bathed and
// happy, waking fully cared for starts a fresh 8-hour window; if it was not,
// the nap neither restarts nor ends the window -- whatever was left of it is
// still left, and nothing more.
// Bed by hand after NAP_END is bedtime and lasts until somebody wakes it, and
// with no clock there is no daytime, so it never naps. Pet::napWake().
#define NAP_START 6
#define NAP_END 18
enum : uint8_t { SLEEP_NONE = 0, SLEEP_AUTO, SLEEP_PLAYER, SLEEP_NAP };
#define DEF_TRAIN_TICKS 60                 // minutos de bienestar por +1 de DEF
// Weight. A creature starts at a natural weight rather than 0.
//   - WEIGHT_MIN is absolute: nothing takes weight below 1, because a creature
//     weighing nothing does not exist. Every loss goes through loseWeight().
//   - While the board was OFF, the catch-up in syncClock() burns no lower than
//     WEIGHT_OFFLINE_FLOOR -- the same 15 its need floors use, so time away is
//     gentler than time spent on a running board.
//   - A running board's passive burn (tick()) and exercise reach WEIGHT_MIN.
#define WEIGHT_START 25                    // fitness 75: exactly the Pokedex weight
#define WEIGHT_MIN 1
#define WEIGHT_OFFLINE_FLOOR 15
#define WEIGHT_BURN_AWAKE_MIN 9            // -1 every 9 min awake
#define WEIGHT_BURN_ASLEEP_MIN 27          // -1 every 27 min asleep (3x slower)
#define WEIGHT_FIT_MAX 20                  // the "fit" medal: at or under this (fitness 80+)
// The player never sees `weight`. They see FITNESS, which is the same meter
// read the other way up (100 - weight), and a weight in kg: the species'
// Pokedex weight scaled by 1% per point, exactly the Pokedex figure at
// WEIGHT_START. So fitness 99 is 24% lighter than the Pokedex and fitness 0
// is 75% heavier. Inverting at display time keeps every rule above unchanged.

// ceremonias de fin de ciclo
enum : uint8_t { CER_NONE = 0, CER_FAREWELL, CER_RUNAWAY, CER_RELEASE };

enum PetMood : uint8_t { MOOD_HAPPY, MOOD_SAD, MOOD_EATING, MOOD_SLEEPING };

// medallas del individuo (bitmask)
enum : uint16_t {
  MED_LV10 = 1 << 0, MED_LV25 = 1 << 1, MED_LV50 = 1 << 2,
  MED_BERRY = 1 << 3, MED_STREAK7 = 1 << 4, MED_BOND = 1 << 5,
  MED_FINAL = 1 << 6, MED_FIT = 1 << 7,
};
#define MED_COUNT 8

uint8_t moveUnlockLevel(int16_t dex, uint8_t idx);
// The same question for one of the PLAYER's creatures (v3.51): a TM move is
// learnable only once the player owns that TM, and then at any level. Level-up
// moves keep their real gates. The move picker and the banked-creature editor
// ask THIS; opponents keep moveUnlockLevel(). 255 = not learnable.
class Pet;
uint8_t moveUnlockLevelFor(const Pet &owner, int16_t dex, uint8_t idx);

// ---------------------------------------------------------------------------
// Which regions have their sprite pack on the microSD.
//
// One bit per region and THE DEFAULT IS ALL SET, which is doing two jobs: a
// board with no SD at all keeps today's behaviour rather than presenting an
// empty game, and pet.cpp stays free of any SD dependency. That second part is
// not tidiness -- pet.cpp is compiled into all 33 test binaries and none of
// them link sdmon.cpp, so a direct call would mean stubbing it 33 times. The
// firmware narrows the mask ONCE, after the card mounts. Same shape as
// link.cpp taking a transport pointer instead of calling ESP-NOW across a
// layer, and for the same reason: the tests can drive it.
//
// uint16_t, not uint8_t. REGION_COUNT is 5 today and this project has been
// bitten five separate times by a width chosen for the current size of the dex.
extern uint16_t gRegionArt;
static_assert(REGION_COUNT <= 16, "gRegionArt needs a bit per region");

// Is this region playable? REGION_ALL is available while ANY real region is --
// it is the mixed pool and must not vanish because one pack is missing.
bool regionAvailable(uint8_t r);

// Which region a dex number belongs to, or REGION_ALL if somehow none. The
// regions tile the dex exactly once (dexdata_test pins that), so this is a
// lookup rather than a judgement.
uint8_t regionOfDex(int16_t d);

// A drawable start/middle/end species for checking whether a region pack is
// present. Returns 0 for REGION_ALL or invalid input.
int16_t regionArtProbe(uint8_t region, uint8_t slot);

// The next region the player can actually choose, skipping any whose sprite
// pack is missing. The egg pill cycles with this rather than (region + 1) %
// REGION_COUNT, which would land on a locked region and silently do nothing.
uint8_t nextAvailableRegion(uint8_t from);

// Can this species be put on the panel at all: it has art somewhere, and the
// pack for its region is on the card. THE single answer, asked by the egg pool
// and by every evolution.
bool speciesShowable(int16_t d);

// A remembered LAN opponent. Keyed by the peer's MAC (see recordRivalResult()
// in pet.cpp and linkNowPeerMac() in linknow.h), never by name -- a player can
// rename themselves between fights without the score against them resetting.
// An all-zero mac means "unused slot": a real ESP32 MAC is never literally
// zero, so no separate flag is needed to tell an empty slot from a used one.
#define RIVAL_CAP 10
struct RivalRecord {
  uint8_t mac[6] = { 0 };
  char name[12] = "";     // matches LINK_NAME_LEN without pet.h depending on link.h
  uint16_t wins = 0;
  uint16_t losses = 0;
};

// Room in the save for the legendaries with a home in Explore (WILD_LEGENDS):
// one bit each, met and caught. 16 bytes = 128 legendaries; wild.cpp fails the
// build before WILD_LEGENDS outgrows it.
#define LEGEND_BYTES 16
// Owned TMs (v3.51): one bit per MOVE INDEX, so every index a uint8_t can hold
// has a bit. Keyed by move, never by TM number: the TM list is regenerated,
// the move indices are append-only (CLAUDE.md trap 2).
#define TM_BYTES 32
// The Game Corner's coin case. The games' own ceiling.
#define COINS_CAP 9999

class Pet {
public:
  // Estadisticas 0..100
  uint8_t fullness = 80;  // comida
  uint8_t joy = 80;       // felicidad
  uint8_t energy = 80;    // energia
  uint8_t hygiene = 100;  // limpieza
  uint8_t poops = 0;      // cacas en pantalla (max 3)
  uint8_t weight = WEIGHT_START;  // 1-100: candy fattens, exercise burns (see WEIGHT_*)
  // IV (valores individuales 0-31, como en los juegos de 3a gen en adelante):
  // se tiran al eclosionar y no cambian nunca. Aportan IV x nivel / 100 al
  // stat Y ademas fijan el tope de entrenamiento (trMaxFor): un individuo
  // mediocre no solo empieza peor, es que no puede llegar tan lejos.
  uint8_t ivAtk = 16, ivDef = 16, ivSpe = 16, ivHp = 16;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  bool berryKnown = false;  // ya descubrio su baya favorita
  bool shiny = false;       // variante de color rara (se sortea en el huevo)
  uint32_t ageMinutes = 0;
  int16_t speciesId = -1;      // numero de Pokedex (1..DEX_COUNT), -1 = huevo
  int16_t prevSpeciesId = -1;  // para la animacion de evolucion
  // The live creature's identity (party.h, CreatureUid). Minted when it hatches,
  // carried through every bank and swap, kept by evolution, cleared by newEgg():
  // an egg is not a creature yet.
  CreatureUid uid;
  // Received in a trade and not yet evolved by it: PMON_TRADE_EVO on the
  // stored record, carried across banking and swapping like the ID.
  bool tradeEvo = false;
  bool wasFullyCared = false;   // graceCheck() restarts the window on the rising edge
  uint16_t unattended = 0;      // awake minutes since last fully cared for; see GRACE_MIN
  bool fullyCared() const;      // every need at CARE_FULL_PCT: what restarts the window
  bool bondCappedToday() const { return bondToday >= BOND_DAILY_MAX; }   // why a game paid no bond
  uint8_t careMistakes = 0;   // descuidos: cada uno retrasa la evolucion 1 nivel
  bool sleeping = false;
  uint32_t lastSeenEpoch = 0;   // ultima hora RTC vista (para progresion offline)
  // The day number of that clock, 0 when it was never set: the streak's day,
  // and the day Galar's weather is drawn for (wildAreaWeather()).
  uint32_t today() const { return lastSeenEpoch ? lastSeenEpoch / 86400 : 0; }
  uint8_t ceremony = CER_NONE;  // despedida/escapada/liberacion en curso
  uint8_t lastEnd = CER_NONE;   // como acabo la anterior (afecta al huevo)
  // A finished ceremony hands the creature over here before newEgg() wipes the
  // live state. The UI drains it (endedKind back to CER_NONE) once the pet has
  // either taken a party slot or been let go. Only farewell and release fill
  // it; a runaway leaves endedKind at CER_NONE and the pet is simply gone.
  PartyMon endedMon;
  uint8_t endedKind = CER_NONE;
  // Pokedex bitmaps, one bit per species. Widening these is SAFE on an existing
  // save: getBytes() copies only what was stored, and the array is zeroed by its
  // initialiser, so a 19-byte blob from the Kanto-only build lands in the front
  // and bits 1-151 keep exactly their old meaning.
  uint8_t dexReg[(DEX_COUNT + 7) / 8] = { 0 };       // criados
  uint8_t dexShinyReg[(DEX_COUNT + 7) / 8] = { 0 };  // criados en version shiny
  // racha de cuidado diario (del jugador: persiste entre crianzas)
  uint16_t streak = 0, bestStreak = 0;
  uint32_t lastCareDay = 0;
  // vinculo (del bicho: sube lento con cuidado, se resetea al nacer otro)
  uint8_t bond = 0;
  char nick[12] = "";    // apodo (vacio = nombre de especie)
  // medallas: del individuo + contador acumulado entre todas las crianzas
  uint16_t medals = 0, totalMedals = 0;
  uint16_t newMedal = 0;   // recien conseguida(s), para celebrar
  uint16_t lastMilestone = 0;  // hito de racha ya celebrado
  uint16_t gameHi = 0;     // record del minijuego (del jugador)
  uint16_t strHi = 0;      // record de golpes al saco

  void begin();                 // carga estado de NVS (o crea el primer huevo)
  void update(uint32_t nowMs);  // llamar en cada loop()

  // Acciones (botones tactiles)
  void feed();              // baya roja (compatibilidad)
  void feedBerry(uint8_t color);  // 0 roja, 1 azul, 2 verde
  void feedCandy();
  bool lovesBerry(uint8_t color) const {
    return !isEgg() && (speciesId % 3) == color;  // gusto oculto por especie
  }
  // The ball game: happiness AND defence training. Returns the DEF gained.
  uint8_t playResult(uint8_t score);
  uint8_t fitness() const { return (uint8_t)(100 - weight); }
  // What the card shows: the Pokedex weight (DEX_WEIGHT_HG) scaled by the
  // meter, in hectograms. Never 0 -- even a 0.1 kg Gastly at fitness 99 weighs
  // something. 0 for an egg, which has no species yet.
  uint32_t weightHg() const;
  // The one way weight goes down: never below WEIGHT_MIN, nor below `floor`.
  void loseWeight(uint8_t n, uint8_t floor = WEIGHT_MIN) {
    if (floor < WEIGHT_MIN) floor = WEIGHT_MIN;
    weight = (weight > floor + n) ? (uint8_t)(weight - n) : (weight > floor ? floor : weight);
  }
  uint8_t trainStrength(uint16_t hits);  // saco de entrenamiento (entrena FUE)
  // Reaction test: its own trainer, so the ball game can go back to being purely
  // about joy instead of doubling as a stat grind.
  uint8_t trainSpeed(uint16_t hits);
  // What beating a gym leader is worth. Random WHICH stat, but only among the
  // ones with room left -- a random grant that landed on an already-capped stat
  // would silently evaporate, which reads as a bug rather than as luck. Writes
  // the stat into `which` (0 ATK, 1 DEF, 2 SPE) and returns what was actually
  // gained; 0 means every stat is at its ceiling. Costs nothing: the fight
  // already spent the energy, and that is what rate-limits rematching.
  uint8_t rewardTraining(uint8_t amount, uint8_t &which);
  uint16_t spdHi = 0;    // best reaction-test score

  // stats de combate: base real de gen 1 + nivel + IV + entrenamiento
  uint16_t atkStat() const;
  uint16_t defStat() const;
  uint16_t speStat() const;
  uint16_t vitStat() const;  // vitalidad (bHp): no se entrena, solo IV y nivel
  // The physical/special split lives on the species (bSpA/bSpD), not the
  // individual: special attack reuses ivAtk/trAtk and special defence reuses
  // ivDef/trDef, so no extra IVs and no save migration. See fetch_pokeapi.py.
  uint16_t spaStat() const;
  uint16_t spdStat() const;

  // The four known moves (indices into MOVE_TBL; 0 = empty slot). Player-chosen
  // once the learn/forget prompt exists -- until then, and for saves made before
  // moves were stored at all, relearnFromLevel() fills them in.
  uint8_t moves[MOVE_SLOTS] = { 0, 0, 0, 0 };
  uint8_t moveCount() const;
  bool knowsMove(uint8_t mv) const;
  // The newest MOVE_SLOTS moves this species has learned by its current level,
  // newest last. Used on hatch, on a fresh save, and to backfill empty slots.
  void relearnFromLevel();
  // A WILD creature's moves (v3.55): four at random from everything its level
  // allows -- level-up moves at their level, TMs at the opponents' gate
  // (moveUnlockLevel), so one past 40 MAY know a TM rather than always
  // fielding its best four. At least one attack when the species has any, so
  // a wild fight can never be four status moves.
  void rollWildMoves();
  // The level at which `dex` may legally use learnset entry `i`. A level-up
  // move carries its own; a level-0 entry is a TM/tutor/egg move, which the
  // data gives no level at all, and those unlock together at TM_LEVEL.
  //
  // It is a free function, not a Pet method, because the move PICKER needs the
  // identical answer for a banked party member. Having its own gate is what let
  // a level 22 Charmeleon be offered FIRE BLAST.

  // Moves reachable at this level that are not already known, for the learn
  // prompt. Returns how many were written into out (at most max).
  uint8_t pendingLearnables(uint8_t *out, uint8_t max) const;

  // Player-wide, like the streak and the Pokedex: badges outlive the creature
  // that earned them, so newEgg() must never clear this.
  // A creature brought back out of the party or box. It is FROZEN: it does not
  // age, cannot evolve, and cannot be lost -- a companion rather than a
  // contender. The cost is that its level never rises again, so it stops
  // improving; ageMinutes is simply set to match its banked level, which keeps
  // level() working untouched rather than needing a second source of truth.
  bool frozen = false;
  void reviveFrom(const PartyMon &m);

  // The live creature as a storable record. THE single place a pet becomes a
  // PartyMon: the farewell, a swap and a wild capture all go through it, and
  // CLAUDE.md § "A rule enforced in one path but not its twin" is a list of what
  // happens when three callers each keep their own copy of that.
  PartyMon toPartyMon() const;
  // Make a stored creature the focused one -- the one that ages, levels,
  // evolves and is cared for. Unlike reviveFrom() it is NOT frozen: it picks up
  // its own care state and carries on. A record from before care state existed
  // (stateVersion 0) starts from its banked level with fresh stats, which is
  // the only honest reading of a save that never held them.
  //
  // It does NOT bank whatever is currently live -- the caller owns that, since
  // only it knows whether there is a slot free. Calling this without storing
  // the previous pet first destroys it.
  //
  // Returns whether the resulting save actually landed. This pet's fields are
  // updated in RAM regardless -- the screen shows the new creature either way,
  // which is correct: it does not roll back on a failed write, the same as
  // every other save() caller. What the return value is FOR is the caller's
  // OTHER half of the handover. focusSwap() found this the hard way: it used
  // to bank the outgoing pet into the party slot unconditionally and call this
  // second, so a failed checkpoint write here left the party blob already
  // showing the outgoing pet banked while the on-disk checkpoint still ALSO
  // showed that same pet as live (this call's write never landed) -- the same
  // creature in two places the instant a reboot reloaded the stale checkpoint.
  // A caller with a second write to make on the strength of this one succeeding
  // must check the return value before making it.
  bool switchTo(const PartyMon &m);

  // The player's own name, alongside the badges and the streak: it belongs to
  // whoever is playing, not to the creature, so newEgg() must never clear it.
  char trainerName[12] = "";
  void renameTrainer(const char *n) {
    strncpy(trainerName, n, sizeof(trainerName) - 1);
    trainerName[sizeof(trainerName) - 1] = 0;
    save();
  }

  // The last RIVAL_CAP LAN opponents, most-recently-played first. Its own NVS
  // key, purely additive, same reasoning as the box: this does not need the
  // checkpoint's torn-write protection -- losing the very latest update to a
  // win/loss counter on a power cut is a cosmetic loss, not a mixed creature.
  RivalRecord rivals[RIVAL_CAP];
  const RivalRecord *findRival(const uint8_t mac[6]) const;
  void recordRivalResult(const uint8_t mac[6], const char *name, bool won);

  // Which generation eggs come from. Player-wide, like the badges: it outlives
  // every creature, so newEgg() must never reset it.
  uint8_t region = REGION_ALL;
  // The species this egg would be in each region. Filled in as the player
  // looks, cleared by newEgg(). It exists so that switching region and back
  // shows the SAME creature rather than rolling a fresh one -- without it,
  // toggling would be a re-roll button.
  int16_t eggByRegion[REGION_COUNT] = { 0 };
  void setRegion(uint8_t r);
  const char *regionName() const { return REGIONS[region % REGION_COUNT].name; }
  // How much of one region's dex is filled in, for the Pokedex header.
  uint16_t registeredCountIn(uint16_t lo, uint16_t hi) const {
    uint16_t n = 0;
    for (uint16_t d = lo; d <= hi && d <= DEX_COUNT; d++) if (isRegistered(d)) n++;
    return n;
  }

  uint8_t avatar = 0;       // which player sprite, 0..3
  // Kanto's ladder, under the keys it has always used. Johto and Hoenn live in
  // a SEPARATE array under new keys rather than widening these -- purely
  // additive, so an existing save cannot be misread, exactly the reasoning that
  // put the box under its own key instead of growing the party blob.
  uint16_t badges = 0;      // bit n = trainer n beaten on easy
  uint16_t badgesHard = 0;  // ... and on hard
  uint16_t badgesX[GYM_REGIONS - 1] = { 0 };
  uint16_t badgesHardX[GYM_REGIONS - 1] = { 0 };

  // The Poke Mart. Player-wide, like the badges above: it outlives every
  // creature, so newEgg() must never touch it. 1 pedometer step = $1, capped at
  // the real games' own money ceiling (money is a 3-byte field there, max
  // 999,999) rather than an invented round number.
  static constexpr uint32_t WALLET_CAP = 999999;
  uint32_t wallet = 0;      // spendable, clamped at WALLET_CAP
  uint32_t stepsTotal = 0;  // lifetime steps, uncapped and never spent -- display only
  // Credits n steps. Called every couple of seconds while walking, so like the
  // ordinary stat drains it only marks the periodic save pending rather than
  // writing immediately -- an immediate save() here would turn a walk into a
  // flash write every couple of seconds, the exact write-storm shape
  // CLAUDE.md's NVS section warns about.
  void addSteps(uint32_t n) {
    if (!n) return;
    stepsTotal += n;
    uint32_t room = WALLET_CAP - wallet;
    wallet += (n < room) ? n : room;
    pendingSave = true;
  }
  // A Mart purchase: rare and player-initiated, so unlike addSteps() this saves
  // immediately -- the same as winBadge()/renameTrainer(). Returns false (and
  // spends nothing) if the wallet is short.
  bool spendWallet(uint32_t amount) {
    if (amount > wallet) return false;
    wallet -= amount;
    save();
    return true;
  }

  // Creature IDs. The serial counter is player-wide -- it numbers every
  // creature this save has ever minted, so newEgg() must never touch it -- and
  // rides in the player checkpoint. mintUid() is the ONE place a new ID comes
  // from: a hatch, a wild catch, a debug PARTY fill and the boot backfill all
  // ask it, so they cannot disagree about what an ID is made of.
  uint32_t uidSerial = 0;   // the last serial handed out; the next is +1
  CreatureUid mintUid();

  // Explore's legendaries (wild.h): player-wide, so newEgg() never touches
  // them. Bit i is WILD_LEGENDS[i], an APPEND-ONLY index -- the same rule as
  // move indices. MET is set when the guaranteed first meeting happens, CAUGHT
  // when one is caught; caught ends it for good.
  uint8_t legendMetBits[LEGEND_BYTES] = { 0 };
  uint8_t legendCaughtBits[LEGEND_BYTES] = { 0 };
  bool legendMet(uint8_t i) const {
    return i < 8 * LEGEND_BYTES && (legendMetBits[i >> 3] >> (i & 7) & 1);
  }
  bool legendCaught(uint8_t i) const {
    return i < 8 * LEGEND_BYTES && (legendCaughtBits[i >> 3] >> (i & 7) & 1);
  }
  void setLegendMet(uint8_t i) { if (i < 8 * LEGEND_BYTES) legendMetBits[i >> 3] |= (uint8_t)(1 << (i & 7)); }
  void setLegendCaught(uint8_t i) {
    if (i >= 8 * LEGEND_BYTES) return;
    legendCaughtBits[i >> 3] |= (uint8_t)(1 << (i & 7));
    setLegendMet(i);   // caught implies met, whatever path got there
  }
  // TMs the player owns (v3.51), player-wide like the badges: reusable, the
  // Gen 5 rule -- owning one lets ANY of the player's creatures learn it, at
  // any level. Opponents never ask this; they keep TM_LEVEL (pet.cpp).
  uint8_t tmOwned[TM_BYTES] = { 0 };
  bool hasTm(uint8_t move) const { return (tmOwned[move >> 3] >> (move & 7)) & 1; }
  // True if it was new. Saves itself, like every other player-wide grant.
  bool giveTm(uint8_t move);
  uint16_t tmOwnedCount() const;

  // The Game Corner (v3.53): coins, and Surfing Pikachu's best score (v3.54).
  // In the player record from v5 on, beside the TMs.
  uint16_t coins = 0;
  uint16_t surfHi = 0;
  // Farewell eggs in a row without a legendary (LEGEND_SAFETY_EGGS). Player
  // record v6 tail and legacy key "ldry". Moved only by newEgg().
  uint8_t legendDry = 0;
  // Early-retire / release eggs since the last farewell (LEGEND_FREE_ATTEMPTS).
  // Player record v7 tail and legacy key "lstk". Moved only by newEgg().
  uint8_t legendAttempts = 0;
  // Would an egg laid now, after `ending`, have its legendary chance locked out?
  // THE one answer pickEggSpecies() asks: the attempt it would be is past the
  // free ones. A farewell (or a runaway, already common-only) is never locked.
  bool legendLockedFor(uint8_t ending) const {
    return ending == CER_RELEASE && legendAttempts >= LEGEND_FREE_ATTEMPTS;
  }
  // Is the safety net in play for an egg laid now: past the 25-species gate the
  // legendary tier already has, and the egg's region still holding an
  // unregistered legendary that can hatch. THE one answer pickEggSpecies() and
  // newEgg() both ask.
  bool legendSafetyActive() const;
  void addCoins(uint32_t n) { uint32_t c = (uint32_t)coins + n; coins = c > COINS_CAP ? COINS_CAP : (uint16_t)c; pendingSave = true; }
  bool spendCoins(uint16_t n) { if (coins < n) return false; coins -= n; pendingSave = true; return true; }

  // A new random salt for this boot -- see CreatureUid. begin() calls it; a
  // test that simulates a reboot without re-running begin() can call it too.
  static void uidNewBoot();
  static uint16_t uidSalt();

  uint16_t badgeMask(uint8_t rg, bool hard) const {
    if (rg == 0) return hard ? badgesHard : badges;
    if (rg >= GYM_REGIONS) return 0;
    return hard ? badgesHardX[rg - 1] : badgesX[rg - 1];
  }
  bool hasBadge(uint8_t rg, uint8_t i, bool hard) const {
    return (badgeMask(rg, hard) >> i) & 1;
  }
  void winBadge(uint8_t rg, uint8_t i, bool hard) {
    if (rg >= GYM_REGIONS) return;
    uint16_t bit = (uint16_t)1 << i;
    if (rg == 0) { if (hard) badgesHard |= bit; else badges |= bit; }
    else if (hard) badgesHardX[rg - 1] |= bit;
    else badgesX[rg - 1] |= bit;
    save();
  }
  uint8_t badgeCountIn(uint8_t rg, bool hard) const {
    uint16_t v = badgeMask(rg, hard);
    uint8_t n = 0;
    while (v) { n += v & 1; v >>= 1; }
    return n;
  }
  // Every region's badges together, for the player card's running total.
  uint8_t badgeCount(bool hard) const {
    uint8_t n = 0;
    for (uint8_t r = 0; r < GYM_REGIONS; r++) n += badgeCountIn(r, hard);
    return n;
  }

  // Level-up learning. lastLearnLevel is the highest level whose gates have
  // been handled, so a move declined once is not offered forever, and the
  // offline catch-up -- which can cross a dozen levels in one go -- queues its
  // offers instead of firing a dozen dialogs at boot.
  uint8_t lastLearnLevel = 0;
  uint8_t learnQueue[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  uint8_t learnQCount = 0;
  void checkLearnGates();
  bool hasLearnOffer() const { return learnQCount > 0; }
  uint8_t learnOffer() const { return learnQCount ? learnQueue[0] : 0; }
  void acceptLearn(uint8_t slot);   // put the pending move into slot 0..3
  void declineLearn();
  // tope de entrenamiento que permite un IV: 77 (IV 8) .. 100 (IV 31)
  static uint8_t trMaxFor(uint8_t iv) { return 70 + (30 * (uint16_t)iv) / 31; }
  uint8_t trMaxAtk() const { return trMaxFor(ivAtk); }
  uint8_t trMaxDef() const { return trMaxFor(ivDef); }
  uint8_t trMaxSpe() const { return trMaxFor(ivSpe); }
  void play();
  void toggleLight();
  bool isNightHour() const;
  bool isNapHour() const;     // a bed by hand now is a nap (NAP_START..NAP_END)
  void setScreenOff(bool off);  // the sketch reports the PWR button
  bool screenIsOff = false;
  void dbgTick() { tick(); }
  void dbgResetBondToday() { bondToday = 0; }   // tests: a fresh daily bond allowance   // tests drive minutes directly; tick() is private
  // Keep the legacy key in step too, so tests also cover migration saves.
  void dbgSetSeen(uint32_t e) { lastSeenEpoch = e; prefs.putUInt("seen", e); }
  uint8_t sleepAuto = SLEEP_NONE;   // who decided the current sleep state  // dormir / despertar
  void clean();
  void caress();  // tocar al bicho: joy and bond that WEAR OFF -- see PET_JOY_STEPS
  // A joy/bond minigame: joy and bond scaled by the score, and petting is fresh
  // again. Returns the joy gained.
  uint8_t playJoyGame(uint8_t score);
  // A training game's points split EVENLY across the stats in `mask` (TR_STR /
  // TR_DEF / TR_SPE): the same total a single-stat game pays, divided. Each stat
  // stops at its IV-bound ceiling. Returns what was really gained, all stats.
  uint8_t trainSplit(uint8_t mask, uint8_t points);
  uint8_t petStep = 0;            // how worn out petting is: index into PET_JOY_STEPS
  void eggTap();  // tocar el huevo: 3 toques y eclosiona
  void newEgg();   // empezar de cero con un inicial aleatorio
  void release();  // soltar (pulsacion larga + confirmar)
  void syncClock(uint32_t nowEpoch);  // aplica el tiempo transcurrido apagado
  void setClock(uint32_t nowEpoch);   // fija la hora sin aplicar progresion
  void startFarewell();  // tambien usable desde la consola serie (BYE)
  // Retire on demand. Before the farewell is earned this is the SAME ceremony
  // -- the creature is banked exactly as it would be -- but it hands the next
  // creature EVO_PENALTY_LEVELS on every evolution threshold. Retiring one that
  // has already earned its farewell costs nothing: it is then just the button.
  void startRetire();
  bool canRetireNow() const;
  bool retireIsFree() const { return canFarewellNow(); }
  // Was the retire NOW IN PROGRESS an early one? THE single answer, asked by
  // snapshotForParty() and by the tests. Distinct from retireIsFree(), which asks
  // whether a retire started right now WOULD be free: this one remembers what the
  // retire that is already running actually was, and outlives the moment it began.
  bool retireIsEarly() const { return retirePending; }
  uint8_t evoPenalty() const { return evoPen; }
  void startRunaway();   // tambien usable desde la consola serie (RUN)

  bool isEgg() const { return speciesId < 0; }
  uint8_t eggCracks() const { return eggTaps; }
  bool eating() const { return millis() < eatUntil; }
  bool showHeart() const { return millis() < heartUntil; }
  bool evolving() const { return millis() < evolveUntil; }
  float evolveT() const {     // progreso de la animacion de evolucion 0..1
    uint32_t n = millis();
    uint32_t left = evolveUntil > n ? evolveUntil - n : 0;
    return 1.0f - (float)left / (float)EVOLVE_ANIM_MS;
  }
  bool canEvolveNow() const;  // condiciones de evolucion cumplidas (lista)
  // THE evolution rule, asked by canEvolveNow(), evolve() AND the card. The card
  // used to restate "level >= evolveLevel + careMistakes + evoPenalty" itself,
  // and pure trade evolution is exactly the change that would have left it
  // promising a level-40 Alakazam that never comes (CLAUDE.md § 1).
  enum EvoStatus : uint8_t {
    EVO_FINAL,        // nothing to evolve into
    EVO_NEEDS_TRADE,  // evolves only by being traded (Kadabra, Haunter...)
    EVO_LEVELS,       // levelsLeft more levels, then care decides
    EVO_BLOCKED,      // due, but a need is under 40
    EVO_READY,        // due and cared for
  };
  struct EvoInfo {
    uint8_t status;
    int16_t target;       // what it would become (0 when final)
    uint16_t levelsLeft;  // EVO_LEVELS only
    bool byTrade;         // this evolution is the trade one
  };
  EvoInfo evoInfo() const;
  // What `dex` evolves into by trade (0 if it does not), given what it was
  // traded FOR -- Karrablast and Shelmet only for each other. CLAMPERL has two
  // trade targets; the creature's own ID picks one, so declining and re-asking
  // can never re-roll it.
  static int16_t tradeTarget(int16_t dex, const CreatureUid &u, int16_t partnerDex = 0);
  // Does this species evolve ONLY by trade (its table target is the trade one)?
  static bool onlyByTrade(int16_t dex);
  // Called on a creature arriving by trade, with the species given for it.
  // Marks it to evolve the next time it is the live pet, if it is one that
  // trading evolves. Deliberately does not evolve it here: making it the live
  // pet first registers the unevolved form in the Pokedex, as the owner asked.
  static void markTraded(PartyMon &in, int16_t gaveDex);

  // The Linking Cord: trade evolution without a trade. One cord, one
  // evolution, used up -- and NOT used up when it would do nothing. It works
  // on the live pet only: a mark only ever matters when the creature is live,
  // so a banked one is raised first. KARRABLAST and SHELMET still need each
  // other: the cord asks for the partner, and the pair is marked through the
  // same markTraded() a real trade uses, so the cord and a trade can never
  // disagree about who evolves.
  enum CordUse : uint8_t {
    CORD_NO_EFFECT,      // "it won't have any effect": nothing marked, cord kept
    CORD_SOLO,           // marks the live pet
    CORD_NEEDS_PARTNER,  // ask which party creature to link with
  };
  CordUse cordUse() const;
  bool applyCord();                         // CORD_SOLO; true if it took
  bool applyCordPair(PartyMon &partner);    // CORD_NEEDS_PARTNER; true if they pair
  void evolve();              // dispara la transformacion (la llama un toque del usuario)
  bool canFarewellNow() const;  // forma final + 7 dias: lista para despedirse (boton)
  // Total neglect RIGHT NOW. THE single answer: tick() counts against it and
  // canRunawayNow() re-checks it, because neglectTicks is frozen (neither
  // counted nor cleared) while asleep and so can outlive the state that
  // earned it -- see sleep_test.
  bool inTotalNeglect() const { return !fullness && !joy && !energy && !hygiene; }
  bool canRunawayNow() const;   // abandono total 1h: lista para escaparse (boton triste)
  // Is there a form this creature could evolve into that this player could be
  // SHOWN? Public so a test can drive it without reconstructing canEvolveNow().
  bool evoTargetShowable() const;
  // el usuario decide en un dialogo; "mantener/quedaros" pospone y re-ofrece luego
  bool wantEvolveButton() const { return canEvolveNow() && level() > evoDeclinedLv; }
  bool wantFarewellButton() const { return canFarewellNow() && ageMinutes >= farDeclinedAge; }
  void declineEvolve() { evoDeclinedLv = level(); }              // re-ofrece al subir de nivel
  void declineFarewell() { farDeclinedAge = ageMinutes + 1440; } // re-ofrece dentro de 1 dia
  // primera partida: el jugador elige inicial (Bulbasaur/Charmander/Squirtle)
  bool awaitingStarter() const { return starterPick; }
  void chooseStarter(int16_t dex) {
    eggTarget = dex;
    starterPick = false;
    // One-time default so trainerName is never "" going forward: renameTrainer()
    // can still change it later, and this only fires if nothing set it already.
    if (!trainerName[0]) {
      strncpy(trainerName, "TRAINER", sizeof(trainerName) - 1);
      trainerName[sizeof(trainerName) - 1] = 0;
    }
    save();
  }
  // Empties NVS; the caller restarts into a new game (serial command WIPE).
  // BOTH generation counters go back to zero with it, or the next save would
  // write a record numbered above the empty slots and the parity invariant in
  // ckptSlot() would no longer hold.
  //
  // It does NOT stop later saves. Keeping the in-RAM creature from being written
  // back over the wipe is the job of saveInhibited in the sketch, next to the
  // flush hooks that are the actual risk -- one guard, where the callers are.
  //
  // Returns whether NVS really was cleared. It used to ignore clear()'s result,
  // so a failed wipe restarted into the OLD save while reporting DONE -- and the
  // generation counters were zeroed anyway, so the next save landed in a slot
  // the surviving records outranked. A failure gets one close/reopen retry,
  // the cure for the stale handle behind the checkpoint failures (HANDOVER
  // 1b), and if that fails too nothing is touched.
  bool factoryReset() {
    bool ok = prefs.clear();
    if (!ok) {
      prefs.end();
      ok = prefs.begin("tamapoke", false) && prefs.clear();
    }
    if (!ok) return false;
    saveGeneration = 0;
    playerGeneration = 0;
    saveFailures = 0;
    pendingSave = false;
    return true;
  }
  // test / ABANDON: everything a real runaway needs, the 8 untended hours included
  void dbgRunawayReady() { fullness = joy = energy = hygiene = 0; neglectTicks = RUNAWAY_TICKS; unattended = GRACE_MIN; }
  // test: force what the egg holds and hatch it now (serial command EGG).
  // The legendary/shiny IV guarantees only fire inside hatch(), so without
  // this there is no way to exercise them from outside the class.
  void dbgHatchAs(int16_t dex, bool wantShiny) {
    if (dex < 1 || dex > DEX_COUNT) return;
    eggTarget = dex;
    eggShiny = wantShiny;
    starterPick = false;
    speciesId = -1;
    eggTaps = 0;
    hatch();
  }
  // Capped at 100, the series' own ceiling. The cap is not cosmetic: ageMinutes
  // is never clamped, so without it 1 + ageMinutes/60 overflows this uint8_t
  // past ~10.6 days -- and the RTC catches up to 2 weeks offline, which lands on
  // level 337 and wraps to 81. Reached at 4d 3h; farewell is merely *offered* at
  // 3 days (level 73), so declining it to raise a stronger battler is a real
  // choice rather than an accident.
  uint8_t level() const {
    uint32_t l = 1 + ageMinutes / MINUTES_PER_LEVEL;
    return l > MAX_LEVEL ? MAX_LEVEL : (uint8_t)l;
  }
  bool isRegistered(int16_t dex) const {
    return dex >= 1 && dex <= DEX_COUNT && (dexReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  bool isShinyRegistered(int16_t dex) const {
    return dex >= 1 && dex <= DEX_COUNT && (dexShinyReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  uint16_t registeredCount() const;
  bool lineHasUnregistered(int16_t base) const;
  uint8_t eggRarity() const;       // rareza del huevo actual (sin revelar especie)
  int16_t pickEggSpecies();        // publica para poder simular tiradas (EGGS)
  // What the waiting egg would hatch into. Hidden from the PLAYER, not from the
  // code: the serial console already simulates rolls, and the region tests have
  // to see which creature a switch landed on.
  int16_t eggPeek() const { return eggTarget; }
  int16_t rollInRegion(uint8_t r, uint8_t tier);
  // Which of EEVEE's branches this player can actually be handed right now.
  // THE single answer: evolve() picks from it and lineHasUnregistered() asks it
  // whether the line is finished, and those two having their own opinions is how
  // the branch stayed at 134-136 while five more Eeveelutions sat in the dex.
  //
  // Filtered by the sprite pack AND by whether the species has art at all --
  // evolving into something that can only draw as a dex number is the same fault
  // just removed from the gym rosters, and here it would be permanent. `out`
  // must hold EEVEE_EVO_COUNT; returns how many were written.
  uint8_t eeveeOptions(int16_t *out) const;
  uint8_t lowestStat() const { return min(min(fullness, joy), min(energy, hygiene)); }
  PetMood mood() const;
  // progreso de la ceremonia de despedida/escapada, 0..1 (para animarla)
  float ceremonyT() const {
    if (ceremony == CER_NONE) return 0.0f;
    uint32_t n = millis();
    uint32_t left = ceremonyUntil > n ? ceremonyUntil - n : 0;
    return 1.0f - (float)left / (float)CEREMONY_MS;
  }

  // racha / vinculo / medallas / nombre
  void rename(const char *name);
  bool hasMedal(uint16_t m) const { return medals & m; }
  bool showMedal() const { return millis() < medalUntil; }
  bool showMilestone() const { return millis() < milestoneUntil; }
  int careBonus() const;  // mejora del huevo por racha + vinculo

  // guardado periodico diferido: tick() marca pendiente y el loop lo vuelca
  // cuando la pantalla esta atenuada/apagada (la escritura a flash congela
  // ~1s ambos cores: asi no se ve ni corta el tactil)
  // The handover has been resolved: the creature took a party or box slot, or
  // the player let it go. Persisted straight away, because it now lives in the
  // pet checkpoint -- clearing it only in RAM would re-offer the same creature
  // on the next boot, which is the mirror of the bug that made it durable.
  void clearEnded() {
    if (endedKind == CER_NONE) return;
    endedKind = CER_NONE;
    endedMon = PartyMon();
    saveNow();
  }
  // A creature is waiting for a slot. Used in place of reaching for endedKind so
  // there is one answer, and so the durable copy cannot be set without being
  // written -- see CLAUDE.md § "A rule enforced in one path but not its twin".
  void setEnded(const PartyMon &m, uint8_t kind) {
    endedMon = m;
    endedKind = kind;
    saveNow();
  }
  bool savePending() const { return pendingSave; }
  // Whether ENOUGH time has passed since the last save ATTEMPT (success or
  // failure) to try again. A failed checkpoint write leaves pendingSave true
  // forever -- only a SUCCESS clears it -- and the idle-flush path in the
  // sketch's loop() checks savePending() on every single iteration while the
  // screen is dimmed or the pet is asleep. Without this, one failing NVS
  // write turned into hundreds of retries per second for as long as the
  // condition that broke it lasted: every one doing a real flash write (and
  // on this chip that stalls both cores for about a second while dimmed,
  // which is supposed to be invisible), every one printing its own log line,
  // and every one hammering a namespace that may already be the problem.
  // Found from a real capture: ~250 "save: pet checkpoint failed" lines in
  // under 200ms. This does not fix WHY a write failed -- only how often the
  // firmware is allowed to keep re-trying while it does.
  bool retryDue() const { return millis() - lastSaveAttempt >= SAVE_RETRY_COOLDOWN_MS; }
  // Is persistence actually WORKING? False means the creature on the panel is
  // not being written anywhere -- NVS would not open, or it is refusing writes
  // because it is full or failing. The firmware used to be unable to tell: the
  // only symptom was a line on a serial port no player has attached, so hours
  // of play could be lost with nothing on screen to suggest it. The sketch
  // draws a warning when this goes false, because a player who KNOWS saving is
  // broken can EXPORT, and one who does not cannot.
  //
  // Three consecutive failures rather than one: a single short write can be a
  // transient, three in a row is not coming back on its own.
  bool saveHealthy() const { return opened && saveFailures < 3; }
  void flushSave();
  // Writes NOW, whatever pendingSave says. flushSave() is `if (pendingSave)
  // save()`, so a console command that changed something the game had not
  // already marked dirty wrote nothing at all and quietly depended on the next
  // autosave to notice. Defined out of line: save() is private and declared
  // further down.
  void saveNow();

private:
  Preferences prefs;
  // Scratch pets used for opponents share the player's NVS namespace. Only a
  // Pet explicitly opened with begin() owns that persistent state.
  bool opened = false;
  // Consecutive failed checkpoint writes. Reset by the first success, so it
  // measures "is saving broken NOW" rather than counting a lifetime.
  uint8_t saveFailures = 0;
  // ONE COUNTER PER RECORD, not one shared between them. If the creature's
  // checkpoint succeeded and the player's did not, a shared counter would
  // advance anyway -- and the next player write, one higher again, would land
  // back on the slot holding the only complete player copy and overwrite the
  // very fallback the pair exists to provide. Each record's next generation has
  // to be its OWN last plus one for the parity invariant in ckptSlot() to hold.
  uint32_t saveGeneration = 0;
  uint32_t playerGeneration = 0;
  uint32_t lastSaveAttempt = 0;  // millis() of the last save() call, success or fail
  uint32_t lastTick = 0;
  uint32_t eatUntil = 0;
  uint32_t heartUntil = 0;
  uint32_t evolveUntil = 0;
  int16_t eggTarget = 1;       // dex oculto que saldra del huevo
  bool eggShiny = false;       // sorpresa sorteada al crear el huevo
  uint8_t eggTaps = 0;
  uint8_t mistakeCooldown = 0;
  uint8_t ticksSinceSave = 0;
  bool pendingSave = false;     // guardado periodico pendiente de volcar
  uint8_t evoDeclinedLv = 0;    // "mantener forma": no ofrecer evolucion hasta subir de nivel
  uint32_t farDeclinedAge = 0;  // "quedaros juntos": no ofrecer despedida hasta esta edad
  bool starterPick = false;     // primera partida: esperando que el jugador elija inicial
  uint8_t evoPen = 0;           // levels added to this creature's evolution gate
  bool retirePending = false;   // an early retire is under way; newEgg() spends it
  uint8_t neglectTicks = 0;
  void drainMinute(bool offline);   // one minute of needs: the ONLY copy of the rates
  uint16_t goodTicks = 0;  // racha bien cuidado: forja la DEF
  uint32_t ceremonyUntil = 0;
  uint8_t bondToday = 0;       // tope diario de subida de vinculo
  uint32_t medalUntil = 0;     // celebracion de medalla en pantalla
  uint32_t milestoneUntil = 0; // celebracion de hito de racha

  void registerCare();   // primer cuidado del dia: racha + vinculo
  // Restarts the 8-hour window when every need has just reached CARE_FULL_PCT.
  // The ONE rule for it: care actions, the drain and waking all ask this.
  void graceCheck(bool careAction = false);
  void napWake();        // a daytime nap ends by itself; see NAP_START
  void addBond(uint8_t amt);
  uint8_t rollIV(int bonus) const;  // una tirada 8-31 empujada por el cuidado
  void rollIVs();                   // los 4, con las garantias de legendario/shiny
  uint8_t ivFromGene(uint8_t gene) const;  // migracion de guardados con genes
  void defTick(bool resting);       // la calma forja la defensa (ver pet.cpp)
  void snapshotForParty();          // copy into endedMon before newEgg() wipes it
  void checkMedals();
  void tick();
  void applyAutoSleep();
  void hatch();
  void registerSpecies(int16_t dex);
  bool saveCoreSnapshot();
  bool loadCoreSnapshot();
  bool savePlayerSnapshot();
  bool loadPlayerSnapshot();
  // Returns whether BOTH checkpoints landed. switchTo() propagates this so a
  // caller that is banking the outgoing pet at the same time (focusSwap()) can
  // tell whether it is safe to commit that half too -- see its own comment.
  bool save();
  void load();
  static uint8_t clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
};
