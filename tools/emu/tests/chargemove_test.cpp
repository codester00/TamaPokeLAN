// Two-turn moves that hide (v4.12). DIG goes underground, FLY and BOUNCE into
// the sky, DIVE underwater, SHADOW FORCE vanishes: from the wind-up until the
// strike nothing aimed at the user lands -- "avoided the attack!" -- except the
// moves that reach it in the games (EARTHQUAKE into a DIG and SURF into a DIVE
// at double, GUST into the sky at double, THUNDER / HURRICANE / SKY UPPERCUT
// at normal). The wind-up says the games' line, the sprite leaves with it and
// comes back with the strike, the menu is locked to the strike, and the smart
// AI does not throw a move that cannot land.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "moves.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed = 0xD16; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup(); void render(); void battleTap(int16_t, int16_t);
void startBattle(int16_t dex, uint8_t lvl);
void btlCellCenter(int i, int *x, int *y);
extern Pet pet;
extern bool battleOpen;
extern Combatant btlYou, btlFoe;
extern uint8_t btlMsgCount, btlMenu;
extern char btlMsg[8][40];
extern uint8_t btlHideKind[2];
extern uint16_t squadMask;

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }
static void tapCell(int i) { int x, y; btlCellCenter(i, &x, &y); render(); battleTap(x, y); }
static bool queued(const char *frag) {
  for (uint8_t i = 0; i < btlMsgCount; i++) if (strstr(btlMsg[i], frag)) return true;
  return false;
}
static void clearMsgs() { for (int i = 0; i < 40 && btlMsgCount; i++) { render(); battleTap(233, 300); } }
static Combatant mon(int16_t dex, uint8_t lvl) {
  Pet p; p.dbgHatchAs(dex, false); p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  Combatant c; combatantFromPet(c, p);
  for (int i = 0; i < MOVE_SLOTS; i++) { c.moves[i] = 0; c.pp[i] = 0; }
  return c;
}
static void give(Combatant &c, uint8_t slot, uint8_t mv) { c.moves[slot] = mv; c.pp[slot] = 20; }
// damage `mv` does from a into d, with the same rolls each call
static uint16_t dmgOf(Combatant a, Combatant d, uint8_t mv) {
  randomSeed(777);
  TurnLog lg; battleAct(a, d, mv, lg);
  return lg.evaded ? 0xFFFF : lg.damage;
}

