// The Game Corners' rules (v3.53): the slots pay what they show and give back
// a little under a coin per coin, the rigging actually engages, the card flip
// pays Crystal's fair odds, the prize counters are complete, and the halls open
// on the ladder's gate. The screens are driven by gamecorner_ui_test.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "moves.h"
#include "gamecorner.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed = 0x6C0C;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0; volatile bool g_touchDown = false; bool wasPressed = false;
uint32_t millis() { return 0; } void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; } String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *w) { printf("%s  %s\n", ok ? "PASS" : "FAIL", w); if (!ok) bad++; }

// The return per coin with the rigging, and the "natural" one of the bare
// strips (every stop equally likely, nothing vetoed or lined up).
static double played(uint8_t hall, uint8_t bet, int spins, bool *paysAsShown) {
  const SlotMachine &m = slotMachine(hall);
  double won = 0;
  for (int i = 0; i < spins; i++) {
    SlotResult r = slotSpin(hall, bet);
    if (r.win != slotPayout(m, r.stop, bet)) *paysAsShown = false;
    won += r.win;
  }
  return won / ((double)spins * bet);
}
static double natural(uint8_t hall, uint8_t bet) {
  const SlotMachine &m = slotMachine(hall);
  double won = 0, n = 0;
  uint8_t s[3];
  for (s[0] = 0; s[0] < m.len; s[0]++)
    for (s[1] = 0; s[1] < m.len; s[1]++)
      for (s[2] = 0; s[2] < m.len; s[2]++) { won += slotPayout(m, s, bet); n++; }
  return won / (n * bet);
}

