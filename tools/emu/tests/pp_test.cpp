// PP (v3.52) and the AI's heal loop (docs/next-phase-plan.md sections 3-4).
//
// The rules: every move has its real PP; it is spent once per use (a charge
// move once, on winding up; a turn lost to paralysis or sleep costs nothing);
// a move at 0 PP becomes STRUGGLE in battleAct() whoever chose it; the AI never
// picks an empty move; the player's PP carries across fights and restarts in
// the PP book, keyed by creature, and the whole book empties when the live pet
// is fully rested; a LAN fight counts no PP at all.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "link.h"
#include "moves.h"
#include "pp.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x9952;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render();
void onTap(int16_t x, int16_t y);
void startBattle(int16_t dex, uint8_t lvl);
int btlCellIndexAt(int16_t x, int16_t y);
// the middle of cell i, found by asking the firmware's own hit test
static void btlCellCenter(int i, int16_t *cx, int16_t *cy) {
  long sx = 0, sy = 0, n = 0;
  for (int y = 0; y < 466; y += 2) for (int x = 0; x < 466; x += 2)
    if (btlCellIndexAt(x, y) == i) { sx += x; sy += y; n++; }
  *cx = n ? sx / n : 0; *cy = n ? sy / n : 0;
}
extern Pet pet;
extern bool battleOpen;
extern uint8_t btlMenu, btlMsgCount;
extern Combatant btlYou, btlFoe;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

static uint8_t mv(const char *n) {
  for (uint16_t i = 1; i < MOVE_COUNT; i++) if (!strcmp(MOVE_TBL[i].name, n)) return (uint8_t)i;
  return 0;
}

static void mk(Combatant &c, int16_t dex, uint8_t lvl) {
  Pet p;
  p.dbgHatchAs(dex, false);
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.relearnFromLevel();
  combatantFromPet(c, p);
}

static void setMoves(Combatant &c, uint8_t a, uint8_t b, uint8_t d, uint8_t e) {
  uint8_t m[4] = { a, b, d, e };
  for (int i = 0; i < 4; i++) { c.moves[i] = m[i]; c.pp[i] = ppMax(m[i]); }
}