int main() {
  setup();
  for (int i = 0; i < 3; i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);

  // --- 1. the engine ---------------------------------------------------------
  Combatant digger = mon(51, 50), flier = mon(18, 50), diver = mon(131, 50), ghost = mon(487, 50), atk = mon(150, 50);
  give(digger, 0, MV_DIG); give(flier, 0, MV_FLY); give(diver, 0, MV_DIVE); give(ghost, 0, MV_SHADOW_FORCE);
  ck(!battleHidden(digger), "in plain sight before it winds up");
  TurnLog lg;
  battleAct(digger, atk, MV_DIG, lg);
  ck(lg.charged && digger.charging == MV_DIG && battleHidden(digger) == HIDE_UNDERGROUND, "DIG winds up underground");
  battleAct(flier, atk, MV_FLY, lg);   ck(battleHidden(flier) == HIDE_SKY, "FLY goes into the sky");
  battleAct(diver, atk, MV_DIVE, lg);  ck(battleHidden(diver) == HIDE_UNDERWATER, "DIVE goes underwater");
  battleAct(ghost, atk, MV_SHADOW_FORCE, lg); ck(battleHidden(ghost) == HIDE_VANISHED, "SHADOW FORCE vanishes");
  { Combatant sun = mon(1, 50); give(sun, 0, MV_SOLAR_BEAM); battleAct(sun, atk, MV_SOLAR_BEAM, lg);
    ck(sun.charging && !battleHidden(sun), "SOLAR BEAM charges in plain sight"); }

  Combatant a2 = atk; give(a2, 0, MV_TACKLE);
  battleAct(a2, digger, MV_TACKLE, lg);
  ck(lg.evaded && !lg.damage && digger.hp == digger.maxHp, "a TACKLE at a creature underground: avoided");
  battleAct(a2, digger, MV_GROWL, lg);
  ck(lg.evaded, "so is a status move aimed at it");
  battleAct(a2, digger, MV_SWORDS_DANCE, lg);
  ck(!lg.evaded && a2.stage[SI_ATK] > 0, "a move on itself still works");
  // the ones that reach it, at their multiplier
  Combatant dug = digger, seen = digger; seen.charging = 0;
  uint16_t hid = dmgOf(atk, dug, MV_EARTHQUAKE), open = dmgOf(atk, seen, MV_EARTHQUAKE);
  ck(hid != 0xFFFF && open > 0 && (hid == 2 * open || hid >= open * 2 - 1), "EARTHQUAKE reaches a DIG, at double damage");
  Combatant up = flier, flat = flier; flat.charging = 0;
  uint16_t g1 = dmgOf(atk, up, MV_GUST), g0 = dmgOf(atk, flat, MV_GUST);
  ck(g1 != 0xFFFF && g0 > 0 && g1 >= g0 * 2 - 1, "GUST reaches the sky, at double");
  uint16_t t1 = dmgOf(atk, up, MV_THUNDER), t0 = dmgOf(atk, flat, MV_THUNDER);
  ck(t1 != 0xFFFF && t1 == t0, "THUNDER reaches the sky at normal damage");
  ck(dmgOf(atk, up, MV_EARTHQUAKE) == 0xFFFF, "EARTHQUAKE does not reach the sky");
  ck(dmgOf(atk, diver, MV_SURF) != 0xFFFF && dmgOf(atk, diver, MV_EARTHQUAKE) == 0xFFFF, "SURF reaches a DIVE, EARTHQUAKE does not");
  ck(dmgOf(atk, ghost, MV_EARTHQUAKE) == 0xFFFF && dmgOf(atk, ghost, MV_GUST) == 0xFFFF && dmgOf(atk, ghost, MV_SURF) == 0xFFFF,
     "nothing reaches SHADOW FORCE");
  // the strike: out of hiding as it hits
  battleAct(digger, a2, MV_TACKLE, lg);   // whatever is picked, the wound-up move fires
  ck(lg.move == MV_DIG && !lg.charged && !digger.charging && !battleHidden(digger) && lg.damage > 0,
     "the next turn DIG strikes, whatever was picked, and is back in sight");
  ck(battleStored(flier).charging == 0, "a creature off the field forgets its wind-up");
  // the smart AI does not waste a turn on a move that cannot land
  { Combatant me = mon(150, 50); give(me, 0, MV_TACKLE); give(me, 1, MV_SWORDS_DANCE);
    ck(aiChooseMove(me, dug, true) == MV_SWORDS_DANCE, "facing a creature underground the AI sets up instead of TACKLE");
    Combatant me2 = mon(150, 50); give(me2, 0, MV_GROWL); give(me2, 1, MV_SWORDS_DANCE);
    ck(aiChooseMove(me2, dug, true) == MV_SWORDS_DANCE, "nor a status move aimed at it (GROWL)");
    give(me, 2, MV_EARTHQUAKE);
    ck(aiChooseMove(me, dug, true) == MV_EARTHQUAKE, "and takes the move that reaches it when it has one"); }

  // --- 2. on the screen --------------------------------------------------------
  pet.dbgHatchAs(150, false);
  pet.ageMinutes = 99UL * MINUTES_PER_LEVEL;
  pet.energy = pet.fullness = pet.joy = pet.hygiene = 100;
  pet.sleeping = false;
  squadMask = 1;
  startBattle(79, 30);                     // a slow SLOWPOKE
  for (int i = 0; i < MOVE_SLOTS; i++) { btlFoe.moves[i] = MV_TACKLE; btlFoe.pp[i] = 30; }
  btlYou.moves[0] = MV_DIG; btlYou.pp[0] = 10;
  btlYou.moves[1] = MV_TACKLE; btlYou.pp[1] = 30;
  btlMenu = 1;
  tapCell(0);                              // DIG
  ck(queued("dug a hole"), "the wind-up says the games' line");
  ck(queued("avoided the attack"), "and the slower foe's TACKLE is avoided");
  render();
  ck(btlHideKind[0] == HIDE_UNDERGROUND, "the sprite goes underground with its line");
  clearMsgs();
  ck(btlYou.charging == MV_DIG && btlMenu == 0, "(back on the menu, still underground)");
  tapCell(1);                              // BAG
  ck(btlMenu == 0, "the bag waits while it is underground");
  tapCell(2);                              // POKEMON
  ck(btlMenu == 0, "and so does switching");
  uint16_t foeHp = btlFoe.hp;
  tapCell(0);                              // FIGHT: it strikes, no move list
  ck(!btlYou.charging && btlFoe.hp < foeHp, "FIGHT strikes with DIG at once");
  ck(queued("used DIG"), "said as the move");
  for (int f = 0; f < 3; f++) render();
  ck(btlHideKind[0] == 0, "and the sprite is back with that line");
  clearMsgs();
  battleOpen = false;

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
