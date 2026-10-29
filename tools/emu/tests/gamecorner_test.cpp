// The Game Corners' rules: the slots are Red's and Crystal's own engines
// (v3.73) -- one paying line, each engine's rules, the lucky machines at the
// games' odds, and a machine that pays out more than it takes, as the
// originals did -- the card flip pays Crystal's fair odds, the prize counters
// are complete, and the halls open on the ladder's gate. The screens are driven by gamecorner_ui_test.
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

// The symbol a result paid for: reel 1's symbol on the paying line.
static int paidSym(uint8_t hall, const SlotResult &r) {
  static const int8_t ROW0[5] = { 0, -1, 1, -1, 1 };
  for (int l = 0; l < 5; l++)
    if (r.lines & (1 << l)) return slotSymbol(slotMachine(hall), 0, r.stop[0], ROW0[l]);
  return -1;
}

struct Play { double hit, ret, big; };
// Plays `sittings` sittings of `spins` spins each, every one at a machine as
// lucky as asked, and checks each result pays what its stops show.
static Play play(uint8_t hall, uint8_t bet, bool lucky, int sittings, int spins, bool *shown) {
  long wins = 0, big = 0, n = 0; double won = 0;
  for (int t = 0; t < sittings; t++) {
    SlotSession ss = slotSit(hall);
    ss.lucky = lucky;
    for (int i = 0; i < spins; i++) {
      SlotResult r = slotSpin(ss, bet);
      if (r.win != slotPayout(slotMachine(hall), r.stop, bet)) *shown = false;
      if (r.win) wins++;
      if (r.win >= 100) big++;
      won += r.win; n++;
    }
  }
  return { (double)wins / n, won / ((double)n * bet), big ? (double)n / big : 0 };
}