int main() {
  setup();
  if (pet.isEgg()) pet.dbgHatchAs(1, false);      // BULBASAUR
  if (!pet.uid.valid()) pet.uid = pet.mintUid();

  // --- 1. the table
  {
    bool ok = true;
    for (uint16_t i = 1; i < MOVE_COUNT; i++)
      if (i != MV_STRUGGLE && !MOVE_TBL[i].pp) { printf("      %s has no PP\n", MOVE_TBL[i].name); ok = false; }
    ck(ok, "every move but STRUGGLE has its PP");
    ck(MOVE_TBL[mv("SURF")].pp == 15 && MOVE_TBL[mv("TACKLE")].pp == 35, "the games' values (SURF 15, TACKLE 35)");
  }

  // --- 2. spending
  uint8_t tackle = mv("TACKLE"), solar = mv("SOLAR BEAM"), recover = mv("RECOVER");
  {
    Combatant a, b; TurnLog lg;
    mk(a, 1, 50); mk(b, 143, 80);
    setMoves(a, tackle, 0, 0, 0);
    battleAct(a, b, tackle, lg);
    ck(a.pp[0] == ppMax(tackle) - 1, "a move costs one PP");
    setMoves(a, solar, 0, 0, 0);
    battleAct(a, b, solar, lg);
    ck(lg.charged && a.pp[0] == ppMax(solar) - 1, "winding up SOLAR BEAM costs one");
    battleAct(a, b, solar, lg);
    ck(!lg.charged && a.pp[0] == ppMax(solar) - 1, "and firing it costs nothing more");
    setMoves(a, tackle, 0, 0, 0);
    a.ailment = AIL_PARA;
    int skipped = 0, used = 0;
    for (int i = 0; i < 40; i++) {
      mk(b, 143, 80);
      uint8_t before = a.pp[0];
      battleAct(a, b, tackle, lg);
      if (lg.skipped) { skipped++; if (a.pp[0] != before) used = 99; }
      else used++;
    }
    ck(skipped > 0 && used == 40 - skipped && a.pp[0] == ppMax(tackle) - used,
       "a turn lost to paralysis costs no PP");
  }

  // --- 3. an empty move is STRUGGLE, whoever picked it
  {
    Combatant a, b; TurnLog lg;
    mk(a, 1, 50); mk(b, 143, 80);
    setMoves(a, tackle, mv("VINE WHIP"), 0, 0);
    a.pp[0] = 0;
    battleAct(a, b, tackle, lg);
    ck(lg.move == MV_STRUGGLE, "a move at 0 PP is STRUGGLE in battleAct");
    ck(!battleMustStruggle(a) && !battleSlotReady(a, 0) && battleSlotReady(a, 1), "while another still has PP, only it is ready");
    for (int i = 0; i < 100; i++) if (aiChooseMove(a, b, i & 1) != a.moves[1]) { ck(false, "the AI picks an empty move"); break; }
    a.pp[1] = 0;
    ck(battleMustStruggle(a), "all empty: must STRUGGLE");
    ck(aiChooseMove(a, b, true) == MV_STRUGGLE && aiChooseMove(a, b, false) == MV_STRUGGLE, "and the AI does");
    uint8_t hp = a.hp;
    battleAct(a, b, MV_STRUGGLE, lg);
    ck(lg.move == MV_STRUGGLE && (lg.missed || a.hp < hp), "STRUGGLE hurts its user");
  }

  // --- 4. a LAN combatant counts nothing
  {
    Combatant a, b; TurnLog lg;
    mk(a, 1, 50); mk(b, 143, 80);
    setMoves(a, tackle, 0, 0, 0);
    LinkMon lm; linkMonFrom(lm, a);
    Combatant l; linkMonTo(l, lm);
    for (int i = 0; i < 50; i++) { mk(b, 143, 80); battleAct(l, b, tackle, lg); }
    ck(!l.ppOn && lg.move == tackle && battleSlotReady(l, 0), "a link fight never runs out");
  }

  // --- 5. the book: per creature, per move, saved, refilled by rest
  {
    ppRefillAll();
    CreatureUid u = pet.uid;
    ck(u.valid(), "the live pet has an ID");
    uint8_t ms[4] = { tackle, solar, 0, 0 }, left[4] = { 10, 3, 0, 0 };
    ppRecord(u, ms, left);
    ck(ppLeft(u, tackle) == 10 && ppLeft(u, solar) == 3, "what a fight left is what is left");
    ck(ppLeft(u, recover) == ppMax(recover), "a move it did not have is full");
    CreatureUid other = u; other.serial ^= 0x5555;
    ck(ppLeft(other, tackle) == ppMax(tackle), "another creature is untouched");
    ck(ppDirty() && !nvs().count("pp"), "recorded in RAM, not yet written");
    ppFlush();
    ck(!ppDirty() && nvs().count("pp"), "flushed");
    ppRecord(u, ms, (const uint8_t[4]){ 1, 1, 0, 0 });   // RAM only, then lost
    ppLoad();
    ck(ppLeft(u, tackle) == 10, "and read back at boot");
    // a full rest, through the real drain
    pet.setClock(1800000000UL);
    pet.sleeping = true;
    pet.energy = 10;
    pet.syncClock(1800000000UL + 3600);
    ck(pet.energy >= 100 && ppEntryCount() == 0 && ppLeft(u, tackle) == ppMax(tackle), "a full rest refills the whole team");
    ck(!nvs().count("pp"), "and saves at once");
    pet.sleeping = false;
  }

  // --- 6. through the screen: spent, carried into the next fight
  {
    pet.ageMinutes = 40 * MINUTES_PER_LEVEL;
    pet.moves[0] = tackle; pet.moves[1] = mv("VINE WHIP"); pet.moves[2] = 0; pet.moves[3] = 0;
    ppRefillAll();
    startBattle(129, 50);                        // a MAGIKARP that will not end it
    ck(battleOpen && btlYou.pp[0] == ppMax(tackle), "a fresh fight starts full");
    int16_t x, y;
    btlMenu = 1; btlMsgCount = 0;
    btlCellCenter(0, &x, &y);
    onTap(x, y);
    ck(btlYou.pp[0] == ppMax(tackle) - 1, "tapping TACKLE spends one");
    ck(ppLeft(pet.uid, tackle) == ppMax(tackle) - 1, "and the book has it after the turn");
    // empty the first move: tapping it is refused while the second has PP
    btlYou.pp[0] = 0;
    btlMenu = 1; btlMsgCount = 0;
    uint8_t p1 = btlYou.pp[1];
    onTap(x, y);
    ck(btlMenu == 1 && btlYou.pp[1] == p1 && btlMsgCount == 1, "an empty move is refused, with a message");
    // everything empty: any move is STRUGGLE
    btlYou.pp[1] = 0;
    btlMsgCount = 0;
    uint16_t hp = btlYou.hp;
    onTap(x, y);
    ck(btlMenu == 0 && btlMsgCount > 1, "with nothing left a tap STRUGGLEs");
    (void)hp;
    battleOpen = false;
    ppFlush();
    startBattle(129, 50);
    ck(btlYou.pp[0] == 0 && btlYou.pp[1] == 0, "the next fight starts where the last one left off");
    battleOpen = false;
    ppRefillAll();
  }

  // --- 7. the heal loop is ended by PP (the owner's call, v3.55: no special
  // heal rules in the AI)
  {
    // The shape of the report: the healer's only attack does nothing to the
    // foe (TACKLE into a GENGAR), its SLUDGE BOMB takes 30-45% a turn, and
    // RECOVER gives back 50%. Without PP the AI healed every other turn,
    // forever; with RECOVER's 5 PP it can heal at most five times.
    uint8_t ball = mv("SLUDGE BOMB");
    Combatant ai, foe; TurnLog lg;
    mk(ai, 113, 50);           // CHANSEY
    setMoves(ai, recover, tackle, 0, 0);
    uint8_t flv = 0;
    for (uint8_t l = 5; l <= 100 && !flv; l++) {
      mk(foe, 94, l);          // GENGAR
      setMoves(foe, ball, 0, 0, 0);
      uint32_t d = battleDamage(foe, ai, ball, false, 236);
      if (d * 100 >= ai.maxHp * 30UL && d * 100 <= ai.maxHp * 45UL) flv = l;
    }
    ck(flv != 0, "(a GENGAR that hits CHANSEY for 30-45%)");
    int heals = 0, turns = 0;
    for (; turns < 300 && !ai.fainted() && !foe.fainted(); turns++) {
      uint8_t m = aiChooseMove(ai, foe, true);
      if (m == recover) heals++;
      battleAct(ai, foe, m, lg);
      if (!foe.fainted()) battleAct(foe, ai, ball, lg);
    }
    printf("     GENGAR L%u: %d turns, %d heals\n", flv, turns, heals);
    ck(heals >= 2, "(the AI does heal while it can)");
    ck(heals <= ppMax(recover), "no more heals than RECOVER has PP");
    ck((ai.fainted() || foe.fainted()) && turns <= 40, "and the fight ends within 40 turns");
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