int main() {
  // --- 1. the reels are the games'
  {
    const SlotMachine &r = slotMachine(HALL_CELADON), &g = slotMachine(HALL_GOLDENROD);
    ck(r.len == 18 && g.len == 15, "Red's wheels have 18 stops, Crystal's 15");
    int sevens = 0;
    for (uint8_t i = 0; i < r.len; i++) sevens += r.reel[0][i] == 0;
    ck(sevens == 4 && r.pay[0] == 300 && r.pay[1] == 100, "Red's first wheel has four 7s; 7s pay 300, BARs 100");
    ck(g.pay[0] == 300 && g.pay[1] == 50 && g.pay[2] == 6, "Crystal's payout table (300, 50, 6, ...)");
  }

  // --- 2. what is shown is what is paid, and the house keeps a little
  for (uint8_t h = 0; h < HALL_COUNT; h++) {
    for (uint8_t bet = 1; bet <= SLOT_MAX_BET; bet++) {
      bool shown = true;
      double ev = played(h, bet, 400000, &shown), nat = natural(h, bet);
      printf("     %s bet %u: %.3f a coin (bare strips %.3f)\n", hallName(h), bet, ev, nat);
      ck(shown, "every spin pays exactly what its stops show");
      // The rule aims at SLOT_RETURN_PM (0.88) exactly; the band is wide
      // because the emulator's random() is a 16-bit LCG whose consecutive
      // draws are not independent, which skews three-reel combinations by a
      // few hundredths. The board draws from the hardware generator.
      ck(ev > 0.75 && ev < 0.97, "a coin returns a little under a coin");
      ck(slotExpected(h, bet) > 0, "(the expected return is worked out)");
      // the rigging engages in both directions: Red's strips would pay out
      // more than they take, Crystal's middle line far less
      if (nat > 1.0) ck(ev < nat - 0.15, "a machine that would pay out more vetoes wins");
      if (nat < 0.7) ck(ev > nat + 0.15, "a machine that would pay out less lines wins up");
    }
  }

  // --- 3. more coins, more lines
  {
    const SlotMachine &m = slotMachine(HALL_CELADON);
    uint8_t s[3]; bool found = false;
    for (s[0] = 0; s[0] < m.len && !found; s[0]++)
      for (s[1] = 0; s[1] < m.len && !found; s[1]++)
        for (s[2] = 0; s[2] < m.len && !found; s[2]++)
          if (!slotPayout(m, s, 1) && slotPayout(m, s, 2) && !found) {
            found = true;
            uint8_t lines;
            slotPayout(m, s, 2, &lines);
            ck(lines && !(lines & 1), "a top or bottom line pays at 2 coins, not at 1");
          }
    ck(found, "(a stop where only an outer line wins)");
  }

  // --- 4. the card flip: Crystal's odds, exactly fair
  {
    int sum[3] = { 0, 0, 0 };
    for (uint8_t c = 0; c < CARD_DECK; c++) {
      sum[0] += cardPayout(1, CARD_ANY, c);   // JIGGLYPUFF
      sum[1] += cardPayout(CARD_ANY, 4, c);   // a 4
      sum[2] += cardPayout(1, 4, c);          // the JIGGLYPUFF 4
    }
    ck(sum[0] == CARD_BET * CARD_DECK && sum[1] == CARD_BET * CARD_DECK && sum[2] == CARD_BET * CARD_DECK,
       "a creature pays 12, a number 18, a card 72: each returns its bet");
    ck(cardPayout(CARD_ANY, CARD_ANY, 5) == 0, "guessing nothing is not a bet");
    ck(cardCreature(6) == 2 && cardNumber(6) == 2, "card 6 is POLIWAG 2");
    CardDeck d; d.left = 0;
    bool seen[CARD_DECK] = {};
    bool dup = false;
    for (int i = 0; i < CARD_DECK; i++) { uint8_t c = d.draw(); if (seen[c]) dup = true; seen[c] = true; }
    ck(!dup && d.left == 0, "the deck deals all 24 before repeating");
    d.draw();
    ck(d.left == CARD_DECK - 1, "and is shuffled again when it runs out");
  }

  // --- 5. the prize counters
  {
    bool surf = false;
    for (uint8_t i = 0; i < hallPrizeCount(HALL_CELADON); i++) {
      const Prize &p = hallPrize(HALL_CELADON, i);
      if (p.kind == PRIZE_TM && !strcmp(MOVE_TBL[p.what].name, "SURF")) surf = true;
    }
    ck(hallPrizeCount(HALL_CELADON) == 11 && hallPrizeCount(HALL_GOLDENROD) == 6,
       "every prize is there (a renamed TM would drop out)");
    ck(surf, "Celadon has the SURF TM");
    bool tmsAreTms = true;
    for (uint8_t h = 0; h < HALL_COUNT; h++)
      for (uint8_t i = 0; i < hallPrizeCount(h); i++) {
        const Prize &p = hallPrize(h, i);
        if (p.kind != PRIZE_TM) continue;
        bool isTm = false;
        for (uint16_t k = 0; k < TM_COUNT; k++) if (TM_MOVES[k] == p.what) isTm = true;
        if (!isTm) { printf("      %s is not a TM move\n", MOVE_TBL[p.what].name); tmsAreTms = false; }
      }
    ck(tmsAreTms, "every prize TM is a TM someone can learn");
    Pet live; live.dbgHatchAs(25, false);
    const Prize &por = hallPrize(HALL_CELADON, 4);
    PartyMon m = prizeToPartyMon(por, live);
    ck(m.dex == 137 && m.level == 26 && m.uid.valid() && m.moves[0], "a prize PORYGON is level 26, with moves and an ID");
    PartyMon m2 = prizeToPartyMon(por, live);
    ck(m2.uid != m.uid, "and the next one is a different creature");
  }

  // --- 6. the halls open on the ladder
  {
    Pet p; p.dbgHatchAs(25, false);
    ck(!hallUnlocked(HALL_CELADON, p) && !hallUnlocked(HALL_GOLDENROD, p), "both closed at the start");
    p.winBadge(0, 0, false); p.winBadge(0, 1, false);
    ck(!hallUnlocked(HALL_CELADON, p), "Celadon is still closed before Lt. Surge");
    p.winBadge(0, 2, true);
    ck(hallUnlocked(HALL_CELADON, p), "and opens after him, on either difficulty");
    p.winBadge(1, 0, false);
    ck(!hallUnlocked(HALL_GOLDENROD, p), "Goldenrod waits for Bugsy");
    p.winBadge(1, 1, false);
    ck(hallUnlocked(HALL_GOLDENROD, p), "and opens after him");
    uint8_t hs[HALL_COUNT];
    ck(hallsIn(0, hs) == 1 && hs[0] == HALL_CELADON && hallsIn(1, hs) == 1 && hallsIn(2, hs) == 0,
       "one hall in Kanto, one in Johto, none in Hoenn");
    ck(!hallHasCards(HALL_CELADON) && hallHasCards(HALL_GOLDENROD), "card flip only at Goldenrod");
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