int main() {
  // --- 1. the reels are the games'
  {
    const SlotMachine &r = slotMachine(HALL_CELADON), &g = slotMachine(HALL_GOLDENROD);
    ck(r.len == 15 && g.len == 15, "both machines' reels have 15 stops (Red's tables repeat three to wrap)");
    int sevens = 0;
    for (uint8_t i = 0; i < r.len; i++) sevens += r.reel[0][i] == 0;
    ck(sevens == 3 && r.pay[0] == 300 && r.pay[1] == 100, "Red's first wheel has three 7s; 7s pay 300, BARs 100");
    ck(g.pay[0] == 300 && g.pay[1] == 50 && g.pay[2] == 6, "Crystal's payout table (300, 50, 6, ...)");
  }

  // --- 2. one line at a time
  // No stop set on either machine lines up two lines at once, so a spin's win
  // is one line's prize. Both games pay a single line by design; this pins
  // that the reels never make the question arise.
  for (uint8_t h = 0; h < HALL_CORNERS; h++) {
    const SlotMachine &m = slotMachine(h);
    uint8_t s[3]; int multi = 0, wins = 0;
    for (s[0] = 0; s[0] < m.len; s[0]++)
      for (s[1] = 0; s[1] < m.len; s[1]++)
        for (s[2] = 0; s[2] < m.len; s[2]++) {
          int n = 0;
          static const int8_t L[5][3] = { { 0, 0, 0 }, { -1, -1, -1 }, { 1, 1, 1 }, { -1, 0, 1 }, { 1, 0, -1 } };
          for (auto &l : L) {
            uint8_t x = slotSymbol(m, 0, s[0], l[0]);
            if (x == slotSymbol(m, 1, s[1], l[1]) && x == slotSymbol(m, 2, s[2], l[2])) n++;
          }
          if (n > 1) multi++;
          if (slotPayout(m, s, 3)) wins++;
        }
    printf("     %s: %d stop sets win at 3 coins, %d line up two lines\n", hallName(h), wins, multi);
    ck(wins > 0 && multi == 0, "no stop set lines up two lines at once");
  }
  ck(slotMachine(HALL_CELADON).order[0] == 3 && slotMachine(HALL_GOLDENROD).order[0] == 0,
     "(Red checks a diagonal first, Crystal the middle line)");

  // --- 3. Red's engine
  {
    bool barClears = true, modeHolds = true, shown = true;
    SlotSession ss = slotSit(HALL_CELADON);
    for (int i = 0; i < 200000; i++) {
      bool inMode = ss.flags & SLOT_RED_CAN_7;
      SlotResult r = slotSpin(ss, 1 + i % 3);
      int sym = paidSym(HALL_CELADON, r);
      if (sym == 1 && ss.flags) barClears = false;                    // a BAR ends the mode
      if (inMode && sym != 0 && sym != 1 && !(ss.flags & SLOT_RED_CAN_7)) modeHolds = false;
      if (r.win != slotPayout(slotMachine(HALL_CELADON), r.stop, 1 + i % 3)) shown = false;
    }
    ck(shown, "every Celadon spin pays exactly what its stops show");
    ck(barClears, "three BARs end 7/BAR mode");
    ck(modeHolds, "7/BAR mode lasts until a 7 or a BAR pays");
    // from a cleared machine a spin enters 7/BAR mode 2 times in 256 at most,
    // so that is the most often it can pay a 7 or BAR
    int jackpots = 0, wins = 0;
    for (int i = 0; i < 100000; i++) {
      SlotSession t = slotSit(HALL_CELADON);
      t.flags = 0; t.allow = 0;
      SlotResult r = slotSpin(t, 3);
      int sym = paidSym(HALL_CELADON, r);
      if (sym == 0 || sym == 1) jackpots++;
      if (r.win) wins++;
    }
    printf("     from a cleared machine: %d 7/BAR wins and %d wins in 100000 spins\n", jackpots, wins);
    ck(jackpots < 100000 * 3 / 256 && wins > 100000 / 20, "a 7 or BAR pays only in 7/BAR mode; small wins still come");
    bool shown2 = true;
    Play n1 = play(HALL_CELADON, 1, false, 400, 500, &shown2), n3 = play(HALL_CELADON, 3, false, 400, 500, &shown2);
    Play l1 = play(HALL_CELADON, 1, true, 400, 500, &shown2);
    printf("     Celadon normal bet 1: wins %.1f%%, return %.2f, 7/BAR 1 in %.0f\n", 100 * n1.hit, n1.ret, n1.big);
    printf("     Celadon normal bet 3: wins %.1f%%, return %.2f, 7/BAR 1 in %.0f\n", 100 * n3.hit, n3.ret, n3.big);
    printf("     Celadon lucky  bet 1: wins %.1f%%, return %.2f, 7/BAR 1 in %.0f\n", 100 * l1.hit, l1.ret, l1.big);
    ck(n1.hit > 0.05 && n1.hit < 0.16, "Red wins about 1 spin in 10 at one coin");
    ck(n3.hit > 2 * n1.hit, "and far more often at three");
    ck(n1.ret > 1.2, "it pays out more than it takes, as Red's did");
    ck(l1.big > 0 && l1.big < n1.big / 1.5, "the lucky machine hits 7/BAR much more often");
  }

  // --- 4. Crystal's engine
  {
    bool onlyBias = true, shown = true;
    SlotSession ss = slotSit(HALL_GOLDENROD);
    for (int i = 0; i < 200000; i++) {
      if (i % 50 == 0) ss = slotSit(HALL_GOLDENROD);
      SlotResult r = slotSpin(ss, 1 + i % 3);
      int sym = paidSym(HALL_GOLDENROD, r);
      // a win is the bias's: after a non-7 win the bias is still that symbol;
      // after 7s it is still 7 or cleared
      if (sym > 0 && ss.bias != sym) onlyBias = false;
      if (sym == 0 && ss.bias != 0 && ss.bias != SLOT_NO_BIAS) onlyBias = false;
      if (r.win != slotPayout(slotMachine(HALL_GOLDENROD), r.stop, 1 + i % 3)) shown = false;
    }
    ck(shown, "every Goldenrod spin pays exactly what its stops show");
    ck(onlyBias, "Crystal only pays the symbol the spin was biased to");
    // a 7 bias stays until 7s pay
    bool sevenStays = true;
    for (int i = 0; i < 20000; i++) {
      SlotSession t = slotSit(HALL_GOLDENROD);
      t.bias = 0;
      SlotResult r = slotSpin(t, 1);
      if (paidSym(HALL_GOLDENROD, r) != 0 && t.bias != 0) sevenStays = false;
    }
    ck(sevenStays, "a 7 bias lasts until 7s pay");

    bool sh = true;
    Play n1 = play(HALL_GOLDENROD, 1, false, 400, 50, &sh), n2 = play(HALL_GOLDENROD, 2, false, 400, 50, &sh);
    Play l2 = play(HALL_GOLDENROD, 2, true, 400, 50, &sh);
    printf("     Goldenrod normal bet 1: wins %.1f%%, return %.2f\n", 100 * n1.hit, n1.ret);
    printf("     Goldenrod normal bet 2: wins %.1f%%, return %.2f, 7s 1 in %.0f\n", 100 * n2.hit, n2.ret, n2.big);
    printf("     Goldenrod lucky  bet 2: wins %.1f%%, return %.2f, 7s 1 in %.0f\n", 100 * l2.hit, l2.ret, l2.big);
    ck(n1.hit > 0.01 && n1.hit < 0.08, "Crystal is stingy on one line");
    ck(n2.hit > 0.10 && n2.hit < 0.30, "and wins about 1 spin in 6 at two coins");
    ck(l2.hit > n2.hit * 1.3, "a lucky machine wins clearly more");
    ck(n2.ret > 1.1, "it pays out more than it takes, as Crystal's did");
  }

  // --- 5. the lucky machines come up at the games' own odds
  {
    int redL = 0, gsL = 0, N = 40000;
    for (int i = 0; i < N; i++) {
      slotVisit(HALL_CELADON);
      if (slotSit(HALL_CELADON).lucky) redL++;
      if (slotSit(HALL_GOLDENROD).lucky) gsL++;
    }
    printf("     lucky sittings: Celadon %.1f%%, Goldenrod %.1f%%\n", 100.0 * redL / N, 100.0 * gsL / N);
    ck(redL > N / 60 && redL < N / 20, "Celadon: one lucky machine a visit, about 1 sitting in 35");
    ck(gsL > N * 15 / 100 && gsL < N * 28 / 100, "Goldenrod: about 1 sitting in 5");
  }

  // --- 6. the card flip: Crystal's odds, exactly fair
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

  // --- 7. the prize counters
  {
    bool surf = false;
    for (uint8_t i = 0; i < hallPrizeCount(HALL_CELADON); i++) {
      const Prize &p = hallPrize(HALL_CELADON, i);
      if (p.kind == PRIZE_TM && !strcmp(MOVE_TBL[p.what].name, "SURF")) surf = true;
    }
    ck(hallPrizeCount(HALL_CELADON) == 12 && hallPrizeCount(HALL_GOLDENROD) == 7,
       "every prize is there (a renamed TM would drop out)");
    ck(surf, "Celadon has the SURF TM");
    bool tmsAreTms = true;
    for (uint8_t h = 0; h < HALL_CORNERS; h++)
      for (uint8_t i = 0; i < hallPrizeCount(h); i++) {
        const Prize &p = hallPrize(h, i);
        if (p.kind != PRIZE_TM) continue;
        bool isTm = false;
        for (uint16_t k = 0; k < TM_COUNT; k++) if (TM_MOVES[k] == p.what) isTm = true;
        if (!isTm) { printf("      %s is not a TM move\n", MOVE_TBL[p.what].name); tmsAreTms = false; }
      }
    ck(tmsAreTms, "every prize TM is a TM someone can learn");
    Pet live; live.dbgHatchAs(25, false);
    uint8_t pi = 0;
    for (uint8_t i = 0; i < hallPrizeCount(HALL_CELADON); i++) if (hallPrize(HALL_CELADON, i).what == 137) pi = i;
    const Prize &por = hallPrize(HALL_CELADON, pi);
    bool both = false, both2 = false, s1 = false, s2 = false;
    for (uint8_t i = 0; i < hallPrizeCount(HALL_CELADON); i++) {
      const Prize &q = hallPrize(HALL_CELADON, i);
      if (q.kind == PRIZE_MON && q.what == 123) s1 = true;   // SCYTHER
      if (q.kind == PRIZE_MON && q.what == 127) s2 = true;   // PINSIR
    }
    both = s1 && s2; s1 = s2 = false;
    for (uint8_t i = 0; i < hallPrizeCount(HALL_GOLDENROD); i++) {
      const Prize &q = hallPrize(HALL_GOLDENROD, i);
      if (q.kind == PRIZE_MON && q.what == 23) s1 = true;    // EKANS
      if (q.kind == PRIZE_MON && q.what == 27) s2 = true;    // SANDSHREW
    }
    both2 = s1 && s2;
    ck(both && both2, "both versions' exclusives are on each counter");
    PartyMon m = prizeToPartyMon(por, live);
    ck(m.dex == 137 && m.level == 26 && m.uid.valid() && m.moves[0], "a prize PORYGON is level 26, with moves and an ID");
    PartyMon m2 = prizeToPartyMon(por, live);
    ck(m2.uid != m.uid, "and the next one is a different creature");
  }

  // --- 8. the halls open on the ladder
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
    ck(hallsIn(0, p, hs) == 1 && hs[0] == HALL_CELADON && hallsIn(1, p, hs) == 1 && hallsIn(2, p, hs) == 0,
       "one hall in Kanto, one in Johto, none in Hoenn");
    ck(!hallHasCards(HALL_CELADON) && hallHasCards(HALL_GOLDENROD), "card flip only at Goldenrod");
  }

  printf(bad ? "\n%d FAILED\n" : "\nall passed\n", bad);
  return bad ? 1 : 0;
}
