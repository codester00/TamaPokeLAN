// TamaPoke - tamagotchi pixel art inspirado en la gen 1
// para Waveshare ESP32-S3-Touch-AMOLED-1.75
//
// Librerias (Library Manager o repo de Waveshare):
//   - "GFX Library for Arduino" (moononournation), con soporte CO5300 QSPI
//   - "SensorLib" (Lewis He), driver tactil CST9217
//
// Placa: ESP32S3 Dev Module | Flash 16MB | PSRAM: OPI PSRAM | USB CDC On Boot: Enabled
//
// Los sprites y la tabla de especies se generan con tools/sprites.py (emit).

#include <Arduino.h>
#include <Wire.h>
#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"
#include "pin_config.h"
#include "species.h"
#include "dex.h"
#include "types.h"
#include "moves.h"
#include "battle.h"
#include "trainers.h"
#include "link.h"
#include "linknow.h"
#include "trade.h"
#include "noart.h"
#include "backs.h"
#include "badges.h"
#include "avatars.h"
#include <stdarg.h>
#include "party.h"
#include "save.h"
#include "pet.h"
#include "items.h"
#include "inventory.h"
#include "wild.h"
#include "sdmon.h"
#include "rtcbat.h"
#include "pedometer.h"
#include "nvsinfo.h"
#include "i18n.h"
#include "audio.h"

// Firmware version, shown on the settings screen and printed at boot. Bump it
// with every balance change and every release; the README badge must match, and
// build_web.sh moves web/manifest.json to it when the installer is rebuilt.
// 3.29 was the first build after v3.28 (it was briefly "1.01", which read as a
// downgrade and matched nothing); 3.30 starts creatures at fitness 75.
// 3.33 adds trading between two boards; 3.34 the 8-hour grace decay.
#define FW_VERSION "3.49"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, 0 /*rotation*/, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
// Framebuffer completo en PSRAM: dibujamos todo y hacemos flush() (sin parpadeo)
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

TouchDrvCST92xx touch;
Pet pet;

// sprite animado de la SD para la especie actual (si existe el archivo)
SdMon mon;          // sprite B/N (respaldo y minijuego si no hay PMD)
PmdMon pmd;         // sprite PMD multi-accion (pantalla principal)
PmdMon evoPmd;      // forma anterior, solo durante el parpadeo de evolucion
int16_t monFor = -2;
bool monShinyFor = false;

// comportamiento del bicho en pantalla
struct {
  uint8_t mode = 0;     // 0 idle, 1 paseo, 2 gesto one-shot
  uint8_t act = PMD_IDLE;
  uint32_t t0 = 0;      // inicio de la animacion en curso
  uint32_t until = 0;   // fin del estado actual
  float x = 233, targetX = 233;
} beh;
#define PET_GROUND 304  // linea de suelo de la mascota
PmdMon galleryPmd;  // sprite grande de la vista detalle de la galeria (PMD/TPK2, legal)

// galeria pokedex
bool galleryOpen = false;
bool galleryDirty = false;
// 16 to a page, and the Pokedex is browsed ONE REGION AT A TIME. Three
// generations flat is 25 pages of swiping to reach Hoenn, which is not a
// Pokedex, it is a scroll. A vertical swipe changes region and a horizontal one
// pages within it, so nothing is ever more than ten pages from the front.
// ALL is deliberately not offered here -- it is the thing being replaced.
#define GAL_PER_PAGE 16
#define GAL_REGIONS (REGION_COUNT - 1)          // the real regions, not ALL
#define GAL_LO (REGIONS[galleryRegion % GAL_REGIONS].lo)
#define GAL_HI (REGIONS[galleryRegion % GAL_REGIONS].hi)
#define GAL_SPAN (GAL_HI - GAL_LO + 1)
#define GAL_PAGES ((GAL_SPAN + GAL_PER_PAGE - 1) / GAL_PER_PAGE)
uint8_t galleryRegion = 0;
// Both the Pokedex and the gym ladder now open on a REGION CHOOSER rather than
// dropping you into whichever region was last viewed. The vertical swipe that
// changes region still works, but it is invisible, so on its own it meant the
// Johto and Hoenn content looked absent.
bool galleryPick = false;
bool gymPick = false;
uint8_t rpickPage = 0;      // the region chooser is paged; shared by all 3 modes
uint8_t galleryPage = 0;    // GAL_PAGES paginas de GAL_PER_PAGE
int16_t galleryDetail = 0;  // dex en vista detalle, 0 = rejilla

bool screenOff = false;       // pulsacion corta del boton PWR
bool cardOpen = false;        // ficha del bicho (deslizar vertical)
bool kbOpen = false;
enum : uint8_t { KB_PET = 0, KB_TRAINER };
uint8_t kbTarget = KB_PET;          // teclado para renombrar al bicho
char nameBuf[12] = "";
uint8_t nameLen = 0;
#define CARD_PAGES 4   // profile, stats, moves, progress -- medals moved to
                       // the player card, where the totals already live
uint8_t cardPage = 0;         // 0 perfil, 1 stats+medallas
// Menu overlay: opened by tapping the pet's name on the main screen. The
// horizontal swipe is already taken by the Pokedex (it pages through 10 pages
// internally), so a hub on that axis would be ambiguous; the header was inert
// and is the only free surface left.
bool menuOpen = false;
#define MENU_X 73
// Four rows: PARTY and GYMS came out, since a swipe right and a swipe left now
// reach them directly. Sized to the bezel -- the panel is 320 wide, so 160 from
// the centre, and sqrt(233^2 - 160^2) = 169 means it can only span y 64..402.
#define MENU_Y 70
#define MENU_W 320
#define MENU_H 326
#define MENU_ROW_H 44
#define MENU_ROW_GAP 6
// 6 rows: STATS / BAG / POKEDEX / SETTINGS / RETIRE / CLOSE. The row height came
// down from 52 to fit the sixth: the panel spans 70..396, and the round display
// gives a half-width of 166 there against the 160 a row needs, so the corners
// stay on glass. Anything taller does not -- at 6 rows of 52 it would reach 413
// and the bottom corners would fall off the bezel.
#define MENU_ROWS 6
#define MENU_ROW_Y(i) (MENU_Y + 16 + (i) * (MENU_ROW_H + MENU_ROW_GAP))

// Party screen. partyPick != 0 means the newcomer needs a slot: the player
// either taps someone to replace or lets it go.
bool partyOpen = false;
bool partyPick = false;
// The Linking Cord on a KARRABLAST or SHELMET: the party ring is open to pick
// the creature to link with. Cancelling keeps the cord.
bool cordPick = false;
uint8_t partyDetail = 0;   // 0 = the grid, else slot + 1
// The box, reached from the party screen. `boxSwapFrom` is the party slot
// waiting for something to trade with, 0 when nothing is pending.
bool boxOpen = false;
uint8_t boxPage = 0;
uint8_t boxSwapFrom = 0;   // party slot + 1, armed from the party side
uint8_t boxSel = 0;        // box slot + 1, armed from the box side
// The box gets the same detail sheet the party has. Tapping a box slot used to
// yank the creature into the party immediately, which is a surprising amount to
// happen from one tap -- and left nowhere to put a RELEASE button.
uint8_t boxDetail = 0;        // box slot + 1 whose sheet is open
// The RELEASE confirm, on whichever sheet is up. One flag rather than two,
// because only one sheet can be open at a time and partyDetail/boxDetail
// already say which.
bool releaseConfirm = false;
#define BOX_PER_PAGE 6
uint32_t partyBannerUntil = 0;   // "<name> joined the party!"
char partyBannerName[14] = "";
// The party and box moved from a 2x3 grid to a RING; see partySlotPos(). A grid
// on a circle spends its corners on glass that is not there -- the old 150x70
// cells put the outer corners of the top and bottom rows within a few pixels of
// the bezel, and 70 px is 6.7 mm, under the finger floor, on a screen whose
// controls have been reported as hard to hit twice already. Six 104 px discs fit
// inside radius 181 with room to spare, and hand the middle of the panel to the
// hub instead of to two more cell corners.
#define PSLOT_CY 224       // the ring leaves a prompt band under the title and
#define PSLOT_RING 112     // clears the CLOSE bar below
#define PSLOT_R 50         // 100 px across -- 9.5 mm
// 94 px: the hub is the one control here that had room to reach the finger
// floor outright. The slots' inner edge is 62 px from the ring centre, so it
// can grow to 47 without touching them.
#define PSLOT_HUB_R 47
#define PSLOT_PROMPT_Y 68

bool clockOpen = false;       // pantalla de ajuste de hora (deslizar abajo)
int clockH = 12, clockM = 0;  // hora en edicion

// escena de bano: espuma sobre el bicho y limpieza al reventar
uint32_t bathUntil = 0;
bool bathPending = false;
struct { int16_t x, y; uint8_t r, ph; } bubbles[14];
uint32_t feedMenuUntil = 0;   // selector de comida abierto hasta este millis

// minijuego "toques": mantener la pokeball en el aire
bool gameOpen = false;
uint32_t gameOverUntil = 0;
// The ball game used to run until you missed three times or walked away, so a
// session had no length and the reward no shape. The bag is 10 s and the
// reaction test 15 s; this sits between them, and whichever comes first --
// the clock or three misses -- ends it.
#define GAME_MS 20000UL
uint32_t gameUntil = 0;
float ballX, ballY, ballVX, ballVY, gamePetX;
uint8_t gameScore, gameMisses;
float hitX, hitY;             // ultimo golpe (anillo de impacto)
uint32_t hitTime = 0;
bool gameNewHi = false;

// saco de entrenamiento (entrena la fuerza)
bool sackOpen = false;
uint32_t sackUntil = 0, sackOverUntil = 0;
uint16_t sackHits = 0;
float sackShake = 0;
uint8_t sackGain = 0;
bool sackNewHi = false;

// The training menu (the 5th icon): a list of GAMES, each row naming what it
// raises (v3.34, the owner's design). A game may train one stat, split its
// points across two (Pet::trainSplit), or raise joy and bond instead. One
// table, TRAIN_GAMES below, drives the rows, the subtitles and the taps.
bool trainOpen = false;
uint8_t trainPage = 0;
// Berry Catch: drag the basket, catch berries, dodge rocks. A joy/bond game --
// it is what makes petting fresh again (Pet::playJoyGame).
bool catchOpen = false;
uint32_t catchUntil = 0, catchOverUntil = 0, catchNextSpawn = 0;
float catchBasketX = 233, catchTarget = 233;
uint16_t catchScore = 0;
uint8_t catchJoyGain = 0, catchBondGain = 0;
uint32_t catchOuchUntil = 0;     // a caught rock flashes "-1" over the basket
#define CATCH_MAX 8
struct CatchThing { float x, y, vy; bool rock, live; };
CatchThing catchThings[CATCH_MAX];
#define CATCH_MS 20000UL
#define CATCH_BASKET_Y 352
#define CATCH_BASKET_HALF 46
void startCatch();
void leaveCatch();
void catchStep(uint32_t now);
void renderCatch();
// The hybrid training games (v3.34): each pays through Pet::trainSplit(), so
// a game training two stats gives half to each -- the same total as a
// single-stat game. One set of state, since only one can be open at a time.
enum { MG_NONE, MG_ROCK, MG_BLOCK, MG_TILT, MG_PUSH, MG_SIMON, MG_SEEK };
uint8_t miniGame = MG_NONE;
// Up here, not beside Block!: arduino-cli's generated prototypes take it.
struct BlockAttack { uint8_t side; uint32_t at, dur; bool live; };
void startRock();
void startBlock();
void startTilt();
void startPush();
void startSimon();
void startSeek();
void leaveMini();
void mgPress(int16_t x, int16_t y, uint32_t now);
void mgStep(uint32_t now);
void renderMini();
int mgLeaveStripY();

// move picker, opened from the MOVES card page. Most learnsets are level 0, so
// a level-up "you learned a move" prompt would almost never fire -- the moveset
// is edited on demand instead.
bool movePickOpen = false;
uint8_t movePickSlot = 0;   // which of the 4 slots is being replaced
uint8_t movePickParty = 0;  // 0 = the live pet, else the party slot + 1
uint8_t movePickPage = 0;
#define MOVE_ROW_Y(i) (96 + (i) * 58)
#define MOVE_PICK_PER_PAGE 5
#define MOVE_PICK_Y(i) (76 + (i) * 58)
// level-up learn prompt: modal, and deliberately without a timeout -- it
// decides what the creature is for the rest of its life, and once banked into
// the party, forever.
#define LEARN_ROW_Y(i) (104 + (i) * 56)
#define LEARN_SKIP_Y 334

// ---------- battle ----------
// The move menu is a 2x2 grid rather than four stacked rows: the round panel
// has to fit both creatures, both HP bars and the menu, and four full-width
// rows do not leave room for the sprites.
bool battleOpen = false;

enum : uint8_t {
  SCR_STARTER = 0, SCR_REGION, SCR_GALLERY, SCR_DEXPICK, SCR_MOVEPICK, SCR_BOX,
  SCR_PARTY, SCR_KEYBOARD, SCR_CARD, SCR_PLAYER, SCR_CLOCK, SCR_GYM, SCR_GYMPICK,
  SCR_EXPLORE, SCR_MART, SCR_LAN, SCR_PICK, SCR_BATTLE, SCR_WIN, SCR_LEARN, SCR_TRAIN, SCR_MENU,
  SCR_BAGSCR, SCR_GAME, SCR_MAIN, SCR_TRADE, SCR_AVATAR, SCR_BLACKOUT, SCR_AREA, SCR_COUNT
};
extern const char *const SCREEN_NAME[SCR_COUNT];   // const is internal linkage in C++

// Declared HERE, above every use. tools/emu/build.sh generates a proto.h with
// every prototype at the top, so the emulator compiled these happily while
// arduino-cli -- which relies on the IDE's auto-prototyping and does not always
// produce one -- did not. A green emulator build is not proof the firmware
// builds; only arduino-cli is.
void bootReport();
uint8_t uiCurrentScreen();
// The gesture model, defined next to uiCurrentScreen() but used by handleTouch()
// ~1000 lines above it.
int uiTileIndex();
void uiTileGo(int i);
void onRim(int dir);
uint8_t uiRimPages();
uint8_t uiRimPage();
bool uiOnRim(int16_t x, int16_t y);
float uiRimAngle(int16_t x, int16_t y);
void uiDrawRimBar();
void uiChrome();
uint8_t learnableList(uint8_t *out, uint8_t max);
uint8_t pickCandidates();
// Declared here because renderMonSheet() and the sheet's tap handlers use both
// ~4000 lines above where they are defined. The emulator generates a proto.h and
// would compile it either way; arduino-cli would not, and that has shipped once.
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
void renderParty();
void renderBox();
// The avatar picker and the blackout screen sit below the handlers that open
// them (v3.38).
uint8_t avatarPages();
void avatarCellPos(uint8_t slot, int *x, int *y);
int avatarAt(int16_t x, int16_t y);
void avatarPickTap(int16_t x, int16_t y);
void renderAvatarPick();
void renderBlackout();
void blackoutDismiss();
static void drawArt(const AvatarArt &a, int x, int y, int s);   // static: no generated prototype
// The bag and wild encounters are defined ~3000 lines below render() and the
// touch handler that reach them. arduino-cli does not always auto-prototype,
// and the emulator's generated proto.h would hide that -- see the note above.
void renderBag();
void bagTap(int16_t x, int16_t y);
void cordFromBag();
void cordPickTap(int16_t x, int16_t y);
uint8_t bagPages();
void renderMart();
void martTap(int16_t x, int16_t y);
uint8_t martPages();
bool startWildBattle(uint8_t region, bool hard);
PartyMon wildToPartyMon();
int backfillUids();
void focusSwap(uint8_t slot);
void drawConfirmPanel(const char *q, const char *sub1, const char *sub2,
                      uint16_t subCol, const char *o1, uint16_t c1, uint16_t t1,
                      const char *o2, uint16_t c2, uint16_t t2);
const char *const SCREEN_NAME[SCR_COUNT] = {
  "starter", "region", "gallery", "dexpick", "movepick", "box",
  "party", "keyboard", "card", "player", "clock", "gym", "gympick",
  "explore", "mart", "lan", "pick", "battle", "win", "learn", "train", "menu",
  "bag", "minigame", "main", "trade", "avatar", "blackout", "area"
};

// Badge art for a region, or nullptr when that region has none yet.
//
// NEVER `BADGES_ART[region % BADGE_REGIONS]`. The moment GYM_REGIONS outgrew
// BADGE_REGIONS -- which happened when Kalos landed and the upstream badge set
// stops at Unova -- 5 % 5 = 0 silently dressed Kalos in KANTO's badges, on both
// the win screen and the player card. A wrong badge is worse than an honest
// blank: it claims you won something you did not. The callers draw a
// placeholder instead, and static_assert below stops the array being indexed
// out of range whatever the two counts are.
static_assert(BADGE_REGIONS <= GYM_REGIONS,
              "more badge sheets than ladders -- BADGES_ART would be indexed by a region that does not exist");
static const BadgeArt *badgeArtFor(uint8_t region, uint8_t i) {
  if (region >= BADGE_REGIONS || i >= TRAINER_GYMS) return nullptr;
  return &BADGES_ART[region][i];
}

// Does this region have its own badge art? Split out with a primitive return
// type so the tests can ask: the emulator's genproto.py emits every prototype
// ABOVE the includes, where the BadgeArt struct does not exist yet.
bool badgeArtExists(uint8_t region, uint8_t i) { return badgeArtFor(region, i) != nullptr; }

Combatant btlYou, btlFoe;
bool btlOver = false;
bool btlWon = false;
bool btlNewBadge = false;
uint32_t btlWinUntil = 0;   // the win screen is up
// A gym or wild fight was lost: when its last message is dismissed, the
// blackout screen comes up and Mom sends you home (v3.38). Never for a LAN
// fight -- a friendly match with a rival is not a blackout.
bool btlLost = false;
bool blackoutOpen = false;
uint32_t blackoutAt = 0;
// A trainer fight is a run of 1v1s: both sides queue their squad and the next
// one steps up when the current one faints. This is the whole difficulty curve
// -- no gating, just attrition, so one strong creature sweeps Brock and dies
// four deep into Lance.
// The ladder is now sequential: a leader opens once the previous one is beaten,
// tracked per difficulty so hard mode is its own run. This replaces the earlier
// "no gating, attrition is the gate" rule -- with both ladders level-capped,
// nothing stopped you opening with Lance and simply losing, which read as a
// dead end rather than a challenge.
// reaction test (trains SPEED)
bool spdOpen = false;
uint32_t spdUntil = 0, spdOverUntil = 0, spdBorn = 0;
int16_t spdX = 0, spdY = 0;
uint16_t spdHits = 0, spdMisses = 0;
uint8_t spdGain = 0;
bool spdNewHi = false;

bool gymOpen = false;
bool gymHard = false;   // which ladder the list is showing

// LAN battle. `lanOpen` is the pairing screen; once both squads are known the
// normal battle screen takes over with btlLink set.
bool lanOpen = false;
Link lan;
// Trading (trade.h): one screen for the whole flow, opened from the LAN screen.
// `tradePicking` is its own creature picker on the party ring; `tradeAsk` the
// cannot-be-undone confirm in front of REVERT (1) or KEEP (2).
bool tradeOpen = false;
bool tradePicking = false;
uint8_t tradePickSrc = 0;      // 0 the party, 1.. a box page
uint8_t tradeAsk = 0;
bool tradeNoRadio = false;
Trade trade;
void renderTrade();
void tradeTap(int16_t x, int16_t y);
void tradeOpenScreen();
void tradeBoot();
void tradeLeave();
void tradePoll(uint32_t now);
bool tradeLocked(const PartyMon &m);
bool tradePending();
int thumbCell();
// What the shared region chooser is being used FOR. It only changes the
// subtitle and whether there is a way back: at first boot every count would
// read zero, which tells the player nothing, and there is nowhere to go back to.
#define RPICK_FOR_GYMS  0
#define RPICK_FOR_DEX   1
#define RPICK_FOR_START 2
// Three rows is what the round panel fits above the BACK label. The dex now
// lists four regions and will list more, so the chooser PAGES rather than
// growing -- and every paged screen in this sketch has to be driven by
// swipe_test, which is why that test exists at all.
#define RPICK_PER_PAGE  3
extern uint8_t rpickPage;
static void renderRegionPick(uint8_t mode);   // the region chooser, defined below
// Not static: swipe_test drives these so it asks the FIRMWARE for the page
// count instead of recomputing it from its own copy of RPICK_PER_PAGE, which
// would prove the transcription rather than the screen.
uint8_t rpickRegions(uint8_t mode);           // rows this mode lists (gyms: 3)
uint8_t rpickPageCount(uint8_t mode);
uint8_t rpickModeNow();                       // which chooser is up, 0xFF for none
uint8_t rpickModeNow();                       // which chooser is up, or 0xFF
static bool rpickSwipe(int dir);              // true if it handled the gesture
static int regionPickTap(int16_t x, int16_t y, uint8_t mode);
// Pure hit tests -- no sound, no state -- shared by the tap handlers, the
// renderer and the gesture resolver, so they can never disagree about where a
// control is.
int rpickRowAt(int16_t x, int16_t y, uint8_t mode);
bool lanBtnHit(int16_t x, int16_t y);
bool rpickBackHit(int16_t x, int16_t y, uint8_t mode);
bool uiOnControl(int16_t x, int16_t y);
static void drawEggRegion();          // defined with the egg screen helpers
static int eggRegionTap(int16_t x, int16_t y);
static void drawBtlBack();
static void btlLinkPoll();   // defined with the battle code, called from render()
static void btlSwitchTo(uint8_t i);
static void btlResolve(uint8_t yourMove);
// The peer's whole team, kept live. A trainer's replacements are built fresh
// from TRAINERS[] because they only ever arrive once; a linked opponent can
// switch OUT and back IN, so its creatures have to remember how battered they
// are. Host side only -- the guest takes absolute health off the wire.
Combatant btlFoeSquad[TRAINER_TEAM_MAX];
uint8_t btlFoeSquadN = 0;
uint8_t btlMyAct = 0;        // host: our own action, latched until theirs lands
// Which ladder the gym screen and the current fight belong to. The battle keeps
// its own copy so that leaving the gym list mid-fight cannot retarget the badge.
// The BOX button on the party screen. It was 150x32 with a pixel-exact hit
// test, which is a small target on a round panel -- reported as hard to press,
// same complaint as the battle grid. Now bigger AND padded: the drawn size grew
// too, so the button looks like the size it actually is rather than hiding a
// generous hit area behind a small graphic.
// The smallest a button may be. Three separate "hard to hit" reports -- the
// battle grid's bottom row, the party screen's BOX, and the LAN button -- were
// all the same mistake: a control sized to fit its label rather than a finger.
//
// 44 is the number everyone quotes, but it is 44 POINTS. This panel is 466 px
// across 1.75 in = 266 ppi, so a pixel is 0.095 mm and 44 px is 4.2 mm, under
// half a fingertip. It survives as the HARD FLOOR because several laid-out
// screens cannot grow without being redesigned, but nothing new may use it.
#define UI_TAP_MIN 44
// 9 mm -- what watchOS and Wear OS both land on -- in this panel's pixels.
// Every control laid out from here on is held to THIS by hit_test.
#define UI_TAP_FINGER 94

// The LAN battle button on the gym region chooser. Finger-sized: at 44 px with
// a "tap below = back" zone right under it, a slightly low tap left the gym
// tile and dropped the player on the pet screen -- reported from a board as
// "LAN BATTLE only works with a very deliberate tap". It now runs down to just
// above the tile dots, and the gym chooser has no back zone at all: back is a
// swipe down there, as on every screen. Its corners stay inside the glass
// (radius ~219 of 233 at y 430).
#define LANBTN_W 190
#define LANBTN_H UI_TAP_FINGER
#define LANBTN_X (233 - LANBTN_W / 2)
#define LANBTN_Y 336
// The TRADE button on the LAN screen, under HOST and JOIN (y 120..246).
// Finger-sized, and it stops 20 px short of that screen's tap-to-go-back zone
// (y > 370).
#define LANTRADE_X 90
#define LANTRADE_W 286
#define LANTRADE_Y 256
#define LANTRADE_H UI_TAP_FINGER
static_assert(LANTRADE_Y + LANTRADE_H + 20 <= 370, "keep a dead band above the LAN screen's back zone");
// The Pokedex chooser has no LAN button, so it keeps its BACK label and the
// tap zone under it.
#define RPICK_BACK_LABEL_Y 392
#define RPICK_BACK_ZONE_Y 380

// The BOX button is the hub in the middle of the party ring now (PSLOT_HUB_R),
// so it has no rectangle of its own any more.
// CLOSE sits below BOX with a real gap between them. They used to touch at
// y=372, and BOX's padding then reached to 380 -- so the top of CLOSE was
// inside BOX's hit area and taps there opened the box instead of closing the
// screen. Padding one button into its neighbour just moves the problem along.
#define PARTYCLOSE_Y 376
#define PARTYCLOSE_H UI_TAP_MIN
#define PARTYCLOSE_X 133
#define PARTYCLOSE_W 200

// The confirm panel, in ONE place. Three callers draw it -- the evolve/farewell/
// retire dialog on the main screen, and letting a banked creature go from the
// party or box sheet -- and hit_test asserts against the same numbers through
// uiConfirmRects(). Copies of this geometry in the tap handler are exactly how
// a YES button ends up somewhere the drawing is not.
#define CONFIRM_X 73
#define CONFIRM_Y 156
#define CONFIRM_W 320
#define CONFIRM_H 188
#define CONFIRM_BTN_X 93
#define CONFIRM_BTN_W 280
#define CONFIRM_BTN_H 52
#define CONFIRM_B1_Y 206
#define CONFIRM_B2_Y 268

// The two buttons on the party/box detail sheet. A third full-width row would
// not fit above BACK, and BRING BACK was h=38 -- under UI_TAP_MIN, the shape
// section 4 of CLAUDE.md keeps warning about.
//
// THEY ARE NOT EQUAL HALVES, and that is the whole point. The first version
// centred the pair on 233 with a dead gap between them -- which put the gap at
// the CENTRE OF THE PANEL, the one place a thumb naturally lands, so BRING BACK
// "did not work" and was reported as broken. Worse, every build up to v3.5 drew
// BRING BACK as a FULL-WIDTH button centred exactly there, so muscle memory
// aimed straight at the dead zone.
//
// The primary action now owns the centre. RELEASE is narrower, offset right,
// and still over UI_TAP_MIN -- it is irreversible, so it should be reachable
// but never the thing you hit by aiming at the middle. The gap between them is
// wider than before, not narrower.
#define PDET_BTN_Y 336
#define PDET_BTN_H 48
#define PDET_L_X 70
#define PDET_L_W 180
#define PDET_R_X 266
#define PDET_R_W 120
static_assert(PDET_L_X < 233 && 233 < PDET_L_X + PDET_L_W,
              "the panel centre must land on the sheet's PRIMARY button, not between the two");
static_assert(PDET_R_X > PDET_L_X + PDET_L_W + 8,
              "the destructive button needs a real dead gap before it");

uint8_t gymRegion = 0;
uint8_t btlRegion = 0;
#define TRAINERS (TRAINER_SETS[gymRegion % GYM_REGIONS].list)
#define BTL_TRAINERS (TRAINER_SETS[btlRegion % GYM_REGIONS].list)
bool gShowAllAvatars = false;  // emulator screenshot aid, never set on hardware
bool btlPetIn = false;       // was the live pet in the squad?
uint8_t btlTrainGain = 0;    // what the win trained, for the win screen
uint8_t btlTrainWhich = 0;
bool btlLink = false;      // this fight is against another device
bool btlLinkHost = false;

// "Waiting for the rival" has no upper bound of its own -- the idle heartbeat
// that keeps the link alive during a slow decision looks identical to a peer
// that will never answer. Rather than guess a timeout on the player's behalf,
// a QUIT button appears once the wait has run a while, and the player judges
// for themself. Both host and guest can be the one purely waiting (see
// lanIsWaiting()), so this is not a host/guest split.
#define LAN_QUIT_AFTER_MS 30000
bool lanQuitConfirm = false;
uint32_t lanWaitSince = 0;   // 0 = not currently waiting on the rival

// True whenever this device has done everything it can for the current turn
// and is purely waiting on the rival's radio -- render() and battleTap() must
// agree on this or a tap that looks swallowed by the wait screen could still
// land on whatever is invisibly underneath it.
static bool lanIsWaiting() {
  return btlLink && !btlOver &&
         (btlLinkHost ? (btlMyAct && !lan.hasPeerAct())
                      : (lan.state == LINK_WAITING));
}
static bool gymUnlocked(uint8_t idx, bool hard) {
  return idx == 0 || pet.hasBadge(gymRegion, idx - 1, hard);
}

// Team select. Candidate 0 is the live pet, 1..PARTY_SLOTS are the banked
// members, so one bitmask covers the whole pool.
bool pickOpen = false;
// The team picker serves the gym ladder and the LAN screen both. PICK_LAN is
// not a trainer index: squadCap() already returns an uncapped six for anything
// past the roster, which is what a LAN battle wants -- two players who know
// each other can bring what they like.
#define PICK_LAN 0xFF
static void lanOffer(bool host);
uint8_t pickTrainer = 0;
bool pickHard = false;
// Which region the picker was opened FOR, snapshotted when it opens -- the same
// arrangement as btlRegion, and for the same reason. gymRegion belongs to the
// chooser and the player can move it; anything the picker decides has to keep
// pointing at the ladder it was opened from. Forgetting to fill in btlRegion is
// what sent Brock's Onix into every region's gym (issue #4), so the two reads
// this alias replaces are the same shape one screen earlier.
uint8_t pickRegion = 0;
#define PICK_TRAINERS (TRAINER_SETS[pickRegion % GYM_REGIONS].list)
bool lanWantHost = true;   // which button opened the picker
uint16_t squadMask = 0xFFFF;   // everything, until the player says otherwise
uint8_t pickPage = 0;
#define PICK_PER_PAGE 6
#define PICK_CELL_W 150
#define PICK_CELL_H 74
#define PICK_X(i) (78 + ((i) % 2) * (PICK_CELL_W + 10))
#define PICK_Y(i) (86 + ((i) / 2) * (PICK_CELL_H + 6))
#define PICK_GO_Y 350
// BACK beside FIGHT on the team-select screen. There was no way out of it but a
// swipe, which is invisible -- the same complaint as everywhere else.
#define PICK_BTN_W 155
#define PICK_BTN_H UI_TAP_MIN
#define PICK_BACK_X (233 - PICK_BTN_W - 7)
#define PICK_GO_X (233 + 7)
bool playerOpen = false;
// One badge page per gym region, then the medals. Three ladders will not fit on
// one page, and the page you are on IS the region -- no extra control needed,
// and horizontal paging already works everywhere else.
uint8_t playerPage = 0;
// The avatar picker, opened from the avatar on the player card (v3.38). There
// are too many trainer classes now to step through one tap at a time.
bool avatarPickOpen = false;
uint8_t avatarPage = 0;
// A 3x3 grid of finger-sized cells: UI_TAP_FINGER each, and the grid's corners
// stay inside the glass (radius ~205 of 233).
#define AV_COLS 3
#define AV_PER_PAGE 9
#define AV_CELL UI_TAP_FINGER
#define AV_GRID_Y 96
#define PLAYER_PAGES (GYM_REGIONS + 1)
#define playerBadgeRegion (playerPage % GYM_REGIONS)
uint8_t gymPage = 0;
#define GYM_ROWS 5
#define GYM_ROW_Y(i) (110 + (i) * 50)
#define GYM_ROW_H 44
#define GYM_ROW_INSET 18   // how far a row stops short of the glass, so the
                           // arc scrollbar at RIM_BAR_R has its own lane
void gymRowRect(int i, int *x, int *y, int *w, int *h);
// LAN remains attached to the trainer ladder; Explore is a peer destination on
// the tile axis and no longer hides behind GYMS.
#define GYMBTN_Y 380
#define GYMBTN_W 190
#define GYMBTN_H UI_TAP_MIN
#define GYMBTN_X (CX - GYMBTN_W / 2)
int8_t btlTrainer = -1;      // index into TRAINERS, -1 = a one-off fight
bool btlHard = false;
Combatant btlSquad[TRAINER_TEAM_MAX + 1];
uint8_t btlSquadN = 0, btlSquadAt = 0;
uint8_t btlFoeAt = 0;

// ---------------------------------------------------------------------------
// The bag, and wild encounters.
bool exploreOpen = false;
bool exploreHard = false;
uint8_t exploreRegion = 0xFF;
// The ALL region in Explore is switched off for now (owner's call, 2026-10-21):
// the code stays and still compiles, and setting this to 1 brings it back.
#define EXPLORE_ALLOW_ALL 0
// Explore's areas (wild_areas.h). The area chosen in each region, stored as
// index + 1 so 0 means "not chosen yet". RAM only, like exploreRegion: an
// encounter is not saved, so neither is where it was looked for.
uint16_t exploreAreaPick[REGION_COUNT] = { 0 };
bool areaListOpen = false;   // SCR_AREA, the paged list of a region's areas
uint8_t areaPage = 0;
#define AREA_ROWS 3
#define AREA_ROW_Y(i) (108 + (i) * 100)
#define AREA_ROW_H UI_TAP_FINGER
// Explore's layout, top to bottom: the title, the region selector, the area
// row, the difficulty pill and the big action. Rows the tests aim at are
// functions (exploreAreaRect(), exploreBtnRect()), never numbers to re-type.
#define EXPLORE_REGION_X 78
#define EXPLORE_REGION_Y 86
#define EXPLORE_REGION_W 310
#define EXPLORE_REGION_H UI_TAP_FINGER
#define EXPLORE_REGION_ARROW_W UI_TAP_FINGER
#define EXPLORE_AREA_Y 184
#define EXPLORE_AREA_H UI_TAP_FINGER
#define EXPLORE_DIF_Y 282
#define EXPLORE_DIF_H UI_TAP_MIN
// 72 + 2 x 11 of padding = the 94 px finger floor hit_test holds every primary
// control to; the padding starts below the difficulty pill, so the two never
// share a pixel (the pill is checked first and would win it).
#define EXPLORE_BTN_Y 340
#define EXPLORE_BTN_H 72
#define EXPLORE_BTN_PAD 11
static_assert(EXPLORE_DIF_Y + EXPLORE_DIF_H < EXPLORE_BTN_Y - EXPLORE_BTN_PAD,
              "the Explore button's hit area would reach into the difficulty pill");
bool bagOpen = false;
uint8_t bagPage = 0;
#define BAG_PER_PAGE 5
#define BAG_ROW_Y(i) (112 + (i) * 58)

// ---------------------------------------------------------------------------
// The Poke Mart. Between EXPLORE and GYM on the tile axis: walk to earn,
// spend at the Mart, then prove it at the gym.
bool martOpen = false;
uint8_t martPage = 0;
// The item pending a buy confirmation; ITEM_NONE means no dialog is up. A
// value rather than a bool, like partyDetail/galleryDetail, since the dialog
// needs to remember WHICH row was tapped.
ItemKey martConfirmItem = ITEM_NONE;
#define MART_PER_PAGE 3
// Rows start well below the wallet block -- it is bold/size-4 and, per
// uiSafeHalfWidth(), needs to sit lower on the round panel than a plain title
// to have the chord width to not hang off the glass. See renderMart().
#define MART_ROW_Y(i) (210 + (i) * 60)

// A wild fight is a different fight, not a trainer fight with a flag: a ball
// works, the foe can run, and RUN is a roll rather than a certainty. Every one
// of those is asked through btlWild, so there is no second opinion about what
// kind of battle is on screen.
bool btlWild = false;
// The wild creature's durable state, kept BESIDE the Combatant because a
// Combatant carries no IVs and none of what a stored record needs. Without it a
// capture would have to re-roll the individual it just caught, and the creature
// you fought would not be the creature you kept.
int16_t wildDex = 0;
uint8_t wildLvl = 1;
uint8_t wildIv[4] = { 0, 0, 0, 0 };   // atk, def, spe, hp
int8_t wildLegend = -1;   // the WILD_LEGENDS index being fought, or -1 (wild.h)
bool wildShiny = false;
bool wildCaught = false;
// What a win handed over, for the settlement lines on the win screen.
ItemKey wildDrops[4] = { ITEM_NONE, ITEM_NONE, ITEM_NONE, ITEM_NONE };
uint8_t wildDropN = 0;
uint8_t btlBagPage = 0;

// Animation. Deliberately built on the thumbnails the screen already draws
// rather than on PmdMon: three PmdMon blobs are live already, and the battle
// has to stay graceful on a board with no SD at all (S_NO_SPRITES). Index 0 is
// you, 1 is the foe.
uint32_t btlLungeUntil[2] = { 0, 0 };   // acted: leans toward the opponent
uint32_t btlHitUntil[2] = { 0, 0 };     // was hit: jitters and flashes
uint16_t btlHpShown[2] = { 0, 0 };      // bars ease toward the real value
// Two streamed sprites, so the creatures can actually swing and flinch. They
// cost ~135 KB of PSRAM each on average and are freed when the fight ends. The
// player's side is NOT the global `pmd`: the active creature may be a banked
// party member rather than the live pet.
PmdMon btlPmd[2];
int16_t btlPmdDex[2] = { 0, 0 };
// A faint used to swap the next creature in instantly, inside the same call
// that resolved the turn -- which is why it felt like a jump cut. The swap is
// now deferred: the fainted one drops out of frame, and the replacement slides
// in only once the player dismisses that message.
uint32_t btlFaintUntil[2] = { 0, 0 };
uint32_t btlEnterUntil[2] = { 0, 0 };
int8_t btlSwapWho = -1;        // 0 = your side, 1 = the foe's, -1 = nothing due
#define BTL_FAINT_MS 700
#define BTL_ENTER_MS 420
// battle menu: 0 = FIGHT/POKEMON, 1 = the moves, 2 = the switch list
uint8_t btlMenu = 0;
#define BTL_LUNGE_MS 260
#define BTL_HIT_MS 420
char btlMsg[6][40];
uint8_t btlMsgCount = 0;   // queued lines; a tap shows the next
#define BTL_CELL_W 160
#define BTL_CELL_H 54
#define BTL_GRID_X 69
#define BTL_GRID_Y 270
#define BTL_GRID_H (BTL_CELL_H * 2 + 8)
#define BTL_CELL_X(i) (BTL_GRID_X + ((i) % 2) * (BTL_CELL_W + 8))
#define BTL_CELL_Y(i) (BTL_GRID_Y + ((i) / 2) * (BTL_CELL_H + 8))

// A cell's HIT area is bigger than the cell that is drawn. On the board the two
// bottom buttons were much harder to hit than the top two: the drawn cells are
// only 44 px tall, there was an 8 px dead gap between the rows, and everything
// below the bottom row was dead too -- so a finger landing low, or a touch panel
// reading a few pixels high, missed entirely. Nothing else is tappable in this
// area while the grid is open, so the slop costs nothing.
//
// The gap between the two rows and the two columns is split down the middle, and
// the bottom row additionally claims the empty space beneath it.
#define BTL_HIT_PAD 4
// The bottom row keeps extra room downward, but not so much that it reaches the
// BACK bar below it -- the mistake made once already with BOX and CLOSE.
#define BTL_HIT_BOTTOM 6
// BACK, under the grid: the move and switch screens had no way out except
// choosing something.
// The gym list's EASY/HARD pill. It was 24 px tall, which is half a fingertip.
// Sits between the title and the first leader row. At 72 with a 44 px height it
// ran to 116 and overlapped the first row, which begins at 110 -- introduced
// when the pill was enlarged to a real tap target.
#define GYMDIF_Y 60
#define GYMDIF_H UI_TAP_MIN
#define BTL_BACK_W 190
#define BTL_BACK_H UI_TAP_MIN
#define BTL_BACK_X (233 - BTL_BACK_W / 2)
#define BTL_BACK_Y 396
#define BTL_HIT_X0(i) (BTL_CELL_X(i) - BTL_HIT_PAD)
// The far edges stop one pixel short so the four boxes TILE: the gap between
// two cells is split down the middle with no pixel left over and none shared.
#define BTL_HIT_X1(i) (BTL_CELL_X(i) + BTL_CELL_W + BTL_HIT_PAD - 1)
#define BTL_HIT_Y0(i) (BTL_CELL_Y(i) - BTL_HIT_PAD)
#define BTL_HIT_Y1(i) (BTL_CELL_Y(i) + BTL_CELL_H - 1 + \
                       ((i) / 2 ? BTL_HIT_BOTTOM : BTL_HIT_PAD))

// Which cell a point falls in, or -1. Exposed (not static) so a test can sweep
// the panel and prove there are no dead pixels between the cells -- the bug that
// made the bottom row hard to press was a gap, not a wrong rectangle.
int btlCellIndexAt(int16_t x, int16_t y);

// Used by BOTH the move grid and the switch grid. They had a copy each of the
// same rectangle test, which is exactly how two halves of one control drift.
static inline bool btlCellHit(int i, int16_t x, int16_t y) {
  return x >= BTL_HIT_X0(i) && x <= BTL_HIT_X1(i) &&
         y >= BTL_HIT_Y0(i) && y <= BTL_HIT_Y1(i);
}

// las 9 especies con sprite propio en flash (respaldo sin SD): dex -> indice
int flashIdxForDex(int16_t dex) {
  static const int8_t IDX[10] = { -1, 3, 4, 5, 0, 1, 2, 6, 7, 8 };
  return (dex >= 1 && dex <= 9) ? IDX[dex] : -1;
}

#define CX 233  // centro de la pantalla redonda
#define CY 233
#define UI_R 233   // the glass is a CIRCLE, and every full-width layout must ask
#define HEADER_NAME_Y 52
#define HEADER_NAME_H 24
#define PET_CY 202  // centro vertical del sprite

// Half the usable width at row y: the chord of the panel at that height.
//
// A layout that picks ONE width for every row is wrong twice -- it wastes the
// middle, where the full 466 px is available, and it hangs the corners of its
// top and bottom rows off the glass. Both had shipped: the gym ladder drew five
// identical 326 px rows over y 110..374, where the real chord runs 448 down to
// 372. Derive it, never restate it; this is the only place the circle is solved.
int uiSafeHalfWidth(int y) {
  int dy = y - CY;
  if (dy <= -UI_R || dy >= UI_R) return 0;
  return (int)sqrtf((float)(UI_R * UI_R - dy * dy));
}

// The narrower of the two chords a box spanning y0..y1 must fit inside. A
// rounded row is widest at its vertical middle, but its CORNERS are what leave
// the circle, so it has to be sized by its worst edge rather than its centre.
int uiSafeHalfWidthFor(int y0, int y1) {
  int a = uiSafeHalfWidth(y0), b = uiSafeHalfWidth(y1);
  return a < b ? a : b;
}

// An arc segment: Arduino_GFX has no arc primitive, so it is stamped from
// overlapping dots. Angles are degrees clockwise from 12 o'clock.
void uiArc(int cx, int cy, int r, int a0, int a1, int w, uint16_t col) {
  if (a1 < a0) { int t = a0; a0 = a1; a1 = t; }
  int rad = w / 2;
  if (rad < 1) rad = 1;
  int n = (int)(((a1 - a0) * 0.01745f * r) / rad) + 1;   // dots must overlap
  for (int i = 0; i <= n; i++) {
    float ang = ((float)a0 + (float)(a1 - a0) * (float)i / (float)n - 90.0f) * 0.01745329f;
    gfx->fillCircle(cx + (int)(cosf(ang) * r), cy + (int)(sinf(ang) * r), rad, col);
  }
}

// The gesture model. Its tables and handlers live down beside uiCurrentScreen(),
// but handleTouch() and onSwipe() sit ~1000 lines above them.
#define TILE_COUNT 7   // PLAYER . PARTY . [PET] . EXPLORE . MART . GYM . DEX
#define TILE_PET 2
#define RIM_R0 186     // a drag starting outside this radius pages, not swipes
#define RIM_STEP 20    // degrees of arc per page
#define RIM_BAR_R 222  // where the arc scrollbar is drawn
// 2 to 5 o'clock. Angles are clockwise from 12, so this is the right flank --
// the top is the title's and the bottom is the tile dots'.
#define RIM_BAR_A0 55
#define RIM_BAR_A1 145

// A gym row's rectangle -- as wide as the panel allows AT ITS OWN HEIGHT, so
// the ladder bulges in the middle instead of throwing away 66 px there and
// hanging its top and bottom corners off the glass. The draw path and the tap
// path both ask this; they used to share the literals 70 and 396.
void gymRowRect(int i, int *x, int *y, int *w, int *h) {
  int ry = GYM_ROW_Y(i);
  int half = uiSafeHalfWidthFor(ry, ry + GYM_ROW_H) - GYM_ROW_INSET;
  if (x) *x = CX - half;
  if (y) *y = ry;
  if (w) *w = half * 2;
  if (h) *h = GYM_ROW_H;
}

static const uint16_t INK_K = 0x18C4;  // spriteColor('k')

// botones de icono siguiendo el arco inferior de la pantalla redonda
// (los exteriores van mas altos para no salirse del circulo)
struct Btn {
  int16_t cx, cy;
  const char *const *icon;
};
// Five across the arc: spacing tightened 62 -> 54 so the outer pair stays far
// enough in to keep its old y. Lifting them instead would have run the row into
// the ENE/HYG bars, which end at y=361 -- the buttons are 52 tall, so any centre
// above 387 overlaps them.
// Four, not five. The ball had its own icon here until it became DEFENCE's
// trainer and moved into the training menu -- at which point tapping it just
// opened the same menu the dumbbell does, two icons for one destination.
//
// They sit on the panel's curve: y = 372 - dx^2/1100, the outer pair lifted so
// the row stays on glass. At 80 px and 88 px apart the outermost reaches radius
// 220 of 233 -- the row cannot grow again without losing an icon, because four
// UI_TAP_FINGER targets need 392 px of chord and reach radius 222.
#define BTN_COUNT 4
// Referred to by NAME, never by literal index. Removing the ball icon shifted
// every index by one and drawButtons() still had `i != 2` meaning LIGHT -- which
// silently made the BATH button the one that wakes the pet.
#define BTN_FOOD  0
#define BTN_LIGHT 1
#define BTN_BATH  2
#define BTN_TRAIN 3
Btn buttons[BTN_COUNT] = {
  {  92, 352, SPR_ICON_FOOD },   // comer
  { 186, 370, SPR_ICON_LIGHT },  // luz
  { 280, 370, SPR_ICON_CLEAN },  // bano
  { 374, 352, SPR_ICON_TRAIN },  // entrenar
};
// The GRAPHIC is 72 px and the HIT AREA is 94. They were the same 80 px, which
// looked heavy without actually reaching the finger floor -- the usual answer
// here is a target bigger than the thing it draws, which is what the battle
// grid and the egg pill already do. At a 94 px pitch the four hit discs are
// exactly tangent: no overlap to make a tap ambiguous, no gap to swallow one.
#define BTN_HALF 36
#define BTN_HIT 47

// grietas del huevo (pixeles 'k' sobre el sprite)
static const uint8_t CRACK1[][2] = { {15,8},{16,9},{15,10} };
static const uint8_t CRACK2[][2] = { {11,13},{12,14},{11,15},{20,12},{19,13},{20,14} };
// estrellas del modo noche
static const uint16_t STARS[][2] = { {120,140},{330,120},{370,210},{95,230},{280,90},{160,95} };

bool wasPressed = false;
// eleccion de inicial (primera partida): Bulbasaur / Charmander / Squirtle, 3 filas
// The first-boot starter list is the FRONT of each region's starter array in
// dex.h -- not a copy of it. That array is also the pool a region's first egg
// is drawn from (pet.cpp rollInRegion), where Kanto's five deliberately include
// Pikachu and Eevee; the choice screen shows the canonical three and leaves the
// rest to the egg. starter_test pins the first three of every region, so
// reordering that array cannot silently change the first screen anyone sees.
#define STARTER_SHOWN 3
int16_t starterOf(uint8_t region, uint8_t i) {
  const RegionInfo &rg = REGIONS[region % REGION_COUNT];
  if (i >= rg.starterCount) i = 0;
  return rg.starters[i];
}
uint8_t starterCountShown(uint8_t region) {
  uint8_t n = REGIONS[region % REGION_COUNT].starterCount;
  return n < STARTER_SHOWN ? n : STARTER_SHOWN;
}

// First boot runs region -> starter. This is NOT persisted: a reset between the
// two lands back on the region, which is the harmless direction to fail in --
// nothing has been chosen yet, and pet.setRegion() is idempotent.
static bool starterRegionDone = false;
#define STARTER_ROW_Y 110
#define STARTER_ROW_H 70
#define STARTER_ROW_GAP 8
// boton-CTA de evolucion (centrado, mitad de pantalla)
#define EVO_BTN_W 256
#define EVO_BTN_H 64
#define EVO_BTN_X (CX - EVO_BTN_W / 2)
#define EVO_BTN_Y 172
// boton-CTA de despedida (mas ancho: lleva el nombre + frase)
#define FAR_BTN_W 408
#define FAR_BTN_H 58
#define FAR_BTN_X (CX - FAR_BTN_W / 2)
#define FAR_BTN_Y 176

// The food popup. It had its rectangle written out twice -- once where it is
// drawn and once in the tap handler -- which is the shape that drifts the
// moment either moves, and it had to move: the home row grew up into it.
#define FEED_MENU_X 101
#define FEED_MENU_Y 236
#define FEED_MENU_W 264
#define FEED_MENU_H 64
// el CST9217 avisa por el pin INT cuando hay datos tactiles; lo usamos para no
// leer el bus I2C mientras el chip esta dormido (esa lectura se colgaba ~1s)
volatile bool gTouchIrq = false;
void IRAM_ATTR touchIsr() { gTouchIrq = true; }
uint32_t lastRender = 0;
// proteccion del AMOLED: atenuado por inactividad
uint32_t lastInteract = 0;
uint8_t dimStage = 0;        // 0 despierto, 1 atenuado (90s), 2 casi apagado (5min)
bool swallowGesture = false; // el toque que despierta no acciona nada
uint32_t holdStart = 0;     // pulsacion larga sobre el bicho
uint32_t confirmUntil = 0;  // dialogo "soltar?" activo hasta este millis
uint8_t choiceKind = 0;     // dialogo de decision: 0 ninguno, 1 evolucion, 2 despedida
uint32_t choiceUntil = 0;   // se cierra solo a este millis
int16_t tX0, tY0, tXl, tYl; // gesto en curso (inicio y ultima posicion)
// Touch noise. The CST9217 occasionally reports one reading far from where the
// finger is -- and a single stray just before lift-off became the gesture's END
// point, so a tap on LAN BATTLE read as a swipe down and left the screen.
//
// A plain distance cap cannot tell that from a real flick: at 0.095 mm a pixel,
// a 1 m/s flick covers ~200 px between 20 ms polls. So a reading that jumps
// more than TOUCH_JUMP_PX is HELD, not accepted. If the next reading carries on
// from it (is closer to it than to the last accepted point), the finger really
// moved and both are taken; if the next one comes back, or the finger lifts
// first, it was noise and is dropped.
#define TOUCH_JUMP_PX 60
#define TOUCH_MOVING_PX 30            // "already travelling" at lift-off
bool tHeld = false;
int16_t tHeldX, tHeldY;
// How many accepted polls moved the finger at least TOUCH_STEP_PX in each
// direction. A swipe that starts on a control must show TOUCH_SWIPE_STEPS of
// progress its own way; anything less from a control is a tap.
#define TOUCH_STEP_PX 4
#define TOUCH_SWIPE_STEPS 2
uint8_t tStepsL, tStepsR, tStepsU, tStepsD;
bool tOnControl = false;
uint32_t tStart = 0;
bool holdFired = false;
// Rim-drag state. Decided at touch-down: a gesture that starts outside RIM_R0
// is a scroll for its whole life, and never also resolves as a swipe or a tap.
bool rimActive = false;
bool rimMoved = false;
float rimA0 = 0;
int rimSteps = 0;

// NVS has just been wiped (WIPE) or replaced (IMPORT) and we are waiting to
// restart. Nothing may write the in-RAM creature back over it in the meantime.
bool saveInhibited = false;

// Flush a pending save because power is about to go away: the PWR key on its way
// to the AXP2101's hardware cut, the battery's low warning, or a software
// restart. THE single answer for all three, so the one case where it must NOT
// happen cannot be forgotten by whoever adds the fourth -- see CLAUDE.md § "A
// rule enforced in one path but not in its twin", which is a list of what
// happens when three callers each keep their own copy of a guard.
//
// Deliberately ignores the dim/off gate that the periodic autosave in loop()
// obeys. That gate exists so a ~1 s flash write is never visible mid-animation;
// a visible stutter is a far smaller price than the last few minutes of a
// creature's life.
void flushBeforePowerLoss(const char *why) {
  if (saveInhibited) return;
  if (!pet.savePending()) return;
  Serial.printf("save: flushing (%s)\n", why);
  pet.flushSave();
}

void setup() {
  Serial.setRxBufferSize(8192);  // la transferencia a SD llega en bloques de 2 KB
  Serial.begin(115200);
  // CRITICO: sin esto, Serial.print BLOQUEA el juego cuando no hay un
  // monitor serie abierto en el host (el bufer TX del USB CDC se llena
  // y nadie lo vacia) -> con timeout 0 los mensajes se descartan
  Serial.setTxTimeoutMs(0);
  Serial.printf("TamaPoke fw v%s\n", FW_VERSION);
  bootReport();   // why the last run ended, and what it was doing
  loadLang();  // idioma guardado (ES por defecto)
  Wire.begin(IIC_SDA, IIC_SCL);
  // CST9217 (tactil), AXP2101 (PMU) y PCF85063 (RTC) comparten este bus I2C.
  // Red de seguridad para PMU/RTC (SensorLib NO respeta este timeout en el
  // tactil; el cuelgue del tactil dormido se resuelve gateando por INT, ver
  // handleTouch).
  Wire.setTimeOut(50);

  // CRITICO: encender la alimentacion del panel (BLDO1=OLED VDD 3.3V) ANTES de
  // inicializar el display. Si el PMU se reseteo (drenaje total), este rail
  // queda OFF y la pantalla se ve negra aunque el resto de la placa funcione.
  pmuEnablePanel();

  // QSPI a 80MHz (por defecto 40): el flush del framebuffer es el cuello de
  // botella del fps (~56ms a 40MHz). Si el panel mostrara basura, bajar a 40M.
  if (!gfx->begin(80000000)) Serial.println("gfx->begin() failed");
  panel->setBrightness(180);

  touch.setPins(TP_RESET, TP_INT);
  bool touchOk = false;
  for (int i = 0; i < 3 && !touchOk; i++) {  // a veces falla al primer intento
    touchOk = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL);
    if (!touchOk) delay(150);
  }
  if (!touchOk) Serial.println("CST9217 not detected");
  // begin() deja el chip en modo comando (lee la identidad y no sale);
  // hace falta un reset por hardware para que vuelva a reportar toques
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);  // el panel esta montado girado 180 grados
  // INT activo-bajo: salta cuando hay datos. Gatea las lecturas I2C (ver loop)
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);

  party.begin();
  bag.begin();
  pet.begin();
  backfillUids();
  sdBegin();
  thumbs.load();

  // reloj real: aplica el tiempo que estuvo apagado
  rtcBegin();
  batBegin();
  pwrSetup();
  if (!pedoBegin()) Serial.println("pedometer: no QMI8658 -- the Mart earns nothing this session");
  // After the party, the RTC and the SD scan: it may finish a trade the power
  // cut (party writes), stamps a time, and reopens a pending trade's screen.
  tradeBoot();
  uint32_t e = rtcEpoch();
  if (e == 0) {
    rtcSetEpoch(1767225600UL);  // RTC virgen: semilla (la hora absoluta da igual,
    e = rtcEpoch();             // solo importan las diferencias)
    Serial.println("RTC has no time: seeded, no offline progression this time");
  }
  pet.syncClock(e);

  audioBegin();  // ES8311 + I2S + amplificador (suena un jingle de arranque)

  // After everything has loaded, so the figure reflects a real save rather than
  // an empty one. A full partition is erased WHOLE by the core on the next boot,
  // so this number trending down over a soak test is the only warning there is.
  nvsReport("boot");

  // Covers the SOFTWARE restarts -- ESP.restart() after an IMPORT, and the one
  // the serial console does. It does NOT cover the AXP2101's 4-second hardware
  // power-off, which tells the firmware nothing at all; that is what the
  // long-press flush in loop() is for.
  esp_register_shutdown_handler([]() { flushBeforePowerLoss("shutdown"); });

  lastInteract = millis();
}

// carga/descarga el sprite de SD cuando cambia la especie
void ensureMon() {
  if (pet.speciesId == monFor && monShinyFor == pet.shiny && !sdDirty) return;
  sdDirty = false;
  monFor = pet.speciesId;
  monShinyFor = pet.shiny;
  mon.unload();
  pmd.unload();
  beh.x = beh.targetX = 233;
  beh.mode = 0;
  beh.until = 0;
  if (pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    pmd.load(pet.speciesId, pet.shiny);          // principal: PMD
    if (!pmd.loaded) mon.load(pet.speciesId, pet.shiny);  // respaldo: B/N
  }
}

void loop() {
  uint32_t now = millis();
  pet.update(now);

  // The link is pumped here rather than from the LAN screen, because it has to
  // keep running through the battle too: linkNowPoll() drains what the radio
  // parked on the WiFi task, and tick() is what resends a lost packet and gives
  // up on a peer that has gone quiet.
  if (lan.live()) {
    linkNowPoll();
    lan.tick(now);
  }
  tradePoll(now);   // the same, for a trade: resends, timeouts, the countdown

  // avisa con un sonido cuando el bicho pasa a estar listo para evolucionar
  // (incluye el caso de cumplir al despertar). canEvolveNow es false durmiendo.
  static bool wasEvoReady = false;
  bool evoReady = pet.wantEvolveButton();
  if (evoReady && !wasEvoReady) sfxPlay(SFX_MEDAL);
  wasEvoReady = evoReady;
  // aviso sombrio cuando el bicho esta a punto de escaparse por abandono
  static bool wasRunReady = false;
  bool runReady = pet.canRunawayNow();
  if (runReady && !wasRunReady) sfxPlay(SFX_DENY);
  wasRunReady = runReady;

  handleTouch();
  handleSerial();
  ensureMon();

  // A sprite pack can arrive long after the card was mounted: the web installer
  // streams it over PUT into the FIRMWARE THAT IS ALREADY RUNNING. gRegionArt was
  // computed once in sdBegin(), so without this the region a player just spent ten
  // minutes downloading stayed greyed out reading NEEDS PACK until they rebooted --
  // indistinguishable, from the player's side, from the download having failed.
  // Quietly, because the host is still parsing this serial stream.
  if (sdArtDirty) {
    sdArtDirty = false;
    sdScanRegionArt(false);
  }

  // A farewell or release just finished: the creature is waiting for a slot.
  // With room it simply joins; with a full party the player is taken straight
  // to the party screen to choose who it replaces, or to let it go.
  if (pet.endedKind != CER_NONE && !partyPick) {
    // party first, then the box; only a full box makes it your choice
    if (party.add(pet.endedMon) || party.boxAdd(pet.endedMon)) {
      snprintf(partyBannerName, sizeof(partyBannerName), "%s",
               pet.endedMon.nick[0] ? pet.endedMon.nick : DEX_TBL[pet.endedMon.dex].name);
      partyBannerUntil = now + 3500;
      pet.clearEnded();
      sfxPlay(SFX_MEDAL);
    } else {
      partyPick = true;
      partyOpen = true;
      menuOpen = false;
    }
  }

  // El PMU late sus interrupciones: se leen TODAS de una vez (pwrPoll) y los
  // accesores de abajo consumen lo que vio. Ver rtcbat.cpp: dos sondeos
  // independientes se borrarian los bits el uno al otro.
  static uint32_t lastPwr = 0;
  if (now - lastPwr > 250) {
    lastPwr = now;
    pwrPoll();
    if (pwrShortPressed()) {
      screenOff = !screenOff;
      pet.setScreenOff(screenOff);   // asleep only if it is also night
      if (!screenOff) lastInteract = now;
    }
    // The hold is on its way to the AXP2101's own power-off, and the firmware
    // gets no second warning. Flush NOW, ignoring the dim/off gate below: that
    // gate exists so a ~1 s flash write is never visible mid-animation, and a
    // visible stutter is a much smaller price than the last few minutes of a
    // creature's life. Same for the battery's low warning.
    if (pwrLongPressed()) flushBeforePowerLoss("pwr long press");
    if (batLowWarning()) flushBeforePowerLoss("battery low");
  }

  updateBrightness(now);

  // vuelca el autoguardado periodico SOLO con la pantalla atenuada/apagada o
  // durmiendo: la escritura a NVS congela ~1s ambos cores (caché de flash off),
  // y aqui no hay animacion que se corte ni dedo esperando respuesta. Con 90s
  // de inactividad la pantalla ya atenua, asi que se vuelca enseguida; el uso
  // activo persiste igual por los guardados de cada accion (comer/jugar/...).
  // retryDue() only matters here: a save that just SUCCEEDED clears
  // savePending() regardless, so this can never delay a normal save -- it
  // only throttles retrying a save that is still failing. See its comment in
  // pet.h; without it, this ran on every single loop iteration while dimmed.
  if (pet.savePending() && pet.retryDue() && (screenOff || dimStage >= 1 || pet.sleeping)) {
    pet.flushSave();
  }

  // anota la hora real cada 30 s (se persiste en cada save del juego)
  static uint32_t lastClock = 0;
  if (now - lastClock > 30000) {
    lastClock = now;
    uint32_t e = rtcEpoch();
    if (e) pet.lastSeenEpoch = e;
  }

  // A software magnitude-threshold step detector (see pedometer.cpp -- the
  // chip's own onboard pedometer engine turned out to not work through this
  // library, on this board or apparently anywhere) needs to actually see a
  // footstep's ~50-150ms impact peak, not just sample it once in a while.
  // 30ms keeps that margin without polling every single loop iteration; the
  // CPU is already awake this whole time regardless (loop() never sleeps
  // while the screen is on), so this costs a few extra I2C microseconds per
  // iteration, not a new wake source.
  static uint32_t lastPedo = 0;
  if (now - lastPedo > 30) {
    lastPedo = now;
    uint32_t steps = pedoPollSteps();
    if (steps) pet.addSteps(steps);
  }

  // latido de salud cada 5 min (para el soak test; se descarta si no hay monitor)
  static uint32_t lastHealth = 0;
  if (now - lastHealth > 300000) {
    lastHealth = now;
    Serial.printf("HEALTH up=%lus heap=%u min=%u save=%s\n",
                  (unsigned long)(now / 1000), ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), pet.saveHealthy() ? "ok" : "BROKEN");
    // NVS headroom rides the same heartbeat as the heap, and for the same
    // reason: the value on its own says little, the trend over a soak says
    // everything. See nvsinfo.cpp for what running out costs.
    nvsReport("health");
  }

  // 85 ms en juego/saco: margen seguro para que el redibujado no pise el envio
  // DMA del frame anterior (a 40-65 ms solapaba y causaba flashes negros; con
  // sprites grandes el dibujo tarda mas, asi que se deja colchon)
  if (now - lastRender >= (uint32_t)((gameOpen || sackOpen || spdOpen || catchOpen || miniGame) ? 85 : 100)) {
    lastRender = now;
    render();
  }
}

// brillo segun sueno + inactividad (proteccion del AMOLED)
void updateBrightness(uint32_t now) {
  // los eventos visibles despiertan la pantalla solos
  if (pet.evolving() || pet.ceremony || pet.eating() || pet.showHeart()) {
    lastInteract = now;
  }
  uint32_t idle = now - lastInteract;
  dimStage = (idle > 300000) ? 2 : (idle > 90000) ? 1 : 0;
  uint8_t target = pet.sleeping ? 25 : (usbPresent() ? 180 : 145);
  if (dimStage == 1) target = pet.sleeping ? 10 : 60;
  else if (dimStage == 2) target = 8;
  if (screenOff) target = 0;
  static uint8_t current = 255;
  if (target != current) {
    current = target;
    panel->setBrightness(target);
  }
}

// ---------- consola serie (provision de SD + depuracion) ----------

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  if (sdSerialCommand(line)) return;

  if (line == "HATCH") {
    pet.eggTap(); pet.eggTap(); pet.eggTap();
    Serial.println("DONE");
  } else if (line.startsWith("SPEC ")) {
    int n = line.substring(5).toInt();
    if (n >= 1 && n <= DEX_COUNT) {
      pet.prevSpeciesId = pet.speciesId;
      pet.speciesId = n;
      Serial.printf("species #%d %s\n", n, DEX_TBL[n].name);
    }
    Serial.println("DONE");
  } else if (line.startsWith("LVL ")) {
    // level() is 1 + age/rate, so the age for level N is (N-1) rates -- this
    // used to set N and hand back N+1, which is a poor thing for a command
    // named LVL to do when it is what every balance check is anchored on.
    long want = line.substring(4).toInt();
    if (want < 1) want = 1;
    if (want > MAX_LEVEL) want = MAX_LEVEL;
    pet.ageMinutes = (uint32_t)(want - 1) * MINUTES_PER_LEVEL;
    pet.saveNow();
    Serial.printf("lvl=%u\n", pet.level());
  } else if (line.startsWith("MISS ")) {
    // MISS <n>: sets the care mistakes -- "descuidos", the desc= on STATS.
    // Each one pushes every evolution threshold up a level, so a creature that
    // was neglected early evolves late; MISS 0 forgives that. Its sibling
    // commands are IV, TR and LVL.
    long m = line.substring(5).toInt();
    if (m < 0) m = 0;
    if (m > 255) m = 255;
    pet.careMistakes = (uint8_t)m;
    pet.saveNow();
    Serial.printf("desc=%u\n", pet.careMistakes);
  } else if (line.startsWith("TR ")) {
    // TR <atk> <def> <spe>: sets the TRAINING (this game's EVs), for testing a
    // fully-raised creature without playing the minigames for an hour. Each is
    // clamped to trMaxFor(iv), the same IV-bound ceiling the games enforce, so
    // this cannot produce a creature the player could not have raised.
    int v[3] = { 0, 0, 0 };
    int n = sscanf(line.c_str() + 3, "%d %d %d", &v[0], &v[1], &v[2]);
    if (n >= 1) {
      int a = v[0], d = (n >= 2) ? v[1] : v[0], e = (n >= 3) ? v[2] : v[0];
      pet.trAtk = (uint8_t)(a < 0 ? 0 : (a > pet.trMaxAtk() ? pet.trMaxAtk() : a));
      pet.trDef = (uint8_t)(d < 0 ? 0 : (d > pet.trMaxDef() ? pet.trMaxDef() : d));
      pet.trSpe = (uint8_t)(e < 0 ? 0 : (e > pet.trMaxSpe() ? pet.trMaxSpe() : e));
      pet.saveNow();
    }
    Serial.printf("tr=%u/%u/%u topes=%u/%u/%u\n", pet.trAtk, pet.trDef, pet.trSpe,
                  pet.trMaxAtk(), pet.trMaxDef(), pet.trMaxSpe());
  } else if (line.startsWith("STEPS ")) {
    // STEPS <n>: credits n pedometer steps without the QMI8658 -- the only way
    // to test the Mart's wallet/purchase flow in the emulator, which has no
    // IMU, and a quick way to fast-forward it on a board too.
    long n = line.substring(6).toInt();
    if (n < 0) n = 0;
    pet.addSteps((uint32_t)n);
    pet.saveNow();
    Serial.printf("wallet=%lu steps=%lu\n", (unsigned long)pet.wallet,
                  (unsigned long)pet.stepsTotal);
  } else if (line == "ACCEL") {
    // Live magnitude from the software step detector's own last read --
    // diagnostic only, so a real board can show whether the accelerometer
    // is producing sane, moving values at all, independent of whether
    // anything crossed the step threshold.
    Serial.printf("accel magnitude=%.3fg\n", pedoLastMagnitude());
    Serial.println("DONE");
  } else if (line.startsWith("IV ")) {
    // IV <fue> <def> <vel> <vit>: fija los valores individuales (pruebas).
    // Con "IV 31 31 31 31" se ve el techo; con "IV 8 8 8 8" el suelo.
    int v[4] = { 16, 16, 16, 16 };
    int n = sscanf(line.c_str() + 3, "%d %d %d %d", &v[0], &v[1], &v[2], &v[3]);
    if (n >= 1) {
      for (int i = 0; i < 4; i++) v[i] = v[i] < 0 ? 0 : (v[i] > 31 ? 31 : v[i]);
      pet.ivAtk = v[0];
      pet.ivDef = (n >= 2) ? v[1] : v[0];
      pet.ivSpe = (n >= 3) ? v[2] : v[0];
      pet.ivHp = (n >= 4) ? v[3] : v[0];
      if (pet.trAtk > pet.trMaxAtk()) pet.trAtk = pet.trMaxAtk();
      if (pet.trDef > pet.trMaxDef()) pet.trDef = pet.trMaxDef();
      if (pet.trSpe > pet.trMaxSpe()) pet.trSpe = pet.trMaxSpe();
    }
    pet.saveNow();   // IV used to change RAM only and never write
    Serial.printf("iv=%u/%u/%u/%u topes=%u/%u/%u\n", pet.ivAtk, pet.ivDef,
                  pet.ivSpe, pet.ivHp, pet.trMaxAtk(), pet.trMaxDef(), pet.trMaxSpe());
    Serial.println("DONE");
  } else if (line.startsWith("TIME ")) {
    uint32_t e = (uint32_t)line.substring(5).toInt();
    rtcSetEpoch(e);
    pet.setClock(e);
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line.startsWith("RTCSET ")) {  // solo RTC (simular apagados en pruebas)
    rtcSetEpoch((uint32_t)line.substring(7).toInt());
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "TIME") {
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "GAL") {
    galleryOpen = !galleryOpen;
    galleryDetail = 0;
    galleryDirty = true;
    if (!galleryOpen) galleryPmd.unload();
    Serial.println("DONE");
  } else if (line == "EGGS") {
    // simula 20 tiradas de huevo (no cambia el estado del juego)
    for (int i = 0; i < 20; i++) {
      int16_t d = pet.pickEggSpecies();
      Serial.printf("%d:%s(r%u) ", d, DEX_TBL[d].name, DEX_TBL[d].rarity);
    }
    Serial.println();
    Serial.println("DONE");
  } else if (line == "BAG") {
    for (uint8_t i = 0; i < bag.distinctCount(); i++) {
      ItemKey k = bag.keyAt(i);
      Serial.printf("%u:%s x%u\n", k, itemEntry(k).name, bag.count(k));
    }
    Serial.println("DONE");
  } else if (line.startsWith("GIVE ")) {
    // GIVE <key> [n]: stock the bag, so catching can be exercised without
    // grinding wild wins first.
    int key = 0, n = 1;
    if (sscanf(line.c_str() + 5, "%d %d", &key, &n) >= 1 &&
        itemValid((ItemKey)key)) {
      uint8_t got = bag.add((ItemKey)key, (uint8_t)(n > 0 ? n : 1));
      Serial.printf("%s x%u (+%u)\n", ITEM_TBL[key].name,
                    bag.count((ItemKey)key), got);
    } else {
      Serial.println("bad item");
    }
    Serial.println("DONE");
  } else if (line == "WILD" || line == "WILD HARD") {
    bool hard = (line == "WILD HARD");
    uint8_t rg = currentExploreRegion();
    uint16_t area = exploreAreaOf(rg);
    if (startWildBattle(rg, hard))
      Serial.printf("wild %s%s Lv.%u%s iv=%u/%u/%u/%u in %s\n", DEX_TBL[wildDex].name,
                    wildLegend >= 0 ? " (LEGENDARY)" : "", wildLvl, wildShiny ? " *SHINY*" : "",
                    wildIv[0], wildIv[1], wildIv[2], wildIv[3],
                    area == WILD_NO_AREA ? REGIONS[rg].name : wildAreaName(area));
    else
      Serial.println("no encounter (egg, ceremony, locked area, or no sprite pack)");
    Serial.println("DONE");
  } else if (line == "AREA" || line.startsWith("AREA ")) {
    // AREA lists the Explore region's areas with their gate and band; AREA <n>
    // picks the n-th, through the same rule the list's tap uses.
    uint8_t rg = currentExploreRegion(), n = wildAreaCount(rg);
    uint16_t first = wildAreaFirst(rg);
    int want = -1;
    if (line.length() > 5) sscanf(line.c_str() + 5, "%d", &want);
    if (!n) Serial.printf("%s has no areas yet (whole-region pool)\n", REGIONS[rg].name);
    if (want >= 0 && want < n) {
      uint16_t a = (uint16_t)(first + want);
      if (wildAreaUnlocked(a, pet)) exploreAreaPick[rg] = (uint16_t)(a + 1);
      else Serial.println("locked");
    }
    uint16_t cur = exploreAreaOf(rg);
    for (uint8_t i = 0; i < n; i++) {
      uint16_t a = (uint16_t)(first + i);
      int8_t wx = wildAreaWeather(a, pet);
      Serial.printf("%c%2u %-16s gate %-2u Lv.%u-%u%s%s%s%s\n", a == cur ? '>' : ' ', i,
                    wildAreaName(a), wildAreaGate(a), wildAreaLvMin(a), wildAreaLvMax(a),
                    wildAreaUnlocked(a, pet) ? "" : "  locked",
                    wildAreaHasLegend(a, pet) ? "  legendary" : "",
                    wx >= 0 ? "  today: " : "", wildWeatherName(wx));
    }
    Serial.println("DONE");
  } else if (line.startsWith("FOCUS ")) {
    // FOCUS <slot>: swap the creature on the main screen with a party slot,
    // which is the console half of the RAISE THIS ONE button.
    int s = -1;
    if (sscanf(line.c_str() + 6, "%d", &s) == 1 && s >= 0 && s < PARTY_SLOTS &&
        !party.slots[s].empty()) {
      focusSwap((uint8_t)s);
      Serial.printf("raising %s Lv.%u\n", DEX_TBL[pet.speciesId].name, pet.level());
    } else {
      Serial.println("empty or out of range");
    }
    Serial.println("DONE");
  } else if (line.startsWith("EGG ")) {
    // EGG <dex> [shiny]: hatch a chosen species right now. The legendary and
    // shiny IV guarantees only apply at hatch time, so this is the only way to
    // exercise them on hardware (SHINY below just toggles the flag afterwards).
    int dex = 0, sh = 0;
    int n = sscanf(line.c_str() + 4, "%d %d", &dex, &sh);
    if (n >= 1 && dex >= 1 && dex <= DEX_COUNT) {
      pet.dbgHatchAs(dex, sh != 0);
      Serial.printf("%s%s iv=%u/%u/%u/%u\n", DEX_TBL[dex].name,
                    pet.shiny ? " *SHINY*" : "", pet.ivAtk, pet.ivDef,
                    pet.ivSpe, pet.ivHp);
    }
    Serial.println("DONE");
  } else if (line.startsWith("BATTLE")) {
    // BATTLE <dex> [level] -- the only way in until the trainer roster exists
    int dex = 0, lvl = 0;
    int n = sscanf(line.c_str() + 6, "%d %d", &dex, &lvl);
    if (n >= 1 && dex >= 1 && dex <= DEX_COUNT) {
      startBattle(dex, lvl > 0 ? (uint8_t)lvl : pet.level());
      Serial.printf("battle vs %s Lv.%u\n", DEX_TBL[dex].name, btlFoe.level);
    } else {
      Serial.println("usage: BATTLE <dex> [level]");
    }
    Serial.println("DONE");
  } else if (line == "SHINY") {  // alterna shiny del actual (pruebas)
    pet.shiny = !pet.shiny;
    Serial.printf("shiny=%d\n", pet.shiny);
    Serial.println("DONE");
  } else if (line.startsWith("NICK ")) {
    pet.rename(line.substring(5).c_str());
    Serial.printf("nick=%s\n", pet.nick);
    Serial.println("DONE");
  } else if (line == "CAREDAY") {  // simula un dia nuevo cuidado (pruebas)
    pet.setClock(pet.lastSeenEpoch + 86400);
    pet.caress();
    Serial.printf("streak=%u bond=%u medals=0x%X\n", pet.streak, pet.bond, pet.medals);
    Serial.println("DONE");
  } else if (line == "BYE") {
    pet.startFarewell();
    Serial.println("DONE");
  } else if (line == "RUN") {
    pet.startRunaway();
    Serial.println("DONE");
  } else if (line == "BEEP") {
    sfxPlay(SFX_HATCH);  // prueba de audio
    Serial.println("DONE");
  } else if (line == "ABANDON") {
    pet.dbgRunawayReady();  // fuerza el estado "lista para escaparse" (test del boton)
    Serial.println("DONE");
  } else if (line == "EXPORT") {
    // Prints the whole save as a block of IMPORT commands. Pasting that block
    // back is the restore -- there is no separate format to get wrong, and no
    // single 2000-character line for a terminal to mangle.
    static uint8_t buf[SAVE_TRANSFER_MAX];
    size_t n = saveExport(buf, sizeof(buf));
    if (!n) { Serial.println("EXPORT FAIL"); return; }
    Serial.printf("# TamaPoke save, %u bytes. Paste this whole block back.\n",
                  (unsigned)n);
    for (size_t i = 0; i < n; i += 48) {
      Serial.print("IMPORT ");
      for (size_t j = i; j < i + 48 && j < n; j++) Serial.printf("%02X", buf[j]);
      Serial.println();
    }
    Serial.println("IMPORT");        // the empty one commits
  } else if (line.startsWith("IMPORT")) {
    // IMPORT <hex>   append a chunk
    // IMPORT         commit what has been appended
    static uint8_t in[SAVE_TRANSFER_MAX];
    static size_t inN = 0;
    String hex = line.substring(6);
    hex.trim();
    if (hex.length()) {
      if (hex.length() & 1) { Serial.println("IMPORT ODD"); inN = 0; return; }
      for (size_t i = 0; i + 1 < (size_t)hex.length(); i += 2) {
        if (inN >= sizeof(in)) { Serial.println("IMPORT FULL"); inN = 0; return; }
        auto nyb = [](char c) -> int {
          if (c >= '0' && c <= '9') return c - '0';
          if (c >= 'A' && c <= 'F') return c - 'A' + 10;
          if (c >= 'a' && c <= 'f') return c - 'a' + 10;
          return -1;
        };
        const char *hs = hex.c_str();
        int hi = nyb(hs[i]), lo = nyb(hs[i + 1]);
        if (hi < 0 || lo < 0) { Serial.println("IMPORT BAD"); inN = 0; return; }
        in[inN++] = (uint8_t)((hi << 4) | lo);
      }
      Serial.println("IMPORT MORE");
      return;                        // wait for the next chunk or commit
    }
    if (!inN) { Serial.println("IMPORT EMPTY"); return; }
    bool ok = saveImport(in, inN);
    Serial.println(ok ? "IMPORT OK" : "IMPORT REJECTED");
    inN = 0;
    if (ok) {
      // Same guard as WIPE, for the same reason: the restart runs the shutdown
      // handler, which flushes the creature still in RAM. Its checkpoint
      // generation is usually AHEAD of the backup's, so it won on the next boot
      // and the restore was silently undone. console_test reproduces it.
      saveInhibited = true;
      Serial.println("DONE");
      delay(100);
      ESP.restart();
    }
  } else if (line == "WIPE") {
    // Erases NVS and restarts into a new game (starter choice). A wipe that did
    // not happen must say so and leave the game running, not report DONE and
    // reboot into the save the player asked to be rid of.
    if (!pet.factoryReset()) {
      Serial.println("WIPE FAILED -- NVS would not clear; the save is untouched");
      return;
    }
    // Set BEFORE the restart, because the restart runs the shutdown handler, and
    // that handler's whole job is to flush a pending save. Without this it would
    // write the creature that was just deleted straight back into the empty NVS.
    saveInhibited = true;
    Serial.println("DONE");
    delay(100);
    ESP.restart();
  } else if (line.startsWith("PARTY")) {
    // PARTY          list the party
    // PARTY <dex>    bank a level-50 specimen (fills slots up for testing)
    // PARTY CLEAR    empty it
    String arg = line.substring(5);
    arg.trim();
    if (arg == "CLEAR") {
      for (int i = 0; i < PARTY_SLOTS; i++) party.releaseAt(i);
    } else if (arg.length()) {
      int d = arg.toInt();
      if (d >= 1 && d <= DEX_COUNT) {
        PartyMon m;
        m.dex = d;
        m.level = 50;
        m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
        m.trAtk = m.trDef = m.trSpe = 50;
        m.uid = pet.mintUid();
        Serial.println(party.add(m) ? "added" : "party full");
      }
    }
    Serial.printf("party %u/%u:", party.count(), PARTY_SLOTS);
    for (int i = 0; i < PARTY_SLOTS; i++) {
      const PartyMon &m = party.slots[i];
      if (m.empty()) Serial.print(" -");
      else Serial.printf(" %s%s(nv%u #%lu)", DEX_TBL[m.dex].name, m.shiny ? "*" : "", m.level,
                         (unsigned long)m.uid.serial);
    }
    Serial.println();
    Serial.println("DONE");
  } else if (line == "REG") {
    Serial.printf("pokedex %u/%u:", pet.registeredCount(), DEX_COUNT);
    for (int i = 1; i <= DEX_COUNT; i++)
      if (pet.isRegistered(i)) Serial.printf(" %d", i);
    Serial.println();
    Serial.println("DONE");
  } else if (line == "HEALTH") {
    Serial.printf("up=%lus heap=%u min=%u sd=%d mon=%d\n",
                  (unsigned long)(millis() / 1000), ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), sdReady, pmd.loaded || mon.loaded);
    // PSRAM is where the sprites and the framebuffer live, so a memory problem
    // shows up here long before it shows up in the heap figure above.
    Serial.printf("psram=%u screen=%s btlspr=%d/%d\n", (unsigned)ESP.getFreePsram(),
                  SCREEN_NAME[uiCurrentScreen() % SCR_COUNT],
                  btlPmd[0].loaded ? 1 : 0, btlPmd[1].loaded ? 1 : 0);
    // The same two lines the periodic heartbeat prints. Typed on a board is when
    // you actually want them, and a console HEALTH that reported LESS than the
    // automatic one is § "A rule enforced in one path but not in its twin".
    Serial.printf("save=%s pending=%d\n", pet.saveHealthy() ? "ok" : "BROKEN",
                  pet.savePending() ? 1 : 0);
    nvsReport("health");
    Serial.println("DONE");
  } else if (line == "STATS") {
    // A stable per-board label, for the web installer's backup history so two
    // devices' saves can be told apart. The efuse MAC is fixed in silicon and
    // needs no storage. It is NOT a secret and NOTHING is authorised by it --
    // which is why the installer keeps its history in the browser rather than
    // on a server keyed by this.
    Serial.printf("board=%012llX\n", ESP.getEfuseMac());
    // The live creature's identity and how many this save has minted.
    {
      char id[32];
      uidFormat(pet.uid, id, sizeof(id));
      Serial.printf("id=%s minted=%lu\n", id, (unsigned long)pet.uidSerial);
    }
    Serial.printf("spec=%d nv=%u com=%u fel=%u ene=%u lim=%u desc=%u sd=%d mon=%d bat=%d usb=%d rtc=%u\n",
                  pet.speciesId, pet.level(), pet.fullness, pet.joy, pet.energy,
                  pet.hygiene, pet.careMistakes, sdReady, mon.loaded,
                  batPercent(), usbPresent(), rtcEpoch());
    // weight is the raw meter; fit and kg are what the card shows for it
    Serial.printf("peso=%u fit=%u kg=%lu.%lu fue=%u def=%u vel=%u vit=%u baya=%d\n",
                  pet.weight, pet.fitness(), (unsigned long)(pet.weightHg() / 10),
                  (unsigned long)(pet.weightHg() % 10), pet.atkStat(), pet.defStat(), pet.speStat(),
                  pet.vitStat(), pet.berryKnown);
    Serial.printf("iv=%u/%u/%u/%u tr=%u/%u/%u topes=%u/%u/%u\n",
                  pet.ivAtk, pet.ivDef, pet.ivSpe, pet.ivHp,
                  pet.trAtk, pet.trDef, pet.trSpe,
                  pet.trMaxAtk(), pet.trMaxDef(), pet.trMaxSpe());
    Serial.printf("shiny=%d streak=%u/%u bond=%u medals=0x%X(%u) nick=%s\n",
                  pet.shiny, pet.streak, pet.bestStreak, pet.bond, pet.medals,
                  pet.totalMedals, pet.nick);
    Serial.println("DONE");
  }
}

// ---------- entrada tactil ----------

bool inPetZone(int16_t x, int16_t y) {
  return x > 110 && x < 356 && y > 95 && y < 310;
}

// el toque se resuelve al LEVANTAR el dedo para distinguir tap de deslizar
// Takes a reading as where the finger is, counting the step it made.
void touchAccept(int16_t x, int16_t y) {
  int ddx = x - tXl, ddy = y - tYl;
  if (ddx >= TOUCH_STEP_PX && tStepsR < 255) tStepsR++;
  if (ddx <= -TOUCH_STEP_PX && tStepsL < 255) tStepsL++;
  if (ddy >= TOUCH_STEP_PX && tStepsD < 255) tStepsD++;
  if (ddy <= -TOUCH_STEP_PX && tStepsU < 255) tStepsU++;
  tXl = x;
  tYl = y;
}

void handleTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 20) return;  // 50 Hz le sobra a un dedo
  lastPoll = millis();
  // solo tocamos el bus si el chip aviso por INT o si el dedo sigue abajo (hay
  // que detectar el levantamiento). Leer el CST9217 dormido se colgaba ~1s y
  // congelaba el loop entero; SensorLib no respeta el timeout de Wire.
  if (!gTouchIrq && !wasPressed) return;
  gTouchIrq = false;
  int16_t x, y;
  bool pressed = touch.getPoint(&x, &y, 1) > 0;

  // Berry Catch: the basket follows the finger for as long as it is down; a
  // press in the top strip leaves, keeping what was caught (same as the bag).
  if (catchOpen) {
    if (pressed) {
      lastInteract = millis();
      if (!wasPressed && y < 72) leaveCatch();
      else catchTarget = x;
    }
    wasPressed = pressed;
    return;
  }

  // The hybrid games act on the touch-DOWN, so timing is the tap's own. The
  // top strip leaves, keeping what was earned. In Block! the top is also a
  // side to defend, so its strip is the thin band above where attacks start.
  if (miniGame) {
    if (pressed && !wasPressed) {
      lastInteract = millis();
      if (y < mgLeaveStripY()) leaveMini();
      else mgPress(x, y, millis());
    }
    wasPressed = pressed;
    return;
  }

  // saco de entrenamiento: cada toque cuenta al instante (aporrear rapido)
  if (sackOpen) {
    if (pressed && !wasPressed) {
      lastInteract = millis();
      if (y < 72) leaveSack();       // tocar arriba = salir, conservando lo ganado
      else sackTap();
    }
    wasPressed = pressed;
    return;
  }

  if (pressed && !wasPressed) {  // empieza el gesto
    tX0 = tXl = x;
    tY0 = tYl = y;
    tStart = millis();
    holdFired = false;
    tHeld = false;
    tStepsL = tStepsR = tStepsU = tStepsD = 0;
    tOnControl = uiOnControl(x, y);
    // A drag that STARTS on the rim is a scroll, not a swipe. Deciding it at
    // touch-down rather than on release is what keeps it unambiguous: the
    // finger never has to be told afterwards what it meant.
    rimActive = uiOnRim(x, y) && uiRimPages() > 1;
    rimA0 = uiRimAngle(x, y);
    rimSteps = 0;
    rimMoved = false;
    swallowGesture = (dimStage > 0) || screenOff;  // si estaba a oscuras, solo despierta
    if (screenOff) pet.setScreenOff(false);        // waking the screen wakes it
    screenOff = false;
    lastInteract = millis();
  } else if (pressed) {  // sigue apoyado
    if (tHeld) {                     // last poll's outlier: real, or noise?
      int32_t hx = x - tHeldX, hy = y - tHeldY, bx = x - tXl, by = y - tYl;
      if (hx * hx + hy * hy < bx * bx + by * by) touchAccept(tHeldX, tHeldY);
      tHeld = false;
    }
    bool took = true;
    if (abs(x - tXl) > TOUCH_JUMP_PX || abs(y - tYl) > TOUCH_JUMP_PX) {
      tHeld = true; tHeldX = x; tHeldY = y;
      took = false;
    } else {
      touchAccept(x, y);
    }
    if (took && rimActive && !swallowGesture) {
      float d = uiRimAngle(tXl, tYl) - rimA0;
      while (d > 180.0f) d -= 360.0f;
      while (d < -180.0f) d += 360.0f;
      if (d > 6.0f || d < -6.0f) rimMoved = true;
      int want = (int)(d / RIM_STEP);
      while (rimSteps < want) { onRim(1); rimSteps++; }
      while (rimSteps > want) { onRim(-1); rimSteps--; }
    }
    // pulsacion larga sin moverse sobre el bicho -> dialogo de soltar
    //
    // Gated on the MAIN screen, not on a hand-maintained list of screens to
    // exclude. That list had gallery/card/keyboard/clock on it and nothing
    // else, so the hold still fired on the party, box, gym, battle, player and
    // menu screens -- every one of which draws something inside inPetZone.
    // On the party screen it was genuinely dangerous: the grid overlaps the
    // zone, so holding a party slot opened "release the live pet?", and that
    // dialog's YES box sits on top of party slot 4. Hold a slot, tap where you
    // think a creature is, lose the creature you are actually raising.
    // One question with one answer, so a new screen cannot be forgotten.
    if (!holdFired && !swallowGesture && uiCurrentScreen() == SCR_MAIN && millis() - tStart > 3000 &&
        abs(tXl - tX0) < 30 && abs(tYl - tY0) < 30 && inPetZone(tX0, tY0) &&
        !pet.isEgg() && !confirmUntil && !pet.ceremony) {
      confirmUntil = millis() + 10000;
      holdFired = true;
    }
  } else if (wasPressed) {  // levanta el dedo: resolver gesto
    lastInteract = millis();
    // A reading still held at lift-off was never confirmed by a next one. Off
    // a control it is kept, exactly as every reading always was -- polls slow
    // down while the panel draws, and the end of a quick swipe can arrive as
    // one far sample. ON a control it is kept only if the finger was already
    // travelling that way; otherwise it is the stray, and the gesture ends
    // where the finger really was.
    if (tHeld) {
      int32_t ax = tXl - tX0, ay = tYl - tY0, hx = tHeldX - tXl, hy = tHeldY - tYl;
      bool travelling = ax * ax + ay * ay >= (int32_t)TOUCH_MOVING_PX * TOUCH_MOVING_PX &&
                        ax * hx + ay * hy > 0;
      if (!tOnControl || travelling) touchAccept(tHeldX, tHeldY);
      tHeld = false;
    }
    int dx = tXl - tX0, dy = tYl - tY0;
    uint32_t dt = millis() - tStart;
    if (rimMoved) { rimActive = false; wasPressed = pressed; return; }   // it was a scroll
    if (!holdFired && !swallowGesture) {
      // Off a control, distance alone decides, as it always has. ON one, a
      // swipe must also have moved its own way for TOUCH_SWIPE_STEPS polls, and
      // whatever falls short is a tap on that control rather than nothing.
      bool hOk = !tOnControl || (dx > 0 ? tStepsR : tStepsL) >= TOUCH_SWIPE_STEPS;
      bool vOk = !tOnControl || (dy > 0 ? tStepsD : tStepsU) >= TOUCH_SWIPE_STEPS;
      if (abs(dx) > 80 && abs(dy) < 70 && dt < 800 && hOk) onSwipe(dx > 0 ? 1 : -1);
      else if (abs(dy) > 80 && abs(dx) < 70 && dt < 800 && vOk) onSwipeV(dy > 0 ? 1 : -1);
      else if (dt < 1500 && (tOnControl || (abs(dx) < 40 && abs(dy) < 40))) onTap(tX0, tY0);
    }
    rimActive = false;
  }
  wasPressed = pressed;
}

// deslizar vertical: DOWN is back, UP is deeper.
//
// One meaning per direction, on every screen. It used to be a per-screen
// grab-bag -- vertical closed some screens, changed region on others, and on
// the main screen down opened the player card while up opened the creature's.
// The player card is a tile now, so down is free to mean the one thing a round
// UI most needs and never had: a back gesture that works everywhere.

void openClock();  // prototipo

void onSwipeV(int dir) {
  if (pet.awaitingStarter()) return;  // bloqueado durante la eleccion de inicial
  if (blackoutOpen) { blackoutDismiss(); return; }
  if (menuOpen) { menuOpen = false; return; }   // any swipe closes the menu
  if (battleOpen) return;   // no swiping out of a fight
  const bool back = dir > 0;
  if (tradeOpen) {          // down backs out one level; a pending trade cannot be left
    if (!back) return;
    if (tradePicking) tradePicking = false;
    else if (tradeAsk) tradeAsk = 0;
    else tradeLeave();
    return;
  }

  // Modals and sheets: down backs out, up has nowhere deeper to go.
  if (pickOpen)     { if (back) pickOpen = false; return; }
  if (lanOpen)      { if (back) { lanLeave(); lanOpen = false; } return; }
  if (trainOpen)    { if (back) trainOpen = false; return; }
  if (bagOpen)      { if (back) bagOpen = false; return; }
  if (movePickOpen) { if (back) movePickOpen = false; return; }
  if (boxOpen)      { if (back) { boxOpen = false; boxSel = 0; } return; }
  // Either minigame exits on a swipe, keeping what was earned. A swipe cannot
  // be confused with a ball hit -- the gesture resolver separates them -- which
  // the header tap no longer could once the ball became hittable up there.
  if (gameOpen) { leaveGame(); return; }
  if (sackOpen) { leaveSack(); return; }
  if (catchOpen) { leaveCatch(); return; }
  if (miniGame) { leaveMini(); return; }
  if (spdOpen)  { leaveSpeed(); return; }
  if (kbOpen || pet.ceremony) return;
  if (clockOpen) { if (back) clockOpen = false; return; }

  if (exploreOpen) {
    if (areaListOpen) { if (back) areaListOpen = false; return; }   // back to Explore
    if (back) uiTileGo(TILE_PET);
    return;
  }
  if (martOpen) {
    if (martConfirmItem) { if (back) martConfirmItem = ITEM_NONE; return; }
    if (back) uiTileGo(TILE_PET);
    return;
  }

  // The two multi-region tiles are two levels deep: chooser, then ladder/grid.
  // DOWN walks back out of them one step at a time; UP keeps the region
  // shortcut, which is now a convenience rather than the only way in.
  if (gymOpen) {
    if (gymPick) { if (back) uiTileGo(TILE_PET); return; }
    if (back) { gymPick = true; rpickPage = 0; sfxPlay(SFX_TAP); return; }
    gymRegion = (uint8_t)((gymRegion + 1) % GYM_REGIONS);
    gymPage = 0;
    sfxPlay(SFX_TAP);
    return;
  }
  if (galleryOpen) {
    if (galleryDetail) {
      if (back) { galleryDetail = 0; galleryPmd.unload(); galleryDirty = true; }
      return;
    }
    if (galleryPick) { if (back) uiTileGo(TILE_PET); return; }
    if (back) {
      galleryPick = true; rpickPage = 0;
      galleryPmd.unload();
      galleryDirty = true;
      sfxPlay(SFX_TAP);
      return;
    }
    galleryRegion = (uint8_t)((galleryRegion + 1) % GAL_REGIONS);
    galleryPage = 0;
    galleryDirty = true;
    sfxPlay(SFX_TAP);
    return;
  }
  if (playerOpen) {
    if (avatarPickOpen) { if (back) avatarPickOpen = false; return; }   // back to the card
    if (back) uiTileGo(TILE_PET);
    return;
  }
  if (partyOpen) {
    if (partyDetail) { if (back) partyDetail = 0; return; }
    if (back) {
      if (partyPick) { partyPick = false; pet.clearEnded(); }
      uiTileGo(TILE_PET);
    }
    return;
  }
  if (cardOpen) { if (back) cardOpen = false; return; }

  // The pet tile itself. UP is its detail sheet; DOWN is already home.
  if (!back && !pet.isEgg() && !confirmUntil && !feedMenuUntil) {
    cardOpen = true;
    cardPage = 0;
  }
}

// party screen: pick a slot (when a newcomer is waiting) or just leave
// A banked creature's sheet: its moves above all, since typing alone does not
// tell you whether that Lapras still has ICE BEAM -- and in hard mode that is
// what decides the fight.
// The detail sheet, shared by the party and the box so the two cannot drift.
// `fromBox` picks which action the LEFT button offers; the right one is always
// RELEASE, which is irreversible and therefore always asks first.
void renderMonSheet(const PartyMon &m, bool fromBox) {
  const DexEntry &d = DEX_TBL[m.dex];
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  char head[36];
  snprintf(head, sizeof(head), "%s%s Lv.%u", m.shiny ? "*" : "",
           m.nick[0] ? m.nick : d.name, (unsigned)m.level);
  gfx->setTextColor(d.accent);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(head) * 6, 40);
  gfx->print(head);
  char ty[24];
  if (d.type2 == T_NONE) snprintf(ty, sizeof(ty), "%s", typeName(d.type1));
  else snprintf(ty, sizeof(ty), "%s/%s", typeName(d.type1), typeName(d.type2));
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(ty) * 3, 64);
  gfx->print(ty);

  for (int i = 0; i < MOVE_SLOTS; i++)
    drawMoveRow(78 + i * 52, m.moves[i], false, m.dex);

  char st[40];
  snprintf(st, sizeof(st), "ATK %u  DEF %u  SPD %u  HP %u",
           party.atkOf(m), party.defOf(m), party.speOf(m), party.vitOf(m));
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(st) * 3, 300);
  gfx->print(st);

  // Which creature you are RAISING is now a choice you can make at any time.
  // It is a true exchange -- the one on the main screen takes the slot this one
  // vacates -- so unlike the old frozen BRING BACK it needs no free slot and no
  // "only while an egg waits" rule. Both keep their full care state, so nothing
  // is reset by swapping and swapping back.
  //
  // A box creature still goes to the party first: one rule per screen, and the
  // swap is then offered from the party sheet like any other member.
  bool leftOk = fromBox ? (party.firstFree() >= 0)
                        : (pet.ceremony == CER_NONE && !pet.awaitingStarter());
  const char *leftLbl = fromBox ? T(S_BOX_TAKE) : T(S_FOCUS);
  gfx->fillRoundRect(PDET_L_X, PDET_BTN_Y, PDET_L_W, PDET_BTN_H, 10,
                     leftOk ? UI_BAR_OK : UI_TRACK);
  gfx->drawRoundRect(PDET_L_X, PDET_BTN_Y, PDET_L_W, PDET_BTN_H, 10, UI_INK);
  gfx->setTextColor(leftOk ? UI_BG_DAY : 0x8410);
  gfx->setTextSize(2);
  gfx->setCursor(PDET_L_X + PDET_L_W / 2 - (int)strlen(leftLbl) * 6,
                 PDET_BTN_Y + PDET_BTN_H / 2 - 8);
  gfx->print(leftLbl);

  gfx->fillRoundRect(PDET_R_X, PDET_BTN_Y, PDET_R_W, PDET_BTN_H, 10, UI_BAR_BAD);
  gfx->drawRoundRect(PDET_R_X, PDET_BTN_Y, PDET_R_W, PDET_BTN_H, 10, UI_INK);
  gfx->setTextColor(UI_WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(PDET_R_X + PDET_R_W / 2 - (int)strlen(T(S_RELEASE_BTN)) * 3,
                 PDET_BTN_Y + PDET_BTN_H / 2 - 4);
  gfx->print(T(S_RELEASE_BTN));

  if (!leftOk) {
    const char *why = fromBox ? T(S_PARTY_FULL) : T(S_FOCUS_NOW);
    gfx->setTextColor(UI_BAR_WARN);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(why) * 3, PDET_BTN_Y - 14);
    gfx->print(why);
  }
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_BACK)) * 6, 404);
  gfx->print(T(S_BACK));

  // Asked before it happens, because nothing gets this creature back: it is not
  // a farewell, it does not join anything, and there is no undo.
  if (releaseConfirm) {
    char q[40];
    snprintf(q, sizeof(q), T(S_RELEASE_FMT), m.nick[0] ? m.nick : d.name);
    drawConfirmPanel(q, T(S_RELEASE_GONE), nullptr, UI_BAR_BAD,
                     T(S_YES), UI_BAR_BAD, UI_WHITE, T(S_NO), UI_TRACK, UI_INK);
  }
  gfx->flush();
}

void renderPartyDetail() {
  // An empty slot means the creature was just let go. Fall through to the grid
  // rather than returning: a bare return draws nothing AND never flushes, which
  // leaves the previous frame frozen on the panel -- the exact failure
  // flush_test exists to catch, and invisible to any screenshot.
  if (party.slots[partyDetail - 1].empty()) {
    partyDetail = 0;
    releaseConfirm = false;
    renderParty();
    return;
  }
  renderMonSheet(party.slots[partyDetail - 1], false);
}

void renderBoxDetail() {
  if (party.box[boxDetail - 1].empty()) {
    boxDetail = 0;
    releaseConfirm = false;
    renderBox();
    return;
  }
  renderMonSheet(party.box[boxDetail - 1], true);
}

// The two buttons' hit areas, so a test can prove they do not overlap without
// copying the geometry -- a test that restates the numbers drifts from them.
// Every primary button's height, so a test can hold them all to UI_TAP_MIN
// instead of waiting for somebody to report the next one by hand.
void uiButtonHeights(int *out, int max, int *n) {
  // What a control is worth is what you can HIT, not what it draws: the home
  // icons are a 72 px graphic inside a 94 px disc.
  const int h[] = { PSLOT_HUB_R * 2, PSLOT_R * 2, PARTYCLOSE_H, LANBTN_H,
                    BTL_CELL_H + BTL_HIT_PAD * 2,
                    EXPLORE_BTN_H + EXPLORE_BTN_PAD * 2, BTN_HIT * 2 };
  int c = (int)(sizeof(h) / sizeof(h[0]));
  if (c > max) c = max;
  for (int i = 0; i < c; i++) out[i] = h[i];
  if (n) *n = c;
}

// The gym list's difficulty pill against its first leader row. Enlarging the
// pill to a real tap target once pushed it straight over that row -- the third
// overlap of this kind, after BOX/CLOSE and the battle grid against BACK.
// Which home icon is the only one live while the pet sleeps, and where it is.
// Exposed so a test can prove it is the LIGHT: removing an icon once shifted
// every index and quietly made the BATH button the wake-up button.
// Asleep, the LIGHT is the only live icon -- it is what wakes the pet. Both the
// draw path and the tap path ask THIS, so a greyed button can never still be
// tappable and the greying can never point at the wrong icon.
bool uiButtonDisabled(int i) { return pet.sleeping && i != BTN_LIGHT; }

int uiSleepButton(int *cx, int *cy) {
  if (cx) *cx = buttons[BTN_LIGHT].cx;
  if (cy) *cy = buttons[BTN_LIGHT].cy;
  return BTN_LIGHT;
}

void uiButtonAt(int i, int *cx, int *cy, int *half) {
  if (i < 0 || i >= BTN_COUNT) return;
  if (cx) *cx = buttons[i].cx;
  if (cy) *cy = buttons[i].cy;
  if (half) *half = BTN_HALF;
}

void gymHeaderRects(int *pillTop, int *pillBot, int *rowTop) {
  if (pillTop) *pillTop = GYMDIF_Y;
  if (pillBot) *pillBot = GYMDIF_Y + GYMDIF_H;
  if (rowTop) *rowTop = GYM_ROW_Y(0);
}

void partyButtonRects(int *boxTop, int *boxBot, int *closeTop, int *closeBot) {
  if (boxTop) *boxTop = PSLOT_CY - PSLOT_HUB_R;
  if (boxBot) *boxBot = PSLOT_CY + PSLOT_HUB_R;
  if (closeTop) *closeTop = PARTYCLOSE_Y;
  if (closeBot) *closeBot = PARTYCLOSE_Y + PARTYCLOSE_H;
}

// Which of the detail sheet's two buttons a tap landed on. One answer for the
// party and the box, off the same PDET_* constants the sheet is DRAWN with, so
// the graphic and the hit area cannot drift apart.
bool monSheetBtn(int16_t x, int16_t y, bool left) {
  if (y < PDET_BTN_Y || y > PDET_BTN_Y + PDET_BTN_H) return false;
  int x0 = left ? PDET_L_X : PDET_R_X;
  int w = left ? PDET_L_W : PDET_R_W;
  return x >= x0 && x <= x0 + w;
}

// YES / NO on the release confirm. Returns true when the tap was consumed. The
// caller swallows everything else while it is up: a modal that leaks a miss
// through to what is drawn underneath is precisely how an irreversible action
// gets triggered by a fumbled tap, which is the shape CLAUDE.md section 4 keeps
// warning about.
bool monSheetConfirmTap(int16_t x, int16_t y, bool fromBox) {
  int c1t, c1b, c2t, c2b;
  uiConfirmRects(&c1t, &c1b, &c2t, &c2b);
  if (x < CONFIRM_BTN_X || x > CONFIRM_BTN_X + CONFIRM_BTN_W) return false;
  if (y >= c1t && y <= c1b) {            // YES -- and it does not come back
    // Promised to a trade: never released out from under it.
    const PartyMon &gone = fromBox ? party.box[boxDetail - 1] : party.slots[partyDetail - 1];
    if (tradeLocked(gone)) { releaseConfirm = false; sfxPlay(SFX_DENY); return true; }
    if (fromBox) { party.boxReleaseAt(boxDetail - 1); boxDetail = 0; }
    else { party.releaseAt(partyDetail - 1); partyDetail = 0; }
    // A half-finished swap named a slot that may now be empty, so disarm both
    // sides rather than leaving one pointing at a creature that is gone.
    boxSwapFrom = 0;
    boxSel = 0;
    releaseConfirm = false;
    sfxPlay(SFX_BYE);
    return true;
  }
  if (y >= c2t && y <= c2b) {            // NO
    releaseConfirm = false;
    sfxPlay(SFX_TAP);
    return true;
  }
  return false;
}

// The six party/box slots on a RING rather than a 2x3 grid; the geometry is
// declared with the other party layout constants above.
void partySlotPos(int i, int *cx, int *cy) {
  float a = (-120.0f + 60.0f * (float)i) * 0.01745329f;   // clockwise from top-left
  if (cx) *cx = CX + (int)(cosf(a) * PSLOT_RING);
  if (cy) *cy = PSLOT_CY + (int)(sinf(a) * PSLOT_RING);
}

// Which slot a tap landed in, or -1. The draw path and both tap paths ask this
// one function; the grid they replace had its rectangle written out in three
// places, which is how the party screen's controls drifted apart before.
int partySlotAt(int16_t x, int16_t y) {
  for (int i = 0; i < PARTY_SLOTS; i++) {
    int sx, sy;
    partySlotPos(i, &sx, &sy);
    int dx = x - sx, dy = y - sy;
    if (dx * dx + dy * dy <= PSLOT_R * PSLOT_R) return i;
  }
  return -1;
}

bool partyHubAt(int16_t x, int16_t y) {
  int dx = x - CX, dy = y - PSLOT_CY;
  return dx * dx + dy * dy <= PSLOT_HUB_R * PSLOT_HUB_R;
}

// Exposed so hit_test can hold every control to the finger floor without
// keeping its own copy of the number it is enforcing.
int uiTapFinger() { return UI_TAP_FINGER; }

// The home icons' TAP radius, which is larger than the graphic they draw.
int uiButtonHitR() { return BTN_HIT; }

void partyTap(int16_t x, int16_t y) {
  if (cordPick) { cordPickTap(x, y); return; }
  if (boxOpen) { boxTap(x, y); return; }
  // The SHEET IS CHECKED FIRST, and that is not cosmetic ordering. It covers the
  // whole screen, and its RELEASE button (x240..370, y336..384) lands inside the
  // BOX button's hit area (x138..328, y312..372) below -- so with BOX tested
  // first, tapping RELEASE opened the box instead. Nothing behind a full-screen
  // sheet may answer a tap.
  if (partyDetail) {
    // The confirm is modal: while it is up nothing else on the sheet responds,
    // or a miss on YES would fall through to the move rows underneath it.
    if (releaseConfirm) { monSheetConfirmTap(x, y, false); return; }
    if (monSheetBtn(x, y, true)) {           // RAISE THIS ONE
      if (pet.ceremony != CER_NONE || pet.awaitingStarter()) { sfxPlay(SFX_DENY); return; }
      focusSwap((uint8_t)(partyDetail - 1));
      partyDetail = 0;
      boxSwapFrom = 0;
      partyOpen = false;
      sfxPlay(SFX_HATCH);
      return;
    }
    if (monSheetBtn(x, y, false)) {          // RELEASE -- ask first, always
      releaseConfirm = true;
      sfxPlay(SFX_TAP);
      return;
    }
    for (int i = 0; i < MOVE_SLOTS; i++) {   // tap a move to change it
      int ry = 78 + i * 52;
      if (x < 70 || x > 396 || y < ry || y > ry + 50) continue;
      movePickParty = partyDetail;
      movePickSlot = i;
      movePickPage = 0;
      movePickOpen = true;
      sfxPlay(SFX_TAP);
      return;
    }
    partyDetail = 0;
    releaseConfirm = false;
    sfxPlay(SFX_TAP);
    return;
  }
  if (!partyPick && partyHubAt(x, y)) {
    boxOpen = true;                  // open the box, nothing picked yet
    boxPage = 0;
    boxSwapFrom = 0;
    sfxPlay(SFX_TAP);
    return;
  }
  // exit button, and the top band, both always work
  if ((y >= PARTYCLOSE_Y && y <= PARTYCLOSE_Y + PARTYCLOSE_H &&
       x >= PARTYCLOSE_X && x <= PARTYCLOSE_X + PARTYCLOSE_W) || y < 34) {
    if (partyPick) {                 // declined the swap: the pet is let go
      partyPick = false;
      pet.clearEnded();
    }
    partyOpen = false;
    sfxPlay(SFX_TAP);
    return;
  }
  for (int i = 0; i < PARTY_SLOTS; i++) {
    if (partySlotAt(x, y) != i) continue;
    if (boxSel) {                    // a box creature is waiting for a slot
      party.swapPartyBox(i, boxSel - 1);
      boxSel = 0;
      sfxPlay(SFX_MEDAL);
      return;
    }
    if (!partyPick) {
      if (boxSwapFrom == i + 1) {    // tapped again: take it to the box
        boxOpen = true;
        boxPage = 0;
        sfxPlay(SFX_TAP);
        return;
      }
      if (party.slots[i].empty()) { boxSwapFrom = i + 1; sfxPlay(SFX_TAP); return; }
      partyDetail = i + 1;
      boxSwapFrom = i + 1;           // armed, in case the box is opened next
      sfxPlay(SFX_TAP);
      return;
    }
    if (tradeLocked(party.slots[i])) { sfxPlay(SFX_DENY); return; }   // promised to a trade
    party.replaceAt(i, pet.endedMon);
    snprintf(partyBannerName, sizeof(partyBannerName), "%s",
             pet.endedMon.nick[0] ? pet.endedMon.nick : DEX_TBL[pet.endedMon.dex].name);
    partyBannerUntil = millis() + 3500;
    pet.clearEnded();
    partyPick = false;
    partyOpen = false;
    sfxPlay(SFX_MEDAL);
    return;
  }
}

// deslizar: dir +1 = hacia la derecha
//
// HORIZONTAL IS THE TILE AXIS, and nothing else. Every screen used to keep its
// own opinion in here -- some paged, some closed, and several paged and THEN
// closed off the end, which is one gesture meaning two things and is exactly
// how the same paging bug shipped four times. Paging moved to the rim
// (onRim), which is what frees this axis to mean one thing.
void onSwipe(int dir) {
  // First boot has no axis yet -- nothing but the region chooser exists -- so
  // horizontal still pages it there. The rim is not a gesture anyone has been
  // taught on the first screen they ever see.
  if (pet.awaitingStarter()) { rpickSwipe(dir); return; }
  if (menuOpen) { menuOpen = false; return; }   // any swipe closes the menu
  if (blackoutOpen) { blackoutDismiss(); return; }
  if (battleOpen || tradeOpen) return;   // no swiping out of a fight or a trade

  int t = uiTileIndex();
  if (t >= 0) {
    // A sheet drawn on top of a tile closes first; the axis is underneath it.
    if (galleryDetail) { galleryDetail = 0; galleryPmd.unload(); galleryDirty = true; return; }
    if (partyDetail) { partyDetail = 0; return; }
    if (partyPick) { partyPick = false; pet.clearEnded(); }
    if (pet.ceremony || confirmUntil || martConfirmItem) return;
    int n = t - dir;   // the content follows the finger
    // IT BUMPS. This is the whole point of the axis: a horizontal swipe can no
    // longer close anything, so it cannot be confused with paging.
    if (n < 0 || n >= TILE_COUNT) { sfxPlay(SFX_DENY); return; }
    uiTileGo(n);
    return;
  }

  // Off the axis are modals -- pickers, sheets, minigames. None of them page
  // any more, so here horizontal simply backs out.
  if (avatarPickOpen) { avatarPickOpen = false; return; }
  if (areaListOpen) { areaListOpen = false; return; }
  if (pickOpen) { pickOpen = false; return; }
  if (trainOpen) { trainOpen = false; return; }
  if (bagOpen) { bagOpen = false; return; }
  if (movePickOpen) { movePickOpen = false; return; }
  if (boxOpen) { boxOpen = false; boxSel = 0; return; }
  if (gameOpen) { leaveGame(); return; }   // swipe out, keeping what you earned
  if (spdOpen) { leaveSpeed(); return; }
  if (kbOpen || clockOpen) return;
  if (cardOpen) { cardOpen = false; return; }
}

void onTap(int16_t x, int16_t y) {
  if (pet.awaitingStarter()) {  // primera partida: region y luego inicial
    if (!starterRegionDone) {
      int r = regionPickTap(x, y, RPICK_FOR_START);
      if (r >= 0) {
        pet.setRegion((uint8_t)r);   // the region picked here is where eggs come from too
        starterRegionDone = true;
        sfxPlay(SFX_TAP);
      }
      return;
    }
    for (int i = 0; i < starterCountShown(pet.region); i++) {
      int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
      if (x >= 70 && x <= 396 && y >= ry && y <= ry + STARTER_ROW_H) {
        pet.chooseStarter(starterOf(pet.region, (uint8_t)i));
        sfxPlay(SFX_TAP);
        break;
      }
    }
    return;
  }
  if (tradeOpen) {
    tradeTap(x, y);
    return;
  }
  if (blackoutOpen) {
    blackoutDismiss();
    return;
  }
  if (battleOpen) {
    battleTap(x, y);
    return;
  }
  if (exploreOpen) {
    if (areaListOpen) { areaListTap(x, y); return; }
    int8_t regionStep = exploreRegionHit(x, y);
    if (regionStep) {
      exploreStepRegion(regionStep);
      sfxPlay(SFX_TAP);
      return;
    }
    int ax, ay, aw, ah;
    exploreAreaRect(&ax, &ay, &aw, &ah);
    if (x >= ax && x <= ax + aw && y >= ay && y <= ay + ah) {
      if (!wildRegionHasAreas(currentExploreRegion())) { sfxPlay(SFX_DENY); return; }
      openAreaList();
      sfxPlay(SFX_TAP);
      return;
    }
    if (y >= EXPLORE_DIF_Y && y <= EXPLORE_DIF_Y + EXPLORE_DIF_H) {
      exploreHard = !exploreHard;
      sfxPlay(SFX_TAP);
      return;
    }
    int bx, by, bw, bh;
    exploreBtnRect(&bx, &by, &bw, &bh);
    if (x >= bx - EXPLORE_BTN_PAD && x <= bx + bw + EXPLORE_BTN_PAD &&
        y >= by - EXPLORE_BTN_PAD && y <= by + bh + EXPLORE_BTN_PAD) {
      if (!startWildBattle(currentExploreRegion(), exploreHard)) { sfxPlay(SFX_DENY); return; }
      exploreOpen = false;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  if (martOpen) {
    martTap(x, y);
    return;
  }
  if (bagOpen) {
    bagTap(x, y);
    return;
  }
  if (playerOpen) {
    if (avatarPickOpen) { avatarPickTap(x, y); return; }
    if (playerPage == 0 && y >= 32 && y < 68) {   // the name: rename yourself
      openKeyboardFor(KB_TRAINER);
      sfxPlay(SFX_TAP);
      return;
    }
    // the avatar is the only other live target; everything else backs out
    if (playerPage == 0 && x > CX - 40 && x < CX + 40 && y > 70 && y < 146) {
      avatarPickOpen = true;
      avatarPage = (uint8_t)((pet.avatar % AVATAR_COUNT) / AV_PER_PAGE);   // open on yours
      sfxPlay(SFX_TAP);
      return;
    }
    playerOpen = false;
    return;
  }
  if (pickOpen) {
    pickTap(x, y);
    return;
  }
  if (lanOpen) {
    lanTap(x, y);
    return;
  }
  if (gymOpen && gymPick) {
    int r = regionPickTap(x, y, RPICK_FOR_GYMS);
    if (r >= 0) {
      gymRegion = (uint8_t)r;
      gymPage = 0;
      gymPick = false;
      sfxPlay(SFX_TAP);
      return;
    }
    if (lanBtnHit(x, y)) {                               // LAN battle
      gymOpen = false; gymPick = false;
      lan.state = LINK_OFF;
      lanOpen = true;
      sfxPlay(SFX_TAP);
      return;
    }
    // Anything else is swallowed: there is no tap-to-leave here any more.
    return;
  }
  if (gymOpen) {
    if (y >= GYMDIF_Y && y <= GYMDIF_Y + GYMDIF_H) {   // the difficulty pill
      gymHard = !gymHard;
      sfxPlay(SFX_TAP);
      return;
    }
    if (x >= GYMBTN_X && x <= GYMBTN_X + GYMBTN_W &&
        y >= GYMBTN_Y && y <= GYMBTN_Y + GYMBTN_H) {
      gymOpen = false;
      lan.state = LINK_OFF;
      lanOpen = true;
      sfxPlay(SFX_TAP);
      return;
    }
    for (int i = 0; i < GYM_ROWS; i++) {
      uint8_t idx = gymPage * GYM_ROWS + i;
      if (idx >= TRAINER_COUNT) break;
      int rx, ry, rw, rh;
      gymRowRect(i, &rx, &ry, &rw, &rh);
      if (x < rx || x > rx + rw || y < ry || y > ry + rh) continue;
      if (!gymUnlocked(idx, gymHard)) { sfxPlay(SFX_DENY); return; }
      sfxPlay(SFX_TAP);
      gymOpen = false;
      pickTrainer = idx;
      pickHard = gymHard;
      pickRegion = gymRegion;     // BEFORE squadCap: it reads pickRegion now
      pickPage = 0;
      pickDefault(squadCap(idx, gymHard));
      pickOpen = true;
      return;
    }
    gymOpen = false;
    return;
  }
  if (pet.hasLearnOffer()) {
    for (int i = 0; i < MOVE_SLOTS; i++) {
      int ry = LEARN_ROW_Y(i);
      if (x < 70 || x > 396 || y < ry || y > ry + 50) continue;
      sfxPlay(SFX_TAP);
      pet.acceptLearn(i);
      return;
    }
    if (x >= 70 && x <= 396 && y >= LEARN_SKIP_Y && y <= LEARN_SKIP_Y + 44) {
      sfxPlay(SFX_TAP);
      pet.declineLearn();
    }
    return;   // modal: nothing else on screen responds until it is answered
  }
  if (trainOpen) {
    trainTap(x, y);
    return;
  }
  // The menu is modal and has three independent ways out: the CLOSE row, a tap
  // anywhere on the dimmed area outside the panel, and any swipe (see onSwipe).
  // Deliberately no timeout: a menu that vanishes while you read it is worse
  // than one that lingers.
  if (menuOpen) {
    bool inPanel = (x >= MENU_X && x <= MENU_X + MENU_W &&
                    y >= MENU_Y && y <= MENU_Y + MENU_H);
    if (!inPanel) { menuOpen = false; return; }   // tap outside = back to the pet
    for (int i = 0; i < MENU_ROWS; i++) {
      int ry = MENU_ROW_Y(i);
      if (x < MENU_X + 18 || x > MENU_X + MENU_W - 18) continue;
      if (y < ry || y > ry + MENU_ROW_H) continue;
      sfxPlay(SFX_TAP);
      menuOpen = false;
      if (i == 0) { cardOpen = true; cardPage = 1; }   // straight to the stats page
      else if (i == 1) { bagOpen = true; bagPage = 0; }
      else if (i == 2) { galleryOpen = true; galleryPick = true; galleryPage = 0; rpickPage = 0; galleryDetail = 0; galleryDirty = true; }
      else if (i == 3) { openClock(); }
      else if (i == 4) {
        if (!pet.canRetireNow()) { sfxPlay(SFX_DENY); return; }
        choiceKind = 3; choiceUntil = millis() + 12000;
      }
      return;                                     // i == 5 is CLOSE: just shut
    }
    return;
  }
  if (movePickOpen) {
    uint8_t all[64];
    uint8_t n = learnableList(all, sizeof(all));
    for (uint8_t i = 0; i < MOVE_PICK_PER_PAGE; i++) {
      uint8_t idx = movePickPage * MOVE_PICK_PER_PAGE + i;
      if (idx >= n) break;
      int ry = MOVE_PICK_Y(i);
      if (x < 70 || x > 396 || y < ry || y > ry + 50) continue;
      sfxPlay(SFX_TAP);
      // Swapping for a move already in another slot would silently duplicate
      // it, so trade the two slots instead of overwriting.
      uint8_t *tgt = pickTargetMoves();
      for (int s = 0; s < MOVE_SLOTS; s++)
        if (tgt[s] == all[idx] && s != movePickSlot) tgt[s] = tgt[movePickSlot];
      tgt[movePickSlot] = all[idx];
      if (movePickParty) party.save(); else pet.saveNow();
      movePickOpen = false;
      return;
    }
    movePickOpen = false;   // tap anywhere else = back to the moves page
    return;
  }
  if (partyOpen) {
    partyTap(x, y);
    return;
  }
  if (galleryOpen) {
    if (galleryPick) {
      int r = regionPickTap(x, y, RPICK_FOR_DEX);
      if (r >= 0) {
        galleryRegion = (uint8_t)r;
        galleryPage = 0;
        galleryDetail = 0;
        galleryDirty = true;
        galleryPick = false;
        sfxPlay(SFX_TAP);
      } else if (rpickBackHit(x, y, RPICK_FOR_DEX)) {
        galleryOpen = false;
      }
      return;
    }
    galleryTap(x, y);
    return;
  }
  if (kbOpen) {
    keyboardTap(x, y);
    return;
  }
  if (clockOpen) {
    clockTap(x, y);
    return;
  }
  if (pet.ceremony) return;  // durante la despedida no hay botones
  if (cardOpen) {
    if (cardPage == 0 && y < 84) openKeyboard();  // tocar el nombre = renombrar
    else if (cardPage == 2) {
      for (int i = 0; i < MOVE_SLOTS; i++) {   // tap a slot to change it
        int ry = MOVE_ROW_Y(i);
        if (x < 70 || x > 396 || y < ry || y > ry + 50) continue;
        sfxPlay(SFX_TAP);
        movePickParty = 0;      // the live pet
        movePickSlot = i;
        movePickPage = 0;
        movePickOpen = true;
        return;
      }
      cardOpen = false;            // anywhere else on the page still exits
    } else {
      cardOpen = false;
    }
    return;
  }
  if (spdOpen) {
    spdTap(x, y);
    return;
  }
  if (gameOpen) {
    gameTap(x, y);
    return;
  }
  if (choiceKind) {          // dialogo de decision: boton accion (arriba) / mantener (abajo)
    int c1t, c1b, c2t, c2b;
    uiConfirmRects(&c1t, &c1b, &c2t, &c2b);
    const bool inX = (x >= CONFIRM_BTN_X && x <= CONFIRM_BTN_X + CONFIRM_BTN_W);
    bool b1 = inX && y >= c1t && y <= c1b;   // accion
    bool b2 = inX && y >= c2t && y <= c2b;   // mantener / quedaros
    if (choiceKind == 1) {                 // evolucion
      if (b1) { int16_t old = pet.speciesId; pet.evolve(); evoPmd.load(old, pet.shiny); }
      else if (b2) pet.declineEvolve();
    } else if (choiceKind == 3) {          // retirada a peticion
      if (b1) pet.startRetire();
      // b2 is simply "no": nothing to decline, the row is always there
    } else if (choiceKind == 2) {          // despedida
      if (b1) pet.startFarewell();
      else if (b2) pet.declineFarewell();
    }
    choiceKind = 0;
    return;
  }
  if (confirmUntil) {        // dialogo "soltar?": SI / NO
    if (millis() < confirmUntil && x >= 118 && x <= 218 && y >= 252 && y <= 304) {
      pet.release();
    }
    confirmUntil = 0;
    return;
  }
  if (feedMenuUntil) {       // selector de comida
    if (millis() < feedMenuUntil && y >= FEED_MENU_Y && y <= FEED_MENU_Y + FEED_MENU_H &&
        x >= FEED_MENU_X && x <= FEED_MENU_X + FEED_MENU_W) {
      int item = (x - FEED_MENU_X) / (FEED_MENU_W / 4);
      if (item > 3) item = 3;
      if (item == 3) pet.feedCandy();
      else pet.feedBerry(item);
      sfxPlay(SFX_EAT);
    }
    feedMenuUntil = 0;
    return;
  }
  if (pet.isEgg()) {
    // the region pill first, or choosing a region would also crack the egg --
    // and a near miss is swallowed rather than counted, since three taps hatch
    if (eggRegionTap(x, y)) return;
    pet.eggTap();
    sfxPlay(SFX_TAP);
    return;
  }
  // boton de evolucion: abre el dialogo evolucionar/mantener
  if (pet.wantEvolveButton() && x >= EVO_BTN_X && x <= EVO_BTN_X + EVO_BTN_W &&
      y >= EVO_BTN_Y && y <= EVO_BTN_Y + EVO_BTN_H) {
    choiceKind = 1; choiceUntil = millis() + 12000;
    return;
  }
  // botones de final (mismo recuadro): escapada directa; despedida abre dialogo.
  //
  // The runaway does NOT ask, deliberately. A pet you have to authorise to
  // leave is not really at stake, and neglect having teeth is the whole premise.
  // What was actually wrong is that a night's sleep could reach this state at
  // all -- fixed where it belongs, in the drain, by having the creature put
  // itself to bed (Pet::tick, autoSleep).
  if (x >= FAR_BTN_X && x <= FAR_BTN_X + FAR_BTN_W &&
      y >= FAR_BTN_Y && y <= FAR_BTN_Y + FAR_BTN_H) {
    if (pet.canRunawayNow()) { pet.startRunaway(); return; }
    if (pet.wantFarewellButton()) { choiceKind = 2; choiceUntil = millis() + 12000; return; }
  }
  for (int i = 0; i < BTN_COUNT; i++) {
    int dx = x - buttons[i].cx, dy = y - buttons[i].cy;
    if (dx * dx + dy * dy <= BTN_HIT * BTN_HIT) {
      if (uiButtonDisabled(i)) { sfxPlay(SFX_DENY); return; }
      sfxPlay(SFX_TAP);
      if (i == BTN_FOOD) feedMenuUntil = millis() + 6000;
      else if (i == BTN_LIGHT) pet.toggleLight();
      else if (i == BTN_BATH) startBath();
      else { trainOpen = true; trainPage = 0; }
      return;
    }
  }
  // tapping the name/status band opens the menu. This band was inert before,
  // and it sits clear of inPetZone (which starts at y 95).
  if (y >= 28 && y < 94) {
    menuOpen = true;
    sfxPlay(SFX_TAP);
    return;
  }
  // tocar al bicho = caricia
  if (inPetZone(x, y)) {
    pet.caress();
    if (!pet.sleeping) sfxPlay(SFX_HEART);
  }
}

// ---------- render ----------

bool gNight = false;  // noche real (por hora) o durmiendo: lo fija render()
uint16_t inkColor() { return gNight ? UI_INK_NIGHT : UI_INK; }

// ---------- escena de fondo: bioma del tipo + hora real del RTC ----------

#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define HORIZON 232  // linea donde el cielo se encuentra con el suelo

uint16_t lerp565(uint16_t a, uint16_t b, int i, int n) {
  if (n <= 0) return a;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)((((ar + (br - ar) * i / n) << 11)) |
                    (((ag + (bg - ag) * i / n) << 5)) | (ab + (bb - ab) * i / n));
}

// hora del dia 0-23 (de la hora real cacheada cada 30s; 13 si no hay reloj)
int sceneHour() {
  uint32_t e = pet.lastSeenEpoch;
  return e ? (int)((e / 3600) % 24) : 13;
}

// suelo de cada bioma de dia (de noche se mezcla hacia el azul nocturno)
static const uint16_t BIOME_SOIL[6] = {
  C565(0x7e, 0xc0, 0x7f),  // 0 pradera
  C565(0xdc, 0xca, 0x94),  // 1 playa (arena)
  C565(0x4f, 0x8a, 0x55),  // 2 bosque
  C565(0x8a, 0x55, 0x44),  // 3 volcan
  C565(0xa8, 0x90, 0x6a),  // 4 montana
  C565(0xe6, 0xee, 0xf5),  // 5 nieve
};

void drawClouds(uint32_t now, uint16_t col) {
  for (int k = 0; k < 2; k++) {
    int cx = (int)((now / 50 + k * 250) % 560) - 40;
    int cy = 70 + k * 34;
    gfx->fillCircle(cx, cy, 16, col);
    gfx->fillCircle(cx + 18, cy + 3, 13, col);
    gfx->fillCircle(cx - 15, cy + 4, 12, col);
  }
}

void drawScene(uint8_t biome, uint32_t now, bool night) {
  int h = sceneHour();
  uint16_t top, bot;
  if (night)            { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (h < 8)       { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }  // amanecer
  else if (h < 18)      { top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }  // dia
  else                  { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }  // atardecer

  // cielo en bandas
  for (int y = 0; y < HORIZON; y += 8)
    gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, HORIZON));

  // sol o luna
  if (night) {
    gfx->fillCircle(360, 78, 24, C565(0xe8, 0xee, 0xf5));
    gfx->fillCircle(370, 72, 22, lerp565(top, bot, 78, HORIZON));  // creciente
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  } else if (h < 18) {
    gfx->fillCircle(360, 84, 26, h < 8 ? C565(0xff, 0xd9, 0x8a) : C565(0xff, 0xe7, 0x9f));
    drawClouds(now, C565(0xff, 0xff, 0xff));
  } else {
    gfx->fillCircle(233, HORIZON - 6, 34, C565(0xff, 0xf1, 0xc8));  // sol poniente
  }

  // mar de la playa: una franja de agua sobre la arena
  uint16_t soil = BIOME_SOIL[biome < 6 ? biome : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  if (biome == 1) {
    uint16_t sea = night ? C565(0x1c, 0x34, 0x52) : C565(0x4f, 0x96, 0xc4);
    gfx->fillRect(0, HORIZON - 26, 466, 26, sea);
    for (int i = 0; i < 3; i++) {
      int wy = HORIZON - 22 + i * 7;
      uint16_t fc = night ? C565(0x3a, 0x58, 0x78) : C565(0xbf, 0xe6, 0xf5);
      gfx->fillRect(60 + ((now / 60 + i * 30) % 60), wy, 26, 2, fc);
      gfx->fillRect(300 - ((now / 60 + i * 20) % 60), wy, 26, 2, fc);
    }
  }

  // suelo
  gfx->fillRect(0, HORIZON, 466, 466 - HORIZON, soil);
  uint16_t hill = lerp565(soil, night ? C565(0x0c, 0x12, 0x24) : C565(0xff, 0xff, 0xff), 3, 16);
  gfx->fillRoundRect(-60, HORIZON - 14, 586, 60, 30, hill);

  // detalles del bioma
  uint16_t dk = lerp565(soil, C565(0x10, 0x18, 0x20), night ? 11 : 7, 16);
  if (biome == 2) {  // bosque: coniferas en silueta
    for (int tx : { 60, 150, 360, 416 }) {
      gfx->fillTriangle(tx, HORIZON - 46, tx - 16, HORIZON, tx + 16, HORIZON, dk);
      gfx->fillTriangle(tx, HORIZON - 60, tx - 12, HORIZON - 28, tx + 12, HORIZON - 28, dk);
    }
  } else if (biome == 3) {  // volcan: rocas y brasas
    gfx->fillTriangle(70, HORIZON, 40, HORIZON + 30, 100, HORIZON + 30, dk);
    gfx->fillTriangle(400, HORIZON + 4, 372, HORIZON + 30, 430, HORIZON + 30, dk);
    if (!night)
      for (int e = 0; e < 4; e++)
        gfx->fillRect(120 + e * 70, HORIZON + 8 + (e % 2) * 6, 4, 4, C565(0xff, 0x9b, 0x3a));
  } else if (biome == 4) {  // montana: cumbres al fondo
    gfx->fillTriangle(140, HORIZON - 50, 60, HORIZON, 220, HORIZON, dk);
    gfx->fillTriangle(330, HORIZON - 38, 250, HORIZON, 410, HORIZON, dk);
  } else if (biome == 5 && !night) {  // nieve: copos cayendo
    for (int f = 0; f < 10; f++) {
      int fx = (f * 53 + now / 40) % 466;
      int fy = (f * 90 + now / 18) % HORIZON;
      gfx->fillRect(fx, fy, 3, 3, UI_WHITE);
    }
  } else if (biome == 0) {  // pradera: matas de hierba
    for (int gx : { 80, 175, 300, 395 })
      for (int b = -1; b <= 1; b++)
        gfx->fillRect(gx + b * 5, HORIZON + 6, 2, 8 + (b == 0 ? 4 : 0), dk);
  }
}

// primera partida: elige inicial entre Bulbasaur / Charmander / Squirtle
void renderStarterSelect() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  const char *t = T(S_CHOOSE_STARTER);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(t) * 6, 68);
  gfx->print(t);
  for (int i = 0; i < starterCountShown(pet.region); i++) {
    int16_t d = starterOf(pet.region, i);
    const DexEntry &de = DEX_TBL[d];
    int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
    gfx->fillRoundRect(70, ry, 326, STARTER_ROW_H, 14, lerp565(de.accent, UI_WHITE, 6, 8));
    gfx->drawRoundRect(70, ry, 326, STARTER_ROW_H, 14, de.accent);
    const uint8_t *th = thumbs.get(d);     // miniatura del inicial (si la SD esta lista)
    if (th) drawThumb(th, 76, ry - 5, 3, false);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(3);
    gfx->setCursor(178, ry + 24);
    gfx->print(de.name);
  }
  gfx->flush();
}

// ---------- crash breadcrumbs ----------
//
// "It has a mini crash" is not a bug report you can act on, because the board
// never says WHY it restarted. These three live in RTC memory, which survives
// a panic, a watchdog and a software reset -- everything except pulling the
// power -- so the next boot can say what the firmware was doing when it died.
//
// Written every frame. That is one word to RTC RAM per render, which costs
// nothing next to a full-screen redraw, and it means the crumb always names
// the screen that was actually on the panel rather than the last one opened.
#define CRUMB_MAGIC 0x7AABE10C
RTC_NOINIT_ATTR uint32_t gCrumbMagic;
RTC_NOINIT_ATTR uint32_t gCrumbScreen;
RTC_NOINIT_ATTR uint32_t gCrumbHeap;


// Which screen is on the panel RIGHT NOW, in the same order render() tests.
uint8_t uiCurrentScreen() {
  if (pet.awaitingStarter()) return starterRegionDone ? SCR_STARTER : SCR_REGION;
  if (tradeOpen) return SCR_TRADE;
  if (blackoutOpen) return SCR_BLACKOUT;
  if (galleryOpen) return galleryPick ? SCR_DEXPICK : SCR_GALLERY;
  if (movePickOpen) return SCR_MOVEPICK;
  if (partyOpen) return boxOpen ? SCR_BOX : SCR_PARTY;
  if (bagOpen) return SCR_BAGSCR;
  if (kbOpen) return SCR_KEYBOARD;
  if (cardOpen) return SCR_CARD;
  if (playerOpen) return avatarPickOpen ? SCR_AVATAR : SCR_PLAYER;
  if (clockOpen) return SCR_CLOCK;
  if (btlWinUntil) return SCR_WIN;
  if (battleOpen) return SCR_BATTLE;
  if (pickOpen) return SCR_PICK;
  if (lanOpen) return SCR_LAN;
  if (exploreOpen) return areaListOpen ? SCR_AREA : SCR_EXPLORE;
  if (martOpen) return SCR_MART;
  if (gymOpen) return gymPick ? SCR_GYMPICK : SCR_GYM;
  if (pet.hasLearnOffer()) return SCR_LEARN;
  if (gameOpen || sackOpen || spdOpen || catchOpen || miniGame) return SCR_GAME;
  if (trainOpen) return SCR_TRAIN;
  if (menuOpen) return SCR_MENU;
  return SCR_MAIN;
}

// ---------------------------------------------------------------------------
// The tile axis and the rim.
//
// Two rules replace the pile of per-screen opinions that used to live in
// onSwipe(), and between them they close the bug this project shipped FOUR
// times -- a paged screen that CLOSED instead of paging:
//
//   HORIZONTAL is the tile axis. Six peer screens with the pet near the middle,
//   and it BUMPS at the ends. It can no longer close anything, so the gesture
//   that means "next page" and the gesture that means "exit" are different
//   gestures, which they never were before.
//
//   THE RIM is paging. A drag that starts outside RIM_R0 pages whatever is in
//   front of you, and CLAMPS at both ends. This is what frees the horizontal
//   axis -- A and C only work as a pair, and this is the joint.
//
// Vertical stays depth: up opens, down goes back.

// A tile's screen, and the chooser it opens on where it has one. Both belong to
// the same tile: the region choosers are not a level of depth, they ARE the gym
// and Pokedex tiles until a region is picked. One table, so uiTileIndex() and
// uiTileGo() cannot end up with different ideas of what is on the axis.
//
// PLAYER . PARTY . [PET] . EXPLORE . MART . GYM . DEX -- yours on the left, the
// world on the right. Exploration is a primary loop, not a gym action, so it
// gets the first world-facing stop and the main screen can name it directly.
// The Mart sits right after it: walk to earn (EXPLORE's own encounters cost
// nothing to reach), spend at the Mart, then prove the gear at the gym.
struct TileDef { uint8_t scr, alt; };
static const TileDef TILE[TILE_COUNT] = {
  { SCR_PLAYER, SCR_PLAYER }, { SCR_PARTY, SCR_PARTY }, { SCR_MAIN, SCR_MAIN },
  { SCR_EXPLORE, SCR_EXPLORE }, { SCR_MART, SCR_MART }, { SCR_GYM, SCR_GYMPICK },
  { SCR_GALLERY, SCR_DEXPICK },
};

int uiTileIndex() {
  uint8_t s = uiCurrentScreen();
  for (int i = 0; i < TILE_COUNT; i++)
    if (TILE[i].scr == s || TILE[i].alt == s) return i;
  return -1;   // not on the axis: a modal, a minigame, a fight
}

// Jump to tile i. Every stop on the axis is a top-level screen, so this closes
// the others rather than stacking on them.
void uiTileGo(int i) {
  if (i < 0) i = 0;
  if (i >= TILE_COUNT) i = TILE_COUNT - 1;
  exploreOpen = gymOpen = partyOpen = galleryOpen = playerOpen = false;
  avatarPickOpen = false;
  areaListOpen = false;
  cordPick = false;
  boxOpen = false;
  boxDetail = partyDetail = 0;
  galleryDetail = 0;
  martOpen = false;
  martConfirmItem = ITEM_NONE;
  switch (TILE[i].scr) {
    // The two multi-region screens land on their CHOOSER, never on whichever
    // region was last set. Opening straight into it is how Johto and Hoenn came
    // to be built, reachable, and completely invisible.
    case SCR_GYM: gymOpen = true; gymPick = true; gymPage = 0; rpickPage = 0; break;
    case SCR_EXPLORE: exploreOpen = true; break;
    case SCR_MART: martOpen = true; martPage = 0; break;
    case SCR_PARTY: partyOpen = true; break;
    case SCR_GALLERY:
      galleryOpen = true; galleryPick = true; galleryPage = 0; rpickPage = 0;
      galleryDirty = true;
      break;
    case SCR_PLAYER: playerOpen = true; playerPage = 0; break;
    default: break;   // the pet tile is everything else being shut
  }
  sfxPlay(SFX_TAP);
}

// ---------- the rim ----------

// The paged screens as ONE table: which variable holds the page, and how many
// there are. Three separate switches over the same list -- one to draw the
// scrollbar, one to page, one for the test -- is precisely the shape CLAUDE.md
// warns about, so draw, input and test all come through here.
static uint8_t *uiRimTarget(uint8_t *pages) {
  uint8_t n = 1;
  uint8_t *p = nullptr;
  switch (uiCurrentScreen()) {
    case SCR_GYM:     p = &gymPage;     n = (TRAINER_COUNT + GYM_ROWS - 1) / GYM_ROWS; break;
    case SCR_GALLERY: p = &galleryPage; n = (uint8_t)GAL_PAGES; break;
    case SCR_PLAYER:  p = &playerPage;  n = PLAYER_PAGES; break;
    case SCR_AVATAR:  p = &avatarPage;  n = avatarPages(); break;
    case SCR_AREA:    p = &areaPage;    n = areaPages(currentExploreRegion()); break;
    case SCR_BOX:     p = &boxPage;     n = BOX_SLOTS / BOX_PER_PAGE; break;
    case SCR_BAGSCR:  p = &bagPage;     n = bagPages(); break;
    case SCR_MART:    p = &martPage;    n = martPages(); break;
    case SCR_CARD:    p = &cardPage;    n = CARD_PAGES; break;
    case SCR_TRAIN:   p = &trainPage;   n = trainPages(); break;
    case SCR_MOVEPICK: {
      uint8_t all[64];
      uint8_t k = learnableList(all, sizeof(all));
      p = &movePickPage;
      n = k ? (uint8_t)((k + MOVE_PICK_PER_PAGE - 1) / MOVE_PICK_PER_PAGE) : 1;
      break;
    }
    case SCR_PICK: {
      uint8_t k = pickCandidates();
      p = &pickPage;
      n = k ? (uint8_t)((k + PICK_PER_PAGE - 1) / PICK_PER_PAGE) : 1;
      break;
    }
    default: break;
  }
  if (pages) *pages = n ? n : 1;
  return p;
}

uint8_t uiRimPages() {
  uint8_t n = 1;
  if (rpickModeNow() != 0xFF) return rpickPageCount(rpickModeNow());
  uiRimTarget(&n);
  return n;
}

uint8_t uiRimPage() {
  if (rpickModeNow() != 0xFF) return rpickPage;
  uint8_t *p = uiRimTarget(nullptr);
  return p ? *p : 0;
}

// dir > 0 is clockwise, which is forwards.
void onRim(int dir) {
  if (rpickSwipe(-dir)) return;   // the chooser pages itself, and WRAPS
  uint8_t n = 1;
  uint8_t *p = uiRimTarget(&n);
  if (!p || n < 2) return;
  int v = (int)(*p) + dir;
  if (v < 0) v = 0;
  if (v >= n) v = n - 1;          // CLAMPS: the rim can never close a screen
  if (v == (int)(*p)) return;
  *p = (uint8_t)v;
  if (uiCurrentScreen() == SCR_GALLERY) galleryDirty = true;
  sfxPlay(SFX_TAP);
}

bool uiOnRim(int16_t x, int16_t y) {
  int dx = x - CX, dy = y - CY;
  return dx * dx + dy * dy >= RIM_R0 * RIM_R0;
}

float uiRimAngle(int16_t x, int16_t y) {
  return atan2f((float)(y - CY), (float)(x - CX)) * 57.29578f;
}

// ---------- the chrome that makes both of the above visible ----------
#define TILE_DOT_Y 440
// The finger-sized LAN button runs down towards the tile dots; it must stop
// short of the active dot's pill (TILE_DOT_Y - 3).
static_assert(LANBTN_Y + LANBTN_H < TILE_DOT_Y - 3,
              "the LAN button overlaps the tile dots");

static inline uint16_t uiChromeInk() {
  return uiCurrentScreen() == SCR_MAIN ? inkColor() : UI_INK;
}

// The tile axis, drawn on every screen that is on it. Five bound gestures that
// nothing on the panel mentions is precisely how Johto and Hoenn came to be
// built, reachable and completely invisible; an axis nobody can see is that
// same mistake one level up.
void uiDrawTileDots() {
  int t = uiTileIndex();
  if (t < 0) return;
  uint16_t ink = uiChromeInk();
  if (t == TILE_PET) {
    char left[28], right[24];
    snprintf(left, sizeof(left), "< %s", T(S_PARTY));
    snprintf(right, sizeof(right), "%s >", T(S_EXPLORE));
    gfx->setTextColor(ink);
    gfx->setTextSize(2);
    gfx->setCursor(CX - 8 - (int)strlen(left) * 12, 408);
    gfx->print(left);
    gfx->setCursor(CX + 8, 408);
    gfx->print(right);
  }
  for (int i = 0; i < TILE_COUNT; i++) {
    int x = CX - (TILE_COUNT - 1) * 13 + i * 26;
    if (i == t) gfx->fillRoundRect(x - 9, TILE_DOT_Y - 3, 18, 7, 3, ink);
    else gfx->fillCircle(x, TILE_DOT_Y, 3, UI_TRACK);
  }
}

// The arc scrollbar, at 2-4 o'clock, replacing the dot rows the paged screens
// used to carry. A straight bar is at its thinnest exactly where a list is
// longest, and dots show position without showing extent. Both this and the
// rim drag read uiRimTarget(), so they cannot disagree about where you are.
void uiDrawRimBar() {
  uint8_t n = uiRimPages();
  if (n < 2) return;
  uint8_t p = uiRimPage();
  if (p >= n) p = n - 1;
  int span = RIM_BAR_A1 - RIM_BAR_A0;
  int thumb = span / n;
  if (thumb < 9) thumb = 9;
  int a0 = RIM_BAR_A0 + (span - thumb) * (int)p / (int)(n - 1);
  // Every other paged screen (GYM, GALLERY, BOX, BAG, PLAYER) keeps the
  // existing day-only UI_TRACK/uiChromeInk() bar -- untouched here on purpose.
  // The Mart is the one screen whose rim bar sits directly on the night scene
  // rather than assuming day, so it is the one that actually needs the
  // palette's UI_SCROLL_DAY/NIGHT pair -- already contrast-tested in
  // palette_test for exactly this track/thumb pairing, just never wired up.
  uint16_t track = UI_TRACK, thumbInk = uiChromeInk();
  if (uiCurrentScreen() == SCR_MART) {
    track = gNight ? UI_SCROLL_NIGHT : UI_SCROLL_DAY;
    thumbInk = gNight ? UI_INK_NIGHT : UI_INK;
  }
  uiArc(CX, CY, RIM_BAR_R, RIM_BAR_A0, RIM_BAR_A1, 7, track);
  uiArc(CX, CY, RIM_BAR_R, a0, a0 + thumb, 7, thumbInk);
}

// Called by every renderer that is on the axis or pages, immediately before it
// flushes. One call, so a screen either has the chrome or visibly does not.
void uiChrome() {
  uiDrawTileDots();
  uiDrawRimBar();
}

// Explore may rest on a region only if its pack is installed -- and, while
// EXPLORE_ALLOW_ALL is off, never on ALL. One answer for the selector, the
// default and startWildBattle(), so none of them can disagree.
bool exploreRegionAllowed(uint8_t r) {
  if (r >= REGION_COUNT || !regionAvailable(r)) return false;
#if !EXPLORE_ALLOW_ALL
  if (r == REGION_ALL) return false;
#endif
  return true;
}

uint8_t currentExploreRegion() {
  if (exploreRegionAllowed(exploreRegion)) return exploreRegion;
  uint8_t base = pet.region % REGION_COUNT;
  for (uint8_t i = 0; i < REGION_COUNT; i++) {
    uint8_t r = (uint8_t)((base + i) % REGION_COUNT);
    if (exploreRegionAllowed(r)) { exploreRegion = r; return r; }
  }
  exploreRegion = base;   // nothing installed: the button stays greyed
  return exploreRegion;
}

int8_t exploreRegionHit(int16_t x, int16_t y) {
  if (y < EXPLORE_REGION_Y || y > EXPLORE_REGION_Y + EXPLORE_REGION_H ||
      x < EXPLORE_REGION_X || x > EXPLORE_REGION_X + EXPLORE_REGION_W) return 0;
  if (x <= EXPLORE_REGION_X + EXPLORE_REGION_ARROW_W) return -1;
  if (x >= EXPLORE_REGION_X + EXPLORE_REGION_W - EXPLORE_REGION_ARROW_W) return 1;
  return 0;
}

void exploreStepRegion(int8_t step) {
  int region = currentExploreRegion();
  for (uint8_t i = 0; i < REGION_COUNT; i++) {
    region += step < 0 ? -1 : 1;
    if (region < 0) region = REGION_COUNT - 1;
    else if (region >= REGION_COUNT) region = 0;
    if (exploreRegionAllowed((uint8_t)region)) {
      exploreRegion = (uint8_t)region;
      return;
    }
  }
}

// The hour for the encounter tables' morning/day/night slots, or -1 when the
// clock was never set (wildTimeBand() then counts it as day).
int exploreHour() {
  return pet.lastSeenEpoch ? (int)((pet.lastSeenEpoch / 3600) % 24) : -1;
}

// The area Explore will look in for a region, as a WILD_AREAS index, or
// WILD_NO_AREA for a region without areas. The player's choice when it is still open;
// otherwise the first area of the furthest gate they have reached, so a player
// with three badges does not open on Route 1.
uint16_t exploreAreaOf(uint8_t region) {
  uint8_t n = wildAreaCount(region);
  uint16_t first = wildAreaFirst(region);
  if (!n) return WILD_NO_AREA;
  uint16_t pick = region < REGION_COUNT ? exploreAreaPick[region] : 0;
  if (pick && pick - 1 >= first && pick - 1 < first + n &&
      wildAreaUnlocked((uint16_t)(pick - 1), pet))
    return (uint16_t)(pick - 1);
  // The furthest open area that has something to meet now; failing that, the
  // furthest open one (it will say why it is grey, never hide it).
  uint8_t when = wildTimeBand(exploreHour());
  uint16_t best = first, bestLive = WILD_NO_AREA;
  uint8_t bestGate = 0, bestLiveGate = 0;
  for (uint8_t i = 0; i < n; i++) {
    uint16_t a = (uint16_t)(first + i);
    if (!wildAreaUnlocked(a, pet)) continue;
    if (wildAreaGate(a) > bestGate) { best = a; bestGate = wildAreaGate(a); }
    if (wildAreaHasLive(a, when, pet) &&
        (bestLive == WILD_NO_AREA || wildAreaGate(a) > bestLiveGate)) {
      bestLive = a;
      bestLiveGate = wildAreaGate(a);
    }
  }
  return bestLive != WILD_NO_AREA ? bestLive : best;
}

uint8_t areaPages(uint8_t region) {
  uint8_t n = wildAreaCount(region);
  return n ? (uint8_t)((n + AREA_ROWS - 1) / AREA_ROWS) : 1;
}

// Rows of the area list, each as wide as the glass allows at its own height --
// the same reason gymRowRect() exists. Drawing and tapping both ask this.
void areaRowRect(int i, int *x, int *y, int *w, int *h) {
  int ry = AREA_ROW_Y(i);
  int half = uiSafeHalfWidthFor(ry, ry + AREA_ROW_H) - GYM_ROW_INSET;
  if (x) *x = CX - half;
  if (y) *y = ry;
  if (w) *w = half * 2;
  if (h) *h = AREA_ROW_H;
}

// The WILD_AREAS index under a tap on the list, or -1.
int areaRowAt(int16_t x, int16_t y) {
  uint8_t region = currentExploreRegion();
  for (int i = 0; i < AREA_ROWS; i++) {
    int rx, ry, rw, rh;
    areaRowRect(i, &rx, &ry, &rw, &rh);
    if (x < rx || x > rx + rw || y < ry || y > ry + rh) continue;
    int k = areaPage * AREA_ROWS + i;
    if (k >= wildAreaCount(region)) return -1;
    return wildAreaFirst(region) + k;
  }
  return -1;
}

// The row on Explore's main screen that names the area and opens the list.
void exploreAreaRect(int *x, int *y, int *w, int *h) {
  if (x) *x = EXPLORE_REGION_X;
  if (y) *y = EXPLORE_AREA_Y;
  if (w) *w = EXPLORE_REGION_W;
  if (h) *h = EXPLORE_AREA_H;
}

// The big action, as wide as the glass allows at the bottom of the panel.
void exploreBtnRect(int *x, int *y, int *w, int *h) {
  int half = uiSafeHalfWidthFor(EXPLORE_BTN_Y, EXPLORE_BTN_Y + EXPLORE_BTN_H) - 14;
  if (x) *x = CX - half;
  if (y) *y = EXPLORE_BTN_Y;
  if (w) *w = half * 2;
  if (h) *h = EXPLORE_BTN_H;
}

// What a locked area asks for: "REQUIRES BOULDER BADGE". The badge comes from
// BADGE_NAMES; a region whose names are not filled in yet names the leader
// instead, never a bare number.
void areaGateText(uint16_t area, char *out, size_t n) {
  if (wildAreaNeedsChampion(area)) { snprintf(out, n, "%s", T(S_AREA_REQ_CHAMP)); return; }
  uint8_t rg = wildAreaRegion(area);
  uint8_t idx = (uint8_t)(wildAreaGate(area) - 1);
  const char *badge = (rg < GYM_REGIONS && idx < TRAINER_GYMS)
                          ? BADGE_NAMES[rg][idx] : nullptr;
  if (!badge && rg < GYM_REGIONS && idx < TRAINER_COUNT) badge = TRAINER_SETS[rg].list[idx].name;
  snprintf(out, n, T(S_AREA_REQUIRES), badge ? badge : "?");
}

// Centred text at size 2, dropping to size 1 only when it would not fit.
static void areaCentred(const char *t, int cx, int y, int maxW) {
  int size = (int)strlen(t) * 12 <= maxW ? 2 : 1;
  gfx->setTextSize(size);
  gfx->setCursor(cx - (int)strlen(t) * 3 * size, y);
  gfx->print(t);
}

// An area's name, starred while a legendary that lives there is still to be
// caught and can appear -- the one hint that the area holds something more.
// A locked area never gives its legendary away.
void areaCentredName(uint16_t area, bool open_, int cx, int y, int maxW) {
  char t[24];
  snprintf(t, sizeof(t), (open_ && wildAreaHasLegend(area, pet)) ? "* %s *" : "%s",
           wildAreaName(area));
  areaCentred(t, cx, y, maxW);
}

void openAreaList() {
  uint8_t region = currentExploreRegion();
  if (!wildRegionHasAreas(region)) return;
  uint16_t a = exploreAreaOf(region);
  areaPage = (uint8_t)((a - wildAreaFirst(region)) / AREA_ROWS);   // open on yours
  areaListOpen = true;
}

// The second line of an area's row: its band when it can be explored, else
// why not -- the badge it needs, a missing pack, or no valid sprites (the
// catch-all, wildAreaState()). One answer for the list and the Explore row.
void areaStatusText(uint16_t a, char *buf, size_t n) {
  if (!wildAreaUnlocked(a, pet)) { areaGateText(a, buf, n); return; }
  switch (wildAreaState(a, wildTimeBand(exploreHour()), pet)) {
    case WILD_AREA_LIVE: {
      int len = snprintf(buf, n, T(S_AREA_LV_FMT), wildAreaLvMin(a), wildAreaLvMax(a));
      // A Galar Wild Area zone says today's weather: it decides who is out.
      int8_t wx = wildAreaWeather(a, pet);
      if (wx >= 0 && len > 0 && (size_t)len < n)
        snprintf(buf + len, n - len, " %s", T((StrId)(S_WX_CLEAR + wx)));
      break;
    }
    case WILD_AREA_NEEDS_PACK: snprintf(buf, n, "%s", T(S_NEED_PACK)); break;
    default:                   snprintf(buf, n, "%s", T(S_AREA_NO_SPRITES)); break;
  }
}

void areaListTap(int16_t x, int16_t y) {
  int a = areaRowAt(x, y);
  if (a < 0) return;
  if (!wildAreaUnlocked((uint16_t)a, pet) ||
      wildAreaState((uint16_t)a, wildTimeBand(exploreHour()), pet) != WILD_AREA_LIVE) {
    sfxPlay(SFX_DENY);
    return;
  }
  exploreAreaPick[wildAreaRegion((uint16_t)a)] = (uint16_t)(a + 1);
  areaListOpen = false;   // back to Explore, with the area named on its row
  sfxPlay(SFX_TAP);
}

void renderAreaList() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  uint8_t region = currentExploreRegion();
  const char *title = REGIONS[region].name;
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - (int)strlen(title) * 9, 52);
  gfx->print(title);
  uint8_t when = wildTimeBand(exploreHour());
  uint16_t chosen = exploreAreaOf(region);
  for (int i = 0; i < AREA_ROWS; i++) {
    int k = areaPage * AREA_ROWS + i;
    if (k >= wildAreaCount(region)) break;
    uint16_t a = (uint16_t)(wildAreaFirst(region) + k);
    int rx, ry, rw, rh;
    areaRowRect(i, &rx, &ry, &rw, &rh);
    bool open_ = wildAreaUnlocked(a, pet);
    bool live = open_ && wildAreaState(a, when, pet) == WILD_AREA_LIVE;
    gfx->fillRoundRect(rx, ry, rw, rh, 12, UI_BG_DAY);
    gfx->drawRoundRect(rx, ry, rw, rh, 12, live ? UI_INK : UI_TRACK);
    if (a == chosen && live) {   // the one EXPLORE will look in
      gfx->drawRoundRect(rx + 1, ry + 1, rw - 2, rh - 2, 11, UI_BAR_OK);
      gfx->drawRoundRect(rx + 2, ry + 2, rw - 4, rh - 4, 10, UI_BAR_OK);
    }
    gfx->setTextColor(live ? UI_INK : UI_INK_SOFT);
    areaCentredName(a, open_, CX, ry + 22, rw - 16);
    char sub[40];
    areaStatusText(a, sub, sizeof(sub));
    gfx->setTextColor(live ? UI_INK : UI_INK_SOFT);
    areaCentred(sub, CX, ry + 56, rw - 16);
  }
  uiChrome();
  gfx->flush();
}

void renderExplore() {
  int h = sceneHour();
  gNight = pet.sleeping || h < 6 || h >= 20;
  drawScene(pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome, millis(), gNight);

  gfx->fillRoundRect(78, 28, 310, 54, 14, UI_BG_DAY);
  gfx->drawRoundRect(78, 28, 310, 54, 14, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - (int)strlen(T(S_EXPLORE)) * 9, 44);
  gfx->print(T(S_EXPLORE));

  uint8_t rg = currentExploreRegion();
  const char *region = REGIONS[rg].name;
  gfx->fillRoundRect(EXPLORE_REGION_X, EXPLORE_REGION_Y, EXPLORE_REGION_W,
                     EXPLORE_REGION_H, 16, UI_BG_DAY);
  gfx->drawRoundRect(EXPLORE_REGION_X, EXPLORE_REGION_Y, EXPLORE_REGION_W,
                     EXPLORE_REGION_H, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(EXPLORE_REGION_X + 38, EXPLORE_REGION_Y + 35);
  gfx->print("<");
  gfx->setCursor(EXPLORE_REGION_X + EXPLORE_REGION_W - 56, EXPLORE_REGION_Y + 35);
  gfx->print(">");
  gfx->setTextSize(strlen(region) <= 10 ? 3 : 2);
  int regionCharHalf = strlen(region) <= 10 ? 9 : 6;
  gfx->setCursor(CX - (int)strlen(region) * regionCharHalf,
                 EXPLORE_REGION_Y + (strlen(region) <= 10 ? 35 : 41));
  gfx->print(region);

  // The area row: where EXPLORE will look, and the way into the list. A region
  // without areas yet still draws from its whole pool, and says so.
  uint16_t area = exploreAreaOf(rg);
  uint8_t when = wildTimeBand(exploreHour());
  bool areaLive = area == WILD_NO_AREA || wildAreaState(area, when, pet) == WILD_AREA_LIVE;
  int ax, ay, aw, ah;
  exploreAreaRect(&ax, &ay, &aw, &ah);
  gfx->fillRoundRect(ax, ay, aw, ah, 16, UI_BG_DAY);
  gfx->drawRoundRect(ax, ay, aw, ah, 16, area == WILD_NO_AREA ? UI_TRACK : UI_INK);
  if (area == WILD_NO_AREA) {
    gfx->setTextColor(UI_INK_SOFT);
    areaCentred(T(S_AREA_WHOLE), CX, ay + 39, aw - 24);
  } else {
    gfx->setTextColor(UI_INK);
    areaCentredName(area, true, CX, ay + 22, aw - 60);
    char sub[40];
    areaStatusText(area, sub, sizeof(sub));
    gfx->setTextColor(areaLive ? UI_INK : UI_INK_SOFT);
    areaCentred(sub, CX, ay + 56, aw - 60);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(3);
    gfx->setCursor(ax + aw - 40, ay + 35);
    gfx->print(">");
  }

  // Difficulty changes how the wild creature FIGHTS (the smart AI), not how
  // strong it is.
  const char *dif = T(exploreHard ? S_HARD : S_EASY);
  int dw = (int)strlen(dif) * 12 + 48;
  if (dw < 140) dw = 140;
  gfx->fillRoundRect(CX - dw / 2, EXPLORE_DIF_Y, dw, EXPLORE_DIF_H, 12,
                     exploreHard ? UI_BAR_BAD : UI_TRACK);
  gfx->setTextColor(exploreHard ? UI_BG_DAY : UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(dif) * 6, EXPLORE_DIF_Y + 14);
  gfx->print(dif);

  bool live = !pet.isEgg() && pet.ceremony == CER_NONE && areaLive &&
              exploreRegionAllowed(rg);
  int bx, by, bw, bh;
  exploreBtnRect(&bx, &by, &bw, &bh);
  gfx->fillRoundRect(bx, by, bw, bh, 16, live ? UI_BAR_OK : UI_TRACK);
  gfx->drawRoundRect(bx, by, bw, bh, 16, live ? UI_INK : UI_INK_SOFT);
  gfx->setTextColor(live ? UI_WHITE : UI_INK_SOFT);
  gfx->setTextSize(3);
  gfx->setCursor(CX - (int)strlen(T(S_EXPLORE)) * 9, by + 22);
  gfx->print(T(S_EXPLORE));
  uiChrome();
  gfx->flush();
}

static void crumbDrop() {
  gCrumbMagic = CRUMB_MAGIC;
  gCrumbScreen = uiCurrentScreen();
  gCrumbHeap = ESP.getFreeHeap();
}

static const char *resetReasonName(int r) {
  switch (r) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_EXT:      return "reset pin";
    case ESP_RST_SW:       return "software (our own restart)";
    case ESP_RST_PANIC:    return "PANIC -- a crash";
    case ESP_RST_INT_WDT:  return "INTERRUPT WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK WATCHDOG -- something blocked too long";
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_BROWNOUT: return "BROWNOUT -- the supply sagged";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    default: return "unknown";
  }
}

// Printed once at boot. On a clean start it is one line; after a crash it says
// which screen was up and how much heap was left, which is the whole point.
void bootReport() {
  int r = (int)esp_reset_reason();
  bool bad = (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT ||
              r == ESP_RST_TASK_WDT || r == ESP_RST_WDT || r == ESP_RST_BROWNOUT);
  Serial.printf("boot: reset=%s heap=%u psram=%u\n", resetReasonName(r),
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  if (bad && gCrumbMagic == CRUMB_MAGIC) {
    Serial.printf("CRASH: it died on the '%s' screen with heap=%u\n",
                  gCrumbScreen < SCR_COUNT ? SCREEN_NAME[gCrumbScreen] : "?",
                  (unsigned)gCrumbHeap);
  } else if (bad) {
    Serial.println("CRASH: no breadcrumb (RTC memory was lost too)");
  }
  gCrumbMagic = 0;   // one report per crash, not on every later boot
}

void render() {
  crumbDrop();   // so a crash can name the screen it happened on
  if (pet.awaitingStarter()) {  // primera partida: region y luego inicial
    if (!starterRegionDone) renderRegionPick(RPICK_FOR_START);
    else renderStarterSelect();
    return;
  }
  // Modal, and above everything: a pending trade keeps the player here.
  if (tradeOpen) {
    renderTrade();
    return;
  }
  if (blackoutOpen) {
    renderBlackout();
    return;
  }
  if (galleryOpen) {
    if (galleryPick) { renderRegionPick(RPICK_FOR_DEX); return; }
    renderGallery();
    return;
  }
  if (movePickOpen) {
    renderMovePick();
    return;
  }
  if (partyOpen) {
    if (boxOpen) {
      if (boxDetail) renderBoxDetail();
      else renderBox();
    } else if (partyDetail) renderPartyDetail();
    else renderParty();
    return;
  }
  if (gameOpen) {
    renderGame();
    return;
  }
  if (bagOpen) {
    renderBag();
    return;
  }
  if (sackOpen) {
    renderSack();
    return;
  }
  if (catchOpen) {
    renderCatch();
    return;
  }
  if (miniGame) {
    renderMini();
    return;
  }
  if (spdOpen) {
    renderSpeed();
    return;
  }
  if (trainOpen) {
    renderTrain();
    return;
  }
  if (kbOpen) {
    renderKeyboard();
    return;
  }
  if (clockOpen) {
    renderClock();
    return;
  }
  if (battleOpen) {
    btlLinkPoll();
    renderBattle();
    return;
  }
  if (pickOpen) {
    renderPick();
    return;
  }
  if (lanOpen) {
    renderLan();
    return;
  }
  if (exploreOpen) {
    if (areaListOpen) renderAreaList(); else renderExplore();
    return;
  }
  if (martOpen) {
    renderMart();
    return;
  }
  if (gymOpen) {
    if (gymPick) renderRegionPick(RPICK_FOR_GYMS);
    else renderGyms();
    return;
  }
  if (playerOpen) {
    if (avatarPickOpen) renderAvatarPick();
    else renderPlayer();
    return;
  }
  if (pet.hasLearnOffer()) {
    renderLearn();
    return;
  }
  if (cardOpen) {
    renderCard();
    return;
  }
  int h = sceneHour();
  gNight = pet.sleeping || h < 6 || h >= 20;
  // drawScene cubre los 466x466 completos: sin fillScreen(NEGRO) previo para
  // que un flush DMA solapado nunca capture negro a medias (anti-parpadeo)
  drawScene(pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome, millis(), gNight);

  if (pet.ceremony) {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    const char *msg = (pet.ceremony == CER_FAREWELL) ? T(S_FAREWELL)
                      : (pet.ceremony == CER_RUNAWAY) ? T(S_RUNAWAY)
                                                      : T(S_GOODBYE);
    drawHeader(d.name, d.accent, msg);
    drawCeremony();
    gfx->flush();
    return;
  }

  if (pet.isEgg()) {
    drawHeader(T(S_EGG_HDR), inkColor(), eggMsg());
    int s = 5, x = CX - 16 * s, y = PET_CY - 16 * s;
    drawMap(SPR_EGG, SPRITE_H, x, y, s, false);
    if (pet.eggCracks() >= 1)
      for (auto &c : CRACK1) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggCracks() >= 2)
      for (auto &c : CRACK2) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggRarity() >= R_RARO) {
      const char *rar = (pet.eggRarity() == R_LEGENDARIO) ? T(S_EGG_LEGEND) : T(S_EGG_RARE);
      gfx->setTextColor(pet.eggRarity() == R_LEGENDARIO ? UI_BAR_WARN : 0x4C98);
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(rar) * 6, 316);
      gfx->print(rar);
    }
    char reg[24];
    snprintf(reg, sizeof(reg), T(S_POKEDEX_FMT), pet.registeredCount(), DEX_COUNT);
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    gfx->setTextColor(inkColor());
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(reg) * 6, 344);
    gfx->print(reg);

    // Which generation this egg comes from. It lives HERE rather than in the
    // settings screen because this is the only moment it does anything: the
    // species is decided when the egg appears, so choosing the region is
    // something you do to the egg in front of you.
    drawEggRegion();
  } else {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    char name[28];
    const char *base = pet.nick[0] ? pet.nick : d.name;
    snprintf(name, sizeof(name), T(S_NAME_FMT), pet.shiny ? "*" : "", base, pet.level());
    drawHeader(name, gNight ? UI_INK_NIGHT : d.accent, statusMsg());
    drawStreakBadge();
    drawPet();
    drawBath();
    drawPoops();
    // panel inferior: base limpia para barras y botones sobre el paisaje
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    drawBars();
    drawButtons();
    drawCelebration();
    if (pet.wantEvolveButton()) drawEvolveButton();        // CTA rojo: evolucionar
    else if (pet.canRunawayNow()) drawRunawayButton();     // CTA sombrio: escapada (abandono)
    else if (pet.wantFarewellButton()) drawFarewellButton();  // CTA dorado: despedida
  }

  if (pet.sleeping) {
    gfx->setTextColor(UI_INK_NIGHT);
    gfx->setTextSize(3);
    gfx->setCursor(320, 130);
    gfx->print("Zz");
  }

  // selector de comida
  if (feedMenuUntil) {
    if (millis() > feedMenuUntil) {
      feedMenuUntil = 0;
    } else {
      gfx->fillRoundRect(FEED_MENU_X, FEED_MENU_Y, FEED_MENU_W, FEED_MENU_H, 14, UI_WHITE);
      gfx->drawRoundRect(FEED_MENU_X, FEED_MENU_Y, FEED_MENU_W, FEED_MENU_H, 14, inkColor());
      drawMap(SPR_ICON_FOOD, 16, FEED_MENU_X + 9, FEED_MENU_Y + 8, 3, false);
      drawMap(SPR_ICON_BERRY_B, 16, FEED_MENU_X + 75, FEED_MENU_Y + 8, 3, false);
      drawMap(SPR_ICON_BERRY_G, 16, FEED_MENU_X + 141, FEED_MENU_Y + 8, 3, false);
      drawMap(SPR_ICON_CANDY, 16, FEED_MENU_X + 207, FEED_MENU_Y + 8, 3, false);
    }
  }

  // dialogo "soltar?" (pulsacion larga sobre el bicho)
  if (confirmUntil) {
    if (millis() > confirmUntil) {
      confirmUntil = 0;
    } else {
      gfx->fillRoundRect(94, 168, 278, 152, 16, UI_WHITE);
      gfx->drawRoundRect(94, 168, 278, 152, 16, UI_INK);
      char q[28];
      snprintf(q, sizeof(q), T(S_RELEASE_FMT), DEX_TBL[pet.speciesId].name);
      gfx->setTextColor(UI_INK);
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(q) * 6, 196);
      gfx->print(q);
      gfx->fillRoundRect(118, 252, 100, 52, 12, UI_BAR_OK);
      gfx->setTextColor(UI_WHITE);
      gfx->setCursor(118 + (100 - (int)strlen(T(S_YES)) * 12) / 2, 270);
      gfx->print(T(S_YES));
      gfx->fillRoundRect(248, 252, 100, 52, 12, UI_BAR_BAD);
      gfx->setCursor(248 + (100 - (int)strlen(T(S_NO)) * 12) / 2, 270);
      gfx->print(T(S_NO));
    }
  }

  // dialogo de decision (evolucionar/mantener, despedirse/quedaros)
  if (choiceKind) {
    if (millis() > choiceUntil) choiceKind = 0;
    else drawChoiceDialog();
  }

  // "<name> joined the party!" after a farewell or release
  if (partyBannerUntil) {
    if (millis() > partyBannerUntil) {
      partyBannerUntil = 0;
    } else {
      char b[40];
      snprintf(b, sizeof(b), T(S_PARTY_JOINED), partyBannerName);
      gfx->fillRoundRect(53, 176, 360, 74, 16, UI_BAR_OK);
      gfx->drawRoundRect(53, 176, 360, 74, 16, UI_INK);
      gfx->setTextColor(UI_WHITE);
      gfx->setTextSize(2);
      gfx->setCursor(CX - (int)strlen(b) * 6, 206);
      gfx->print(b);
    }
  }

  uiChrome();
  if (menuOpen) drawMenu();

  gfx->flush();
}

// ---------- minijuego: toques con la pokeball ----------

void startGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  gameOpen = true;
  gameOverUntil = 0;
  gameUntil = millis() + GAME_MS;
  gameScore = 0;
  gameMisses = 0;
  gameNewHi = false;
  hitTime = 0;
  gamePetX = 233;
  respawnBall();
}

void respawnBall() {
  ballX = 150 + random(166);
  ballY = 96;
  float sp = 1.6f + gameScore * 0.05f;  // mas viva segun avanzas
  if (sp > 4.0f) sp = 4.0f;
  ballVX = random(2) ? sp : -sp;
  ballVY = 0;
}

// Leaving a minigame early banks what was actually earned rather than voiding
// it. Quitting used to forfeit everything, which mattered little when the ball
// game trained a stat you could grind back -- but it is now purely about
// happiness, and a pet that just played should be happier for it. The
// gameOver/over guards stop a swipe during the results screen paying twice.
void leaveGame() {
  if (!gameOverUntil) pet.playResult(gameScore);
  gameOpen = false;
}
void leaveSack() {
  if (!sackOverUntil) pet.trainStrength(sackHits);
  sackOpen = false;
}
void leaveSpeed() {
  if (!spdOverUntil) pet.trainSpeed(spdHits);
  spdOpen = false;
}

void gameTap(int16_t x, int16_t y) {
  if (gameOverUntil) return;
  // The ball is checked BEFORE the quit strip. The ball bounces inside a circle
  // of radius 205 about the centre, so it reaches y=28 -- well inside the y<72
  // header. Reaching up to hit a high ball used to abandon the game instead,
  // silently forfeiting the score, the speed training and the record.
  float dx = ballX - x, dy = ballY - y;
  bool onBall = (dx * dx + dy * dy < 74 * 74);
  if (!onBall && y < 72) {  // tocar la cabecera = salir, conservando lo ganado
    leaveGame();
    return;
  }
  if (onBall) {  // toque a la bola!
    gameScore++;
    sfxPlay(SFX_PLAY);
    // golpe mas suave: impulso moderado que crece poco a poco con la puntuacion
    float lift = 6.6f + (gameScore > 16 ? 3.5f : gameScore * 0.22f);
    ballVY = -lift;
    ballVX += dx * 0.12f;
    if (ballVX > 6.5f) ballVX = 6.5f;
    if (ballVX < -6.5f) ballVX = -6.5f;
    hitX = ballX;
    hitY = ballY;
    hitTime = millis();
  }
}

void stepGame() {
  float grav = 0.40f + gameScore * 0.013f;  // cae un poco mas rapido cada vez
  if (grav > 0.80f) grav = 0.80f;
  ballVY += grav;
  ballX += ballVX;
  ballY += ballVY;
  // rebote en la pared circular
  float dx = ballX - CX, dy = ballY - CY;
  float d = sqrtf(dx * dx + dy * dy);
  if (d > 205) {
    float nx = dx / d, ny = dy / d;
    float dot = ballVX * nx + ballVY * ny;
    if (dot > 0) {
      ballVX = (ballVX - 2 * dot * nx) * 0.85f;
      ballVY = (ballVY - 2 * dot * ny) * 0.85f;
    }
    ballX = CX + nx * 205;
    ballY = CY + ny * 205;
  }
  // the clock, or three misses, whichever lands first
  if (gameUntil && millis() >= gameUntil && !gameOverUntil) {
    gameNewHi = (gameScore > pet.gameHi);
    pet.playResult(gameScore);
    sfxPlay(gameNewHi && gameScore > 0 ? SFX_MEDAL : SFX_LEVEL);
    gameOverUntil = millis() + 4000;
    return;
  }
  if (ballY > 384) {  // al suelo
    if (++gameMisses >= 3) {
      gameNewHi = (gameScore > pet.gameHi);
      pet.playResult(gameScore);  // actualiza el record y da felicidad
      sfxPlay(gameNewHi && gameScore > 0 ? SFX_MEDAL : SFX_LEVEL);
      gameOverUntil = millis() + 4000;
    } else {
      respawnBall();
    }
  }
  // el bicho la sigue por abajo
  float chase = (ballX - gamePetX) * 0.12f;
  if (chase > 7) chase = 7;
  if (chase < -7) chase = -7;
  gamePetX += chase;
}

// ---------- saco de entrenamiento (entrena la fuerza) ----------

void startSack() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  sackOpen = true;
  sackUntil = millis() + 10000;
  sackOverUntil = 0;
  sackHits = 0;
  sackShake = 0;
  sackNewHi = false;
}

void sackTap() {
  if (millis() >= sackUntil) return;  // ya termino el tiempo
  sackHits++;
  sackShake = 16;  // sacude el saco
}

void drawGameScene();  // prototipo (definida mas abajo)

void renderSack() {
  uint32_t now = millis();
  drawGameScene();  // fondo del habitat
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  // pantalla de resultado
  if (sackOverUntil) {
    if (now > sackOverUntil) { sackOpen = false; return; }
    char b[20];
    snprintf(b, sizeof(b), T(S_HITS_FMT), sackHits);
    gfx->setTextColor(ink);
    gfx->setTextSize(4);
    gfx->setCursor(CX - strlen(b) * 12, 150);
    gfx->print(b);
    char g[32];
    if (!sackGain && pet.trAtk >= pet.trMaxAtk())       // +0 at the ceiling says why
      snprintf(g, sizeof(g), "%s %s", T(S_TR_ATK), T(S_ST_MAX));
    else snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), sackGain);
    gfx->setTextColor(UI_BAR_BAD);
    gfx->setTextSize(3);
    gfx->setCursor(CX - strlen(g) * 9, 210);
    gfx->print(g);
    gfx->setTextSize(2);
    if (sackNewHi && sackHits > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      gfx->setCursor(CX - strlen(T(S_NEW_RECORD)) * 6, 256);
      gfx->print(T(S_NEW_RECORD));
    } else {
      char r[18];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.strHi);
      gfx->setTextColor(ink);
      gfx->setCursor(CX - strlen(r) * 6, 256);
      gfx->print(r);
    }
    gfx->flush();
    return;
  }

  // se acabaron los 10 s: aplicar entrenamiento
  if (now >= sackUntil) {
    sackNewHi = (sackHits > pet.strHi);
    sackGain = pet.trainStrength(sackHits);
    sfxPlay(sackNewHi ? SFX_MEDAL : SFX_PLAY);
    sackOverUntil = now + 3500;
    gfx->flush();
    return;
  }

  // aporreo activo
  sackShake *= 0.84f;
  int off = (int)(sackShake * sinf(now * 0.05f));
  int sx = CX + off, top = 86, sy = 150;
  gfx->fillRect(CX - 3, 56, 6, top - 56, ink);          // gancho/cuerda
  gfx->fillRect(sx - 4, top - 30, 8, 34, ink);          // cadena
  gfx->fillRoundRect(sx - 42, top, 84, 150, 26, C565(0xb5, 0x3a, 0x3a));  // saco
  gfx->fillRoundRect(sx - 42, top, 84, 22, 18, C565(0x7e, 0x28, 0x28));   // tapa
  gfx->drawRoundRect(sx - 42, top, 84, 150, 26, ink);
  gfx->fillRect(sx - 42, top + 70, 84, 4, C565(0x7e, 0x28, 0x28));        // costura

  // contador de golpes
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", sackHits);
  gfx->setTextColor(ink);
  gfx->setTextSize(6);
  gfx->setCursor(CX - strlen(buf) * 18, 268);
  gfx->print(buf);

  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_HIT_FAST)) * 6, 322);
  gfx->print(T(S_HIT_FAST));

  // barra de tiempo
  uint32_t left = sackUntil - now;
  int bw = 280, fw = (int)((uint32_t)bw * left / 10000);
  gfx->fillRoundRect(CX - bw / 2, 350, bw, 16, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 350, fw, 16, 5, UI_BAR_OK);

  gfx->flush();
}

// fondo del minijuego: hatibat del bicho (cielo por hora + suelo del bioma)
void drawGameScene() {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)       { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8) { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18){ top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else             { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  int hor = 376;
  for (int y = 0; y < hor; y += 8)
    gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, hor));
  if (night)
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < 6 ? bio : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRect(0, hor, 466, 466 - hor, soil);
}

void renderGame() {
  // sin fillScreen(NEGRO): drawGameScene cubre los 466x466 completos. Si el
  // DMA del flush anterior aun lee el buffer, vera contenido valido (no negro
  // a medio pintar), que era el parpadeo a 25 fps.
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  if (gameOverUntil) {
    drawGameScene();
    if (millis() > gameOverUntil) {
      gameOpen = false;
      return;
    }
    char buf[22];
    snprintf(buf, sizeof(buf), T(S_SCORE_FMT), gameScore);
    gfx->setTextColor(ink);
    gfx->setTextSize(4);
    gfx->setCursor(CX - strlen(buf) * 12, 160);
    gfx->print(buf);
    gfx->setTextSize(2);
    if (gameNewHi && gameScore > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      gfx->setCursor(CX - strlen(T(S_NEW_RECORD)) * 6, 214);
      gfx->print(T(S_NEW_RECORD));
    } else {
      char rec[20];
      snprintf(rec, sizeof(rec), T(S_RECORD_FMT), pet.gameHi);
      gfx->setTextColor(ink);
      gfx->setCursor(CX - strlen(rec) * 6, 214);
      gfx->print(rec);
    }
    const char *msg = T(S_PLUS_JOY);   // the owner's call: no "So much fun!" cheer
    gfx->setTextColor(ink);
    gfx->setCursor(CX - strlen(msg) * 6, 250);
    gfx->print(msg);
    gfx->flush();
    return;
  }

  drawGameScene();
  stepGame();

  // marcador, record y vidas
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", gameScore);
  gfx->setTextColor(ink);
  gfx->setTextSize(4);
  gfx->setCursor(CX - strlen(buf) * 12, 30);
  gfx->print(buf);
  char rec[12];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.gameHi);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(rec) * 6, 76);
  gfx->print(rec);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - gameMisses) gfx->fillCircle(180 + i * 28, 104, 6, UI_BAR_BAD);
    else gfx->drawCircle(180 + i * 28, 104, 6, UI_TRACK);
  }
  // The clock, drawn like the bag's and the reaction test's so all three games
  // read the same way. Thin and near the rim: the middle belongs to the ball.
  if (!gameOverUntil) {
    uint32_t now2 = millis();
    uint32_t left = (gameUntil > now2) ? gameUntil - now2 : 0;
    int bw = 200, fw = (int)((uint32_t)bw * left / GAME_MS);
    gfx->fillRoundRect(CX - bw / 2, 124, bw, 10, 4, UI_TRACK);
    if (fw > 2)
      gfx->fillRoundRect(CX - bw / 2, 124, fw, 10, 4,
                         left < 5000 ? UI_BAR_WARN : UI_BAR_OK);
  }

  if (pmd.loaded) {
    uint8_t act = (ballX > gamePetX + 4) ? PMD_WALKR : (ballX < gamePetX - 4) ? PMD_WALKL : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdAct(act, (int)gamePetX, 394, millis(), true, false, 3);
  } else if (mon.loaded) {
    int s = (mon.h * 2 > 130) ? 1 : 2;
    int w = mon.w * s, h = mon.h * s;
    uint16_t fm = mon.frameMs ? mon.frameMs : 100;
    uint16_t fi = (millis() / fm) % mon.frames;
    const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
    int px = (int)gamePetX - w / 2, py = 394 - h;
    for (int r = 0; r < mon.h; r++)
      for (int c = 0; c < mon.w; c++) {
        uint8_t idx = fr[r * mon.w + c];
        if (idx == 0xFF) continue;
        gfx->fillRect(px + c * s, py + r * s, s, s, mon.pal[idx]);
      }
  }

  // anillo de impacto que se expande y desvanece (feedback suave del golpe)
  uint32_t ht = millis() - hitTime;
  if (hitTime && ht < 260) {
    int rad = 22 + (int)(ht / 6);
    gfx->drawCircle((int)hitX, (int)hitY, rad, C565(0xff, 0xe7, 0x9f));
    gfx->drawCircle((int)hitX, (int)hitY, rad - 2, C565(0xff, 0xd9, 0x8a));
  }

  // la pokeball
  drawMap(SPR_ICON_PLAY, 16, (int)ballX - 24, (int)ballY - 24, 3, false);

  gfx->flush();
}

// ---------- ficha del bicho (deslizar vertical) ----------

// una fila de la ficha: etiqueta, barra, valor y (si iv != IV_NONE) el valor
// individual que fija el techo de ese stat
// (sin argumento por defecto: el generador de prototipos de Arduino los
// descarta y las llamadas que lo omitan no compilarian)
#define IV_NONE 0xFF
void drawCardStat(int y, const char *label, uint16_t val, uint16_t maxBar,
                  uint16_t color, uint8_t iv) {
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(70, y);
  gfx->print(label);
  // The bar used to start at 112, which leaves 42px for a label drawn at size 2
  // -- three characters. BOND (EN), LIEN (FR) and LACO (PT) are four, so the
  // label ran under the bar. 132 fits five, with the bar narrowed to keep the
  // number clear of it.
  int bw = 130;
  int fw = (int)val * bw / maxBar;
  if (fw > bw) fw = bw;
  gfx->fillRoundRect(132, y + 2, bw, 11, 3, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(132, y + 2, fw, 11, 3, color);
  char num[8];
  snprintf(num, sizeof(num), "%u", val);
  gfx->setCursor(272, y);
  gfx->print(num);
  if (iv != IV_NONE) {
    char b[10];
    snprintf(b, sizeof(b), T(S_IV_FMT), iv);
    // un IV perfecto se resalta: es el golpe de suerte que el jugador busca
    gfx->setTextColor(iv >= 31 ? UI_BAR_WARN : UI_TRACK);
    gfx->setCursor(344, y);
    gfx->print(b);
  }
}

// ---------- ajuste de hora en pantalla (deslizar abajo) ----------
// El usuario pone su hora LOCAL a ojo; el firmware la usa tal cual, asi que
// no hay que gestionar zona horaria. Preserva el dia (no rompe racha/edad).

void openClock() {
  uint32_t e = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  clockH = (e / 3600) % 24;
  clockM = (e / 60) % 60;
  clockOpen = true;
}

void applyClock() {
  uint32_t base = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  uint32_t e = (base / 86400) * 86400 + (uint32_t)clockH * 3600 + (uint32_t)clockM * 60;
  rtcSetEpoch(e);
  pet.setClock(e);
  clockOpen = false;
}

void drawClockBtn(int x, int y, const char *l) {
  gfx->fillRoundRect(x, y, 58, 58, 12, UI_WHITE);
  gfx->drawRoundRect(x, y, 58, 58, 12, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(4);
  gfx->setCursor(x + 17, y + 15);
  gfx->print(l);
}

// pildoras de idioma centradas en y; rellena la activa
#define LANG_PILL_Y 296
#define LANG_PILL_H 30
#define LANG_PILL_X 336          // pildora de idioma (cicla los 6 al tocar)
#define LANG_PILL_W 96
// the volume mixer sits in the gap between the sound switch and the language
// pill: minus, the level, plus
#define VOL_MINUS_X 146
#define VOL_PLUS_X 276
#define VOL_BTN_W 48
static const char *const LANG_CODES[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT" };

void renderClock() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_SET_TIME)) * 9, 44);
  gfx->print(T(S_SET_TIME));

  char t[8];
  snprintf(t, sizeof(t), "%02d:%02d", clockH, clockM);
  gfx->setTextSize(7);
  gfx->setCursor(CX - 105, 108);
  gfx->print(t);

  drawClockBtn(104, 190, "-");  // hora -
  drawClockBtn(170, 190, "+");  // hora +
  drawClockBtn(252, 190, "-");  // min -
  drawClockBtn(318, 190, "+");  // min +
  gfx->setTextSize(2);
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setCursor(120, 256);
  gfx->print(T(S_HOUR));
  gfx->setCursor(276, 256);
  gfx->print(T(S_MIN));

  // interruptor de sonido (izquierda de la fila de idioma)
  bool snd = audioEnabled();
  const char *sl = snd ? T(S_SND_ON) : T(S_SND_OFF);
  gfx->fillRoundRect(34, LANG_PILL_Y, 96, LANG_PILL_H, 8, snd ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(34, LANG_PILL_Y, 96, LANG_PILL_H, 8, UI_INK);
  gfx->setTextColor(snd ? UI_BG_DAY : UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(34 + (96 - (int)strlen(sl) * 12) / 2, LANG_PILL_Y + 8);
  gfx->print(sl);

  // volume: a level, not a toggle. The sound switch beside it is still the
  // master -- this is how loud it is when it is on, and 0 is silent.
  {
    uint8_t v = audioVolume();
    for (int i = 0; i < 2; i++) {
      int bx = i ? VOL_PLUS_X : VOL_MINUS_X;
      bool live = i ? (v < 10) : (v > 0);
      gfx->fillRoundRect(bx, LANG_PILL_Y, VOL_BTN_W, LANG_PILL_H, 8,
                         live ? UI_WHITE : UI_TRACK);
      gfx->drawRoundRect(bx, LANG_PILL_Y, VOL_BTN_W, LANG_PILL_H, 8, UI_INK);
      gfx->setTextColor(live ? UI_INK : 0x8410);
      gfx->setTextSize(2);
      gfx->setCursor(bx + VOL_BTN_W / 2 - 6, LANG_PILL_Y + 8);
      gfx->print(i ? "+" : "-");
    }
    char vl[12];
    snprintf(vl, sizeof(vl), T(S_VOL_FMT), v);
    gfx->setTextColor(v ? UI_INK : UI_TRACK);
    gfx->setTextSize(1);
    gfx->setCursor(210 + (56 - (int)strlen(vl) * 6) / 2, LANG_PILL_Y + 4);
    gfx->print(vl);
    // a small bar under the number, so the level reads at a glance
    gfx->fillRoundRect(210, LANG_PILL_Y + 18, 56, 8, 3, UI_TRACK);
    if (v) gfx->fillRoundRect(210, LANG_PILL_Y + 18, 56 * v / 10, 8, 3, UI_BAR_OK);
  }

  // selector de idioma: una pildora que cicla los 6 idiomas al tocar
  gfx->fillRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_WHITE);
  gfx->drawRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_INK);
  char lp[10];
  snprintf(lp, sizeof(lp), "%s >", LANG_CODES[gLang]);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(LANG_PILL_X + (LANG_PILL_W - (int)strlen(lp) * 12) / 2, LANG_PILL_Y + 8);
  gfx->print(lp);

  gfx->fillRoundRect(133, 340, 200, 48, 14, UI_BAR_OK);
  gfx->setTextColor(UI_BG_DAY);
  gfx->setTextSize(3);
  gfx->setCursor(CX - 18, 352);
  gfx->print("OK");

  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_CLOCK_CANCEL)) * 6, 410);
  gfx->print(T(S_CLOCK_CANCEL));

  // version del firmware (discreta, abajo del todo)
  char ver[20];
  snprintf(ver, sizeof(ver), "TamaPoke v%s", FW_VERSION);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(ver) * 3, 436);
  gfx->print(ver);
  gfx->flush();
}

void clockTap(int16_t x, int16_t y) {
  if (y >= 190 && y <= 248) {  // fila de botones +/-
    if (x >= 104 && x < 162) clockH = (clockH + 23) % 24;
    else if (x >= 170 && x < 228) clockH = (clockH + 1) % 24;
    else if (x >= 252 && x < 310) clockM = (clockM + 59) % 60;
    else if (x >= 318 && x < 376) clockM = (clockM + 1) % 60;
    return;
  }
  if (y >= LANG_PILL_Y && y <= LANG_PILL_Y + LANG_PILL_H) {
    if (x >= 34 && x < 130) {                  // interruptor de sonido
      audioSetEnabled(!audioEnabled());
      if (audioEnabled()) sfxPlay(SFX_TAP);    // confirma al encender
      return;
    }
    if (x >= VOL_MINUS_X && x < VOL_MINUS_X + VOL_BTN_W) {
      if (audioVolume() > 0) audioSetVolume(audioVolume() - 1);
      sfxPlay(SFX_TAP);                        // so the new level is audible
      return;
    }
    if (x >= VOL_PLUS_X && x < VOL_PLUS_X + VOL_BTN_W) {
      if (audioVolume() < 10) audioSetVolume(audioVolume() + 1);
      sfxPlay(SFX_TAP);
      return;
    }
    if (x >= LANG_PILL_X && x < LANG_PILL_X + LANG_PILL_W) {  // cicla idioma
      setLang((Lang)((gLang + 1) % LANG_COUNT));
      sfxPlay(SFX_TAP);
      return;
    }
  }
  if (y >= 340 && y <= 388 && x >= 133 && x <= 333) { applyClock(); return; }
}

// llama + numero de racha arriba a la izquierda
void drawStreakBadge() {
  if (pet.streak < 1) return;
  int x = 26, y = 16;
  gfx->fillTriangle(x + 8, y, x + 1, y + 17, x + 15, y + 17, UI_BAR_BAD);
  gfx->fillTriangle(x + 8, y + 7, x + 4, y + 17, x + 12, y + 17, UI_BAR_WARN);
  char s[6];
  snprintf(s, sizeof(s), "%u", pet.streak);
  gfx->setTextColor(inkColor());
  gfx->setTextSize(2);
  gfx->setCursor(x + 22, y + 2);
  gfx->print(s);
}

// banner temporal: medalla nueva o hito de racha
void drawCelebration() {
  const char *l1 = nullptr, *l2 = nullptr;
  char buf[20];
  if (pet.showMedal()) {
    for (int i = 0; i < MED_COUNT; i++)
      if (pet.newMedal & (1 << i)) { l2 = medalName(i); break; }
    l1 = T(S_MEDAL_BANNER);
  } else if (pet.showMilestone()) {
    snprintf(buf, sizeof(buf), T(S_STREAK_DAYS_FMT), pet.streak);
    l1 = T(S_GREAT);
    l2 = buf;
  }
  if (!l1) return;
  gfx->fillRoundRect(73, 150, 320, 96, 16, UI_BAR_WARN);
  gfx->drawRoundRect(73, 150, 320, 96, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(l1) * 9, 176);
  gfx->print(l1);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(l2) * 6, 212);
  gfx->print(l2);
}

// medallas en la ficha: badge con etiqueta, color si conseguida
void drawMedalBadge(int x, int y, int i) {
  bool got = pet.hasMedal(1 << i);
  gfx->fillRoundRect(x, y, 100, 24, 6, got ? UI_BAR_OK : UI_TRACK);
  if (!got) gfx->drawRoundRect(x, y, 100, 24, 6, UI_TRACK);
  gfx->setTextColor(got ? UI_BG_DAY : 0x9492);
  gfx->setTextSize(2);
  gfx->setCursor(x + (100 - (int)strlen(medalLabel(i)) * 12) / 2, y + 5);
  gfx->print(medalLabel(i));
}

// pagina 0: perfil (retrato grande, identidad, racha, vinculo, baya)
void renderCardProfile() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *nm = pet.nick[0] ? pet.nick : d.name;
  char head[26];
  snprintf(head, sizeof(head), T(S_NAME_FMT), pet.shiny ? "*" : "", nm, pet.level());
  gfx->setTextColor(d.accent);
  // auto-encoge: a tamano 3 los nombres largos no caben en la franja estrecha de
  // arriba de la pantalla redonda, asi que se cortaban por el borde
  int hlen = strlen(head);
  int hts = (hlen <= 11) ? 3 : 2;
  gfx->setTextSize(hts);
  gfx->setCursor(CX - hlen * (hts == 3 ? 9 : 6), hts == 3 ? 34 : 40);
  gfx->print(head);
  if (pet.nick[0]) {  // especie real bajo el apodo
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (strlen(d.name) + 2) * 6, 64);
    gfx->printf("(%s)", d.name);
  }

  // retrato grande animado
  if (pmd.loaded) drawPmdAct(PMD_IDLE, CX, 206, millis(), true, false, 4);

  // racha con llama
  int sx = 138, sy = 224;
  gfx->fillTriangle(sx + 8, sy, sx + 1, sy + 18, sx + 15, sy + 18, UI_BAR_BAD);
  gfx->fillTriangle(sx + 8, sy + 7, sx + 4, sy + 18, sx + 12, sy + 18, UI_BAR_WARN);
  char rl[30];
  snprintf(rl, sizeof(rl), T(S_STREAK_FMT), pet.streak, pet.bestStreak);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(sx + 24, sy + 2);
  gfx->print(rl);

  drawCardStat(258, T(S_VIN), pet.bond, 100, C565(0xd4, 0x52, 0x7e), IV_NONE);

  const char *berry = !pet.berryKnown ? T(S_BERRY_UNK)
                      : pet.lovesBerry(0) ? T(S_BERRY_RED)
                      : pet.lovesBerry(1) ? T(S_BERRY_BLUE)
                                          : T(S_BERRY_GREEN);
  char info[40];
  snprintf(info, sizeof(info), T(S_INFO_FMT), berry,
           (unsigned long)(pet.ageMinutes / 1440));
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(info) * 6, 296);
  gfx->print(info);

  gfx->setTextColor(UI_INK_SOFT);
  gfx->setCursor(CX - strlen(T(S_RENAME_HINT)) * 6, 332);
  gfx->print(T(S_RENAME_HINT));
}

// pagina 1: combate (4 barras + boton de entrenar)
void renderCardStats() {
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_BATTLE)) * 9, 44);
  gfx->print(T(S_BATTLE));

  // typing, in the accent colour of the species (English in every language,
  // same as the species names themselves)
  const DexEntry &de = DEX_TBL[pet.speciesId];
  char ty[24];
  if (de.type2 == T_NONE) snprintf(ty, sizeof(ty), "%s", typeName(de.type1));
  else snprintf(ty, sizeof(ty), "%s/%s", typeName(de.type1), typeName(de.type2));
  gfx->setTextColor(de.accent);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(ty) * 6, 76);
  gfx->print(ty);

  // 360 de tope de barra: a nivel 73 (fin de ciclo) el stat mas alto de toda
  // la dex es la vitalidad de CHANSEY (355). El 260 anterior ya se desbordaba.
  drawCardStat(104, T(S_STAT_ATK), pet.atkStat(), 360, UI_BAR_BAD, pet.ivAtk);
  drawCardStat(144, T(S_STAT_DEF), pet.defStat(), 360, 0x4C98, pet.ivDef);
  drawCardStat(184, T(S_STAT_SPE), pet.speStat(), 360, UI_BAR_WARN, pet.ivSpe);
  drawCardStat(224, T(S_STAT_VIT), pet.vitStat(), 360, UI_BAR_OK, pet.ivHp);
  // Fitness is the weight meter read the other way up, so a fuller bar is the
  // healthier one; the kg beside it is the species' own Pokedex weight scaled
  // by that meter (Pet::weightHg()). It sits where the other rows show an IV.
  drawCardStat(264, T(S_STAT_FIT), pet.fitness(), 100, 0xB3C8, IV_NONE);
  {
    uint32_t hg = pet.weightHg();
    char kg[16];
    snprintf(kg, sizeof(kg), "%lu.%lukg", (unsigned long)(hg / 10), (unsigned long)(hg % 10));
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(2);
    gfx->setCursor(320, 264);
    gfx->print(kg);
  }

}

// Draws one move as a row: name, its type in the type's own colour, and either
// power or a STATUS marker. Shared by the moves page and the picker so a move
// looks the same wherever you meet it.
// A filled chip in the type's own colour, label in whichever of black/white
// reads on it. Returns its width so a caller can lay out beside it.
int drawTypeChip(int x, int y, uint8_t type) {
  const char *nm = typeName(type);
  int w = (int)strlen(nm) * 6 + 10;
  gfx->fillRoundRect(x, y, w, 15, 4, typeColor(type));
  gfx->setTextSize(1);
  gfx->setTextColor(typeColorIsLight(type) ? UI_INK : UI_WHITE);
  gfx->setCursor(x + 5, y + 4);
  gfx->print(nm);
  return w;
}

void drawMoveRow(int y, uint8_t mv, bool highlight, int16_t dex) {
  gfx->fillRoundRect(70, y, 326, 50, 12, highlight ? UI_BAR_WARN : UI_BG_DAY);
  gfx->drawRoundRect(70, y, 326, 50, 12, UI_INK);
  if (!mv) {
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(T(S_MOVE_EMPTY)) * 6, y + 17);
    gfx->print(T(S_MOVE_EMPTY));
    return;
  }
  const MoveEntry &m = MOVE_TBL[mv];
  const char *name = moveName(mv);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(82, y + 8);
  gfx->print(name);
  // There is no per-type palette (DexEntry.accent is per species), and inventing
  // one by hand would duplicate what gen_dex.py generates. Colouring same-type
  // moves in the species accent is more useful anyway: STAB is a 1.5x damage
  // bonus, so this marks the moves that actually hit hardest for this creature.
  // The chip carries the TYPE; STAB moved onto the power figure, where it
  // belongs -- STAB is a damage bonus, so saying it next to the damage reads
  // straight, and it leaves the type free to be its own colour.
  bool stab = hasStab(dex, m.type) && m.cat != MC_STATUS;
  int cw = drawTypeChip(82, y + 29, m.type);
  if (stab) {
    gfx->setTextColor(DEX_TBL[dex].accent);
    gfx->setTextSize(1);
    gfx->setCursor(82 + cw + 6, y + 33);
    gfx->print("STAB");
  }
  char pw[16];
  if (m.cat == MC_STATUS) snprintf(pw, sizeof(pw), "%s", T(S_MOVE_STATUS));
  else snprintf(pw, sizeof(pw), T(S_MOVE_PWR), m.power);
  gfx->setTextColor(stab ? DEX_TBL[dex].accent : UI_INK);
  gfx->setTextSize(1);
  gfx->setCursor(384 - (int)strlen(pw) * 6, y + 33);
  gfx->print(pw);
}

// card page 4: the four known moves. Tapping a slot opens the picker.
void renderCardMoves() {
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_MOVES)) * 9, 44);
  gfx->print(T(S_MOVES));
  for (int i = 0; i < MOVE_SLOTS; i++) drawMoveRow(MOVE_ROW_Y(i), pet.moves[i], false, pet.speciesId);
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(T(S_MOVE_TAP)) * 3, 340);
  gfx->print(T(S_MOVE_TAP));
}

// Every move the species can learn by this level, so a slot can be swapped for
// anything legal -- not just the handful a level-up would have offered.
uint8_t learnableFor(int16_t dex, uint8_t lvl, uint8_t *out, uint8_t max) {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  uint8_t n = learnCount(dex), w = 0;
  for (uint8_t i = 0; i < n && w < max; i++) {
    // moveUnlockLevel(), NOT learnLevel(): a TM is stored as level 0 and would
    // otherwise clear this check at level 1. That is how a level 22 Charmeleon
    // came to be offered FIRE BLAST -- the same class of bug as the level 1
    // Squirtle with SURF, in the one path that fix did not reach.
    if (moveUnlockLevel(dex, i) > lvl) continue;
    uint8_t mv = learnMove(dex, i);
    if (!mv || mv >= MOVE_COUNT) continue;
    bool dup = false;
    for (uint8_t j = 0; j < w; j++)
      if (out[j] == mv) { dup = true; break; }
    if (!dup) out[w++] = mv;
  }
  return w;
}

// The picker targets either the live pet or a banked member. A banked one keeps
// its frozen level, so it can only relearn what it could have known back then.
uint8_t learnableList(uint8_t *out, uint8_t max) {
  if (movePickParty) {
    const PartyMon &m = party.slots[movePickParty - 1];
    return learnableFor(m.dex, (uint8_t)m.level, out, max);
  }
  return pet.isEgg() ? 0 : learnableFor(pet.speciesId, pet.level(), out, max);
}

uint8_t *pickTargetMoves() {
  return movePickParty ? party.slots[movePickParty - 1].moves : pet.moves;
}
int16_t pickTargetDex() {
  return movePickParty ? party.slots[movePickParty - 1].dex : pet.speciesId;
}

void renderMovePick() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_MOVE_PICK)) * 6, 40);
  gfx->print(T(S_MOVE_PICK));

  uint8_t all[64];
  uint8_t n = learnableList(all, sizeof(all));
  uint8_t pages = n ? (n + MOVE_PICK_PER_PAGE - 1) / MOVE_PICK_PER_PAGE : 1;
  if (movePickPage >= pages) movePickPage = 0;
  for (uint8_t i = 0; i < MOVE_PICK_PER_PAGE; i++) {
    uint8_t idx = movePickPage * MOVE_PICK_PER_PAGE + i;
    if (idx >= n) break;
    // the move already in this slot is highlighted, so replacing like for like
    // is obvious rather than a guess
    drawMoveRow(MOVE_PICK_Y(i), all[idx], all[idx] == pickTargetMoves()[movePickSlot], pickTargetDex());
  }
  for (uint8_t i = 0; i < pages && pages > 1; i++) {
    if (i == movePickPage) gfx->fillCircle(CX - (pages - 1) * 13 + i * 26, 380, 5, UI_INK);
    else gfx->drawCircle(CX - (pages - 1) * 13 + i * 26, 380, 4, UI_INK);
  }
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_BACK)) * 6, 402);
  gfx->print(T(S_BACK));
  gfx->flush();
}

// ---------- battle ----------

// Draws a battle backdrop scaled 2x. Emitted as runs of identical indices
// rather than a write per pixel: this is flat pixel art with long horizontal
// runs, and 240x112 at 2x would otherwise be 26,880 fillRect calls a frame.
// The round bezel crops the overhang physically, so nothing is clipped here.
static void drawBack(const BackScene &b, int y0) {
  const int SC = 2;
  int x0 = CX - (b.w * SC) / 2;
  for (int r = 0; r < b.h; r++) {
    const uint8_t *row = b.idx + (uint32_t)r * b.w;
    int c = 0;
    while (c < b.w) {
      uint8_t v = row[c];
      int run = 1;
      while (c + run < b.w && row[c + run] == v) run++;
      uint16_t col = b.pal[v];
      gfx->fillRect(x0 + c * SC, y0 + r * SC, run * SC, SC, col);
      c += run;
    }
  }
}

// Which scene: the FOE's biome, since a battle happens where it lives, and the
// same day/night split the main screen already uses.
static void drawBattleBack() {
  int16_t dex = btlFoe.dex;
  if (dex < 1 || dex > DEX_COUNT) { gfx->fillCircle(CX, CY, 231, UI_BG_DAY); return; }
  uint8_t bi = DEX_TBL[dex].biome;
  if (bi >= BACK_BIOMES) bi = 0;
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  drawBack(BACKS[bi][night ? 1 : 0], 30);
}

// Streams a side's sprite if it is not already the one loaded. Called whenever
// a creature steps in, never per frame.
static void btlSyncSprite(uint8_t who, const Combatant &c) {
  int16_t key = c.dex * (c.shiny ? -1 : 1);
  if (btlPmdDex[who] == key && btlPmd[who].loaded) return;
  btlPmd[who].unload();
  btlPmdDex[who] = 0;
  if (c.dex < 1 || c.dex > DEX_COUNT) return;
  if (btlPmd[who].load(c.dex, c.shiny)) btlPmdDex[who] = key;   // NOT (uint8_t): Hoenn runs past 255
}

static void btlFreeSprites() {
  for (int i = 0; i < 2; i++) { btlPmd[i].unload(); btlPmdDex[i] = 0; }
}

static void btlSay(const char *fmt, ...) {
  if (btlMsgCount >= 6) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(btlMsg[btlMsgCount], sizeof(btlMsg[0]), fmt, ap);
  va_end(ap);
  btlMsgCount++;
}

// Turns a TurnLog into narration. Everything here was already decided by the
// engine -- nothing is recomputed, so the text can never disagree with the maths.
// Picks the cue for an action from the TurnLog, so the sound can never
// disagree with what actually happened.
static void btlSfxFor(const TurnLog &lg) {
  if (lg.targetFainted) { sfxPlay(SFX_FAINT); return; }
  if (lg.inflicted) { sfxPlay(SFX_STATUS); return; }
  if (lg.damage && lg.effPct > 100) { sfxPlay(SFX_SUPER); return; }
  if (lg.damage) {
    sfxPlay(lg.move && MOVE_TBL[lg.move].cat == MC_SPEC ? SFX_BEAM : SFX_HIT);
    return;
  }
  if (lg.move && MOVE_TBL[lg.move].cat == MC_STATUS && !lg.missed) sfxPlay(SFX_STATUS);
}

static void btlNarrate(const Combatant &actor, const Combatant &target, const TurnLog &lg) {
  if (lg.skipped) return;
  btlSfxFor(lg);
  if (lg.hurtSelf) { btlSay(T(S_BTL_HURTSELF)); return; }
  if (lg.charged) { btlSay(T(S_BTL_USED), actor.name, moveName(lg.move)); return; }
  if (lg.move) btlSay(T(S_BTL_USED), actor.name, moveName(lg.move));
  if (lg.missed) { btlSay(T(S_BTL_MISS), actor.name); return; }
  if (lg.immune) { btlSay(T(S_BTL_IMMUNE)); return; }
  if (lg.crit) btlSay(T(S_BTL_CRIT));
  if (lg.damage && lg.effPct > 100) btlSay(T(S_BTL_SUPER));
  else if (lg.damage && lg.effPct < 100) btlSay(T(S_BTL_WEAK));
  if (lg.inflicted) {
    static const StrId AIL_STR[] = { S_AIL_PARA, S_AIL_PARA, S_AIL_BURN, S_AIL_POISON,
                                     S_AIL_SLEEP, S_AIL_FREEZE, S_AIL_CONFUSE };
    if (lg.inflicted < 7) btlSay(T(S_BTL_STATUS), target.name, T(AIL_STR[lg.inflicted]));
  }
  if (lg.targetFainted) btlSay(T(S_BTL_FAINT), target.name);
}

// Builds one opponent through Pet, so it gets the same stat formula and the
// same learnset-driven moveset the player's creatures do.
static void foeFromSpecies(Combatant &c, int16_t dex, uint8_t lvl, uint8_t iv) {
  Pet foe;
  foe.dbgHatchAs(dex, false);
  foe.ivAtk = foe.ivDef = foe.ivSpe = foe.ivHp = iv;
  foe.ageMinutes = (uint32_t)(lvl ? lvl - 1 : 0) * MINUTES_PER_LEVEL;
  foe.relearnFromLevel();
  combatantFromPet(c, foe);
}

// Your side: the live pet first, then the banked party.
//
// Both ladders cap your LEVEL to the leader's best, so a gym is always fought
// on its own terms and grinding is never the answer -- the type chart, the
// movesets and the choices are. Hard additionally caps your team SIZE to the
// leader's, so Brock is two-on-two. The caps are applied while BUILDING the
// combatants, so nothing is ever written back to the stored creature, exactly
// like ailments.
static void buildSquad(uint8_t maxLvl, uint8_t maxCount, uint16_t mask) {
  btlSquadN = 0;
  btlSquadAt = 0;
  btlPetIn = false;
  if (maxCount > TRAINER_TEAM_MAX) maxCount = TRAINER_TEAM_MAX;
  if (!pet.isEgg() && btlSquadN < maxCount && (mask & 1)) {
    Pet tmp = pet;                       // a copy: the real pet is untouched
    if (maxLvl && tmp.level() > maxLvl)
      tmp.ageMinutes = (uint32_t)(maxLvl - 1) * MINUTES_PER_LEVEL;
    combatantFromPet(btlSquad[btlSquadN++], tmp);
    btlPetIn = true;      // the training reward goes to whoever fought for it
  }
  for (int i = 0; i < PARTY_SLOTS && btlSquadN < maxCount; i++) {
    if (party.slots[i].empty() || !(mask & (1 << (i + 1)))) continue;
    PartyMon m = party.slots[i];
    if (maxLvl && m.level > maxLvl) m.level = maxLvl;
    combatantFromParty(btlSquad[btlSquadN++], m);
  }
  if (btlSquadN) btlYou = btlSquad[0];
}

// How many you may bring: the leader's own count in hard mode, six otherwise.
// PICK_TRAINERS, not TRAINERS -- this answers for the fight being set up, so it
// must read the region the picker was opened for.
uint8_t squadCap(uint8_t idx, bool hard) {
  if (idx >= TRAINER_COUNT) return TRAINER_TEAM_MAX;   // PICK_LAN: uncapped six
  return hard ? PICK_TRAINERS[idx].count : TRAINER_TEAM_MAX;
}

// A fight against another device. The squads are already exchanged; the host
// owns resolution and the guest renders what it is sent.
void startLinkBattle() {
  if (!lan.mineN || !lan.theirsN) return;
  // Rebuilt from lan.mine, NOT from squadMask. What we fight with has to be
  // exactly what the peer was told we have -- rebuilding from the party would
  // silently diverge if anything changed between offering and starting.
  btlSquadN = 0;
  btlSquadAt = 0;
  for (uint8_t i = 0; i < lan.mineN && i < TRAINER_TEAM_MAX; i++)
    linkMonTo(btlSquad[btlSquadN++], lan.mine[i]);
  if (!btlSquadN) return;
  btlYou = btlSquad[0];
  btlLink = true;
  btlLinkHost = lan.isHost;
  btlWild = false;
  btlTrainer = -1;
  btlHard = false;
  btlFoeAt = 0;
  btlFoeSquadN = 0;
  for (uint8_t i = 0; i < lan.theirsN && i < TRAINER_TEAM_MAX; i++)
    linkMonTo(btlFoeSquad[btlFoeSquadN++], lan.theirs[i]);
  btlFoe = btlFoeSquad[0];
  btlMyAct = 0;
  btlMsgCount = 0;
  btlOver = false;
  btlWon = false;
  btlMenu = 0;
  btlWinUntil = 0;
  btlLost = false;
  btlSwapWho = -1;
  lanWaitSince = 0;      // a rematch reuses this screen; the old wait is over
  lanQuitConfirm = false;
  btlFaintUntil[0] = btlFaintUntil[1] = 0;
  btlEnterUntil[0] = btlEnterUntil[1] = 0;
  btlHpShown[0] = btlYou.maxHp;
  btlHpShown[1] = btlFoe.maxHp;
  btlSyncSprite(0, btlYou);
  btlSyncSprite(1, btlFoe);
  audioMusic(MUS_BATTLE);
  battleOpen = true;
}

void startTrainerBattle(uint8_t idx, bool hard) {
  if (idx >= TRAINER_COUNT || pet.isEgg() || pet.ceremony != CER_NONE) return;
  uint8_t region = gymRegion % GYM_REGIONS;
  const Trainer &tr = TRAINER_SETS[region].list[idx];
  uint8_t top = 0;
  for (int k = 0; k < tr.count; k++)
    if (tr.team[k].level > top) top = tr.team[k].level;
  // BOTH ladders cap your level to the leader's best. Without it a L73 team
  // walks every trainer at 100% and the type chart never matters. Hard adds the
  // size cap on top, plus a smarter AI and better opposing IVs.
  buildSquad(top, hard ? tr.count : TRAINER_TEAM_MAX, squadMask);
  if (!btlSquadN) return;
  btlTrainer = (int8_t)idx;
  btlRegion = region;
  btlHard = hard;
  btlWild = false;
  btlFoeAt = 0;
  foeFromSpecies(btlFoe, tr.team[0].dex, tr.team[0].level, hard ? HARD_IV : EASY_IV);
  btlMsgCount = 0;
  btlOver = false;
  btlWon = false;
  btlMenu = 0;
  btlWinUntil = 0;
  btlLost = false;
  btlSwapWho = -1;
  btlFaintUntil[0] = btlFaintUntil[1] = 0;
  btlEnterUntil[0] = btlEnterUntil[1] = 0;
  btlHpShown[0] = btlYou.maxHp;
  btlHpShown[1] = btlFoe.maxHp;
  btlSyncSprite(0, btlYou);
  btlSyncSprite(1, btlFoe);
  audioMusic(MUS_BATTLE);
  btlLungeUntil[0] = btlLungeUntil[1] = 0;
  btlHitUntil[0] = btlHitUntil[1] = 0;
  battleOpen = true;
}

void startBattle(int16_t dex, uint8_t lvl) {
  if (pet.isEgg() || pet.ceremony != CER_NONE) return;
  if (dex < 1 || dex > DEX_COUNT) return;
  buildSquad(0, TRAINER_TEAM_MAX, 0xFFFF);
  if (!btlSquadN) return;
  // The opponent is built through Pet so it gets the same stat formula and the
  // same learnset-driven moveset the player's creature does -- no special-cased
  // "enemy" maths that could quietly diverge.
  Pet foe;
  foe.dbgHatchAs(dex, false);
  foe.ivAtk = foe.ivDef = foe.ivSpe = foe.ivHp = 20;
  foe.ageMinutes = (uint32_t)(lvl ? lvl - 1 : 0) * MINUTES_PER_LEVEL;
  foe.relearnFromLevel();
  combatantFromPet(btlFoe, foe);
  btlWild = false;
  btlMsgCount = 0;
  btlOver = false;
  btlWon = false;
  btlMenu = 0;
  btlWinUntil = 0;
  btlLost = false;
  btlSwapWho = -1;
  btlFaintUntil[0] = btlFaintUntil[1] = 0;
  btlEnterUntil[0] = btlEnterUntil[1] = 0;
  btlHpShown[0] = btlYou.maxHp;
  btlHpShown[1] = btlFoe.maxHp;
  btlSyncSprite(0, btlYou);
  btlSyncSprite(1, btlFoe);
  audioMusic(MUS_BATTLE);
  btlLungeUntil[0] = btlLungeUntil[1] = 0;
  btlHitUntil[0] = btlHitUntil[1] = 0;
  battleOpen = true;
}

// The guest's whole turn: copy in what the host resolved and play the same
// animations the host is playing. It runs no battle logic at all -- that is the
// entire point of one side being authoritative (see link.h).
static void btlApplyResult() {
  if (lan.resultN < sizeof(LinkResult)) { lan.resultNew = false; return; }
  LinkResult r;
  memcpy(&r, lan.result, sizeof(r));
  lan.resultNew = false;

  // The wire says "host"/"guest"; here we are always the guest, so their fields
  // are the foe's and ours are ours.
  uint32_t now = millis();
  if (r.guestIdx < btlSquadN && r.guestIdx != btlSquadAt) {
    btlSquad[btlSquadAt] = btlYou;
    btlSquadAt = r.guestIdx;
    btlYou = btlSquad[btlSquadAt];
    btlSyncSprite(0, btlYou);
    btlLungeUntil[0] = btlHitUntil[0] = btlFaintUntil[0] = 0;
    btlEnterUntil[0] = now + BTL_ENTER_MS;
  }
  if (r.hostIdx != btlFoeAt && r.hostIdx < lan.theirsN) {
    btlFoeAt = r.hostIdx;
    linkMonTo(btlFoe, lan.theirs[btlFoeAt]);
    btlHpShown[1] = btlFoe.maxHp;
    btlSyncSprite(1, btlFoe);
    btlLungeUntil[1] = btlHitUntil[1] = btlFaintUntil[1] = 0;
    btlEnterUntil[1] = now + BTL_ENTER_MS;
  }
  btlYou.hp = r.guestHp > btlYou.maxHp ? btlYou.maxHp : r.guestHp;
  btlFoe.hp = r.hostHp > btlFoe.maxHp ? btlFoe.maxHp : r.hostHp;
  btlYou.ailment = r.guestAil;
  btlFoe.ailment = r.hostAil;

  btlMsgCount = 0;
  if (r.hostMove) btlSay(T(S_BTL_USED), btlFoe.name, moveName(r.hostMove));
  if (r.guestMove) btlSay(T(S_BTL_USED), btlYou.name, moveName(r.guestMove));
  if (r.guestDmg) { btlHitUntil[0] = now + BTL_HIT_MS; sfxPlay(SFX_HIT); }
  if (r.hostDmg) { btlHitUntil[1] = now + BTL_HIT_MS; sfxPlay(SFX_HIT); }
  if (btlYou.fainted()) {
    btlFaintUntil[0] = now + BTL_FAINT_MS;
    btlSay(T(S_BTL_FAINT), btlYou.name);
  }
  if (btlFoe.fainted()) {
    btlFaintUntil[1] = now + BTL_FAINT_MS;
    btlSay(T(S_BTL_FAINT), btlFoe.name);
  }
}

// Radio packets land on another task, so the guest picks them up here, once a
// frame, rather than rendering from inside an interrupt.
static void btlLinkPoll() {
  if (!btlLink) return;

  // A peer that stopped answering. Ending the fight is the only honest thing to
  // do -- there is no result coming, and pretending otherwise is the hang this
  // whole layer exists to remove.
  if (!lan.live() && !btlOver) {
    btlOver = true;
    btlWon = false;
    audioMusic(MUS_NONE);
    btlMsgCount = 0;
    btlSay("%s", T(S_LAN_GONE));
    return;
  }

  if (btlLinkHost) {
    // Our own action was latched when it was tapped; theirs arrives whenever
    // the radio manages it. Whichever is second sets the turn going.
    if (btlMyAct && lan.hasPeerAct() && !btlOver && !btlMsgCount &&
        btlSwapWho < 0) {
      uint8_t act = btlMyAct;
      btlMyAct = 0;
      if (LINK_ACT_IS_SWITCH(act)) btlSwitchTo(LINK_ACT_SLOT(act));
      else btlResolve(btlYou.moves[LINK_ACT_SLOT(act) % MOVE_SLOTS]);
    }
    return;
  }

  if (lan.resultNew) btlApplyResult();
  if (lan.state == LINK_DONE && !btlOver) {
    btlOver = true;
    btlWon = lan.youWon;
    audioMusic(btlWon ? MUS_VICTORY : MUS_NONE);
    if (btlWon) sfxPlay(SFX_VICTORY);
    btlSay("%s", btlWon ? T(S_BTL_WIN) : T(S_BTL_LOSE));
    const uint8_t *rivalMac = linkNowPeerMac();
    if (rivalMac) pet.recordRivalResult(rivalMac, lan.peerName, btlWon);
  }
}

// Packs the outcome for the guest. Only the host ever calls this.
static void btlShipResult(uint8_t yourMove, uint8_t theirMove,
                          uint16_t hp0You, uint16_t hp0Foe) {
  LinkResult r = {};
  r.hostHp = btlYou.hp;   r.guestHp = btlFoe.hp;
  r.hostAil = btlYou.ailment; r.guestAil = btlFoe.ailment;
  r.hostMove = yourMove;  r.guestMove = theirMove;
  r.hostDmg = (hp0You > btlYou.hp) ? hp0You - btlYou.hp : 0;
  r.guestDmg = (hp0Foe > btlFoe.hp) ? hp0Foe - btlFoe.hp : 0;
  r.hostIdx = btlSquadAt; r.guestIdx = btlFoeAt;
  if (btlYou.fainted() || btlFoe.fainted()) r.flags |= 0x04;
  lan.sendResult((const uint8_t *)&r, (uint8_t)sizeof(r));
}

// ---------- wild battle actions ----------

// What beating a wild creature is worth. Each draw EXCLUDES every earlier one,
// so a settlement can never list the same item twice -- which would read as a
// bug rather than as luck, the same reasoning that stops rewardTraining()
// picking a stat with no headroom.
static void wildGrantDrops() {
  wildDropN = 0;
  uint8_t n = wildDropCount(btlHard, (uint8_t)random(100));
  for (uint8_t i = 0; i < n && i < 4; i++) {
    uint32_t total = bag.weightTotal(wildDrops, wildDropN);
    if (!total) break;
    ItemKey k = bag.weightedDrop((uint32_t)random(total), wildDrops, wildDropN);
    if (!k) break;
    bag.add(k);
    wildDrops[wildDropN++] = k;
    btlSay(T(S_FOUND_ITEM), itemEntry(k).name);
  }
}

// Where a caught creature goes. It reuses the SAME slot-finding path a farewell
// uses -- party, then box, then ask -- rather than growing a second copy of it.
// CLAUDE.md's most repeated bug is a rule enforced in one path but not its
// twin, and "which slot does a creature take" is exactly that shape.
static void wildStoreCaught() {
  PartyMon m = wildToPartyMon();
  // NOT registered in the Pokedex here. Registration means "you raised this",
  // and switchTo() does it when the creature actually becomes the one you are
  // raising -- so catching one and leaving it in the box does not fill the dex.
  if (party.add(m) || party.boxAdd(m)) return;
  // Both full: hand it to the chooser the endings already own. endedKind is
  // only ever tested against CER_NONE by that flow, so this is the intended
  // handover and not a borrowed meaning. Through setEnded() so it is written to
  // the pet checkpoint as well -- a creature you just caught and have not yet
  // found a slot for must not evaporate on a power cut either.
  pet.setEnded(m, CER_RELEASE);
}

// Throwing a ball. Spends the turn either way: a free capture attempt every
// round would make weakening the target pointless.
static void btlThrowBall(ItemKey k) {
  if (!bag.consume(k)) { sfxPlay(SFX_DENY); return; }
  btlMenu = 0;
  btlSay(T(S_THREW), itemEntry(k).name);
  uint8_t chance = wildCaptureChance(DEX_TBL[btlFoe.dex].rarity, btlFoe.hp,
                                     btlFoe.maxHp, btlFoe.ailment != AIL_NONE,
                                     itemEntry(k).param);
  if (random(100) < chance) {
    wildCaught = true;
    btlOver = true;
    btlWon = true;
    // A legendary with a home is caught for good: it stops appearing in
    // Explore. Saved by the saveNow() below, with the creature itself.
    if (wildLegend >= 0) pet.setLegendCaught((uint8_t)wildLegend);
    wildStoreCaught();
    // wildStoreCaught() saves the PARTY (or the pet checkpoint, via
    // setEnded(), only in the both-full fallback) -- but the common case
    // never re-saves the live pet's OWN checkpoint here. That left it
    // several actions stale by the time anything else finally saved it,
    // which is the moment real hardware chose to have a transient NVS write
    // failure -- discovered only then, with a party-screen swap already in
    // flight, instead of here where nothing is at risk either way.
    pet.saveNow();
    audioMusic(MUS_VICTORY);
    sfxPlay(SFX_VICTORY);
    btlSay(T(S_CAUGHT), btlFoe.name);
    wildGrantDrops();
    return;
  }
  sfxPlay(SFX_DENY);
  btlSay("%s", T(S_BROKE_FREE));
  btlResolve(0);   // move 0 = no attack, so only the foe acts
}

// A potion or a full heal, on whichever creature is out. It works on the
// Combatant, so like every ailment it is battle-only and is never written back.
static void btlUseItem(ItemKey k) {
  const ItemEntry &e = itemEntry(k);
  bool useful = (e.category == IC_HEAL) ? btlYou.hp < btlYou.maxHp
              : (e.category == IC_CURE) ? btlYou.ailment != AIL_NONE
              : false;
  if (!useful) { sfxPlay(SFX_DENY); btlSay("%s", T(S_ITEM_NOUSE)); btlMenu = 0; return; }
  if (!bag.consume(k)) { sfxPlay(SFX_DENY); return; }
  if (e.category == IC_HEAL) {
    uint32_t hp = (uint32_t)btlYou.hp + (uint16_t)e.param;
    btlYou.hp = hp > btlYou.maxHp ? btlYou.maxHp : (uint16_t)hp;
  } else {
    btlYou.ailment = AIL_NONE;
    btlYou.ailTurns = 0;
    btlYou.confuseTurns = 0;
  }
  btlMenu = 0;
  sfxPlay(SFX_TAP);
  btlSay(T(S_ITEM_USED), e.name);
  btlResolve(0);   // using an item costs the turn, as it does in the real games
}

// One exchange: both sides act in speed order, then burn/poison chip.
static void btlResolve(uint8_t yourMove) {
  TurnLog lg;
  // Against another device the opponent's move comes off the wire, never from
  // the AI -- and the host is the only side that runs this at all.
  uint8_t foeMove;
  bool foeSwitched = false;
  uint32_t now = millis();
  if (btlLink) {
    // Off the wire, never from the AI. A switch is carried in the same message
    // as a move, and like our own switch it costs the turn: they change, we act.
    uint8_t act = lan.pendingAct;
    if (LINK_ACT_IS_SWITCH(act)) {
      uint8_t to = LINK_ACT_SLOT(act);
      if (to < btlFoeSquadN && to != btlFoeAt && !btlFoeSquad[to].fainted()) {
        btlFoeSquad[btlFoeAt] = btlFoe;     // remember how battered it was
        btlFoeAt = to;
        btlFoe = btlFoeSquad[to];
        btlHpShown[1] = btlFoe.hp;
        btlSyncSprite(1, btlFoe);
        btlLungeUntil[1] = btlHitUntil[1] = btlFaintUntil[1] = 0;
        btlEnterUntil[1] = now + BTL_ENTER_MS;
        btlSay(T(S_BTL_SENDS), lan.peerName, btlFoe.name);
        foeSwitched = true;
      }
      foeMove = 0;
    } else {
      foeMove = btlFoe.moves[LINK_ACT_SLOT(act) % MOVE_SLOTS];
    }
    lan.pendingAct = 0;
  } else {
    foeMove = aiChooseMove(btlFoe, btlYou, btlHard);
    // A wild creature can leave. It spends its ACTION doing so rather than
    // fleeing and attacking in the same turn, so the fight simply ends -- with
    // no drops, which is what makes weakening it worth doing before it goes.
    if (btlWild && random(100) < wildFoeEscapeChance(btlFoe.hp, btlFoe.maxHp)) {
      btlOver = true;
      btlWon = false;
      audioMusic(MUS_NONE);
      btlSay(T(S_WILD_FLED), btlFoe.name);
      return;
    }
  }
  (void)foeSwitched;

  bool youFirst = battleMovesFirst(btlYou, yourMove, btlFoe, foeMove);
  Combatant *a = youFirst ? &btlYou : &btlFoe;
  Combatant *b = youFirst ? &btlFoe : &btlYou;
  uint8_t ma = youFirst ? yourMove : foeMove;
  uint8_t mb = youFirst ? foeMove : yourMove;

  uint16_t hp0You = btlYou.hp, hp0Foe = btlFoe.hp;
  battleAct(*a, *b, ma, lg);
  btlNarrate(*a, *b, lg);
  if (lg.damage && !lg.hurtSelf) btlLungeUntil[a == &btlYou ? 0 : 1] = now + BTL_LUNGE_MS;
  if (!b->fainted()) {
    battleAct(*b, *a, mb, lg);
    btlNarrate(*b, *a, lg);
    if (lg.damage && !lg.hurtSelf)
      btlLungeUntil[b == &btlYou ? 0 : 1] = now + BTL_LUNGE_MS + BTL_LUNGE_MS;
  }
  // whoever actually lost health flinches, whichever side dealt it
  if (btlYou.hp < hp0You) btlHitUntil[0] = now + BTL_HIT_MS;
  if (btlFoe.hp < hp0Foe) btlHitUntil[1] = now + BTL_HIT_MS;
  if (btlLink && btlLinkHost) btlShipResult(yourMove, foeMove, hp0You, hp0Foe);
  if (!btlYou.fainted() && !btlFoe.fainted()) {
    battleEndTurn(btlYou, lg);
    if (lg.damage) btlNarrate(btlYou, btlYou, lg);
    battleEndTurn(btlFoe, lg);
    if (lg.damage) btlNarrate(btlFoe, btlFoe, lg);
  }
  // Someone went down. The replacement is NOT swapped in here -- that made the
  // change instant and read as a jump cut. Flag it, let the sprite drop out of
  // frame, and swap when the player dismisses the message.
  if (btlFoe.fainted() && btlLink && btlFoeAt + 1 < btlFoeSquadN) {
    btlFaintUntil[1] = millis() + BTL_FAINT_MS;
    btlSwapWho = 1;
    return;
  }
  if (btlFoe.fainted() && btlTrainer >= 0 && btlFoeAt + 1 < BTL_TRAINERS[btlTrainer].count) {
    btlFaintUntil[1] = millis() + BTL_FAINT_MS;
    btlSwapWho = 1;
    return;
  }
  if (btlYou.fainted() && btlSquadAt + 1 < btlSquadN) {
    btlFaintUntil[0] = millis() + BTL_FAINT_MS;
    btlSwapWho = 0;
    return;
  }
  if (btlFoe.fainted() || btlYou.fainted()) {
    btlOver = true;
    btlWon = btlFoe.fainted();
    btlNewBadge = false;
    btlTrainGain = 0;
    if (btlWon && btlTrainer >= 0 && !pet.hasBadge(btlRegion, btlTrainer, btlHard)) {
      pet.winBadge(btlRegion, btlTrainer, btlHard);
      btlNewBadge = true;
    }
    // A badge and nothing else made the ladder a one-way checklist. A win now
    // trains the creature that fought for it -- so a leader you can already
    // beat is worth returning to. It goes to the LIVE pet only: banked members
    // are frozen at the level and training they were banked with, and battling
    // already costs the live pet energy, which is what rate-limits the grind
    // without needing a cooldown.
    if (btlWon && btlTrainer >= 0 && btlPetIn) {
      // Later leaders are worth more, and hard mode is worth roughly double.
      uint8_t amt = (btlHard ? 6 + random(5) : 3 + random(3)) + btlTrainer / 3;
      btlTrainGain = pet.rewardTraining(amt, btlTrainWhich);
    }
    // Every ending shares this point before branching into wild/trainer/LAN
    // narration below (all of which `return` after it), so one call here
    // covers all of them rather than needing one per branch -- a badge win
    // already saves via winBadge()/rewardTraining(), but a wild win with no
    // catch, a loss, and a LAN result (the GUEST side especially: only the
    // host's branch below writes a rival record) had nothing else that
    // saved the live pet's own checkpoint here at all. Same reasoning as the
    // wild-catch save above: cheap and safe to do now, while nothing is at
    // risk, rather than leaving it to whatever the player does next.
    pet.saveNow();
    audioMusic(btlWon ? MUS_VICTORY : MUS_NONE);
    if (btlWon) sfxPlay(SFX_VICTORY);
    // Tell the peer before anything else: if we stop here without sending, the
    // other device sits on a battle that will never take another turn.
    if (btlLink && btlLinkHost) {
      lan.sendEnd(btlWon);
      const uint8_t *rivalMac = linkNowPeerMac();
      if (rivalMac) pet.recordRivalResult(rivalMac, lan.peerName, btlWon);
    }
    if (btlLink) { btlSay("%s", btlWon ? T(S_BTL_WIN) : T(S_BTL_LOSE)); return; }
    // A wild win reports its drops as narration rather than on the win screen:
    // that screen is the badge ceremony, and it has no badge to show.
    if (btlWon && btlWild) { btlSay("%s", T(S_BTL_WIN)); wildGrantDrops(); return; }
    if (btlWon && btlTrainer >= 0) { btlWinUntil = millis() + 60000; return; }
    btlLost = !btlWon;   // a LAN fight returned above: only gyms and the wild black out
    btlSay("%s", T(S_BTL_LOSE));
  }
}

static void btlHpBar(int x, int y, int w, const Combatant &c, uint16_t shown) {
  int fw = c.maxHp ? (w - 4) * shown / c.maxHp : 0;
  uint16_t col = (shown * 2 > c.maxHp) ? UI_BAR_OK
                 : (shown * 4 > c.maxHp) ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(x, y, w, 14, 4, UI_TRACK);
  if (fw > 0) gfx->fillRoundRect(x + 2, y + 2, fw, 10, 3, col);
  gfx->drawRoundRect(x, y, w, 14, 4, UI_INK);
}

static void btlSide(int tx, int ty, int sx, int sy, const Combatant &c, uint8_t who) {
  // the scenes are busy, so the name and bar sit on their own plate rather
  // than fighting the artwork for contrast
  int ph = (who == 0) ? 54 : 40;
  gfx->fillRoundRect(tx - 8, ty - 8, 158, ph, 8, UI_BG_DAY);
  gfx->drawRoundRect(tx - 8, ty - 8, 158, ph, 8, UI_INK);
  char l[28];
  snprintf(l, sizeof(l), "%s Lv.%u", c.name, c.level);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(1);
  gfx->setCursor(tx, ty);
  gfx->print(l);
  gfx->setTextColor(UI_BAR_WARN);
  gfx->setCursor(tx, ty + 14);
  gfx->print("HP");
  btlHpBar(tx + 18, ty + 12, 122, c, btlHpShown[who]);
  if (who == 0) {                 // your own numbers, as the games do
    char hp[16];
    snprintf(hp, sizeof(hp), "%u/%u", btlHpShown[who], c.maxHp);
    gfx->setTextColor(UI_INK);
    gfx->setCursor(tx + 140 - (int)strlen(hp) * 6, ty + 28);
    gfx->print(hp);
  }
  if (c.ailment != AIL_NONE) {   // a status is the thing you most need to see
    static const StrId AIL_STR[] = { S_AIL_PARA, S_AIL_PARA, S_AIL_BURN, S_AIL_POISON,
                                     S_AIL_SLEEP, S_AIL_FREEZE, S_AIL_CONFUSE };
    gfx->setTextColor(UI_BAR_BAD);
    gfx->setCursor(tx + 18, ty + 28);
    gfx->print(T(AIL_STR[c.ailment]));
  }
  // a platform under each creature, so they stand in the scene rather than
  // floating over it
  uint32_t now = millis();
  int ox = 0, oy = 0;
  bool flash = false;
  if (now < btlFaintUntil[who]) {          // sinks out of frame as it faints
    uint32_t left = btlFaintUntil[who] - now;
    oy += (int)((BTL_FAINT_MS - left) * 70 / BTL_FAINT_MS);
  } else if (c.fainted() && btlSwapWho == (int8_t)who) {
    return;                                // gone, waiting to be replaced
  }
  if (now < btlEnterUntil[who]) {          // and the next one rises into place
    uint32_t left = btlEnterUntil[who] - now;
    oy += (int)(left * 70 / BTL_ENTER_MS);
  }
  if (now < btlLungeUntil[who]) {          // lean in, then back out
    uint32_t left = btlLungeUntil[who] - now;
    int amt = (int)(left > BTL_LUNGE_MS / 2 ? BTL_LUNGE_MS - left : left) * 22 / (BTL_LUNGE_MS / 2);
    ox = who == 0 ? amt : -amt;            // you lunge right, the foe lunges left
    oy = who == 0 ? -amt / 2 : amt / 2;
  }
  if (now < btlHitUntil[who]) {
    uint32_t left = btlHitUntil[who] - now;
    ox += ((left / 50) % 2) ? 5 : -5;      // jitter
  }

  // Real PMD playback when the sprite streamed: attack while lunging, hurt
  // while flinching, idle otherwise. `has()` guards every one, because not
  // every species ships every action -- falling through to idle, and to the
  // flat thumbnail if the sprite is missing entirely (no SD).
  if (btlPmd[who].loaded) {
    uint8_t act = PMD_IDLE;
    bool loop = true;
    uint32_t t = now;
    if (now < btlHitUntil[who] && btlPmd[who].has(PMD_HURT)) {
      act = PMD_HURT; loop = false; t = now - (btlHitUntil[who] - BTL_HIT_MS);
    } else if (now < btlLungeUntil[who] && btlPmd[who].has(PMD_ATTACK)) {
      act = PMD_ATTACK; loop = false; t = now - (btlLungeUntil[who] - BTL_LUNGE_MS);
    }
    drawPmdActM(btlPmd[who], act, sx + 24 + ox, sy + 78 + oy, t, loop, false, 4);
    return;
  }
  const uint8_t *th = thumbs.get(c.dex);
  if (!th) return;
  if (now < btlHitUntil[who]) flash = ((btlHitUntil[who] - now) / 60) % 2 == 0;
  drawThumb(th, sx + ox, sy + oy, 3, flash);
}

// Bars drain rather than snap: a hit that removes half your health should be
// visible as it happens, not as a value that was already different.
static void btlEaseBars() {
  const uint16_t real[2] = { btlYou.hp, btlFoe.hp };
  for (int i = 0; i < 2; i++) {
    int diff = (int)real[i] - (int)btlHpShown[i];
    if (!diff) continue;
    int step = diff / 5;
    if (!step) step = diff > 0 ? 1 : -1;
    btlHpShown[i] = (uint16_t)((int)btlHpShown[i] + step);
  }
}

// The moment the ladder builds toward. It used to be one more line in the same
// message box as "It's super effective!", with the badge awarded silently.
void renderWin() {
  const Trainer &t = BTL_TRAINERS[btlTrainer];
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);

  gfx->setTextColor(UI_BAR_WARN);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_BTL_WIN)) * 9, 54);
  gfx->print(T(S_BTL_WIN));

  char l[40];
  snprintf(l, sizeof(l), T(S_BTL_BEAT), t.name);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(l) * 6, 96);
  gfx->print(l);

  // the badge, large, with the hard-mode halo if that is how it was won
  if (btlTrainer < TRAINER_GYMS) {
    int by = 190;
    if (btlHard) {
      for (int r = 62; r >= 56; r--) gfx->drawCircle(CX, by, r, r % 2 ? 0xFEA0 : 0xFF60);
    }
    const BadgeArt *a = badgeArtFor(btlRegion, btlTrainer);
    if (a) {
      for (int r = 0; r < BADGE_PX; r++)
        for (int c = 0; c < BADGE_PX; c++) {
          uint8_t v = a->idx[r * BADGE_PX + c];
          if (v == 0xFF) continue;
          // 3x, so it reads as a prize rather than a list entry
          gfx->fillRect(CX - BADGE_PX * 3 / 2 + c * 3, by - BADGE_PX * 3 / 2 + r * 3,
                        3, 3, a->pal[v]);
        }
    } else {
      // This region has no badge art. Say so with a plain medal in the leader's
      // type colour rather than borrowing another region's badge.
      const Trainer &tr = TRAINER_SETS[btlRegion % GYM_REGIONS].list[btlTrainer];
      gfx->fillCircle(CX, by, 26, typeColor(tr.type));
      gfx->drawCircle(CX, by, 26, UI_INK);
      char n[4];
      snprintf(n, sizeof(n), "%u", (unsigned)(btlTrainer + 1));
      gfx->setTextSize(3);
      gfx->setTextColor(typeColorIsLight(tr.type) ? UI_INK : UI_WHITE);
      gfx->setCursor(CX - (int)strlen(n) * 9, by - 11);
      gfx->print(n);
    }
    if (btlNewBadge) {
      gfx->setTextColor(UI_BAR_OK);
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(T(S_BTL_NEWBADGE)) * 6, 286);
      gfx->print(T(S_BTL_NEWBADGE));
    }
  }
  snprintf(l, sizeof(l), T(S_BADGES_FMT), pet.badgeCountIn(btlRegion, btlHard));
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(l) * 6, 316);
  gfx->print(l);

  // what the win was worth beyond the badge
  if (btlTrainGain) {
    static const StrId NAMES[3] = { S_TR_ATK, S_TR_DEF, S_TR_SPE };
    snprintf(l, sizeof(l), T(S_WIN_TRAIN_FMT),
             T(NAMES[btlTrainWhich % 3]), btlTrainGain);
    gfx->setTextColor(UI_BAR_OK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(l) * 6, 344);
    gfx->print(l);
  } else if (btlPetIn && btlTrainer >= 0) {
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(T(S_WIN_MAXED)) * 3, 348);
    gfx->print(T(S_WIN_MAXED));
  }

  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_BACK)) * 6, 380);
  gfx->print(T(S_BACK));
  gfx->flush();
}

void renderBattle() {
  if (btlWinUntil) { renderWin(); return; }
  btlEaseBars();
  gfx->fillScreen(RGB565_BLACK);
  drawBattleBack();
  // the lower band stays flat so the move grid and the HP text keep their
  // contrast against it
  gfx->fillRect(0, 254, 466, 212, UI_BG_DAY);

  // x=82 not 58: at y=60 the round bezel starts around x=77, and a longer
  // name like BLASTOISE was losing its first characters off the edge
  btlSide(82, 82, 300, 40, btlFoe, 1);    // foe reads top-left, sprite top-right
  btlSide(250, 190, 76, 168, btlYou, 0);  // you read bottom-right, sprite bottom-left

  // Waiting on the other device. Without this the screen is identical to the
  // one where it is your turn, so a tap that has been sent and a tap that was
  // never registered look exactly the same.
  bool lanWait = lanIsWaiting();
  if (lanWait) {
    if (!lanWaitSince) lanWaitSince = millis();
  } else {
    lanWaitSince = 0;
    lanQuitConfirm = false;   // the dialog cannot outlive the wait it belongs to
  }
  if (lanWait && !btlMsgCount) {
    gfx->fillRoundRect(BTL_GRID_X, BTL_GRID_Y, 328, BTL_GRID_H, 12, UI_WHITE);
    gfx->drawRoundRect(BTL_GRID_X, BTL_GRID_Y, 328, BTL_GRID_H, 12, UI_INK);
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(2);
    const char *w = T(S_LAN_WAITFOE);
    gfx->setCursor(CX - (int)strlen(w) * 6, BTL_GRID_Y + 48);
    gfx->print(w);
    // BTL_BACK_X/Y is unused space at this menu level (drawBtlBack() is only
    // called from the OTHER btlMenu branches below) -- reused here rather than
    // claiming new geometry for a button that only appears sometimes.
    if (millis() - lanWaitSince >= LAN_QUIT_AFTER_MS) {
      gfx->fillRoundRect(BTL_BACK_X, BTL_BACK_Y, BTL_BACK_W, BTL_BACK_H, 11, UI_BAR_WARN);
      gfx->drawRoundRect(BTL_BACK_X, BTL_BACK_Y, BTL_BACK_W, BTL_BACK_H, 11, UI_INK);
      gfx->setTextColor(UI_INK);
      gfx->setTextSize(2);
      const char *qb = T(S_LAN_QUIT);
      gfx->setCursor(CX - (int)strlen(qb) * 6, BTL_BACK_Y + 14);
      gfx->print(qb);
    }
    if (lanQuitConfirm) {
      drawConfirmPanel(T(S_LAN_QUIT_Q), nullptr, nullptr, UI_BAR_BAD,
                        T(S_YES), UI_BAR_BAD, UI_WHITE, T(S_NO), UI_TRACK, UI_INK);
    }
  } else if (btlMsgCount) {            // narration takes over the menu area
    gfx->fillRoundRect(BTL_GRID_X, BTL_GRID_Y, 328, BTL_GRID_H, 12, UI_WHITE);
    gfx->drawRoundRect(BTL_GRID_X, BTL_GRID_Y, 328, BTL_GRID_H, 12, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    char lines[3][25] = {{0}};
    const char *p = btlMsg[0];
    uint8_t lineCount = 0;
    while (*p && lineCount < 3) {
      while (*p == ' ') p++;
      int take = (int)strlen(p);
      if (take > 24) take = 24;
      if (p[take] && p[take] != ' ') {
        int word = take;
        while (word > 0 && p[word] != ' ') word--;
        if (word > 0) take = word;
      }
      memcpy(lines[lineCount], p, take);
      lines[lineCount][take] = 0;
      p += take;
      lineCount++;
    }
    int firstY = BTL_GRID_Y + 18 + (3 - lineCount) * 10;
    for (uint8_t i = 0; i < lineCount; i++) {
      gfx->setCursor(CX - (int)strlen(lines[i]) * 6, firstY + i * 24);
      gfx->print(lines[i]);
    }
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(1);
    gfx->setCursor(CX - 18, BTL_GRID_Y + BTL_GRID_H - 14);
    gfx->print("tap...");
  } else if (btlMenu == 0) {
    // Four options on the 2x2 grid: FIGHT and BAG on top, POKEMON and RUN
    // below. FIGHT used to be a full-width row, which left nowhere for the bag;
    // all four now reuse the move grid's cells and so inherit its padded hit
    // areas -- the same padding that fixed POKEMON being hard to press.
    const char *opt[4] = { T(S_FIGHT), T(S_BAG), T(S_BTL_SWITCH), T(S_BTL_RUN) };
    for (int i = 0; i < 4; i++) {
      int x = BTL_CELL_X(i), y = BTL_CELL_Y(i);
      // The bag is dead in a link fight: an item the peer was never told about
      // would desync the two devices, and the host is authoritative.
      bool live = !(i == 1 && btlLink);
      gfx->fillRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, live ? UI_BG_DAY : UI_TRACK);
      gfx->drawRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, live ? UI_INK : 0x8410);
      gfx->setTextColor(live ? UI_INK : 0x8410);
      gfx->setTextSize(2);
      gfx->setCursor(x + (BTL_CELL_W - (int)strlen(opt[i]) * 12) / 2, y + 19);
      gfx->print(opt[i]);
    }
  } else if (btlMenu == 3) {
    drawBtlBack();
    // The battle bag: balls and medicine, never vitamins -- training is
    // permanent and belongs to the care sim.
    uint8_t shown = 0;
    uint8_t n = bag.distinctCount();
    for (uint8_t idx = 0; idx < n && shown < 4; idx++) {
      ItemKey k = bag.keyAt(idx);
      if (!itemUsableInBattle(k)) continue;
      if (shown < btlBagPage * 4) { shown++; continue; }
      int cell = shown - btlBagPage * 4;
      if (cell >= 4) break;
      int x = BTL_CELL_X(cell), y = BTL_CELL_Y(cell);
      bool live = !itemIsBall(k) || btlWild;   // a ball needs a wild target
      gfx->fillRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, live ? UI_BG_DAY : UI_TRACK);
      gfx->drawRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, live ? UI_INK : 0x8410);
      gfx->setTextColor(live ? UI_INK : 0x8410);
      gfx->setTextSize(strlen(itemEntry(k).name) <= 11 ? 2 : 1);
      gfx->setCursor(x + 10, y + 8);
      gfx->print(itemEntry(k).name);
      gfx->setTextSize(1);
      char cnt[8];
      snprintf(cnt, sizeof(cnt), "x%u", bag.count(k));
      gfx->setCursor(x + 10, y + 28);
      gfx->print(cnt);
      shown++;
    }
    if (!shown) {
      gfx->setTextColor(UI_INK_SOFT);
      gfx->setTextSize(1);
      gfx->setCursor(CX - (int)strlen(T(S_BAG_EMPTY)) * 3, BTL_GRID_Y + 40);
      gfx->print(T(S_BAG_EMPTY));
    }
  } else if (btlMenu == 2) {
    drawBtlBack();
    // who to bring on instead; the current one and anything fainted is inert
    for (uint8_t i = 0; i < btlSquadN && i < 4; i++) {
      int x = BTL_CELL_X(i), y = BTL_CELL_Y(i);
      const Combatant &m = (i == btlSquadAt) ? btlYou : btlSquad[i];
      bool usable = (i != btlSquadAt) && !m.fainted();
      gfx->fillRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, usable ? UI_BG_DAY : UI_TRACK);
      gfx->drawRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, usable ? UI_INK : 0x8410);
      gfx->setTextColor(usable ? UI_INK : 0x8410);
      gfx->setTextSize(strlen(m.name) <= 11 ? 2 : 1);
      gfx->setCursor(x + 10, y + 8);
      gfx->print(m.name);
      gfx->setTextSize(1);
      char hp[20];
      snprintf(hp, sizeof(hp), "%u/%u", m.hp, m.maxHp);
      gfx->setCursor(x + 10, y + 28);
      gfx->print(hp);
    }
  } else {
    drawBtlBack();
    for (int i = 0; i < MOVE_SLOTS; i++) {
      int x = BTL_CELL_X(i), y = BTL_CELL_Y(i);
      uint8_t mv = btlYou.moves[i];
      gfx->fillRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, mv ? UI_BG_DAY : UI_TRACK);
      gfx->drawRoundRect(x, y, BTL_CELL_W, BTL_CELL_H, 10, UI_INK);
      if (!mv) continue;
      const char *name = moveName(mv);
      gfx->setTextColor(UI_INK);
      gfx->setTextSize(strlen(name) <= 11 ? 2 : 1);
      gfx->setCursor(x + 10, y + 7);
      gfx->print(name);
      // Same chip as the move list: in a fight the type IS the decision, and
      // grey 6px text was the least visible thing on the busiest screen.
      int cw = drawTypeChip(x + 10, y + 34, MOVE_TBL[mv].type);
      if (hasStab(btlYou.dex, MOVE_TBL[mv].type) &&
          MOVE_TBL[mv].cat != MC_STATUS) {
        gfx->setTextSize(1);
        gfx->setTextColor(DEX_TBL[btlYou.dex].accent);
        gfx->setCursor(x + 10 + cw + 4, y + 38);
        gfx->print("+");
      }
    }
  }
  gfx->flush();
}

// Brings on the flagged replacement and starts its entrance.
static void btlDoSwap() {
  uint32_t now = millis();
  if (btlSwapWho == 1 && btlLink) {
    btlFoeSquad[btlFoeAt] = btlFoe;
    // the next one still standing, not simply the next index
    uint8_t nxt = btlFoeAt;
    while (++nxt < btlFoeSquadN && btlFoeSquad[nxt].fainted()) {}
    if (nxt >= btlFoeSquadN) { btlSwapWho = -1; return; }
    btlFoeAt = nxt;
    btlFoe = btlFoeSquad[btlFoeAt];
    btlHpShown[1] = btlFoe.hp;
    btlSyncSprite(1, btlFoe);
    btlLungeUntil[1] = btlHitUntil[1] = btlFaintUntil[1] = 0;
    btlEnterUntil[1] = now + BTL_ENTER_MS;
    btlSay(T(S_BTL_SENDS), lan.peerName, btlFoe.name);
  } else if (btlSwapWho == 1) {
    const Trainer &t = BTL_TRAINERS[btlTrainer];
    btlFoeAt++;
    foeFromSpecies(btlFoe, t.team[btlFoeAt].dex, t.team[btlFoeAt].level,
                   btlHard ? HARD_IV : EASY_IV);
    btlHpShown[1] = btlFoe.maxHp;
    btlSyncSprite(1, btlFoe);
    btlLungeUntil[1] = btlHitUntil[1] = btlFaintUntil[1] = 0;
    btlEnterUntil[1] = now + BTL_ENTER_MS;
    btlSay(T(S_BTL_SENDS), t.name, btlFoe.name);
  } else if (btlSwapWho == 0) {
    btlSquad[btlSquadAt] = btlYou;     // remember how battered it was
    btlSquadAt++;
    btlYou = btlSquad[btlSquadAt];
    btlHpShown[0] = btlYou.hp;
    btlSyncSprite(0, btlYou);
    btlLungeUntil[0] = btlHitUntil[0] = btlFaintUntil[0] = 0;
    btlEnterUntil[0] = now + BTL_ENTER_MS;
    btlSay(T(S_BTL_GO), btlYou.name);
  }
  btlSwapWho = -1;
}

// Switching spends your turn: the opponent still acts. That is what stops it
// being a free look at the matchup every round.
static void btlSwitchTo(uint8_t i) {
  if (i >= btlSquadN || i == btlSquadAt) return;
  btlSquad[btlSquadAt] = btlYou;
  btlSquadAt = i;
  btlYou = btlSquad[i];
  btlHpShown[0] = btlYou.hp;
  btlSyncSprite(0, btlYou);
  btlLungeUntil[0] = btlHitUntil[0] = btlFaintUntil[0] = 0;
  btlEnterUntil[0] = millis() + BTL_ENTER_MS;
  btlMenu = 0;
  btlSay(T(S_BTL_GO), btlYou.name);
  btlResolve(0);          // move 0 = no attack, so only the foe acts
}

int btlCellIndexAt(int16_t x, int16_t y) {
  for (int i = 0; i < 4; i++)
    if (btlCellHit(i, x, y)) return i;
  return -1;
}

// The way out of the move and switch screens. Without it the only exits were
// choosing something or leaving the fight entirely.
static void drawBtlBack() {
  gfx->fillRoundRect(BTL_BACK_X, BTL_BACK_Y, BTL_BACK_W, BTL_BACK_H, 11, UI_TRACK);
  gfx->drawRoundRect(BTL_BACK_X, BTL_BACK_Y, BTL_BACK_W, BTL_BACK_H, 11, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(T(S_BACK)) * 6, BTL_BACK_Y + 14);
  gfx->print(T(S_BACK));
}

static bool btlBackTap(int16_t x, int16_t y) {
  if (x < BTL_BACK_X || x > BTL_BACK_X + BTL_BACK_W ||
      y < BTL_BACK_Y || y > BTL_BACK_Y + BTL_BACK_H) return false;
  btlMenu = 0;
  sfxPlay(SFX_TAP);
  return true;
}

// Running. A gym leader keeps their badge and the fight simply ends; against
// another device the peer is told, so it does not sit waiting for a move that
// will never come.
//
// A WILD fight is the exception: getting away is a roll, and failing it costs
// the turn. That is what stops a ball being risk-free -- you cannot throw one,
// see it fail and simply leave.
static void btlRun() {
  if (btlWild && !btlOver) {
    btlMenu = 0;
    if (random(100) >= wildEscapeChance(btlYou.level, btlFoe.level)) {
      sfxPlay(SFX_DENY);
      btlSay("%s", T(S_NO_ESCAPE));
      btlResolve(0);   // move 0 = no attack, so only the foe acts
      return;
    }
    // Narrated and left OVER rather than closed here, so the message is
    // actually seen: battleTap() dismisses a finished fight, and closing on the
    // same beat would flash the line for one frame.
    sfxPlay(SFX_TAP);
    btlOver = true;
    btlWon = false;
    audioMusic(MUS_NONE);
    btlSay("%s", T(S_GOT_AWAY));
    return;
  }
  sfxPlay(SFX_DENY);
  btlFreeSprites();
  audioMusic(MUS_NONE);
  if (btlLink) { lanLeave(); btlLink = false; lanOpen = true; }
  battleOpen = false;
  btlWild = false;
  btlMenu = 0;
}

void battleTap(int16_t x, int16_t y) {
  if (btlWinUntil) {          // dismiss the win screen and leave the fight
    btlWinUntil = 0;
    btlFreeSprites();
    audioMusic(MUS_NONE);
    battleOpen = false;
    btlWild = false;
    // lanLeave() tears down the radio (ESP-NOW + WiFi off) and resets
    // lan.state to LINK_OFF. btlRun()'s own exit does this; this one and the
    // one below did not, which left the radio running and lan.state stuck
    // wherever the fight left it every time a link battle was dismissed from
    // here instead -- see lanLeave()'s own comment.
    if (btlLink) { lanLeave(); btlLink = false; lanOpen = true; }
    return;
  }
  if (btlMsgCount) {          // one readable event per tap; never skip queued text
    for (uint8_t i = 1; i < btlMsgCount; i++)
      memcpy(btlMsg[i - 1], btlMsg[i], sizeof(btlMsg[0]));
    btlMsgCount--;
    if (btlMsgCount) return;
    if (btlOver) {
      btlFreeSprites();
      // A wild win or catch never sets btlWinUntil (that is trainer-only, see
      // below), so it never passes through that dismiss's own audioMusic(MUS_NONE)
      // either -- MUS_VICTORY from the catch/win above was otherwise left running
      // straight into the main screen and whatever came after it.
      audioMusic(MUS_NONE);
      battleOpen = false;
      btlWild = false;
      if (btlLost && !btlLink) {   // out of creatures: black out, Mom takes over
        btlLost = false;
        blackoutOpen = true;
        blackoutAt = millis();
      }
      // Back to the LAN screen rather than all the way out: that is where a
      // rematch is offered, and re-pairing for every fight would be tedious.
      // lanLeave() matters here specifically: this is the path a mid-battle
      // disconnect dismisses through (see btlLinkPoll()'s S_LAN_GONE), and
      // without it the radio was left running -- fully initialised, never
      // torn down -- while lan.state stayed at LINK_LOST instead of
      // LINK_OFF, going straight to a LAN screen that expects neither.
      if (btlLink) { lanLeave(); btlLink = false; lanOpen = true; }
      return;
    }
    if (btlSwapWho >= 0) btlDoSwap();   // the replacement arrives on this beat
    return;
  }
  if (lanQuitConfirm) {
    int c1t, c1b, c2t, c2b;
    uiConfirmRects(&c1t, &c1b, &c2t, &c2b);
    bool inX = (x >= CONFIRM_BTN_X && x <= CONFIRM_BTN_X + CONFIRM_BTN_W);
    if (inX && y >= c1t && y <= c1b) {      // YES -- leave for good
      lanQuitConfirm = false;
      btlRun();      // a link fight is never wild, so this IS the LAN exit path
      return;
    }
    if (inX && y >= c2t && y <= c2b) {      // NO
      lanQuitConfirm = false;
      sfxPlay(SFX_TAP);
    }
    return;   // modal: a miss must not fall through to whatever is underneath
  }
  if (lanIsWaiting()) {
    // Nothing else on this screen may respond while purely waiting on the
    // rival -- the four cells underneath the wait panel are still hit-tested
    // otherwise, invisible but tappable, which is exactly the "modal that
    // leaks a miss through" shape CLAUDE.md section 4 warns about.
    if (millis() - lanWaitSince >= LAN_QUIT_AFTER_MS &&
        x >= BTL_BACK_X && x <= BTL_BACK_X + BTL_BACK_W &&
        y >= BTL_BACK_Y && y <= BTL_BACK_Y + BTL_BACK_H) {
      sfxPlay(SFX_TAP);
      lanQuitConfirm = true;
    }
    return;
  }
  if (btlMenu == 0) {
    if (btlCellHit(0, x, y)) { sfxPlay(SFX_TAP); btlMenu = 1; return; }   // FIGHT
    if (btlCellHit(1, x, y)) {
      if (btlLink) { sfxPlay(SFX_DENY); return; }   // see renderBattle
      sfxPlay(SFX_TAP); btlMenu = 3; btlBagPage = 0; return;
    }
    if (btlCellHit(2, x, y)) { sfxPlay(SFX_TAP); btlMenu = 2; return; }
    if (btlCellHit(3, x, y)) { btlRun(); return; }
    return;
  }
  if (btlMenu == 3) {
    if (btlBackTap(x, y)) return;
    uint8_t shown = 0;
    uint8_t n = bag.distinctCount();
    for (uint8_t idx = 0; idx < n; idx++) {
      ItemKey k = bag.keyAt(idx);
      if (!itemUsableInBattle(k)) continue;
      if (shown < btlBagPage * 4) { shown++; continue; }
      int cell = shown - btlBagPage * 4;
      if (cell >= 4) break;
      shown++;
      if (!btlCellHit(cell, x, y)) continue;
      // ONE predicate decides whether a ball may be thrown, and the draw above
      // asks the same one -- a greyed cell that still worked is the exact fault
      // uiButtonDisabled() exists to prevent on the home row.
      if (itemIsBall(k)) {
        if (!btlWild) { sfxPlay(SFX_DENY); return; }
        btlThrowBall(k);
      } else {
        btlUseItem(k);
      }
      return;
    }
    btlMenu = 0;      // anywhere else backs out
    return;
  }
  if (btlMenu == 2) {
    if (btlBackTap(x, y)) return;
    for (uint8_t i = 0; i < btlSquadN && i < 4; i++) {
      if (!btlCellHit(i, x, y)) continue;
      const Combatant &m = (i == btlSquadAt) ? btlYou : btlSquad[i];
      if (i == btlSquadAt || m.fainted()) { sfxPlay(SFX_DENY); return; }
      sfxPlay(SFX_TAP);
      if (btlLink && !btlLinkHost) {
        // The guest asks; it never switches on its own. A switch rides the same
        // message as a move, so the host spends the turn on it exactly as it
        // would for us.
        lan.sendAct(LINK_ACT_SWITCH_TO(i));
        btlMenu = 0;
        return;
      }
      if (btlLink) {            // host: latched like a move, see btlLinkPoll
        btlMyAct = LINK_ACT_SWITCH_TO(i);
        btlMenu = 0;
        if (!lan.hasPeerAct()) return;
        btlMyAct = 0;
      }
      btlSwitchTo(i);
      return;
    }
    btlMenu = 0;      // anywhere else backs out
    return;
  }
  if (btlBackTap(x, y)) return;
  for (int i = 0; i < MOVE_SLOTS; i++) {
    if (!btlYou.moves[i]) continue;
    if (!btlCellHit(i, x, y)) continue;
    sfxPlay(SFX_TAP);
    btlMenu = 0;
    if (btlLink && !btlLinkHost) {
      lan.sendAct(LINK_ACT_MOVE(i));   // the guest asks; the host decides
      return;
    }
    if (btlLink) {
      // Latched, not discarded. The host used to throw the tap away when the
      // rival had not chosen yet, so you had to keep jabbing at the move until
      // the timing happened to line up.
      btlMyAct = LINK_ACT_MOVE(i);
      if (!lan.hasPeerAct()) return;   // resolved by btlLinkPoll when it lands
      btlMyAct = 0;
    }
    btlResolve(btlYou.moves[i]);
    return;
  }
  btlMenu = 0;        // a tap off the grid goes back to FIGHT/POKEMON
}

// ---------- player card (swipe down) ----------
// Everything here is player-wide and outlives the creature: badges, the daily
// streak, the Pokedex and the party. No player sprite yet -- the SD carries
// PMD creature sprites only, so a trainer portrait needs new art.
// Page 1: who you are -- avatar, badges, totals. Tap the avatar to cycle it;
// the four sprites are hand-drawn (see species.h) because SpriteCollab has no
// trainer art and ripped sprites would be unlicensed.
static void renderPlayerBadges() {
  // the player's name if they have set one, the generic title if not; either
  // way tapping it opens the keyboard
  const char *tn = pet.trainerName[0] ? pet.trainerName : T(S_TRAINER);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - (int)strlen(tn) * 9, 40);
  gfx->print(tn);

  // Pages 1 and 2 are the other regions' ladders: name them, and drop the
  // avatar so the badges have the room. Only page 0 is "you".
  if (playerBadgeRegion != 0) {
    const char *rn = TRAINER_SETS[playerBadgeRegion].region;
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(rn) * 6, 120);
    gfx->print(rn);
  } else if (gShowAllAvatars) {          // emulator only: every avatar at once
    for (uint8_t i = 0; i < AVATAR_COUNT; i++)
      drawAvatar(i, 60 + (i % 4) * 88, 60 + (i / 4) * 60, 3);
  } else {
    drawAvatar(pet.avatar, CX - AVATAR_PX * 2, 72, 4);
    // the sprite alone is not always obvious at 16x16, so it is named
    const char *an = AVATARS[pet.avatar % AVATAR_COUNT].name;
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(an) * 3, 143);
    gfx->print(an);
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setCursor(CX - (int)strlen(T(S_AVATAR_HINT)) * 3, 154);
    gfx->print(T(S_AVATAR_HINT));
  }

  // The real badges, 2x4. Unearned ones draw as a faint outline so the shape
  // of what is missing is still visible.
  for (int i = 0; i < TRAINER_GYMS; i++) {
    int bx = 140 + (i % 4) * 62, by = 188 + (i / 4) * 62;
    bool got = pet.hasBadge(playerBadgeRegion, i, false);
    bool hard = pet.hasBadge(playerBadgeRegion, i, true);
    if (hard) {
      // Beaten on hard: a golden halo. Concentric rings, not a filled disc --
      // a disc sat behind the art and read as a gold coin rather than a glow.
      gfx->drawCircle(bx, by, 25, 0xFDE0);
      gfx->drawCircle(bx, by, 24, 0xFEA0);
      gfx->drawCircle(bx, by, 23, 0xFF60);
      gfx->drawCircle(bx, by, 22, 0xFEA0);
      gfx->drawCircle(bx, by, 21, 0xFDE0);
    }
    if (!got) {
      gfx->drawCircle(bx, by, 20, UI_TRACK);
      continue;
    }
    const BadgeArt *a = badgeArtFor(playerBadgeRegion, i);
    if (!a) {                 // no art for this region: a filled disc, won is won
      gfx->fillCircle(bx, by, 14, typeColor(TRAINER_SETS[playerBadgeRegion % GYM_REGIONS].list[i].type));
      gfx->drawCircle(bx, by, 14, UI_INK);
      continue;
    }
    for (int r = 0; r < BADGE_PX; r++)
      for (int c = 0; c < BADGE_PX; c++) {
        uint8_t v = a->idx[r * BADGE_PX + c];
        if (v == 0xFF) continue;
        gfx->fillRect(bx - BADGE_PX / 2 + c, by - BADGE_PX / 2 + r, 1, 1, a->pal[v]);
      }
  }
  char l[32];
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  snprintf(l, sizeof(l), T(S_STREAK_FMT), pet.streak, pet.bestStreak);
  gfx->setCursor(CX - (int)strlen(l) * 6, 286);
  gfx->print(l);
  snprintf(l, sizeof(l), T(S_POKEDEX_FMT), pet.registeredCount(), DEX_COUNT);
  gfx->setCursor(CX - (int)strlen(l) * 6, 312);
  gfx->print(l);
  snprintf(l, sizeof(l), T(S_PARTY_FMT), party.count());
  gfx->setCursor(CX - (int)strlen(l) * 6, 338);
  gfx->print(l);
}

// Page 2: the medals. They used to sit on the creature's card; they belong with
// the player, since totalMedals accumulates across every pet you raise.
static void renderPlayerMedals() {
  int got = 0;
  for (int i = 0; i < MED_COUNT; i++)
    if (pet.hasMedal(1 << i)) got++;
  char head[24];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), got, MED_COUNT);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(head) * 9, 44);
  gfx->print(head);

  for (int i = 0; i < MED_COUNT; i++) {
    int x = 46 + (i % 2) * 190, y = 96 + (i / 2) * 58;
    bool g = pet.hasMedal(1 << i);
    gfx->fillRoundRect(x, y, 180, 48, 10, g ? UI_BAR_OK : UI_TRACK);
    gfx->drawRoundRect(x, y, 180, 48, 10, UI_INK);
    gfx->setTextColor(g ? UI_BG_DAY : UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(x + (180 - (int)strlen(medalLabel(i)) * 12) / 2, y + 6);
    gfx->print(medalLabel(i));
    gfx->setTextSize(1);
    gfx->setCursor(x + (180 - (int)strlen(medalDesc(i)) * 6) / 2, y + 30);
    gfx->print(medalDesc(i));
  }
  char tot[28];
  snprintf(tot, sizeof(tot), T(S_MEDALS_TOTAL_FMT), pet.totalMedals);
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(tot) * 3, 344);
  gfx->print(tot);
}

void renderPlayer() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (playerPage < GYM_REGIONS) renderPlayerBadges();
  else renderPlayerMedals();

  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_BACK)) * 6, 392);
  gfx->print(T(S_BACK));
  uiChrome();
  gfx->flush();
}

// ---------- the avatar picker (v3.38) ----------
// Every trainer class is playable now, which is too many to step through by
// tapping the avatar. The grid's geometry is answered by functions, so a test
// taps where the firmware draws rather than re-typing the numbers.
uint8_t avatarPages() { return (AVATAR_COUNT + AV_PER_PAGE - 1) / AV_PER_PAGE; }

// The top-left corner of grid cell `slot` (0..AV_PER_PAGE-1) on any page.
void avatarCellPos(uint8_t slot, int *x, int *y) {
  *x = CX - AV_COLS * AV_CELL / 2 + (slot % AV_COLS) * AV_CELL;
  *y = AV_GRID_Y + (slot / AV_COLS) * AV_CELL;
}

// Which avatar is under (x, y) on the current page, or -1.
int avatarAt(int16_t x, int16_t y) {
  for (uint8_t slot = 0; slot < AV_PER_PAGE; slot++) {
    int i = avatarPage * AV_PER_PAGE + slot;
    if (i >= AVATAR_COUNT) break;
    int cx, cy;
    avatarCellPos(slot, &cx, &cy);
    if (x >= cx && x < cx + AV_CELL && y >= cy && y < cy + AV_CELL) return i;
  }
  return -1;
}

void avatarPickTap(int16_t x, int16_t y) {
  int i = avatarAt(x, y);
  if (i < 0) return;                 // between cells or past the last: nothing
  pet.avatar = (uint8_t)i;
  pet.saveNow();
  avatarPickOpen = false;            // back to the card, wearing it
  sfxPlay(SFX_TAP);
}

void renderAvatarPick() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(T(S_AVATAR_PICK)) * 6, 62);
  gfx->print(T(S_AVATAR_PICK));
  for (uint8_t slot = 0; slot < AV_PER_PAGE; slot++) {
    int i = avatarPage * AV_PER_PAGE + slot;
    if (i >= AVATAR_COUNT) break;
    int cx, cy;
    avatarCellPos(slot, &cx, &cy);
    if (i == pet.avatar % AVATAR_COUNT) {   // the one you are wearing
      gfx->fillRoundRect(cx + 3, cy + 3, AV_CELL - 6, AV_CELL - 6, 10, UI_TRACK);
      gfx->drawRoundRect(cx + 3, cy + 3, AV_CELL - 6, AV_CELL - 6, 10, UI_INK);
    }
    drawAvatar((uint8_t)i, cx + (AV_CELL - AVATAR_PX * 4) / 2, cy + 6, 4);
    const char *an = AVATARS[i].name;
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(1);
    gfx->setCursor(cx + AV_CELL / 2 - (int)strlen(an) * 3, cy + 76);
    gfx->print(an);
  }
  uiChrome();
  gfx->flush();
}

// ---------- the blackout (v3.38) ----------
// Losing a gym or wild fight used to end on one more line of battle text. Now
// the screen goes dark, as it does in the games, and Mom is waiting at home.
// Nothing is taken from the player -- no money, no training; it is a moment,
// not a penalty.
#define BLACKOUT_MIN_MS 700   // a quick double tap on the last message must not skip her

void blackoutDismiss() {
  if (millis() - blackoutAt < BLACKOUT_MIN_MS) return;
  blackoutOpen = false;
  uiTileGo(TILE_PET);                // you wake up at home
}

void renderBlackout() {
  gfx->fillScreen(RGB565_BLACK);
  char l[48];
  const char *tn = pet.trainerName[0] ? pet.trainerName : T(S_TRAINER);
  snprintf(l, sizeof(l), T(S_BLACKOUT_FMT), tn);
  gfx->setTextColor(UI_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(l) * 6, 76);
  gfx->print(l);
  gfx->setTextColor(UI_TRACK);
  gfx->setCursor(CX - (int)strlen(T(S_BLACKOUT_SUB)) * 6, 106);
  gfx->print(T(S_BLACKOUT_SUB));
  // Home: a warm card with Mom on it.
  gfx->fillRoundRect(CX - 160, 146, 320, 226, 24, UI_BG_DAY);
  drawArt(NPC_ART[NPC_MOM], CX - AVATAR_PX * 3, 160, 6);
  gfx->setTextColor(UI_INK);
  gfx->setCursor(CX - (int)strlen(T(S_MOM_1)) * 6, 280);
  gfx->print(T(S_MOM_1));
  gfx->setCursor(CX - (int)strlen(T(S_MOM_2)) * 6, 310);
  gfx->print(T(S_MOM_2));
  gfx->setTextColor(UI_TRACK);
  gfx->setCursor(CX - (int)strlen(T(S_BACK)) * 6, 392);
  gfx->print(T(S_BACK));
  gfx->flush();
}

// ---------- reaction test (trains SPEED) ----------
// The third training verb. The bag is a masher and the ball is a juggler, so
// this one is a reaction: a target appears somewhere on the panel and you tap
// it before it expires. The window shrinks as you go, which is what makes it
// read as speed rather than as endurance.
#define SPD_MS 15000UL       // session length
#define SPD_LIFE0 1100       // first target's window, ms
#define SPD_LIFE_MIN 380
#define SPD_R 46             // target radius

void spdSpawn() {
  // Keep the whole target inside the bezel: pick an angle and a radius that
  // leave SPD_R of margin, rather than a square that clips at the corners.
  int ang = random(360);
  int rad = random(150);
  float a = ang * 3.14159f / 180.0f;
  spdX = CX + (int)(cosf(a) * rad);
  spdY = CY + (int)(sinf(a) * rad);
  spdBorn = millis();
}

void startSpeedGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  spdOpen = true;
  spdUntil = millis() + SPD_MS;
  spdOverUntil = 0;
  spdHits = 0;
  spdMisses = 0;
  spdGain = 0;
  spdNewHi = false;
  spdSpawn();
}

static uint16_t spdLife() {
  int life = SPD_LIFE0 - spdHits * 32;
  return life < SPD_LIFE_MIN ? SPD_LIFE_MIN : life;
}

void spdTap(int16_t x, int16_t y) {
  if (spdOverUntil) return;
  int dx = x - spdX, dy = y - spdY;
  if (dx * dx + dy * dy <= (SPD_R + 14) * (SPD_R + 14)) {
    spdHits++;
    sfxPlay(SFX_TAP);
    spdSpawn();
  }
}

void renderSpeed() {
  uint32_t now = millis();
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  if (spdOverUntil) {
    if (now > spdOverUntil) { spdOpen = false; return; }
    char b[24];
    snprintf(b, sizeof(b), T(S_SCORE_FMT), spdHits);
    gfx->setTextColor(ink);
    gfx->setTextSize(4);
    gfx->setCursor(CX - strlen(b) * 12, 150);
    gfx->print(b);
    char g[32];
    if (!spdGain && pet.trSpe >= pet.trMaxSpe())        // +0 at the ceiling says why
      snprintf(g, sizeof(g), "%s %s", T(S_TR_SPE), T(S_ST_MAX));
    else snprintf(g, sizeof(g), T(S_SPD_GAIN_FMT), spdGain);
    gfx->setTextColor(UI_BAR_WARN);
    gfx->setTextSize(3);
    gfx->setCursor(CX - strlen(g) * 9, 210);
    gfx->print(g);
    gfx->setTextSize(2);
    if (spdNewHi && spdHits > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      gfx->setCursor(CX - strlen(T(S_NEW_RECORD)) * 6, 256);
      gfx->print(T(S_NEW_RECORD));
    } else {
      char r[20];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.spdHi);
      gfx->setTextColor(ink);
      gfx->setCursor(CX - strlen(r) * 6, 256);
      gfx->print(r);
    }
    gfx->flush();
    return;
  }

  // session over?
  if (now >= spdUntil) {
    spdNewHi = (spdHits > pet.spdHi);
    spdGain = pet.trainSpeed(spdHits);
    sfxPlay(spdNewHi ? SFX_MEDAL : SFX_PLAY);
    spdOverUntil = now + 3500;
    gfx->flush();
    return;
  }
  // target expired?
  if (now - spdBorn > spdLife()) {
    spdMisses++;
    spdSpawn();
  }

  // the target, shrinking as its window runs out so the urgency is visible
  uint32_t age = now - spdBorn;
  int life = spdLife();
  int r = SPD_R - (int)((uint32_t)SPD_R * age / (life ? life : 1) / 2);
  if (r < 8) r = 8;
  gfx->fillCircle(spdX, spdY, r, UI_BAR_BAD);
  gfx->fillCircle(spdX, spdY, r * 2 / 3, UI_WHITE);
  gfx->fillCircle(spdX, spdY, r / 3, UI_BAR_BAD);

  char b[12];
  snprintf(b, sizeof(b), "%u", spdHits);
  gfx->setTextColor(ink);
  gfx->setTextSize(4);
  gfx->setCursor(CX - strlen(b) * 12, 30);
  gfx->print(b);
  // seconds left
  uint32_t left = (spdUntil > now) ? (spdUntil - now + 999) / 1000 : 0;
  snprintf(b, sizeof(b), "%us", (unsigned)left);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(b) * 6, 76);
  gfx->print(b);
  gfx->flush();
}

// ---------- team select ----------
// Which creatures come to this fight. It exists because hard mode caps your
// team to the leader's size, so the difference between a sweep and a wipe is
// bringing the right type -- and the squad used to be simply whoever sat first
// in the party.

// candidate n: 0 = the live pet, 1..PARTY_SLOTS = banked members
bool pickExists(uint8_t n) {
  if (n == 0) return !pet.isEgg();
  return n <= PARTY_SLOTS && !party.slots[n - 1].empty();
}
uint8_t pickChosen() {
  uint8_t c = 0;
  for (uint8_t n = 0; n <= PARTY_SLOTS; n++)
    if (pickExists(n) && (squadMask & (1 << n))) c++;
  return c;
}
uint8_t pickCandidates() {
  uint8_t c = 0;
  for (uint8_t n = 0; n <= PARTY_SLOTS; n++)
    if (pickExists(n)) c++;
  return c;
}
// Trims the selection to the first `cap` candidates. The default used to be
// "everything", which with a live pet plus six banked is seven against a cap of
// six -- so the screen opened already invalid.
void pickDefault(uint8_t cap) {
  squadMask = 0;
  uint8_t taken = 0;
  for (uint8_t n = 0; n <= PARTY_SLOTS && taken < cap; n++)
    if (pickExists(n)) { squadMask |= (1 << n); taken++; }
}

static void drawPickCell(uint8_t n, int x, int y, uint8_t capLvl) {
  bool on = (squadMask & (1 << n)) != 0;
  int16_t dex; uint16_t lvl; const char *nm; bool shiny;
  if (n == 0) {
    dex = pet.speciesId; lvl = pet.level(); shiny = pet.shiny;
    nm = pet.nick[0] ? pet.nick : DEX_TBL[dex].name;
  } else {
    const PartyMon &m = party.slots[n - 1];
    dex = m.dex; lvl = m.level; shiny = m.shiny;
    nm = m.nick[0] ? m.nick : DEX_TBL[dex].name;
  }
  if (capLvl && lvl > capLvl) lvl = capLvl;   // show the level it will FIGHT at
  gfx->fillRoundRect(x, y, PICK_CELL_W, PICK_CELL_H, 10, on ? UI_BG_DAY : UI_TRACK);
  gfx->drawRoundRect(x, y, PICK_CELL_W, PICK_CELL_H, 10, on ? UI_INK : 0x8410);
  const uint8_t *th = thumbs.get(dex);
  if (th) drawThumb(th, x - 12, y - 6, 2, !on);
  gfx->setTextColor(on ? UI_INK : 0x8410);
  gfx->setTextSize(1);
  gfx->setCursor(x + 54, y + 14);
  gfx->print(nm);
  char l[16];
  snprintf(l, sizeof(l), "Lv.%u%s", (unsigned)lvl, shiny ? " *" : "");
  gfx->setCursor(x + 54, y + 30);
  gfx->print(l);
  // its typing is the whole reason you are on this screen
  const DexEntry &d = DEX_TBL[dex];
  gfx->setTextColor(on ? d.accent : 0x8410);
  gfx->setCursor(x + 54, y + 48);
  gfx->print(typeName(d.type1));
  if (d.type2 != T_NONE) {
    gfx->setCursor(x + 54, y + 60);
    gfx->print(typeName(d.type2));
  }
  if (on) {
    gfx->fillCircle(x + PICK_CELL_W - 16, y + 16, 9, UI_BAR_OK);
    gfx->setTextColor(UI_BG_DAY);
    gfx->setCursor(x + PICK_CELL_W - 19, y + 13);
    gfx->print("*");
  }
}

void renderPick() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  uint8_t cap = squadCap(pickTrainer, pickHard);
  uint8_t top = 0;          // the level cap shown on each cell; 0 = uncapped
  char head[40];
  if (pickTrainer == PICK_LAN) {
    // "HOST BATTLE" / "JOIN BATTLE" already say what this is; no "LAN: " prefix.
    snprintf(head, sizeof(head), "%s", lanWantHost ? T(S_LAN_HOST) : T(S_LAN_JOIN));
  } else {
    const Trainer &t = PICK_TRAINERS[pickTrainer];
    for (int k = 0; k < t.count; k++)
      if (t.team[k].level > top) top = t.team[k].level;
    snprintf(head, sizeof(head), "%s  Lv.%u x%u", t.name, top, t.count);
  }
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(head) * 6, 44);
  gfx->print(head);
  char sub[28];
  snprintf(sub, sizeof(sub), T(S_PICK_FMT), pickChosen(), cap);
  gfx->setTextSize(1);
  gfx->setTextColor(pickChosen() > cap ? UI_BAR_BAD : UI_TRACK);
  gfx->setCursor(CX - (int)strlen(sub) * 3, 68);
  gfx->print(sub);

  uint8_t seen = 0, drawn = 0;
  for (uint8_t n = 0; n <= PARTY_SLOTS; n++) {
    if (!pickExists(n)) continue;
    if (seen++ < pickPage * PICK_PER_PAGE) continue;
    if (drawn >= PICK_PER_PAGE) break;
    drawPickCell(n, PICK_X(drawn), PICK_Y(drawn), top);
    drawn++;
  }
  uint8_t pages = (pickCandidates() + PICK_PER_PAGE - 1) / PICK_PER_PAGE;
  if (!pages) pages = 1;
  for (uint8_t i = 0; i < pages && pages > 1; i++) {
    int dx = CX - (pages - 1) * 13 + i * 26;
    if (i == pickPage) gfx->fillCircle(dx, 332, 5, UI_INK);
    else gfx->drawCircle(dx, 332, 4, UI_INK);
  }

  bool ok = pickChosen() > 0 && pickChosen() <= cap;
  gfx->fillRoundRect(PICK_BACK_X, PICK_GO_Y, PICK_BTN_W, PICK_BTN_H, 12, UI_TRACK);
  gfx->drawRoundRect(PICK_BACK_X, PICK_GO_Y, PICK_BTN_W, PICK_BTN_H, 12, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(PICK_BACK_X + (PICK_BTN_W - (int)strlen(T(S_BACK)) * 12) / 2,
                 PICK_GO_Y + 14);
  gfx->print(T(S_BACK));
  gfx->fillRoundRect(PICK_GO_X, PICK_GO_Y, PICK_BTN_W, PICK_BTN_H, 12,
                     ok ? UI_BAR_OK : UI_TRACK);
  gfx->drawRoundRect(PICK_GO_X, PICK_GO_Y, PICK_BTN_W, PICK_BTN_H, 12, UI_INK);
  gfx->setTextColor(ok ? UI_BG_DAY : 0x8410);
  gfx->setTextSize(2);
  gfx->setCursor(PICK_GO_X + (PICK_BTN_W - (int)strlen(T(S_FIGHT)) * 12) / 2,
                 PICK_GO_Y + 14);
  gfx->print(T(S_FIGHT));
  gfx->flush();
}

void pickTap(int16_t x, int16_t y) {
  if (y >= PICK_GO_Y && y <= PICK_GO_Y + PICK_BTN_H &&
      x >= PICK_BACK_X && x <= PICK_BACK_X + PICK_BTN_W) {   // BACK
    sfxPlay(SFX_TAP);
    pickOpen = false;
    if (pickTrainer == PICK_LAN) { lanOpen = true; }
    else { gymOpen = true; }
    return;
  }
  if (y >= PICK_GO_Y && y <= PICK_GO_Y + PICK_BTN_H &&
      x >= PICK_GO_X && x <= PICK_GO_X + PICK_BTN_W) {
    uint8_t cap = squadCap(pickTrainer, pickHard);
    if (pickChosen() == 0 || pickChosen() > cap) return;   // GO stays inert
    sfxPlay(SFX_TAP);
    pickOpen = false;
    if (pickTrainer == PICK_LAN) {
      // The squad is chosen BEFORE the radio comes up, so what gets offered to
      // the peer is what the player picked -- lanOffer() builds lan.mine from
      // squadMask, and the fight is then rebuilt from lan.mine rather than from
      // the party (see startLinkBattle).
      lanOffer(lanWantHost);
      lanOpen = true;
      return;
    }
    // The fight has to be the one the picker just showed you. startTrainerBattle()
    // snapshots gymRegion into btlRegion, so handing the picker's own region back
    // to the chooser here is what keeps the cap you were shown, the leader you
    // were shown and the ladder the badge lands on all the same region -- without
    // giving the function a second source of truth to disagree with.
    gymRegion = pickRegion;
    startTrainerBattle(pickTrainer, pickHard);
    return;
  }
  uint8_t seen = 0, drawn = 0;
  for (uint8_t n = 0; n <= PARTY_SLOTS; n++) {
    if (!pickExists(n)) continue;
    if (seen++ < pickPage * PICK_PER_PAGE) continue;
    if (drawn >= PICK_PER_PAGE) break;
    int cx0 = PICK_X(drawn), cy0 = PICK_Y(drawn);
    drawn++;
    if (x < cx0 || x > cx0 + PICK_CELL_W || y < cy0 || y > cy0 + PICK_CELL_H) continue;
    squadMask ^= (1 << n);
    sfxPlay(SFX_TAP);
    return;
  }
}

// ---------- LAN battle ----------
// Pairing on a touch-only screen: one device hosts, the other joins, and the
// protocol does the rest. There is no MAC entry because there is no keyboard
// worth typing one on.
void renderLan() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_LAN)) * 6, 44);
  gfx->print(T(S_LAN));

  const char *msg = T(S_LAN_PICK);
  switch (lan.state) {
    case LINK_HANDSHAKE:
    case LINK_LISTENING: msg = T(S_LAN_WAIT); break;
    case LINK_SQUADS:    msg = T(S_LAN_WAIT); break;
    case LINK_READY:     msg = T(S_LAN_READY); break;
    case LINK_REFUSED:   msg = T(S_LAN_REFUSED); break;
    case LINK_LOST:      msg = T(S_LAN_GONE); break;
    case LINK_DONE:      msg = lan.youWon ? T(S_BTL_WIN) : T(S_BTL_LOSE); break;
    default: break;
  }
  gfx->setTextColor((lan.state == LINK_REFUSED || lan.state == LINK_LOST)
                      ? UI_BAR_BAD : UI_TRACK);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(msg) * 3, 76);
  gfx->print(msg);

  if (lan.state == LINK_OFF || lan.state == LINK_REFUSED ||
      lan.state == LINK_LOST) {
    const char *lab[2] = { T(S_LAN_HOST), T(S_LAN_JOIN) };
    for (int i = 0; i < 2; i++) {
      int y = 120 + i * 70;
      gfx->fillRoundRect(90, y, 286, 56, 12, UI_BG_DAY);
      gfx->drawRoundRect(90, y, 286, 56, 12, UI_INK);
      gfx->setTextColor(UI_INK);
      gfx->setTextSize(2);
      gfx->setCursor(CX - (int)strlen(lab[i]) * 6, y + 20);
      gfx->print(lab[i]);
    }
    // Trading shares the radio and the room, so it lives here, under the two
    // ways to fight. Taller than its neighbours: new controls are finger-sized.
    gfx->fillRoundRect(LANTRADE_X, LANTRADE_Y, LANTRADE_W, LANTRADE_H, 12, UI_BAR_WARN);
    gfx->drawRoundRect(LANTRADE_X, LANTRADE_Y, LANTRADE_W, LANTRADE_H, 12, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(T(S_TRADE)) * 6, LANTRADE_Y + LANTRADE_H / 2 - 8);
    gfx->print(T(S_TRADE));
  } else if (lan.state == LINK_READY) {
    char l[40];
    if (lan.peerName[0]) {
      gfx->setTextColor(UI_INK);
      gfx->setTextSize(2);
      gfx->setCursor(CX - (int)strlen(lan.peerName) * 6, 130);
      gfx->print(lan.peerName);
    }
    snprintf(l, sizeof(l), T(S_LAN_VS), lan.theirsN);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(l) * 6, 150);
    gfx->print(l);

    // Remembered by the peer's MAC (recordRivalResult()), not by name -- a
    // rival who renames themselves between fights keeps their score.
    {
      const uint8_t *rivalMac = linkNowPeerMac();
      const RivalRecord *riv = rivalMac ? pet.findRival(rivalMac) : nullptr;
      uint16_t wins = riv ? riv->wins : 0, losses = riv ? riv->losses : 0;
      gfx->setTextSize(2);
      snprintf(l, sizeof(l), T(wins == 1 ? S_LAN_DEFEATED_1 : S_LAN_DEFEATED_N), wins);
      gfx->setTextColor(UI_BAR_OK);
      gfx->setCursor(CX - (int)strlen(l) * 6, 170);
      gfx->print(l);
      snprintf(l, sizeof(l), T(losses == 1 ? S_LAN_LOST_1 : S_LAN_LOST_N), losses);
      gfx->setTextColor(UI_BAR_BAD);
      gfx->setCursor(CX - (int)strlen(l) * 6, 190);
      gfx->print(l);
    }

    gfx->fillRoundRect(120, 220, 226, 56, 12, UI_BAR_OK);
    gfx->drawRoundRect(120, 220, 226, 56, 12, UI_INK);
    gfx->setTextColor(UI_BG_DAY);
    gfx->setCursor(CX - strlen(T(S_FIGHT)) * 6, 240);
    gfx->print(T(S_FIGHT));
  } else if (lan.state == LINK_DONE) {
    // Both squads are still in hand on both devices, so going again costs one
    // packet -- there is nothing to re-exchange.
    if (lan.peerName[0]) {
      gfx->setTextColor(UI_INK);
      gfx->setTextSize(2);
      gfx->setCursor(CX - (int)strlen(lan.peerName) * 6, 140);
      gfx->print(lan.peerName);
    }
    gfx->fillRoundRect(120, 220, 226, 56, 12, UI_BG_DAY);
    gfx->drawRoundRect(120, 220, 226, 56, 12, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(T(S_LAN_REMATCH)) * 6, 240);
    gfx->print(T(S_LAN_REMATCH));
  }
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_BACK)) * 6, 392);
  gfx->print(T(S_BACK));
  gfx->flush();
}

// Hands our chosen squad to the link, then announces.
// Leaving deliberately: tell the peer so it reports at once instead of sitting
// out the timeout, then free the radio -- it costs real current.
void lanLeave() {
  if (lan.live()) lan.sendBye();
  linkNowEnd();
  lan.state = LINK_OFF;
}

static void lanOffer(bool host) {
  // The squad is built BEFORE the radio is touched. What we advertise has to be
  // exactly what the player just chose in the picker, and that does not depend
  // on whether the radio comes up -- doing it the other way round meant a
  // failed radio skipped the squad entirely and left nothing to inspect.
  lan.begin(host, pet.trainerName);
  buildSquad(0, TRAINER_TEAM_MAX, squadMask);
  for (uint8_t i = 0; i < btlSquadN; i++) {
    LinkMon m;
    linkMonFrom(m, btlSquad[i]);
    lan.addMon(m);
  }
  if (!linkNowBegin(&lan)) {          // no radio: say so rather than hanging
    lan.state = LINK_REFUSED;
    return;
  }
  // BOTH sides announce. Which of them ends up hosting is settled by id inside
  // the hello, so the buttons are only a preference -- two players who both tap
  // HOST still get a working fight instead of two authorities, and two who both
  // tap JOIN still get one instead of mutual silence.
  lan.start();
}

bool lanTradeHit(int16_t x, int16_t y) {
  return x >= LANTRADE_X && x <= LANTRADE_X + LANTRADE_W &&
         y >= LANTRADE_Y && y <= LANTRADE_Y + LANTRADE_H;
}

void lanTap(int16_t x, int16_t y) {
  if (lan.state == LINK_OFF || lan.state == LINK_REFUSED ||
      lan.state == LINK_LOST) {
    if (lanTradeHit(x, y)) {
      sfxPlay(SFX_TAP);
      lanLeave();            // the battle link lets the radio go first
      lanOpen = false;
      tradeOpenScreen();
      return;
    }
    for (int i = 0; i < 2; i++) {
      int by = 120 + i * 70;
      if (x < 90 || x > 376 || y < by || y > by + 56) continue;
      sfxPlay(SFX_TAP);
      lanWantHost = (i == 0);
      lanOpen = false;
      pickTrainer = PICK_LAN;
      pickHard = false;
      pickRegion = gymRegion;   // no ladder in a LAN fight; kept in step anyway
      pickPage = 0;
      pickDefault(squadCap(PICK_LAN, false));
      pickOpen = true;
      return;
    }
  } else if (lan.state == LINK_READY) {
    if (x >= 120 && x <= 346 && y >= 220 && y <= 276) {
      sfxPlay(SFX_TAP);
      lanOpen = false;
      startLinkBattle();
      return;
    }
  } else if (lan.state == LINK_DONE) {
    if (x >= 120 && x <= 346 && y >= 220 && y <= 276) {
      sfxPlay(SFX_TAP);
      lan.sendRematch();      // both sides go back to READY and tap FIGHT
      return;
    }
  }
  if (y > 370) { lanLeave(); lanOpen = false; }   // back
}

// ---------- trading ----------
// The protocol is trade.cpp's, and every decision about a creature's fate is
// made there; this is the board it runs on (party + box + one NVS key) and the
// screen. Reached from the LAN screen's TRADE button.
//
// The one screen rule that matters: while a trade is PENDING the player cannot
// leave. The owner's design has both players stuck on this screen until the
// trade settles or they decide, because each screen has to say what ITS board
// holds -- that is what the other player reads to choose KEEP or REVERT -- and
// a reboot with a trade pending lands straight back here (tradeBoot()).

static Preferences tradePrefs;
static bool tradePrefsOpen = false;
static bool tradePrefsReady() {
  if (!tradePrefsOpen) tradePrefsOpen = tradePrefs.begin("tamapoke", false);
  return tradePrefsOpen;
}

// The firmware's TradeBoard: the party and the box ONLY, never the live pet.
// Slots 0..5 are the party, 6.. the box.
struct FwTradeBoard : TradeBoard {
  static PartyMon &slotAt(int w) {
    return w < PARTY_SLOTS ? party.slots[w] : party.box[w - PARTY_SLOTS];
  }
  static int where(const CreatureUid &u) {
    if (!u.valid()) return -1;
    for (int w = 0; w < PARTY_SLOTS + BOX_SLOTS; w++)
      if (!slotAt(w).empty() && slotAt(w).uid == u) return w;
    return -1;
  }
  // Writes one slot and says whether it LANDED. On a failed write the RAM copy
  // is put back: the protocol retries a commit by looking for the creature, so
  // a RAM slot claiming what NVS does not hold would end the retry early and
  // a reboot would then lose the trade.
  static bool put(int w, const PartyMon &in) {
    PartyMon &slot = slotAt(w);
    PartyMon was = slot;
    slot = in;
    bool ok = w < PARTY_SLOTS ? party.save() : party.boxSave();
    if (!ok) slot = was;
    return ok;
  }
  void mac(uint8_t out[6]) override {
    uint64_t m = ESP.getEfuseMac();   // the same bytes Pet::mintUid() stamps
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)(m >> (8 * i));
  }
  uint32_t epoch() override { return rtcEpoch(); }
  bool findMon(const CreatureUid &u, PartyMon *out) override {
    int w = where(u);
    if (w < 0) return false;
    if (out) *out = slotAt(w);
    return true;
  }
  bool replaceMon(const CreatureUid &out, const PartyMon &in) override {
    int w = where(out);
    return w >= 0 && put(w, in);
  }
  bool storeNew(const PartyMon &in) override {
    int i = party.firstFree();
    if (i >= 0) return put(i, in);
    i = party.boxFirstFree();
    return i >= 0 && put(PARTY_SLOTS + i, in);
  }
  bool saveJournal(const uint8_t *buf, size_t n) override {
    return tradePrefsReady() && tradePrefs.putBytes("trade", buf, n) == n;
  }
  size_t loadJournal(uint8_t *buf, size_t cap) override {
    if (!tradePrefsReady() || !tradePrefs.isKey("trade")) return 0;
    size_t n = tradePrefs.getBytesLength("trade");
    if (!n || n > cap) return 0;
    return tradePrefs.getBytes("trade", buf, n);
  }
  // Never agree to a creature this board could only draw as a bare number --
  // the same rule that keeps a region without its pack out of the egg pool.
  bool canReceive(const PartyMon &m) override {
    return speciesHasArt(m.dex) && regionAvailable(regionOfDex(m.dex));
  }
};
static FwTradeBoard tradeBoard;

static void tradeRx(void *ctx, const uint8_t *buf, uint8_t len) {
  static_cast<Trade *>(ctx)->onPacket(buf, len);
}

// Is this creature promised to a trade? Asked by every path that could
// release it, raise it or overwrite its slot.
bool tradeLocked(const PartyMon &m) { return !m.empty() && trade.locks(m.uid); }

bool tradePending() { return trade.j.active == TRADE_PENDING; }

// A fresh session: a new nonce (so the same pair traded twice is two trades),
// the radio up, and an announcement. A trade still pending from before starts
// calling its peer again; see Trade::start().
void tradeOpenScreen() {
  trade.begin(&tradeBoard, pet.trainerName, (uint32_t)random(1, 0x7FFFFFFF));
  trade.send = linkNowSend;
  trade.ctx = nullptr;
  tradeOpen = true;
  tradePicking = false;
  tradeAsk = 0;
  tradeNoRadio = !linkNowOpen(tradeRx, &trade, TM_FIRST, TM_LAST, false);
  // Started even without a radio: a pending trade must still count down to
  // the player's decision, and a fresh one simply finds nobody.
  trade.start();
}

// Boot: finish a commit the power interrupted (Trade::begin() does), and if a
// trade is still pending put the player straight back on its screen.
void tradeBoot() {
  trade.begin(&tradeBoard, pet.trainerName, 1);
  if (tradePending()) {
    Serial.println("trade: a trade is pending -- reopening the trade screen");
    tradeOpenScreen();
  }
}

void tradeLeave() {
  if (tradePending()) { sfxPlay(SFX_DENY); return; }   // stuck until it settles or they decide
  trade.end();
  linkNowEnd();
  tradeOpen = false;
  tradePicking = false;
  tradeAsk = 0;
}

void tradePoll(uint32_t now) {
  if (!tradeOpen) return;
  linkNowPoll();
  trade.tick(now);
  if (!tradePending()) tradeAsk = 0;   // settled by a reconnect while asking
}

static const char *monName(const PartyMon &m) {
  return m.nick[0] ? m.nick : DEX_TBL[m.dex].name;
}

static void tradeText(const char *s, int y, int size, uint16_t col) {
  gfx->setTextColor(col);
  gfx->setTextSize(size);
  gfx->setCursor(CX - (int)strlen(s) * 3 * size, y);
  gfx->print(s);
}

// A creature: its thumbnail at scale s centred on (cx, cy), name and level
// under it. A thumbnail is up to 32 px a side, so it reaches 16*s below the centre.
static void tradeMon(const PartyMon &m, int cx, int cy, int s) {
  if (m.empty()) return;
  const uint8_t *th = thumbs.get(m.dex);
  // drawThumb() centres the sprite in a box thumbCell() wide.
  if (th) drawThumb(th, cx - thumbCell() / 2, cy - thumbCell() / 2, s, false);
  const char *nm = monName(m);
  gfx->setTextColor(DEX_TBL[m.dex].accent);
  gfx->setTextSize(1);
  const int below = cy + 16 * s + 4;
  gfx->setCursor(cx - (int)strlen(nm) * 3, below);
  gfx->print(nm);
  char lv[12];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)m.level);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(cx - (int)strlen(lv) * 6, below + 12);
  gfx->print(lv);
}

// "Trade appears ... (you have X)" -- this board's own account, in the owner's
// words. It is what the OTHER player reads to decide.
static void tradeStatusLines(const TradeStatus &st, int y) {
  if (st.view == TV_NONE) return;
  tradeText(T(st.view == TV_INCOMPLETE ? S_TRADE_INCOMPLETE : S_TRADE_UNCONF), y, 1, UI_INK);
  if (st.have.empty()) return;
  char l[40];
  snprintf(l, sizeof(l), T(S_TRADE_YOUHAVE), monName(st.have));
  tradeText(l, y + 14, 2, UI_INK);
}

static void tradeButton(int x, int y, int w, int h, const char *lab, uint16_t fill, uint16_t ink) {
  gfx->fillRoundRect(x, y, w, h, 14, fill);
  gfx->drawRoundRect(x, y, w, h, 14, UI_INK);
  gfx->setTextColor(ink);
  gfx->setTextSize(2);
  gfx->setCursor(x + w / 2 - (int)strlen(lab) * 6, y + h / 2 - 8);
  gfx->print(lab);
}

// The layout, as functions of nothing, so the renderer and the tap handler
// read the same numbers. Every control is UI_TAP_FINGER-sized or larger.
#define TR_CARD_Y 92
#define TR_CARD_H 160
#define TR_GIVE_X 56
#define TR_GET_X 240
#define TR_CARD_W 170
#define TR_BTN_X 118
#define TR_BTN_Y 258
#define TR_BTN_W 230
#define TR_BTN_H UI_TAP_FINGER
#define TR_BACK_LABEL_Y 396
#define TR_BACK_ZONE_Y 384   // a 30 px dead band under the button, see CLAUDE.md § LAN BATTLE
static_assert(TR_BTN_Y + TR_BTN_H + 30 <= TR_BACK_ZONE_Y, "a back zone flush under a button is how LAN BATTLE misfired");
static_assert(TR_CARD_H >= UI_TAP_FINGER, "trade controls are finger-sized");
// The decision screen's two answers.
#define TR_DEC_Y 150
#define TR_DEC_H 170

static bool inRect(int16_t x, int16_t y, int rx, int ry, int rw, int rh) {
  return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}
bool tradeGiveHit(int16_t x, int16_t y) { return inRect(x, y, TR_GIVE_X, TR_CARD_Y, TR_CARD_W, TR_CARD_H); }
bool tradeAcceptHit(int16_t x, int16_t y) { return inRect(x, y, TR_BTN_X, TR_BTN_Y, TR_BTN_W, TR_BTN_H); }
bool tradeRevertHit(int16_t x, int16_t y) { return inRect(x, y, TR_GIVE_X, TR_DEC_Y, TR_CARD_W, TR_DEC_H); }
bool tradeKeepHit(int16_t x, int16_t y) { return inRect(x, y, TR_GET_X, TR_DEC_Y, TR_CARD_W, TR_DEC_H); }
bool tradeCloseHit(int16_t x, int16_t y) { return tradeAcceptHit(x, y); }   // one button slot, two jobs by state
bool tradeBackHit(int16_t x, int16_t y) { (void)x; return y >= TR_BACK_ZONE_Y; }
// The cannot-be-undone confirm: its first button is the answer chosen, the second NO.
bool tradeSureHit(int16_t x, int16_t y) {
  return inRect(x, y, CONFIRM_BTN_X, CONFIRM_B1_Y, CONFIRM_BTN_W, CONFIRM_BTN_H);
}
bool tradeNotSureHit(int16_t x, int16_t y) {
  return inRect(x, y, CONFIRM_BTN_X, CONFIRM_B2_Y, CONFIRM_BTN_W, CONFIRM_BTN_H);
}

static void renderTradePick() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  tradeText(T(S_TRADE_PICK), 46, 2, UI_INK);
  char top[16];
  if (tradePickSrc == 0) snprintf(top, sizeof(top), "%s", T(S_TRADE_PARTYLBL));
  else snprintf(top, sizeof(top), "%s %u", T(S_BOX_BTN), (unsigned)tradePickSrc);
  drawPartyHub(top, T(S_TRADE_HUBHINT), false);
  for (int i = 0; i < PARTY_SLOTS; i++) {
    const PartyMon &m = tradePickSrc == 0 ? party.slots[i]
                                          : party.box[(tradePickSrc - 1) * BOX_PER_PAGE + i];
    drawPartySlot(i, m);
  }
  gfx->fillRoundRect(PARTYCLOSE_X, PARTYCLOSE_Y, PARTYCLOSE_W, PARTYCLOSE_H, 12, UI_TRACK);
  gfx->drawRoundRect(PARTYCLOSE_X, PARTYCLOSE_Y, PARTYCLOSE_W, PARTYCLOSE_H, 12, UI_INK);
  tradeText(T(S_CLOSE), PARTYCLOSE_Y + 14, 2, UI_INK);
  gfx->flush();
}

static void renderTradeDecide(const TradeStatus &st) {
  tradeText(T(S_TRADE_ASK1), 66, 1, UI_BAR_BAD);
  tradeText(T(S_TRADE_ASK2), 82, 1, UI_INK);
  char l[64];
  snprintf(l, sizeof(l), T(S_TRADE_ASK3), T(S_TRADE_KEEP), T(S_TRADE_REVERT));
  tradeText(l, 96, 1, UI_INK);
  tradeStatusLines(st, 114);
  const PartyMon *pick[2] = { &st.have, &st.other };
  const char *lab[2] = { T(S_TRADE_REVERT), T(S_TRADE_KEEP) };
  const char *sub[2] = { T(S_TRADE_REVERT_SUB), T(S_TRADE_KEEP_SUB) };
  for (int i = 0; i < 2; i++) {
    int x = i ? TR_GET_X : TR_GIVE_X;
    gfx->fillRoundRect(x, TR_DEC_Y, TR_CARD_W, TR_DEC_H, 14, UI_WHITE);
    gfx->drawRoundRect(x, TR_DEC_Y, TR_CARD_W, TR_DEC_H, 14, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(x + TR_CARD_W / 2 - (int)strlen(lab[i]) * 6, TR_DEC_Y + 10);
    gfx->print(lab[i]);
    tradeMon(*pick[i], x + TR_CARD_W / 2, TR_DEC_Y + 70, 2);
    if (!pick[i]->empty()) {
      snprintf(l, sizeof(l), sub[i], monName(*pick[i]));
      gfx->setTextColor(UI_INK_SOFT);
      gfx->setTextSize(1);
      gfx->setCursor(x + TR_CARD_W / 2 - (int)strlen(l) * 3, TR_DEC_Y + TR_DEC_H - 16);
      gfx->print(l);
    }
  }
}

void renderTrade() {
  if (tradePicking) { renderTradePick(); return; }
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  tradeText(T(S_TRADE), 40, 2, UI_INK);
  const TradeStatus st = trade.status();
  char l[64];
  // "tap: back" only where no CLOSE button already says the same thing.
  bool backLabel = !tradePending() &&
                   (trade.state == TRADE_CHOOSING || trade.state == TRADE_LISTENING);

  if (tradeAsk) {
    // The warning the owner asked for, in front of the choice it guards.
    const PartyMon &m = tradeAsk == 1 ? st.have : st.other;
    snprintf(l, sizeof(l), T(tradeAsk == 1 ? S_TRADE_REVERT_SUB : S_TRADE_KEEP_SUB), monName(m));
    drawConfirmPanel(T(S_TRADE_SURE), T(S_TRADE_SURE2), l, UI_INK,
                     T(tradeAsk == 1 ? S_TRADE_REVERT : S_TRADE_KEEP), UI_BAR_BAD, UI_WHITE,
                     T(S_NO), UI_TRACK, UI_INK);
    gfx->flush();
    return;
  }

  if (tradePending()) {
    if (st.mustDecide) {
      renderTradeDecide(st);
    } else if (st.connected) {
      tradeText(T(S_TRADE_BUSY), 80, 3, UI_INK);
      tradeMon(st.have, CX - 90, 190, 2);
      tradeMon(st.other, CX + 90, 190, 2);
      tradeText("<->", 190, 2, UI_INK_SOFT);
      tradeText(T(S_TRADE_KEEPON), 320, 1, UI_INK);
    } else {
      // The stuck screen, as the owner specified it.
      tradeText(T(S_TRADE_LOST), 74, 1, UI_BAR_BAD);
      snprintf(l, sizeof(l), T(S_TRADE_LEFT), (unsigned)(st.secsLeft / 60), (unsigned)(st.secsLeft % 60));
      tradeText(l, 92, 3, UI_INK);
      tradeMon(st.have, CX, 190, 3);
      tradeStatusLines(st, 290);
      tradeText(T(S_TRADE_KEEPON), 340, 1, UI_INK_SOFT);
    }
  } else if (trade.state == TRADE_DONE) {
    tradeText(T(S_TRADE_DONE), 72, 3, UI_BAR_OK);
    tradeMon(st.have, CX, 140, 2);
    snprintf(l, sizeof(l), T(S_TRADE_GOT), st.have.empty() ? "?" : monName(st.have));
    tradeText(l, 214, 1, UI_INK);
    if (st.have.flags & PMON_TRADE_EVO) tradeText(T(S_TRADE_EVOHINT), 228, 1, UI_INK_SOFT);
    if (st.view == TV_COMPLETE_UNCONFIRMED) tradeText(T(S_TRADE_UNCONF), 242, 1, UI_INK_SOFT);
    tradeButton(TR_BTN_X, TR_BTN_Y, TR_BTN_W, TR_BTN_H, T(S_CLOSE), UI_TRACK, UI_INK);
  } else if (trade.state == TRADE_CHOOSING) {
    snprintf(l, sizeof(l), T(S_TRADE_WITH), trade.peerName);
    tradeText(l, 64, 2, UI_INK_SOFT);
    // YOU GIVE: the whole card is the control that opens the picker.
    gfx->fillRoundRect(TR_GIVE_X, TR_CARD_Y, TR_CARD_W, TR_CARD_H, 14, UI_WHITE);
    gfx->drawRoundRect(TR_GIVE_X, TR_CARD_Y, TR_CARD_W, TR_CARD_H, 14, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(1);
    gfx->setCursor(TR_GIVE_X + TR_CARD_W / 2 - (int)strlen(T(S_TRADE_GIVE)) * 3, TR_CARD_Y + 8);
    gfx->print(T(S_TRADE_GIVE));
    if (trade.mine.empty()) {
      gfx->setTextSize(1);
      gfx->setCursor(TR_GIVE_X + TR_CARD_W / 2 - (int)strlen(T(S_TRADE_CHOOSE)) * 3, TR_CARD_Y + 74);
      gfx->print(T(S_TRADE_CHOOSE));
    } else {
      tradeMon(trade.mine, TR_GIVE_X + TR_CARD_W / 2, TR_CARD_Y + 70, 2);
    }
    // YOU GET: what they offered, or why it cannot be had.
    gfx->fillRoundRect(TR_GET_X, TR_CARD_Y, TR_CARD_W, TR_CARD_H, 14, UI_BG_DAY);
    gfx->drawRoundRect(TR_GET_X, TR_CARD_Y, TR_CARD_W, TR_CARD_H, 14, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(1);
    gfx->setCursor(TR_GET_X + TR_CARD_W / 2 - (int)strlen(T(S_TRADE_GET)) * 3, TR_CARD_Y + 8);
    gfx->print(T(S_TRADE_GET));
    const char *why = nullptr;
    if (trade.theirsWhy == 1) why = T(S_TRADE_WHY_BAD);
    else if (trade.theirsWhy == 2) why = T(S_TRADE_WHY_PACK);
    else if (trade.theirsWhy == 3) why = T(S_TRADE_WHY_HAVE);
    else if (trade.theirs.empty()) why = T(S_TRADE_WAIT);
    // An invalid offer is never drawn: its name need not even be terminated.
    if (trade.theirsWhy != 1 && !trade.theirs.empty())
      tradeMon(trade.theirs, TR_GET_X + TR_CARD_W / 2, TR_CARD_Y + 70, 2);
    if (why) {
      gfx->setTextColor(trade.theirsWhy ? UI_BAR_BAD : UI_INK_SOFT);
      gfx->setTextSize(1);
      gfx->setCursor(TR_GET_X + TR_CARD_W / 2 - (int)strlen(why) * 3, TR_CARD_Y + TR_CARD_H - 16);
      gfx->print(why);
    } else if (trade.theirAccept && trade.theirAccept == trade.fingerprint()) {
      gfx->setTextColor(UI_BAR_OK);
      gfx->setTextSize(1);
      gfx->setCursor(TR_GET_X + TR_CARD_W / 2 - (int)strlen(T(S_TRADE_THEYOK)) * 3, TR_CARD_Y + TR_CARD_H - 16);
      gfx->print(T(S_TRADE_THEYOK));
    }
    bool ready = !trade.mine.empty() && trade.theirsOk;
    tradeButton(TR_BTN_X, TR_BTN_Y, TR_BTN_W, TR_BTN_H,
                T(trade.iAccepted ? S_TRADE_ACCEPTED : S_TRADE_ACCEPT),
                trade.iAccepted ? UI_BAR_WARN : (ready ? UI_BAR_OK : UI_TRACK),
                ready && !trade.iAccepted ? UI_WHITE : UI_INK);
  } else if (trade.state == TRADE_LISTENING) {
    tradeText(T(S_TRADE_LOOK), 150, 2 , UI_INK);
    tradeText(T(S_TRADE_LOOK2), 190, 1, UI_INK_SOFT);
  } else {
    // Ended with nothing changed here: say why, plainly.
    const char *why = T(S_TRADE_CANCELLED);
    if (tradeNoRadio) why = T(S_TRADE_NORADIO);
    else if (trade.state == TRADE_REFUSED) {
      if (trade.refusal == TREF_VERSION) why = T(S_TRADE_VERSION);
      else if (trade.refusal == TREF_SELF) why = T(S_TRADE_SELF);
      else if (trade.refusal == TREF_LOG_FULL) why = T(S_TRADE_LOGFULL);
    } else if (!trade.havePeer) why = T(S_TRADE_NOBODY);
    tradeText(why, 170, 1, trade.state == TRADE_REFUSED || tradeNoRadio ? UI_BAR_BAD : UI_INK);
    tradeButton(TR_BTN_X, TR_BTN_Y, TR_BTN_W, TR_BTN_H, T(S_CLOSE), UI_TRACK, UI_INK);
  }
  if (backLabel) tradeText(T(S_BACK), TR_BACK_LABEL_Y, 2, UI_INK_SOFT);
  gfx->flush();
}

static void tradePickTap(int16_t x, int16_t y) {
  if (partyHubAt(x, y)) {   // party -> box 1 -> box 2 -> box 3 -> party
    tradePickSrc = (uint8_t)((tradePickSrc + 1) % (1 + BOX_SLOTS / BOX_PER_PAGE));
    sfxPlay(SFX_TAP);
    return;
  }
  if (inRect(x, y, PARTYCLOSE_X, PARTYCLOSE_Y, PARTYCLOSE_W, PARTYCLOSE_H) || y < 34) {
    tradePicking = false;
    sfxPlay(SFX_TAP);
    return;
  }
  int i = partySlotAt(x, y);
  if (i < 0) return;
  const PartyMon &m = tradePickSrc == 0 ? party.slots[i]
                                        : party.box[(tradePickSrc - 1) * BOX_PER_PAGE + i];
  if (m.empty() || !trade.offer(m.uid)) { sfxPlay(SFX_DENY); return; }
  tradePicking = false;
  sfxPlay(SFX_TAP);
}

void tradeTap(int16_t x, int16_t y) {
  if (tradePicking) { tradePickTap(x, y); return; }
  if (tradeAsk) {
    // Modal: anything but its two buttons does nothing.
    if (tradeSureHit(x, y)) {
      bool ok = tradeAsk == 1 ? trade.decideRevert() : trade.decideKeep();
      tradeAsk = 0;
      sfxPlay(ok ? SFX_MEDAL : SFX_DENY);
    } else if (tradeNotSureHit(x, y)) {
      tradeAsk = 0;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  if (tradePending()) {
    if (!trade.canDecide()) return;   // nothing to press while it settles
    if (tradeRevertHit(x, y)) { tradeAsk = 1; sfxPlay(SFX_TAP); }
    else if (tradeKeepHit(x, y)) { tradeAsk = 2; sfxPlay(SFX_TAP); }
    return;
  }
  if (trade.state == TRADE_CHOOSING) {
    if (tradeGiveHit(x, y)) {
      tradePicking = true;
      tradePickSrc = 0;
      sfxPlay(SFX_TAP);
      return;
    }
    if (tradeAcceptHit(x, y)) {
      if (trade.iAccepted) { trade.unaccept(); sfxPlay(SFX_TAP); }
      else if (trade.accept()) sfxPlay(SFX_TAP);
      else sfxPlay(SFX_DENY);
      return;
    }
  } else if (trade.state != TRADE_LISTENING && tradeCloseHit(x, y)) {
    sfxPlay(SFX_TAP);
    tradeLeave();
    return;
  }
  if (tradeBackHit(x, y)) { sfxPlay(SFX_TAP); tradeLeave(); }
}

// ---------- gym list ----------
void renderGyms() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  // The ladder's own region in the title, since a vertical swipe moves between
  // three of them and "GYMS" alone would not say which you are looking at.
  char title[28];
  snprintf(title, sizeof(title), "%s %s", TRAINER_SETS[gymRegion % GYM_REGIONS].region,
           T(S_GYMS));
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(title) * 6, 42);
  gfx->print(title);
  // The badge count used to sit here, directly behind the difficulty pill. The
  // region chooser already shows it per region, which is where you are choosing
  // from, so it was both redundant and in the way.
  // difficulty pill: hard caps YOUR team to the leader's size and level, so it
  // is a different ladder with its own badges rather than a damage multiplier
  const char *dif = T(gymHard ? S_HARD : S_EASY);
  int dw = (int)strlen(dif) * 12 + 48;      // wider as well as taller
  if (dw < 120) dw = 120;
  gfx->fillRoundRect(CX - dw / 2, GYMDIF_Y, dw, GYMDIF_H, 12,
                     gymHard ? UI_BAR_BAD : UI_TRACK);
  gfx->setTextColor(gymHard ? UI_BG_DAY : UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(dif) * 6, GYMDIF_Y + 14);
  gfx->print(dif);

  for (int i = 0; i < GYM_ROWS; i++) {
    uint8_t idx = gymPage * GYM_ROWS + i;
    if (idx >= TRAINER_COUNT) break;
    const Trainer &t = TRAINERS[idx];
    int rx, y, rw, rh;
    gymRowRect(i, &rx, &y, &rw, &rh);
    bool done = pet.hasBadge(gymRegion, idx, gymHard);
    bool open_ = gymUnlocked(idx, gymHard);
    gfx->fillRoundRect(rx, y, rw, rh, 10, done ? UI_TRACK : UI_BG_DAY);
    gfx->drawRoundRect(rx, y, rw, rh, 10, open_ ? UI_INK : UI_TRACK);
    gfx->setTextColor(open_ ? UI_INK : UI_TRACK);
    gfx->setTextSize(2);
    gfx->setCursor(rx + 14, y + 8);
    gfx->print(t.name);
    gfx->setTextSize(1);
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setCursor(rx + 14, y + 28);
    gfx->print(open_ ? t.place : T(S_LOCKED));
    // the level of the strongest creature: the honest measure of the wall
    uint8_t top = 0;
    for (int k = 0; k < t.count; k++)
      if (t.team[k].level > top) top = t.team[k].level;
    char lv[16];
    snprintf(lv, sizeof(lv), "Lv.%u x%u", top, t.count);
    gfx->setTextColor(done ? UI_BAR_OK : (open_ ? UI_INK : UI_TRACK));
    gfx->setCursor(rx + rw - 12 - (int)strlen(lv) * 6, y + 28);
    gfx->print(lv);
    if (done) {
      gfx->setTextColor(UI_BAR_OK);
      gfx->setCursor(rx + rw - 26, y + 8);
      gfx->print("*");
    }
  }
  gfx->fillRoundRect(GYMBTN_X, GYMBTN_Y, GYMBTN_W, GYMBTN_H, 11, UI_BG_DAY);
  gfx->drawRoundRect(GYMBTN_X, GYMBTN_Y, GYMBTN_W, GYMBTN_H, 11, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(GYMBTN_X + (GYMBTN_W - (int)strlen(T(S_LAN)) * 12) / 2, GYMBTN_Y + 14);
  gfx->print(T(S_LAN));
  uiChrome();
  gfx->flush();
}

// The region pill under a waiting egg. Tapping it cycles; Pet::setRegion swaps
// the egg to that region's creature, keeping the rarity it was granted and
// remembering each region's answer so flipping back and forth is not a re-roll.
#define EGGREG_X 133
#define EGGREG_Y 374
#define EGGREG_W 200
#define EGGREG_H 34
// The hit area is BIGGER than the pill, like the BOX button and the battle
// grid, and for the same reason: a 34 px target is under UI_TAP_MIN and a
// finger is not a stylus.
//
// The guard band matters more than the padding. Missing this pill fell through
// to pet.eggTap(), and THREE taps hatch the egg -- so fumbling at the region
// selector hatched the very egg you were trying to re-aim. A near miss now
// does nothing at all, which is the correct answer for a control whose
// neighbour is irreversible.
#define EGGREG_PAD 16
#define EGGREG_GUARD 14

static void drawEggRegion() {
  char l[24];
  snprintf(l, sizeof(l), "%s >", pet.regionName());
  gfx->fillRoundRect(EGGREG_X, EGGREG_Y, EGGREG_W, EGGREG_H, 10, UI_WHITE);
  gfx->drawRoundRect(EGGREG_X, EGGREG_Y, EGGREG_W, EGGREG_H, 10, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(EGGREG_X + (EGGREG_W - (int)strlen(l) * 12) / 2, EGGREG_Y + 9);
  gfx->print(l);
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(T(S_EGG_REGION)) * 3, EGGREG_Y + EGGREG_H + 6);
  gfx->print(T(S_EGG_REGION));
}

// True if the tap was on the region pill, so the egg does not also get cracked.
// The egg's region pill: its graphic, and the area that actually accepts a tap.
// Exposed so a test can prove the second is bigger than the first and that a
// near miss does not reach pet.eggTap().
void uiEggPillRect(int *x, int *y, int *w, int *h, bool hitArea) {
  int pad = hitArea ? EGGREG_PAD : 0;
  if (x) *x = EGGREG_X - pad;
  if (y) *y = EGGREG_Y - pad;
  if (w) *w = EGGREG_W + 2 * pad;
  if (h) *h = EGGREG_H + 2 * pad;
}

// 1 = cycled the region, -1 = a near miss that must NOT reach the egg, 0 = not
// ours at all.
static int eggRegionTap(int16_t x, int16_t y) {
  if (!pet.isEgg()) return 0;
  int inset = EGGREG_PAD, guard = EGGREG_PAD + EGGREG_GUARD;
  bool hit = x >= EGGREG_X - inset && x <= EGGREG_X + EGGREG_W + inset &&
             y >= EGGREG_Y - inset && y <= EGGREG_Y + EGGREG_H + inset;
  if (hit) {
    pet.setRegion(nextAvailableRegion(pet.region));
    sfxPlay(SFX_TAP);
    return 1;
  }
  bool near = x >= EGGREG_X - guard && x <= EGGREG_X + EGGREG_W + guard &&
              y >= EGGREG_Y - guard && y <= EGGREG_Y + EGGREG_H + guard;
  if (near) { sfxPlay(SFX_DENY); return -1; }
  return 0;
}

// The region chooser used by the Pokedex and the gym ladder. Each row carries
// its own progress, so the screen answers "where am I up to" as well as "where
// do I want to go".
#define RPICK_X 74
#define RPICK_W 318
#define RPICK_H 62
#define RPICK_Y(i) (108 + (i) * 72)
// The page dots sit BETWEEN the last row and the LAN button, not over it.
// They used to be at y=366, which is inside the LAN button (336..380) and on
// top of its label -- invisible until a region chooser had more than one page,
// so it appeared the moment Sinnoh made GYM_REGIONS 4 and went unnoticed
// because swipe_test drives the paging and never looks at the pixels.
#define RPICK_DOTS_Y 325
#define RPICK_DOT_R 5
// Guards, not decoration: both of these are the collision that shipped, and a
// static_assert is the only check that cannot be forgotten when somebody moves
// a button. The dots must clear the LAN button below and the last row above.
static_assert(RPICK_DOTS_Y + RPICK_DOT_R < LANBTN_Y,
              "the region-chooser page dots overlap the LAN button");
static_assert(RPICK_DOTS_Y - RPICK_DOT_R > RPICK_Y(RPICK_PER_PAGE - 1) + RPICK_H,
              "the region-chooser page dots overlap the last region row");

// How many regions this mode lists. Gyms genuinely only exist for three
// (GYM_REGIONS); the Pokedex and the starter screen list every REAL region,
// which is GAL_REGIONS -- REGION_COUNT minus the ALL pseudo-region.
//
// This used to be GYM_REGIONS for ALL THREE MODES, with the names read out of
// TRAINER_SETS -- a trainer table driving the Pokedex chooser. So the moment
// Sinnoh landed it had no row, while the gallery's vertical swipe cycled it
// happily: built, reachable, and looking absent. That is the exact failure the
// chooser was added to prevent.
uint8_t rpickRegions(uint8_t mode) {
  return (mode == RPICK_FOR_GYMS) ? (uint8_t)GYM_REGIONS : (uint8_t)GAL_REGIONS;
}

uint8_t rpickPageCount(uint8_t mode) {
  uint8_t n = rpickRegions(mode);
  uint8_t p = (uint8_t)((n + RPICK_PER_PAGE - 1) / RPICK_PER_PAGE);
  return p ? p : 1;
}

// The chooser WRAPS rather than closing off the end, unlike the other paged
// screens. It is the root of its own screen, and at first boot there is
// nowhere to go back to at all -- exiting would strand the player before they
// have chosen anything.
// Which chooser is on the panel, or 0xFF for none. THE single answer: the
// swipe handler and swipe_test both ask this rather than each deciding.
uint8_t rpickModeNow() {
  switch (uiCurrentScreen()) {
    case SCR_REGION:  return RPICK_FOR_START;
    case SCR_DEXPICK: return RPICK_FOR_DEX;
    case SCR_GYMPICK: return RPICK_FOR_GYMS;
    default: return 0xFF;
  }
}

static bool rpickSwipe(int dir) {
  uint8_t mode = rpickModeNow();
  if (mode == 0xFF) return false;
  uint8_t pages = rpickPageCount(mode);
  int p = (int)rpickPage + (dir > 0 ? -1 : 1);
  if (p < 0) p = pages - 1;
  if (p >= pages) p = 0;
  rpickPage = (uint8_t)p;
  sfxPlay(SFX_TAP);
  return true;
}

static void renderRegionPick(uint8_t mode) {
  bool forGyms = (mode == RPICK_FOR_GYMS);
  uint8_t nreg = rpickRegions(mode);
  uint8_t pages = rpickPageCount(mode);
  if (rpickPage >= pages) rpickPage = 0;
  uint8_t first = (uint8_t)(rpickPage * RPICK_PER_PAGE);
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  char ttl[40];
  if (mode == RPICK_FOR_START) snprintf(ttl, sizeof(ttl), "%s", T(S_CHOOSE_REGION));
  else if (forGyms) snprintf(ttl, sizeof(ttl), "%s", T(S_GYMS));
  else snprintf(ttl, sizeof(ttl), T(S_POKEDEX_FMT), pet.registeredCount(), DEX_COUNT);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(ttl) * 6, 48);
  gfx->print(ttl);

  for (uint8_t row = 0; row < RPICK_PER_PAGE; row++) {
    uint8_t i = (uint8_t)(first + row);
    if (i >= nreg) break;
    int y = RPICK_Y(row);
    // The sprite pack is the gate. A region without it is drawn GREYED with a
    // reason rather than dropped from the list: hiding it would say "this
    // region does not exist" when what we mean is "download its pack".
    bool open = forGyms || regionAvailable(i);
    gfx->fillRoundRect(RPICK_X, y, RPICK_W, RPICK_H, 12, open ? UI_WHITE : UI_BG_DAY);
    gfx->drawRoundRect(RPICK_X, y, RPICK_W, RPICK_H, 12, open ? UI_INK : UI_TRACK);
    const char *nm = forGyms ? TRAINER_SETS[i].region : REGIONS[i].name;
    gfx->setTextColor(open ? UI_INK : UI_TRACK);
    gfx->setTextSize(3);
    gfx->setCursor(RPICK_X + 18, y + 12);
    gfx->print(nm);
    // At first boot there is no subtitle: naming the starter here would give
    // away the next screen, and the counts the other two modes show would all
    // read zero on a new save anyway.
    char sub[28];
    sub[0] = 0;
    if (!open)
      snprintf(sub, sizeof(sub), "%s", T(S_NEED_PACK));
    else if (mode == RPICK_FOR_GYMS)
      snprintf(sub, sizeof(sub), T(S_BADGES_FMT), pet.badgeCountIn(i, gymHard));
    else if (mode == RPICK_FOR_DEX)
      snprintf(sub, sizeof(sub), "%u/%u",
               pet.registeredCountIn(REGIONS[i].lo, REGIONS[i].hi),
               (unsigned)(REGIONS[i].hi - REGIONS[i].lo + 1));
    if (sub[0]) {
      gfx->setTextColor(UI_INK_SOFT);
      gfx->setTextSize(2);
      gfx->setCursor(RPICK_X + RPICK_W - 18 - (int)strlen(sub) * 12, y + 22);
      gfx->print(sub);
    }
  }
  if (forGyms) {
    gfx->fillRoundRect(LANBTN_X, LANBTN_Y, LANBTN_W, LANBTN_H, 11, UI_BG_DAY);
    gfx->drawRoundRect(LANBTN_X, LANBTN_Y, LANBTN_W, LANBTN_H, 11, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(T(S_LAN)) * 6, LANBTN_Y + (LANBTN_H - 16) / 2);
    gfx->print(T(S_LAN));
  }
  if (pages > 1) {                        // dots: which page of regions this is
    int total = pages * 16 - 8;
    for (uint8_t d = 0; d < pages; d++) {
      int cx = CX - total / 2 + d * 16;
      if (d == rpickPage) gfx->fillCircle(cx, RPICK_DOTS_Y, RPICK_DOT_R, UI_INK);
      else gfx->drawCircle(cx, RPICK_DOTS_Y, RPICK_DOT_R, UI_TRACK);
    }
  }
  // Drawn exactly where rpickBackHit() answers: the Pokedex chooser only.
  if (mode == RPICK_FOR_DEX) {
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(T(S_BACK)) * 6, RPICK_BACK_LABEL_Y);
    gfx->print(T(S_BACK));
  }
  uiChrome();
  gfx->flush();
}

// Returns the region tapped, or -1. Takes the mode because the row on screen is
// an offset into the current page, not the region index -- and because a region
// whose sprite pack is missing must not be selectable, which is the whole point
// of the gate. The chooser still SHOWS it, greyed, saying why.
static int regionPickTap(int16_t x, int16_t y, uint8_t mode) {
  int i = rpickRowAt(x, y, mode);
  if (i < 0) return -1;
  if (mode != RPICK_FOR_GYMS && !regionAvailable((uint8_t)i)) {
    sfxPlay(SFX_DENY);      // locked: say no out loud rather than do nothing
    return -1;
  }
  return i;
}

// The region whose row is under (x,y) on the current page, locked or not, or -1.
int rpickRowAt(int16_t x, int16_t y, uint8_t mode) {
  if (x < RPICK_X || x > RPICK_X + RPICK_W) return -1;
  uint8_t nreg = rpickRegions(mode);
  for (uint8_t row = 0; row < RPICK_PER_PAGE; row++) {
    uint8_t i = (uint8_t)(rpickPage * RPICK_PER_PAGE + row);
    if (i >= nreg) break;
    if (y >= RPICK_Y(row) && y <= RPICK_Y(row) + RPICK_H) return i;
  }
  return -1;
}

bool lanBtnHit(int16_t x, int16_t y) {
  return y >= LANBTN_Y && y <= LANBTN_Y + LANBTN_H &&
         x >= LANBTN_X && x <= LANBTN_X + LANBTN_W;
}

// Only the Pokedex chooser has a tap-to-go-back zone. The gym chooser's LAN
// button fills that space, and a zone next to it is what turned a low tap on
// LAN BATTLE into "back to the pet screen". First boot has nowhere to go back to.
bool rpickBackHit(int16_t x, int16_t y, uint8_t mode) {
  (void)x;
  return mode == RPICK_FOR_DEX && y > RPICK_BACK_ZONE_Y;
}

// Whether a gesture starting at (x,y) starts ON a tappable control. Such a
// gesture resolves as a tap unless it moves like a real swipe -- see
// handleTouch(). Knows the region choosers, where the misfire was reported;
// any other screen answers false and keeps the plain distance rules.
bool uiOnControl(int16_t x, int16_t y) {
  uint8_t mode = rpickModeNow();
  if (mode == 0xFF) return false;
  if (rpickRowAt(x, y, mode) >= 0) return true;
  return mode == RPICK_FOR_GYMS && lanBtnHit(x, y);
}

// ---------- level-up learn prompt ----------
void renderLearn() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  uint8_t mv = pet.learnOffer();
  char head[40];
  const char *nm = pet.nick[0] ? pet.nick : DEX_TBL[pet.speciesId].name;
  snprintf(head, sizeof(head), T(S_LEARN_Q), nm);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(head) * 3, 48);
  gfx->print(head);
  gfx->setTextColor(DEX_TBL[pet.speciesId].accent);
  const char *move = moveName(mv);
  int moveSize = strlen(move) <= 17 ? 3 : 2;
  gfx->setTextSize(moveSize);
  gfx->setCursor(CX - (int)strlen(move) * moveSize * 3, moveSize == 3 ? 66 : 72);
  gfx->print(move);

  for (int i = 0; i < MOVE_SLOTS; i++) drawMoveRow(LEARN_ROW_Y(i), pet.moves[i], false, pet.speciesId);

  gfx->fillRoundRect(70, LEARN_SKIP_Y, 326, 44, 12, UI_TRACK);
  gfx->drawRoundRect(70, LEARN_SKIP_Y, 326, 44, 12, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_LEARN_SKIP)) * 6, LEARN_SKIP_Y + 14);
  gfx->print(T(S_LEARN_SKIP));
  gfx->flush();
}

// pagina 2: medallas con etiqueta descriptiva
void renderCardMedals() {
  int got = 0;
  for (int i = 0; i < MED_COUNT; i++)
    if (pet.hasMedal(1 << i)) got++;
  char head[20];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), got, MED_COUNT);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(head) * 9, 48);
  gfx->print(head);

  for (int i = 0; i < MED_COUNT; i++) {
    int x = 28 + (i % 2) * 206, y = 104 + (i / 2) * 54;
    bool g = pet.hasMedal(1 << i);
    gfx->fillRoundRect(x, y, 196, 44, 10, g ? UI_BAR_OK : UI_TRACK);
    if (g) {  // marca de conseguida
      gfx->fillCircle(x + 22, y + 22, 11, UI_BG_DAY);
      gfx->setTextColor(UI_BAR_OK);
      gfx->setTextSize(2);
      gfx->setCursor(x + 16, y + 13);
      gfx->print("v");
    }
    gfx->setTextColor(g ? UI_BG_DAY : 0x8410);
    gfx->setTextSize(2);
    gfx->setCursor(x + 44, y + 14);
    gfx->print(medalDesc(i));
  }
}

// pagina 3: progreso (nivel, evolucion, descuidos) — saca a la luz mecanicas
// que antes eran invisibles (cuanto falta para subir/evolucionar y por que)
// The card's evolution line, asked of evoInfo() -- the one rule canEvolveNow()
// and evolve() also use. The card used to restate the level sum itself, which
// is CLAUDE.md § 1's own example, and pure trade evolution would have left it
// promising a level-40 Alakazam that never comes. A function rather than inline
// so a test can read what the card SAYS, not just what the rule returns.
const char *uiEvoLine(char *buf, size_t n, uint16_t *col) {
  const Pet::EvoInfo ei = pet.evoInfo();
  *col = UI_INK;
  switch (ei.status) {
    case Pet::EVO_FINAL: return T(S_FINAL_FORM);
    case Pet::EVO_NEEDS_TRADE: return T(S_EVO_TRADE);
    case Pet::EVO_READY: *col = UI_BAR_OK; return T(S_EVO_READY);
    case Pet::EVO_BLOCKED: *col = UI_BAR_BAD; return T(S_EVO_BLOCKED);
    default:
      snprintf(buf, n, T(S_EVO_IN_FMT), ei.levelsLeft);
      return buf;
  }
}

void renderCardProgress() {
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_PROGRESS)) * 9, 44);
  gfx->print(T(S_PROGRESS));

  // nivel grande
  char lv[10];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), pet.level());
  gfx->setTextSize(5);
  gfx->setCursor(CX - strlen(lv) * 15, 86);
  gfx->print(lv);

  // barra de progreso al siguiente nivel (1 nivel = 60 min de juego)
  uint8_t into = pet.ageMinutes % MINUTES_PER_LEVEL;
  int bx = 93, bw = 280, by = 158, bh = 22;
  gfx->fillRoundRect(bx, by, bw, bh, 6, UI_TRACK);
  int fw = (bw - 4) * into / MINUTES_PER_LEVEL;
  if (fw > 0) gfx->fillRoundRect(bx + 2, by + 2, fw, bh - 4, 5, UI_BAR_OK);
  char nx[26];
  snprintf(nx, sizeof(nx), T(S_NEXT_LVL_FMT), MINUTES_PER_LEVEL - into, pet.level() + 1);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(nx) * 6, by + 32);
  gfx->print(nx);

  // estado de evolucion
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setCursor(CX - strlen(T(S_EVO_LABEL)) * 6, 230);
  gfx->print(T(S_EVO_LABEL));
  char evoBuf[28];
  uint16_t evoCol = UI_INK;
  const char *evo = uiEvoLine(evoBuf, sizeof(evoBuf), &evoCol);
  gfx->setTextColor(evoCol);
  gfx->setCursor(CX - strlen(evo) * 6, 256);
  gfx->print(evo);

  // the day inherited from an early retire, said out loud -- otherwise this
  // creature simply evolves late and the player has no way to know why
  if (pet.evoPenalty()) {
    gfx->setTextSize(1);
    gfx->setTextColor(UI_BAR_WARN);
    gfx->setCursor(CX - (int)strlen(T(S_EVO_SLOW)) * 3, 286);
    gfx->print(T(S_EVO_SLOW));
    gfx->setTextSize(2);
  }

  // descuidos (retrasan la evolucion)
  char ms[24];
  snprintf(ms, sizeof(ms), T(S_MISTAKES_FMT), pet.careMistakes);
  gfx->setTextColor(pet.careMistakes > 0 ? UI_BAR_BAD : UI_INK);
  gfx->setCursor(CX - strlen(ms) * 6, 312);
  gfx->print(ms);
}

void renderCard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (cardPage == 0) renderCardProfile();
  else if (cardPage == 1) renderCardStats();
  else if (cardPage == 2) renderCardMoves();
  else renderCardProgress();

  // indicador de paginas + ayuda
  for (int i = 0; i < CARD_PAGES; i++) {
    int dx = CX - (CARD_PAGES - 1) * 13 + i * 26;
    if (i == cardPage) gfx->fillCircle(dx, 374, 5, UI_INK);
    else gfx->drawCircle(dx, 374, 4, UI_INK);
  }
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_BACK)) * 6, 398);
  gfx->print(T(S_BACK));
  uiChrome();
  gfx->flush();
}

// ---------- menu overlay ----------

// Row labels are built fresh each frame because two of them carry live counts.
static void menuRowLabel(int i, char *out, size_t n) {
  switch (i) {
    case 0: snprintf(out, n, "%s", T(S_STATS)); break;
    case 1: snprintf(out, n, "%s", T(S_BAG)); break;
    case 2: snprintf(out, n, T(S_POKEDEX_FMT), pet.registeredCount(), DEX_COUNT); break;
    case 3: snprintf(out, n, "%s", T(S_SETTINGS)); break;
    case 4: snprintf(out, n, "%s", T(S_RETIRE)); break;
    default: snprintf(out, n, "%s", T(S_CLOSE)); break;
  }
}

// Where a menu row is, asked of the firmware rather than restated.
//
// touch_test held the literal `104 + 16 + 22` next to a comment claiming it was
// MENU_ROW_Y(0) + 22 -- which it never was. It landed one pixel inside row 0
// and passed for that reason alone, until the panel gained a sixth row and the
// same tap started opening the BAG. A test that keeps its own copy of a layout
// proves the transcription; see CLAUDE.md on rpickPageCount() and
// uiButtonHeights(), which exist for exactly this.
int uiMenuRowCenterY(int i) { return MENU_ROW_Y(i) + MENU_ROW_H / 2; }

void drawMenu() {
  // dim the game behind the panel so the overlay reads as modal, and so it is
  // obvious that tapping the darkened area is a way out
  for (int y = 0; y < 466; y += 2)
    gfx->drawFastHLine(0, y, 466, gNight ? 0x0000 : 0x2104);

  gfx->fillRoundRect(MENU_X, MENU_Y, MENU_W, MENU_H, 18, UI_WHITE);
  gfx->drawRoundRect(MENU_X, MENU_Y, MENU_W, MENU_H, 18, UI_INK);

  for (int i = 0; i < MENU_ROWS; i++) {
    int y = MENU_ROW_Y(i);
    bool close = (i == MENU_ROWS - 1);
    bool dead = (i == 4 && !pet.canRetireNow());   // an egg or a companion
    gfx->fillRoundRect(MENU_X + 18, y, MENU_W - 36, MENU_ROW_H, 12,
                       close || dead ? UI_TRACK : UI_BG_DAY);
    gfx->drawRoundRect(MENU_X + 18, y, MENU_W - 36, MENU_ROW_H, 12, UI_INK);
    char lbl[28];
    menuRowLabel(i, lbl, sizeof(lbl));
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(lbl) * 6, y + MENU_ROW_H / 2 - 8);
    gfx->print(lbl);
  }
}

// ---------- who you are raising ----------

// Exchange the creature on the main screen with a stored one.
//
// A TRUE exchange: the live pet takes the slot the newcomer vacates, so this
// needs no free slot and nothing can be lost. Both records carry their full
// care state, so swapping away and back returns the creature exactly as it was
// rather than handing back a blanked copy -- which is the whole reason
// PartyMon grew a care block.
//
// An EGG is the one asymmetric case: it has nothing to bank, so the slot simply
// empties. That is what the old frozen BRING BACK did, and it is the only way
// this can cost you anything.
//
// switchTo() is called BEFORE either party write, and both are skipped if it
// reports its save failed. This used to be the other order -- bank the
// outgoing pet into the slot first, then switchTo() the incoming one -- and
// on real hardware that put the SAME creature in two places at once: the
// party write always landed, and a failed checkpoint write left the pet's
// own on-disk record still showing the OUTGOING pet as live, so a reboot
// before the next successful save reloaded it as live AND banked. Skipping
// both party writes on a failed switchTo() cannot lose the outgoing pet --
// it is still sitting in this pet's own fields in RAM, and the next
// successful save (the existing pendingSave/retryDue() machinery) persists
// whatever is currently live -- it can only cost the last few seconds of its
// care state if a crash lands in the narrow window before that catches up,
// which is the same trade every other save() caller already makes.
// Retried before giving up, rather than accepting the first failure as final
// -- saveHealthy()'s own rule is "a single short write can be a transient,
// three in a row is not coming back on its own", and focusSwap() used to not
// follow that rule at all. Real hardware showed these failures clearing up
// within the same session with nothing else changing, which is exactly the
// transient shape this retries for. Each attempt is a real flash write
// (~1s stall on this chip), so a genuinely broken NVS costs a few seconds of
// visible pause here -- a fair price for not orphaning a creature over it.
#define FOCUS_SWAP_RETRIES 3

void focusSwap(uint8_t slot) {
  if (slot >= PARTY_SLOTS) return;
  PartyMon incoming = party.slots[slot];   // by value: the slot is about to change
  if (incoming.empty()) return;
  if (tradeLocked(incoming)) { sfxPlay(SFX_DENY); return; }   // promised to a trade
  bool wasEgg = pet.isEgg();
  PartyMon outgoing = wasEgg ? PartyMon() : pet.toPartyMon();   // read BEFORE switchTo() overwrites it

  bool ok = false;
  for (uint8_t i = 0; i < FOCUS_SWAP_RETRIES && !ok; i++) ok = pet.switchTo(incoming);
  if (!ok) {
    // Roll back to what was live before, rather than leaving `incoming` on
    // screen with `outgoing` orphaned -- neither live nor banked anywhere,
    // recoverable only by a reboot landing on a stale checkpoint before the
    // next autosave overwrites it. This is a real bug that shipped: real
    // hardware left a starter unreachable through any UI path until reboot.
    // If THIS save also fails, RAM and disk already agree on `outgoing` --
    // it was never touched -- so nothing is lost either way, only another
    // entry in the existing pendingSave/retryDue() retry queue.
    Serial.println("focus swap: pet checkpoint failed after retries, rolling back");
    pet.switchTo(outgoing);
    return;
  }
  if (wasEgg) party.releaseAt(slot);
  else party.replaceAt(slot, outgoing);
}

// ---------- the bag ----------

// How many pages the field bag needs. One even when empty, so the screen can
// say so instead of drawing nothing and reading as a crash.
uint8_t bagPages() {
  uint8_t n = bag.distinctCount();
  return n ? (uint8_t)((n + BAG_PER_PAGE - 1) / BAG_PER_PAGE) : 1;
}

int uiBagRowCenterY(int i) { return BAG_ROW_Y(i) + 25; }

// A toast over the bag: what the last item did. Not a dialog -- using a vitamin
// is not a decision, and a confirmation for every tap would make training worse
// than the punching bag it competes with.
static char bagMsg[40] = "";
static uint32_t bagMsgUntil = 0;

static void bagSay(const char *fmt, const char *arg) {
  snprintf(bagMsg, sizeof(bagMsg), fmt, arg);
  bagMsgUntil = millis() + 2200;
}

// Applies a field item to the live pet. Returns false when the item was valid
// but had nothing to do -- a fully trained stat, say -- which the caller reports
// rather than silently consuming the item.
static bool bagUseOnPet(ItemKey k) {
  const ItemEntry &e = itemEntry(k);
  if (e.category != IC_TRAIN) return false;
  // The IV-bound ceiling applies here exactly as it does to the punching bag and
  // to a gym reward. An item that could push past trMaxFor() would make the IV
  // roll decorative, which is the one thing it must not be.
  uint8_t *cur;
  uint8_t cap;
  switch ((uint8_t)e.param) {
    case TRAIN_ATK: cur = &pet.trAtk; cap = pet.trMaxAtk(); break;
    case TRAIN_DEF: cur = &pet.trDef; cap = pet.trMaxDef(); break;
    default:        cur = &pet.trSpe; cap = pet.trMaxSpe(); break;
  }
  if (*cur >= cap) return false;
  uint16_t next = (uint16_t)*cur + e.amount;
  *cur = next > cap ? cap : (uint8_t)next;
  pet.saveNow();
  return true;
}

void renderBag() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(T(S_BAG)) * 6, 40);
  gfx->print(T(S_BAG));

  uint8_t n = bag.distinctCount();
  uint8_t pages = bagPages();
  if (bagPage >= pages) bagPage = 0;
  if (!n) {
    gfx->setTextColor(UI_INK_SOFT);
    gfx->setCursor(CX - (int)strlen(T(S_BAG_EMPTY)) * 6, CY - 8);
    gfx->print(T(S_BAG_EMPTY));
  }
  for (uint8_t i = 0; i < BAG_PER_PAGE; i++) {
    uint8_t idx = (uint8_t)(bagPage * BAG_PER_PAGE + i);
    if (idx >= n) break;
    ItemKey k = bag.keyAt(idx);
    int y = BAG_ROW_Y(i);
    bool usable = itemUsableInField(k);
    gfx->fillRoundRect(76, y, 314, 50, 10, usable ? UI_WHITE : UI_TRACK);
    gfx->drawRoundRect(76, y, 314, 50, 10, UI_INK);
    gfx->setTextColor(usable ? UI_INK : 0x8410);
    gfx->setTextSize(2);
    gfx->setCursor(92, y + 17);
    gfx->print(itemEntry(k).name);
    char cnt[8];
    snprintf(cnt, sizeof(cnt), "x%u", bag.count(k));
    gfx->setCursor(374 - (int)strlen(cnt) * 12, y + 17);
    gfx->print(cnt);
  }
  // A page NUMBER, not a dot row: the catalogue is append-only and a dot per
  // page stopped fitting the round panel once the Pokedex reached 25 of them.
  if (pages > 1) {
    char pg[12];
    snprintf(pg, sizeof(pg), "%u/%u", bagPage + 1, pages);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(pg) * 6, 386);
    gfx->print(pg);
  }
  if (bagMsgUntil && millis() < bagMsgUntil) {
    gfx->fillRoundRect(66, CY - 26, 334, 52, 12, UI_WHITE);
    gfx->drawRoundRect(66, CY - 26, 334, 52, 12, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(bagMsg) * 6, CY - 8);
    gfx->print(bagMsg);
  }
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(T(S_BACK)) * 6, 414);
  gfx->print(T(S_BACK));
  uiChrome();
  gfx->flush();
}

// The Linking Cord, from the bag, on the live pet. Every decision is
// Pet::cordUse()'s; this only routes it. Used up only when it took.
void cordFromBag() {
  const char *name = itemEntry(IT_LINKCORD).name;
  switch (pet.cordUse()) {
    case Pet::CORD_SOLO:
      if (!pet.applyCord()) { sfxPlay(SFX_DENY); return; }
      bag.consume(IT_LINKCORD);
      sfxPlay(SFX_MEDAL);
      bagSay(T(S_ITEM_USED), name);
      return;
    case Pet::CORD_NEEDS_PARTNER:
      // KARRABLAST and SHELMET: pick who to link with on the party ring.
      bagOpen = false;
      partyOpen = true;
      partyDetail = 0;
      boxSwapFrom = 0;
      boxSel = 0;
      cordPick = true;
      sfxPlay(SFX_TAP);
      return;
    default:
      sfxPlay(SFX_DENY);
      bagSay("%s", T(S_ITEM_NOUSE));   // and the cord is kept
      return;
  }
}

// The partner pick. Returns to the bag either way, with a toast saying what
// happened; only a real pair spends the cord.
void cordPickTap(int16_t x, int16_t y) {
  int i = partySlotAt(x, y);
  bool cancel = (y >= PARTYCLOSE_Y && y <= PARTYCLOSE_Y + PARTYCLOSE_H &&
                 x >= PARTYCLOSE_X && x <= PARTYCLOSE_X + PARTYCLOSE_W) || y < 34;
  if (i < 0 && !cancel) return;                       // a miss on the ring: nothing
  if (i >= 0 && (party.slots[i].empty() || tradeLocked(party.slots[i]))) { sfxPlay(SFX_DENY); return; }
  cordPick = false;
  partyOpen = false;
  bagOpen = true;
  if (cancel) { sfxPlay(SFX_TAP); return; }           // cord kept
  PartyMon m = party.slots[i];
  if (!pet.applyCordPair(m)) {
    sfxPlay(SFX_DENY);
    bagSay("%s", T(S_ITEM_NOUSE));
    return;
  }
  party.replaceAt((uint8_t)i, m);
  bag.consume(IT_LINKCORD);
  sfxPlay(SFX_MEDAL);
  bagSay(T(S_ITEM_USED), itemEntry(IT_LINKCORD).name);
}

void bagTap(int16_t x, int16_t y) {
  uint8_t n = bag.distinctCount();
  for (uint8_t i = 0; i < BAG_PER_PAGE; i++) {
    uint8_t idx = (uint8_t)(bagPage * BAG_PER_PAGE + i);
    if (idx >= n) break;
    int ry = BAG_ROW_Y(i);
    if (x < 76 || x > 390 || y < ry || y > ry + 50) continue;
    ItemKey k = bag.keyAt(idx);
    if (!itemUsableInField(k) || pet.isEgg()) { sfxPlay(SFX_DENY); return; }
    if (k == IT_LINKCORD) { cordFromBag(); return; }
    if (!bagUseOnPet(k)) { sfxPlay(SFX_DENY); bagSay("%s", T(S_ITEM_NOUSE)); return; }
    bag.consume(k);
    sfxPlay(SFX_TAP);
    bagSay(T(S_ITEM_USED), itemEntry(k).name);
    return;
  }
  bagOpen = false;   // anywhere else closes, like every other list screen
}

// ---------- the Poke Mart ----------
//
// 1 pedometer step = $1 (Pet::addSteps()), spent here on the real games' own
// Poke Mart prices (see items.h). The catalogue is DERIVED from itemEntry().price
// rather than a second list kept here -- the same reasoning as the bag's
// weighted drop table, so a future item only needs a price to appear.

uint8_t martPages() {
  uint8_t n = martSellableCount();
  return n ? (uint8_t)((n + MART_PER_PAGE - 1) / MART_PER_PAGE) : 1;
}

void renderMart() {
  int h = sceneHour();
  gNight = pet.sleeping || h < 6 || h >= 20;
  drawScene(pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome, millis(), gNight);

  gfx->fillRoundRect(78, 28, 310, 54, 14, UI_BG_DAY);
  gfx->drawRoundRect(78, 28, 310, 54, 14, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - (int)strlen(T(S_MART)) * 9, 44);
  gfx->print(T(S_MART));

  // Wallet is bold (size 4) and, unlike the title, sits directly on the night
  // scene rather than a light pill -- inkColor() is what the main screen's own
  // text uses to stay legible after dark, and these three lines need the same
  // switch since nothing behind them does it for them.
  uint16_t ink = inkColor();
  char wl[24];
  snprintf(wl, sizeof(wl), T(S_WALLET_FMT), (unsigned long)pet.wallet);
  gfx->setTextColor(ink);
  gfx->setTextSize(4);
  gfx->setCursor(CX - (int)strlen(wl) * 12, 116);
  gfx->print(wl);

  gfx->setTextSize(2);
  gfx->setTextColor(ink);
  gfx->setCursor(CX - (int)strlen(T(S_MART_RATE)) * 6, 156);
  gfx->print(T(S_MART_RATE));
  char sv[64];
  snprintf(sv, sizeof(sv), T(S_MART_STEPS_FMT), (unsigned long)pet.stepsTotal);
  gfx->setCursor(CX - (int)strlen(sv) * 6, 176);
  gfx->print(sv);

  uint8_t n = martSellableCount();
  uint8_t pages = martPages();
  if (martPage >= pages) martPage = 0;
  for (uint8_t i = 0; i < MART_PER_PAGE; i++) {
    uint8_t idx = (uint8_t)(martPage * MART_PER_PAGE + i);
    if (idx >= n) break;
    ItemKey k = martKeyAt(idx);
    uint16_t price = itemEntry(k).price;
    bool afford = pet.wallet >= price;
    int y = MART_ROW_Y(i);
    // Green when affordable, matching Settings' SND ON pill (UI_BAR_OK /
    // UI_BG_DAY); grey/dimmed otherwise, matching the bag's unusable rows.
    gfx->fillRoundRect(76, y, 314, 50, 10, afford ? UI_BAR_OK : UI_TRACK);
    gfx->drawRoundRect(76, y, 314, 50, 10, UI_INK);
    gfx->setTextColor(afford ? UI_BG_DAY : 0x8410);
    gfx->setTextSize(2);
    gfx->setCursor(92, y + 17);
    gfx->print(itemEntry(k).name);
    char pr[10];
    snprintf(pr, sizeof(pr), "$%u", price);
    gfx->setCursor(374 - (int)strlen(pr) * 12, y + 17);
    gfx->print(pr);
  }
  if (pages > 1) {
    char pg[12];
    snprintf(pg, sizeof(pg), "%u/%u", martPage + 1, pages);
    gfx->setTextColor(ink);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(pg) * 6, 386);
    gfx->print(pg);
  }
  gfx->setTextColor(ink);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(T(S_BACK)) * 6, 414);
  gfx->print(T(S_BACK));

  // Asked before it happens -- what the choice costs is on screen before the
  // tap, same rule as the release/retire dialogs.
  if (martConfirmItem) {
    char q[40];
    snprintf(q, sizeof(q), T(S_MART_BUY_Q), itemEntry(martConfirmItem).name);
    char cost[24];
    snprintf(cost, sizeof(cost), T(S_MART_PRICE_FMT),
             (unsigned long)itemEntry(martConfirmItem).price);
    drawConfirmPanel(q, cost, nullptr, UI_INK_SOFT,
                     T(S_YES), UI_BAR_OK, UI_WHITE, T(S_NO), UI_TRACK, UI_INK);
  }
  uiChrome();
  gfx->flush();
}

void martTap(int16_t x, int16_t y) {
  if (martConfirmItem) {
    int b1Top, b1Bot, b2Top, b2Bot;
    uiConfirmRects(&b1Top, &b1Bot, &b2Top, &b2Bot);
    bool inX = (x >= CONFIRM_BTN_X && x <= CONFIRM_BTN_X + CONFIRM_BTN_W);
    if (inX && y >= b1Top && y <= b1Bot) {           // YES
      if (pet.spendWallet(itemEntry(martConfirmItem).price)) {
        bag.add(martConfirmItem);
        sfxPlay(SFX_TAP);
      } else {
        sfxPlay(SFX_DENY);   // the wallet moved between the tap and the confirm
      }
      martConfirmItem = ITEM_NONE;
    } else if (inX && y >= b2Top && y <= b2Bot) {     // NO
      martConfirmItem = ITEM_NONE;
    }
    return;
  }
  uint8_t n = martSellableCount();
  for (uint8_t i = 0; i < MART_PER_PAGE; i++) {
    uint8_t idx = (uint8_t)(martPage * MART_PER_PAGE + i);
    if (idx >= n) break;
    int ry = MART_ROW_Y(i);
    if (x < 76 || x > 390 || y < ry || y > ry + 50) continue;
    ItemKey k = martKeyAt(idx);
    if (pet.wallet < itemEntry(k).price) { sfxPlay(SFX_DENY); return; }
    martConfirmItem = k;
    sfxPlay(SFX_TAP);
    return;
  }
  // Anywhere else, including the "tap: back" label, returns to the pet --
  // same affordance the bag screen's own "anywhere else closes" gives, just
  // going to the pet tile instead of closing a modal since the Mart is on
  // the tile axis.
  uiTileGo(TILE_PET);
}

// ---------- wild encounters ----------

// Rolls a whole wild individual and opens the fight.
//
// The species, the level band, the tier and the shiny roll are all decided
// HERE and remembered, because a capture has to hand back the creature that was
// actually fought. Re-rolling any of it at capture time would mean the thing
// you caught was not the thing on screen.
bool startWildBattle(uint8_t region, bool hard) {
  if (pet.isEgg() || pet.ceremony != CER_NONE) return false;
  if (!exploreRegionAllowed(region)) return false;
  int16_t dex = 0;
  uint8_t lvl = 1;
  int legend = -1;
  if (wildRegionHasAreas(region)) {
    // A region with areas draws from the one the player chose, weighted by the
    // game's own encounter rates, at the level band its gym ladder sets.
    // Difficulty does not touch either: hard is the smart AI (btlHard below).
    uint16_t area = exploreAreaOf(region);
    if (area == WILD_NO_AREA || !wildAreaUnlocked(area, pet)) return false;
    // A legendary first: its guaranteed meeting at home, or the rare repeat.
    // It is MET as the fight starts, and saved then: wild battles end in half a
    // dozen places (caught, fled, ran, fainted, won), and one start is a single
    // place to get right. A power cut mid-fight costs the guarantee, never the
    // legendary -- the rare repeat still applies.
    legend = wildLegendFor(area, pet, (uint32_t)random(65536) * 31u + random(31));
    if (legend >= 0) {
      dex = wildLegendDex((uint8_t)legend);
      lvl = wildLegendLevel((uint8_t)legend);
    } else {
      dex = wildAreaPick(area, wildTimeBand(exploreHour()), pet,
                         (uint32_t)random(65536) * 31u + random(31));
      if (!dex) return false;   // nothing live: the area reads NEEDS PACK
      lvl = wildAreaLevel(area, dex, (uint32_t)random(65536));
    }
  } else {
    // A region whose areas have not landed yet keeps the region-wide pool.
    uint8_t tier = wildTierForRoll((uint8_t)random(100));
    dex = wildPickSpecies(region, tier, (uint32_t)random(65536) * 31u + random(31));
    // A tier can be empty when only some packs are installed -- Kanto has no
    // R_LEGENDARIO gap, but a future region might. Fall back rather than refusing
    // the encounter, since "nothing happened" reads as a broken button.
    if (!dex) dex = wildPickSpecies(region, R_COMUN, random(65536));
    if (!dex) return false;
    uint8_t lo = wildLevelMin(pet.level());
    uint8_t hi = wildLevelMax(pet.level());
    lvl = (uint8_t)(lo + random(hi - lo + 1));
  }
  wildDex = dex;
  wildLvl = lvl;
  wildLegend = (int8_t)legend;
  for (int i = 0; i < 4; i++) wildIv[i] = (uint8_t)(8 + random(24));
  // Bonus 0: nothing in this build earns one yet, so every wild shiny is the
  // flat 1/4096. The parameter stays because it is where a farewell reward
  // would land, and wild_test already pins the curve it would follow.
  wildShiny = wildShinyForRoll((uint32_t)random(WILD_SHINY_SCALE), 0);
  wildApplyShiny(wildShiny, wildIv[0], wildIv[1], wildIv[2], wildIv[3]);
  wildCaught = false;
  wildDropN = 0;

  buildSquad(0, TRAINER_TEAM_MAX, squadMask);
  if (!btlSquadN) return false;
  if (legend >= 0 && !pet.legendMet((uint8_t)legend)) {
    pet.setLegendMet((uint8_t)legend);   // the guaranteed meeting is spent
    pet.saveNow();
  }

  // Built through Pet like every other opponent, so it uses the same stat
  // formula and the same learnset-driven moveset the player's creature does.
  Pet foe;
  foe.dbgHatchAs(dex, wildShiny);
  foe.ivAtk = wildIv[0]; foe.ivDef = wildIv[1];
  foe.ivSpe = wildIv[2]; foe.ivHp = wildIv[3];
  foe.ageMinutes = (uint32_t)(wildLvl ? wildLvl - 1 : 0) * MINUTES_PER_LEVEL;
  foe.relearnFromLevel();
  combatantFromPet(btlFoe, foe);
  btlFoe.shiny = wildShiny;

  btlWild = true;
  btlLink = false;
  btlTrainer = -1;
  btlHard = hard;
  btlFoeAt = 0;
  btlFoeSquadN = 0;
  btlMsgCount = 0;
  btlOver = false;
  btlWon = false;
  btlMenu = 0;
  btlBagPage = 0;
  btlWinUntil = 0;
  btlLost = false;
  btlSwapWho = -1;
  btlFaintUntil[0] = btlFaintUntil[1] = 0;
  btlEnterUntil[0] = btlEnterUntil[1] = 0;
  btlHpShown[0] = btlYou.maxHp;
  btlHpShown[1] = btlFoe.maxHp;
  btlSyncSprite(0, btlYou);
  btlSyncSprite(1, btlFoe);
  audioMusic(MUS_BATTLE);
  btlLungeUntil[0] = btlLungeUntil[1] = 0;
  btlHitUntil[0] = btlHitUntil[1] = 0;
  battleOpen = true;
  btlSay(T(S_WILD_MET), DEX_TBL[dex].name);
  return true;
}

// Gives an ID to every creature that has none: the live pet, the six, the box
// and a handover still waiting for a slot. Those are records from before IDs
// existed (v3.30 and earlier), so each is treated as having started on this
// board -- which, on the board it was raised on, it did.
//
// Every ordinary path mints its own ID when the creature is made (hatch(), a
// wild catch, the debug PARTY fill); this is the safety net for what is already
// saved, and run_once by construction, since a record that has an ID is left
// alone. Saved only if something changed, so a normal boot writes nothing.
int backfillUids() {
  int given = 0;
  bool partyDirty = false, boxDirty = false;
  if (!pet.isEgg() && !pet.uid.valid()) { pet.uid = pet.mintUid(); given++; }
  if (!pet.endedMon.empty() && !pet.endedMon.uid.valid()) {
    pet.endedMon.uid = pet.mintUid();
    given++;
  }
  for (auto &m : party.slots)
    if (!m.empty() && !m.uid.valid()) { m.uid = pet.mintUid(); given++; partyDirty = true; }
  for (auto &m : party.box)
    if (!m.empty() && !m.uid.valid()) { m.uid = pet.mintUid(); given++; boxDirty = true; }
  if (!given) return 0;
  // The pet checkpoint first: it carries the counter, so once it lands no
  // serial handed out above can be handed out again by this save.
  pet.saveNow();
  if (partyDirty) party.save();
  if (boxDirty) party.boxSave();
  Serial.printf("ids: gave %d creature(s) an ID\n", given);
  return given;
}

// The wild creature as a storable record. It goes through the same PartyMon the
// party and box already hold, so a caught creature is a first-class member the
// moment it lands: it can be swapped in, raised, evolved and battled with.
PartyMon wildToPartyMon() {
  PartyMon m;
  m.dex = wildDex;
  m.level = wildLvl;
  m.ivAtk = wildIv[0]; m.ivDef = wildIv[1];
  m.ivSpe = wildIv[2]; m.ivHp = wildIv[3];
  m.shiny = wildShiny ? 1 : 0;
  for (int i = 0; i < MOVE_SLOTS; i++) m.moves[i] = btlFoe.moves[i];
  // A caught creature has real care state from the start rather than being a
  // v0 record, so switching to it does not reset a creature that was never
  // banked in the first place. Its age IS its level: nothing raised it.
  m.stateVersion = 1;
  m.ageMinutes = (uint32_t)(wildLvl ? wildLvl - 1 : 0) * MINUTES_PER_LEVEL;
  m.lastLearnLevel = wildLvl;   // do not replay gates it already passed
  // Caught here, so this board is where it started. Minted from the LIVE pet's
  // counter: btlFoe was built in a throwaway Pet whose own counter means nothing.
  m.uid = pet.mintUid();
  return m;
}

// ---------- training submenu (5th icon) ----------

// Bars here show progress toward the IV-capped ceiling, not a raw stat: 100%
// means this individual cannot train the stat any higher, which is the whole
// point of trMaxFor() gating training by IV.
static uint8_t trainPct(uint8_t cur, uint8_t cap) {
  return cap ? (uint8_t)((uint16_t)cur * 100 / cap) : 0;
}

// ---------- the training menu ----------
#define TR_JOY 0x08   // display only: what a joy/bond game raises
#define TR_BOND 0x10
struct TrainGame {
  StrId name;
  uint8_t mask;       // TR_STR / TR_DEF / TR_SPE, or TR_JOY | TR_BOND
  void (*start)();
};
// Order is the menu's order. A new game is one row here.
static const TrainGame TRAIN_GAMES[] = {
  { S_GAME_BAG,   TR_STR,          startSack },
  { S_GAME_REACT, TR_SPE,          startSpeedGame },
  { S_GAME_BALL,  TR_DEF,          startGame },
  { S_GAME_ROCK,  TR_STR | TR_SPE, startRock },
  { S_GAME_BLOCK, TR_DEF | TR_SPE, startBlock },
  { S_GAME_TILT,  TR_SPE | TR_DEF, startTilt },
  { S_GAME_PUSH,  TR_STR | TR_DEF, startPush },
  { S_GAME_BERRY, TR_JOY | TR_BOND, startCatch },
  { S_GAME_SIMON, TR_JOY | TR_BOND, startSimon },
  { S_GAME_SEEK,  TR_JOY | TR_BOND, startSeek },
};
#define TRAIN_GAME_COUNT (sizeof(TRAIN_GAMES) / sizeof(TRAIN_GAMES[0]))
#define TRAIN_PER_PAGE 3
#define TRAIN_LIST_Y 80
#define TRAIN_LIST_H UI_TAP_FINGER
#define TRAIN_LIST_GAP 6
#define TRAIN_LIST_ROW_Y(i) (TRAIN_LIST_Y + (i) * (TRAIN_LIST_H + TRAIN_LIST_GAP))
#define TRAIN_BACK_ZONE_Y 396
#define TRAIN_BACK_LABEL_Y 404
static_assert(TRAIN_LIST_ROW_Y(TRAIN_PER_PAGE - 1) + TRAIN_LIST_H + 20 <= TRAIN_BACK_ZONE_Y,
              "a dead band between the last row and the back zone");
uint8_t trainPages() { return (uint8_t)((TRAIN_GAME_COUNT + TRAIN_PER_PAGE - 1) / TRAIN_PER_PAGE); }
uint8_t trainGameCount() { return (uint8_t)TRAIN_GAME_COUNT; }

// The rectangle of row i on the current page, asked of the round panel rather
// than assumed: a box's CORNERS are what leave the glass.
void trainRowRect(int i, int *x, int *y, int *w, int *h) {
  int ry = TRAIN_LIST_ROW_Y(i);
  // One width for every row: the narrowest the panel allows across the whole
  // list, which also keeps the middle row clear of the rim scrollbar.
  int half = uiSafeHalfWidthFor(TRAIN_LIST_ROW_Y(0),
                                TRAIN_LIST_ROW_Y(TRAIN_PER_PAGE - 1) + TRAIN_LIST_H) - 12;
  if (x) *x = CX - half;
  if (y) *y = ry;
  if (w) *w = 2 * half;
  if (h) *h = TRAIN_LIST_H;
}
// Which game a tap lands on, or -1.
int trainGameAt(int16_t x, int16_t y) {
  for (int i = 0; i < TRAIN_PER_PAGE; i++) {
    int idx = trainPage * TRAIN_PER_PAGE + i;
    if (idx >= (int)TRAIN_GAME_COUNT) break;
    int rx, ry, rw, rh;
    trainRowRect(i, &rx, &ry, &rw, &rh);
    if (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh) return idx;
  }
  return -1;
}
bool trainBackHit(int16_t x, int16_t y) { (void)x; return y >= TRAIN_BACK_ZONE_Y; }
// The centre of row i of the current page, for tests that tap a game: asked of
// the firmware, never retyped (CLAUDE.md § 3, a test's own copy of a layout).
void trainRowCenter(int i, int *x, int *y) {
  int rx, ry, rw, rh;
  trainRowRect(i, &rx, &ry, &rw, &rh);
  if (x) *x = rx + rw / 2;
  if (y) *y = ry + rh / 2;
}

// "STRENGTH 45% + SPEED 80%", or "JOY + BOND": what a game raises, and how
// far along each trained stat already is.
static void trainSubtitle(uint8_t mask, char *out, size_t n) {
  out[0] = 0;
  struct { uint8_t bit; StrId name; uint8_t cur, cap; } part[] = {
    { TR_STR, S_TR_ATK, pet.trAtk, pet.trMaxAtk() },
    { TR_SPE, S_TR_SPE, pet.trSpe, pet.trMaxSpe() },
    { TR_DEF, S_TR_DEF, pet.trDef, pet.trMaxDef() },
    { TR_JOY, S_ST_JOY, 0, 0 },
    { TR_BOND, S_ST_BOND, 0, 0 },
  };
  for (auto &p : part) {
    if (!(mask & p.bit)) continue;
    char one[28];
    if (p.cap) {
      uint8_t pct = trainPct(p.cur, p.cap);
      if (pct >= 100) snprintf(one, sizeof(one), "%s %s", T(p.name), T(S_ST_MAX));
      else snprintf(one, sizeof(one), "%s %u%%", T(p.name), (unsigned)pct);
    } else {
      snprintf(one, sizeof(one), "%s", T(p.name));
    }
    size_t l = strlen(out);
    snprintf(out + l, n - l, "%s%s", l ? " + " : "", one);
  }
}

void renderTrain() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(T(S_TRAIN)) * 6, 44);
  gfx->print(T(S_TRAIN));
  for (int i = 0; i < TRAIN_PER_PAGE; i++) {
    int idx = trainPage * TRAIN_PER_PAGE + i;
    if (idx >= (int)TRAIN_GAME_COUNT) break;
    const TrainGame &g = TRAIN_GAMES[idx];
    int rx, ry, rw, rh;
    trainRowRect(i, &rx, &ry, &rw, &rh);
    bool joyGame = g.mask & (TR_JOY | TR_BOND);
    gfx->fillRoundRect(rx, ry, rw, rh, 14, joyGame ? UI_BG_DAY : UI_WHITE);
    gfx->drawRoundRect(rx, ry, rw, rh, 14, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(T(g.name)) * 6, ry + 24);
    gfx->print(T(g.name));
    char sub[64];
    trainSubtitle(g.mask, sub, sizeof(sub));
    gfx->setTextColor(joyGame ? UI_BAR_BAD : UI_INK_SOFT);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(sub) * 3, ry + 58);
    gfx->print(sub);
  }
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(T(S_BACK)) * 6, TRAIN_BACK_LABEL_Y);
  gfx->print(T(S_BACK));
  uiChrome();     // the rim scrollbar: the list pages with a rim drag
  gfx->flush();   // without this the panel never updates and the screen freezes
}

void trainTap(int16_t x, int16_t y) {
  int idx = trainGameAt(x, y);
  if (idx >= 0) {
    sfxPlay(SFX_TAP);
    trainOpen = false;
    TRAIN_GAMES[idx].start();
    return;
  }
  if (trainBackHit(x, y)) { sfxPlay(SFX_TAP); trainOpen = false; }
}

// ---------- Berry Catch (joy + bond) ----------
// One gain on a result screen. A zero says WHY instead of reading as a broken
// game: "JOY MAX" when the stat is already at its ceiling, "BOND MAX TODAY"
// when the daily bond allowance is spent. A zero with room left is just +0.
static void gainPart(char *out, size_t n, uint8_t gain, StrId name, bool atMax, bool today) {
  if (!gain && today) snprintf(out, n, "%s %s", T(name), T(S_MAX_TODAY));
  else if (!gain && atMax) snprintf(out, n, "%s %s", T(name), T(S_ST_MAX));
  else snprintf(out, n, T(S_PLUS_FMT), gain, T(name));
}

// "+n JOY  +n BOND", or the reason either one paid nothing. Public for tests.
void joyGainText(char *out, size_t n, uint8_t joyGain, uint8_t bondGain) {
  char a[24], b[28];
  gainPart(a, sizeof(a), joyGain, S_ST_JOY, pet.joy >= 100, false);
  gainPart(b, sizeof(b), bondGain, S_ST_BOND, pet.bond >= 100, pet.bond < 100 && pet.bondCappedToday());
  snprintf(out, n, "%s  %s", a, b);
}
void startCatch() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  catchOpen = true;
  catchUntil = millis() + CATCH_MS;
  catchOverUntil = 0;
  catchNextSpawn = millis() + 400;
  catchBasketX = catchTarget = CX;
  catchScore = 0;
  catchOuchUntil = 0;
  for (auto &t : catchThings) t.live = false;
}

static void catchFinish() {
  uint8_t b0 = pet.bond;
  catchJoyGain = pet.playJoyGame(catchScore > 255 ? 255 : (uint8_t)catchScore);
  catchBondGain = pet.bond - b0;
  sfxPlay(catchScore >= 10 ? SFX_MEDAL : SFX_LEVEL);
  catchOverUntil = millis() + 3500;
}

// Leaving early keeps what was caught, like every other game.
void leaveCatch() {
  if (!catchOverUntil) pet.playJoyGame(catchScore > 255 ? 255 : (uint8_t)catchScore);
  catchOpen = false;
}

// One frame of the game. Public so a test can drive it without a panel.
void catchStep(uint32_t now) {
  if (catchOverUntil) return;
  if (now >= catchUntil) { catchFinish(); return; }
  float d = catchTarget - catchBasketX;                // the basket eases after the finger
  catchBasketX += d > 18 ? 18 : (d < -18 ? -18 : d);
  if (catchBasketX < 130) catchBasketX = 130;
  if (catchBasketX > 336) catchBasketX = 336;
  // Faster as the clock runs down: a spawn every 700 ms at the start, 380 at the end.
  uint32_t left = catchUntil - now;
  if (now >= catchNextSpawn) {
    for (auto &t : catchThings) {
      if (t.live) continue;
      t.live = true;
      t.x = 130 + random(207);
      t.y = 70;
      t.vy = 4.0f + (float)(CATCH_MS - left) / CATCH_MS * 4.0f;
      t.rock = random(100) < 22;
      break;
    }
    catchNextSpawn = now + 380 + left * 320 / CATCH_MS;
  }
  for (auto &t : catchThings) {
    if (!t.live) continue;
    t.y += t.vy;
    if (t.y >= CATCH_BASKET_Y - 8 && t.y <= CATCH_BASKET_Y + 14 &&
        fabsf(t.x - catchBasketX) <= CATCH_BASKET_HALF) {
      t.live = false;
      if (t.rock) { if (catchScore) catchScore--; catchOuchUntil = now + 600; sfxPlay(SFX_DENY); }
      else { catchScore++; sfxPlay(SFX_TAP); }
    } else if (t.y > 410) {
      t.live = false;                                   // missed: no penalty
    }
  }
}

void renderCatch() {
  uint32_t now = millis();
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  if (catchOverUntil) {
    if (now > catchOverUntil) { catchOpen = false; return; }
    char b[24];
    snprintf(b, sizeof(b), T(S_CATCH_FMT), catchScore);
    gfx->setTextColor(ink);
    gfx->setTextSize(3);
    gfx->setCursor(CX - (int)strlen(b) * 9, 160);
    gfx->print(b);
    char g[56];
    joyGainText(g, sizeof(g), catchJoyGain, catchBondGain);
    gfx->setTextColor(UI_BAR_BAD);
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(g) * 6, 220);
    gfx->print(g);
    gfx->flush();
    return;
  }
  catchStep(now);
  for (auto &t : catchThings) {
    if (!t.live) continue;
    if (t.rock) {
      // A rock has to read as "avoid" at a glance: dark and lumpy, with a red
      // cross on it -- a plain grey ball looked like it might be another berry.
      int x = (int)t.x, y = (int)t.y;
      uint16_t stone = C565(0x5a, 0x55, 0x50);
      gfx->fillCircle(x - 4, y + 2, 11, stone);
      gfx->fillCircle(x + 5, y + 3, 10, stone);
      gfx->fillCircle(x, y - 5, 10, stone);
      for (int k = -1; k <= 1; k++) {
        gfx->drawLine(x - 7 + k, y - 7, x + 7 + k, y + 7, UI_BAR_BAD);
        gfx->drawLine(x + 7 + k, y - 7, x - 7 + k, y + 7, UI_BAR_BAD);
      }
    } else {
      gfx->fillCircle((int)t.x, (int)t.y, 13, C565(0xd8, 0x3a, 0x4a));
      gfx->fillCircle((int)t.x - 4, (int)t.y - 4, 3, UI_WHITE);
      gfx->fillRect((int)t.x - 2, (int)t.y - 19, 4, 7, C565(0x3a, 0x8a, 0x3a));
    }
  }
  int bx = (int)catchBasketX;
  gfx->fillRoundRect(bx - CATCH_BASKET_HALF, CATCH_BASKET_Y, 2 * CATCH_BASKET_HALF, 26, 8,
                     C565(0xa8, 0x70, 0x38));
  gfx->drawRoundRect(bx - CATCH_BASKET_HALF, CATCH_BASKET_Y, 2 * CATCH_BASKET_HALF, 26, 8, ink);
  gfx->drawFastHLine(bx - CATCH_BASKET_HALF + 4, CATCH_BASKET_Y + 9, 2 * CATCH_BASKET_HALF - 8, ink);
  if (now < catchOuchUntil) {                       // a rock in the basket costs a berry
    gfx->setTextColor(UI_BAR_BAD);
    gfx->setTextSize(3);
    gfx->setCursor(bx - 18, CATCH_BASKET_Y - 34);
    gfx->print("-1");
  }
  char sc[8];
  snprintf(sc, sizeof(sc), "%u", catchScore);
  gfx->setTextColor(ink);
  gfx->setTextSize(4);
  gfx->setCursor(CX - (int)strlen(sc) * 12, 92);
  gfx->print(sc);
  if (CATCH_MS - (catchUntil - now) < 2500) {       // the hint, for the first moments
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(T(S_CATCH_HINT)) * 6, 140);
    gfx->print(T(S_CATCH_HINT));
  }
  uint32_t left = catchUntil > now ? catchUntil - now : 0;
  int bw = 260, fw = (int)((uint64_t)bw * left / CATCH_MS);
  gfx->fillRoundRect(CX - bw / 2, 396, bw, 14, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 396, fw, 14, 5, UI_BAR_OK);
  gfx->flush();
}

// ---------- the hybrid training games ----------
// Rock Smash (strength + speed), Block! (defence + speed), Tilt Maze (speed +
// defence). Each counts what the player did into mgScore, converts that into
// training points at the end, and pays them through Pet::trainSplit().
uint8_t mgMask = 0;
uint32_t mgStart = 0, mgUntil = 0, mgOverUntil = 0, mgLast = 0;
uint16_t mgScore = 0;          // what the screen counts: smashes, blocks, goals
uint16_t mgPoints = 0;         // what it pays, before trainSplit's session cap
uint8_t mgGain[3] = { 0, 0, 0 };   // STR, DEF, SPE actually gained
uint8_t mgJoyGain = 0, mgBondGain = 0;   // what a joy/bond game paid
bool mgNoSensor = false;
StrId mgGradeStr = S_MISS;     // PERFECT / GOOD / MISS, flashed until mgGradeUntil
uint32_t mgGradeUntil = 0;

// Rock Smash: a ring closes on the rock; tap as it lands. Beats speed up.
#define ROCK_BEATS 16
#define ROCK_LEAD_MS 1200
#define ROCK_PERIOD0 1100
#define ROCK_PERIOD_STEP 30
#define ROCK_PERIOD_MIN 650
#define ROCK_PERFECT_MS 90
#define ROCK_GOOD_MS 180
#define ROCK_EARLY_MS 350      // a tap earlier than this is not aimed at the beat
#define ROCK_R 46
#define ROCK_RING_R 150
#define ROCK_CY 236
uint8_t rockBeat = 0;          // the beat being waited on
uint32_t rockBeatAt = 0, rockPeriod = ROCK_PERIOD0;
uint8_t rockPerfects = 0, rockGoods = 0;

// Block!: attacks fly in from the four sides; tap that side before they land.
#define BLOCK_MS 20000UL
#define BLOCK_MAX 4
#define BLOCK_FROM 200
#define BLOCK_SHIELD 60
#define BLOCK_ATK_R 14         // an attack's drawn radius
#define BLOCK_FULL 20          // blocks for a full session
#define BLOCK_WHIFF_MS 400     // a tap at an empty side locks the shield this long
BlockAttack blockAtk[BLOCK_MAX];
uint32_t blockNext = 0, blockLockUntil = 0;
static const int8_t BLOCK_DX[4] = { 0, 1, 0, -1 };   // up, right, down, left
static const int8_t BLOCK_DY[4] = { -1, 0, 1, 0 };

// Tilt Maze: roll the ball into the goal, around the holes, by tilting the
// board. The QMI8658 sits a quarter-turn round from the panel, so its axes are
// SWAPPED relative to the screen, not merely signed differently: on a real
// board (2026-10-20) tilting down sent the ball left, left sent it up and up
// sent it right. tiltToScreen() is the one place that maps them.
#define TILT_MS 30000UL
#define TILT_ARENA 180
#define TILT_BALL 14
#define TILT_GOAL 26
#define TILT_HOLE 22
#define TILT_HOLES 2
#define TILT_K 9.0f            // px per frame per g of tilt
#define TILT_FRICTION 0.95f
// Sensor-axis change (dx, dy) -> the screen direction the ball should roll.
// Board-verified: sensor +x is the screen tilting DOWN, sensor +y is LEFT.
void tiltToScreen(float dx, float dy, float *sx, float *sy) {
  *sx = -dy;
  *sy = dx;
}
#define TILT_GOAL_PTS 3
float tiltBx = CX, tiltBy = CY, tiltVx = 0, tiltVy = 0, tilt0x = 0, tilt0y = 0;
float tiltGx = CX, tiltGy = CY, tiltHx[TILT_HOLES], tiltHy[TILT_HOLES];

static uint8_t mgCap(uint16_t v) { return v > 255 ? 255 : (uint8_t)v; }

static bool mgCanStart() { return !(pet.isEgg() || pet.sleeping || pet.ceremony); }

static void mgBegin(uint8_t which, uint8_t mask) {
  miniGame = which;
  mgMask = mask;
  mgStart = mgLast = millis();
  mgOverUntil = 0;
  mgScore = mgPoints = 0;
  mgGain[0] = mgGain[1] = mgGain[2] = 0;
  mgNoSensor = false;
  mgGradeUntil = 0;
}

static void mgGrading(StrId g, uint32_t now) { mgGradeStr = g; mgGradeUntil = now + 450; }

// Pays what was earned. Leaving early pays too, like every other game.
// A joy/bond game pays through Pet::playJoyGame() -- which also makes petting
// fresh again -- and pays for turning up even at 0, like Berry Catch; a
// training game pays through trainSplit() and charges nothing for nothing.
static void mgPay(bool finished) {
  if (mgMask & TR_JOY) {
    if (!finished && !mgPoints) return;
    uint8_t b0 = pet.bond;
    mgJoyGain = pet.playJoyGame(mgCap(mgPoints));
    mgBondGain = pet.bond - b0;
    return;
  }
  if (!mgPoints) return;                             // nothing earned, nothing charged
  uint8_t a = pet.trAtk, d = pet.trDef, sp = pet.trSpe;
  pet.trainSplit(mgMask, mgCap(mgPoints));
  mgGain[0] = pet.trAtk - a;
  mgGain[1] = pet.trDef - d;
  mgGain[2] = pet.trSpe - sp;
}

static void mgFinish(uint32_t now) {
  if (mgOverUntil) return;
  if (!mgNoSensor) mgPay(true);
  sfxPlay(mgNoSensor ? SFX_DENY : (mgPoints >= TRAIN_SESSION_MAX ? SFX_MEDAL : SFX_PLAY));
  mgOverUntil = now + 3500;
}

void leaveMini() {
  if (!mgOverUntil && !mgNoSensor) mgPay(false);
  miniGame = MG_NONE;
}

// --- Rock Smash
void startRock() {
  if (!mgCanStart()) return;
  mgBegin(MG_ROCK, TR_STR | TR_SPE);
  rockBeat = 0;
  rockPeriod = ROCK_PERIOD0;
  rockBeatAt = mgStart + ROCK_LEAD_MS;
  rockPerfects = rockGoods = 0;
}

static void rockAdvance() {
  rockBeat++;
  rockPeriod = rockPeriod > ROCK_PERIOD_MIN + ROCK_PERIOD_STEP ? rockPeriod - ROCK_PERIOD_STEP
                                                               : ROCK_PERIOD_MIN;
  rockBeatAt += rockPeriod;
  // Two points a perfect, one a good: a flawless round is exactly a full session.
  mgPoints = (uint16_t)((2 * rockPerfects + rockGoods) * TRAIN_SESSION_MAX / (2 * ROCK_BEATS));
}

static void rockPress(uint32_t now) {
  if (rockBeat >= ROCK_BEATS) return;
  if (now + ROCK_EARLY_MS < rockBeatAt) return;      // not aimed at this beat yet
  uint32_t dt = now > rockBeatAt ? now - rockBeatAt : rockBeatAt - now;
  if (dt <= ROCK_PERFECT_MS) { rockPerfects++; mgScore++; mgGrading(S_PERFECT, now); sfxPlay(SFX_HIT); }
  else if (dt <= ROCK_GOOD_MS) { rockGoods++; mgScore++; mgGrading(S_GOOD, now); sfxPlay(SFX_TAP); }
  else { mgGrading(S_MISS, now); sfxPlay(SFX_DENY); }
  rockAdvance();
}

static void rockStep(uint32_t now) {
  if (rockBeat < ROCK_BEATS && now > rockBeatAt + ROCK_GOOD_MS) {
    mgGrading(S_MISS, now);                          // let it pass: a miss
    rockAdvance();
  }
  if (rockBeat >= ROCK_BEATS && now > rockBeatAt - rockPeriod + ROCK_GOOD_MS) mgFinish(now);
}

// --- Block!
void startBlock() {
  if (!mgCanStart()) return;
  mgBegin(MG_BLOCK, TR_DEF | TR_SPE);
  mgUntil = mgStart + BLOCK_MS;
  blockNext = mgStart + 700;
  blockLockUntil = 0;
  for (auto &b : blockAtk) b.live = false;
}

int blockAttackR() { return BLOCK_ATK_R; }

// Which side of the creature a tap is on: the dominant axis from the centre.
uint8_t blockSideAt(int16_t x, int16_t y) {
  int dx = x - CX, dy = y - CY;
  if (abs(dx) > abs(dy)) return dx > 0 ? 1 : 3;
  return dy > 0 ? 2 : 0;
}

// How far in an attack is, 0 at spawn to 1 when it lands.
static float blockFrac(const BlockAttack &b, uint32_t now) {
  return now <= b.at ? 0.0f : (float)(now - b.at) / (float)b.dur;
}

// Where attack i is drawn, or false if it is not in the air. The render and
// the tests both ask this, so neither keeps its own copy of the lanes.
bool blockAttackPos(int i, uint32_t now, int *x, int *y) {
  const BlockAttack &b = blockAtk[i];
  if (!b.live) return false;
  float fr = blockFrac(b, now);
  int d = BLOCK_FROM - (int)((BLOCK_FROM - BLOCK_SHIELD) * (fr > 1 ? 1 : fr));
  *x = CX + BLOCK_DX[b.side] * d;
  *y = CY + BLOCK_DY[b.side] * d;
  return true;
}

static void blockPress(int16_t x, int16_t y, uint32_t now) {
  if (now >= mgUntil || now < blockLockUntil) return;
  uint8_t side = blockSideAt(x, y);
  BlockAttack *best = nullptr;
  for (auto &b : blockAtk)
    if (b.live && b.side == side && (!best || blockFrac(b, now) > blockFrac(*best, now))) best = &b;
  if (!best) {                                       // nothing there: the shield wobbles
    blockLockUntil = now + BLOCK_WHIFF_MS;
    mgGrading(S_MISS, now);
    sfxPlay(SFX_DENY);
    return;
  }
  best->live = false;
  mgScore++;
  mgPoints = (uint16_t)(mgScore * TRAIN_SESSION_MAX / BLOCK_FULL);
  sfxPlay(SFX_HIT);
}

static void blockStep(uint32_t now) {
  if (now >= mgUntil) { mgFinish(now); return; }
  uint32_t el = now - mgStart;
  for (auto &b : blockAtk)
    if (b.live && blockFrac(b, now) >= 1.0f) { b.live = false; mgGrading(S_MISS, now); sfxPlay(SFX_DENY); }
  if (now >= blockNext) {
    uint8_t taken = 0;
    for (auto &b : blockAtk) if (b.live) taken |= 1 << b.side;
    if (taken != 0x0F) {
      uint8_t side;
      do { side = random(4); } while (taken & (1 << side));
      for (auto &b : blockAtk) {
        if (b.live) continue;
        b.live = true;
        b.side = side;
        b.at = now;
        b.dur = 1500 - el * 700 / BLOCK_MS;          // 1.5 s to land, 0.8 s by the end
        break;
      }
    }
    blockNext = now + 1100 - el * 600 / BLOCK_MS;    // one every 1.1 s, 0.5 s by the end
  }
}

// --- Tilt Maze
static float tiltDist(float ax, float ay, float bx, float by) {
  return sqrtf((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}

// A random point inside the arena, at least `gap` from (x, y).
static void tiltSpot(float *ox, float *oy, float x, float y, float gap) {
  for (int tries = 0; tries < 40; tries++) {
    float a = random(360) * 0.0174533f, r = 30 + random(TILT_ARENA - 30 - TILT_GOAL);
    *ox = CX + r * cosf(a);
    *oy = CY + r * sinf(a);
    if (tiltDist(*ox, *oy, x, y) >= gap) return;
  }
}

static void tiltLayout() {
  tiltSpot(&tiltGx, &tiltGy, tiltBx, tiltBy, 110);
  for (int i = 0; i < TILT_HOLES; i++) {
    for (int tries = 0; tries < 40; tries++) {
      tiltSpot(&tiltHx[i], &tiltHy[i], tiltBx, tiltBy, 70);
      bool ok = tiltDist(tiltHx[i], tiltHy[i], tiltGx, tiltGy) >= TILT_GOAL + TILT_HOLE + 20;
      for (int j = 0; j < i; j++)
        if (tiltDist(tiltHx[i], tiltHy[i], tiltHx[j], tiltHy[j]) < 2 * TILT_HOLE + 10) ok = false;
      if (ok) break;
    }
  }
}

void startTilt() {
  if (!mgCanStart()) return;
  mgBegin(MG_TILT, TR_SPE | TR_DEF);
  mgUntil = mgStart + TILT_MS;
  tiltBx = CX; tiltBy = CY; tiltVx = tiltVy = 0;
  // Whatever angle the board is held at when the game opens is level.
  if (!pedoTilt(&tilt0x, &tilt0y)) {
    mgNoSensor = true;
    mgOverUntil = mgStart + 2500;
    sfxPlay(SFX_DENY);
    return;
  }
  tiltLayout();
}

static void tiltStep(uint32_t now) {
  if (now >= mgUntil) { mgFinish(now); return; }
  float tx, ty;
  if (!pedoTilt(&tx, &ty)) return;
  // Scaled by the real frame time: the panel draws slower on hardware.
  float f = (float)(now - mgLast) / 85.0f;
  if (f > 3) f = 3;
  float ax, ay;
  tiltToScreen(tx - tilt0x, ty - tilt0y, &ax, &ay);
  tiltVx = (tiltVx + ax * TILT_K * f) * TILT_FRICTION;
  tiltVy = (tiltVy + ay * TILT_K * f) * TILT_FRICTION;
  tiltBx += tiltVx * f;
  tiltBy += tiltVy * f;
  float d = tiltDist(tiltBx, tiltBy, CX, CY), lim = TILT_ARENA - TILT_BALL;
  if (d > lim) {                                     // the wall: clamp and bounce
    float nx = (tiltBx - CX) / d, ny = (tiltBy - CY) / d;
    tiltBx = CX + nx * lim;
    tiltBy = CY + ny * lim;
    float vn = tiltVx * nx + tiltVy * ny;
    if (vn > 0) { tiltVx -= 1.6f * vn * nx; tiltVy -= 1.6f * vn * ny; }
  }
  if (tiltDist(tiltBx, tiltBy, tiltGx, tiltGy) < TILT_GOAL - 6) {
    mgScore++;
    mgPoints = (uint16_t)(mgScore * TILT_GOAL_PTS);
    sfxPlay(SFX_LEVEL);
    tiltLayout();
    return;
  }
  for (int i = 0; i < TILT_HOLES; i++)
    if (tiltDist(tiltBx, tiltBy, tiltHx[i], tiltHy[i]) < TILT_HOLE - 4) {
      tiltBx = CX; tiltBy = CY; tiltVx = tiltVy = 0;  // fell in: back to the middle
      mgGrading(S_MISS, now);
      sfxPlay(SFX_DENY);
      return;
    }
}

// --- Boulder Push (strength + defence): a needle swings across a power bar;
// tap while it is in the green to shove the boulder up the slope. A tap
// outside the bar's middle lets it slip back, so mashing loses ground.
#define PUSH_MS 20000UL
#define PUSH_FULL 45           // steps for a full session (a perfect round is ~54)
#define PUSH_TOP 10            // steps from the foot of the slope to the top
#define PUSH_GREEN 0.12f       // half-widths of the zones, as a fraction of the bar
#define PUSH_YELLOW 0.30f
#define PUSH_SLIP_MS 350
// Slowed 25% in v3.37 (was 1/1400 and 1/900): on the board the panel's touch
// latency made the green too hard to hit in time.
#define PUSH_F0 (1.0f / 1867)  // needle swings per ms at the start...
#define PUSH_F1 (1.0f / 1200)  // ...and by the end
uint16_t pushSteps = 0;
uint32_t pushLockUntil = 0;

void startPush() {
  if (!mgCanStart()) return;
  mgBegin(MG_PUSH, TR_STR | TR_DEF);
  mgUntil = mgStart + PUSH_MS;
  pushSteps = 0;
  pushLockUntil = 0;
}

// Where the needle is, -1..1 across the bar. Its swing quickens over the round.
float pushNeedle(uint32_t now) {
  float t = (float)(now > mgStart ? now - mgStart : 0);
  float turns = PUSH_F0 * t + (PUSH_F1 - PUSH_F0) * t * t / (2.0f * PUSH_MS);
  return sinf(turns * 6.2831853f);
}

// 2 in the green, 1 in the yellow, 0 outside.
uint8_t pushZone(float v) {
  float a = fabsf(v);
  return a <= PUSH_GREEN ? 2 : a <= PUSH_YELLOW ? 1 : 0;
}

static void pushPress(uint32_t now) {
  if (now >= mgUntil || now < pushLockUntil) return;
  uint8_t z = pushZone(pushNeedle(now));
  if (z) {
    pushSteps += z;
    mgGrading(z == 2 ? S_PERFECT : S_GOOD, now);
    sfxPlay(z == 2 ? SFX_HIT : SFX_TAP);
  } else {
    if (pushSteps) pushSteps--;                      // it slips back
    pushLockUntil = now + PUSH_SLIP_MS;
    mgGrading(S_MISS, now);
    sfxPlay(SFX_DENY);
  }
  mgScore = pushSteps;
  mgPoints = (uint16_t)((uint32_t)pushSteps * TRAIN_SESSION_MAX / PUSH_FULL);
}

static void pushStep(uint32_t now) {
  if (now >= mgUntil) mgFinish(now);
}

// --- Memory (joy + bond): four berries light up in a sequence; tap them back.
// Each round adds one. A wrong berry ends the game; the longest run scores.
#define SIMON_MAX 12
#define SIMON_R 120            // the berries' distance from the centre
#define SIMON_BERRY 44
#define SIMON_ON_MS 450
#define SIMON_GAP_MS 200
#define SIMON_PAUSE_MS 700     // before each showing
uint8_t simonSeq[SIMON_MAX];
uint8_t simonLen = 0, simonPos = 0;
bool simonShowing = false;
uint32_t simonShowAt = 0, simonLitUntil = 0;
int8_t simonLit = -1;
static const uint16_t SIMON_COL[4] = { C565(0xd8, 0x3a, 0x4a), C565(0x3a, 0x7a, 0xd8),
                                       C565(0xe8, 0xc0, 0x30), C565(0x4a, 0xa8, 0x4a) };

void startSimon() {
  if (!mgCanStart()) return;
  mgBegin(MG_SIMON, TR_JOY | TR_BOND);
  for (auto &s : simonSeq) s = random(4);
  simonLen = 1;
  simonPos = 0;
  simonShowing = true;
  simonShowAt = mgStart + SIMON_PAUSE_MS;
  simonLit = -1;
}

// The centre of berry i (0 up, 1 right, 2 down, 3 left -- Block!'s sides).
void simonBerryPos(uint8_t i, int *x, int *y) {
  *x = CX + BLOCK_DX[i] * SIMON_R;
  *y = CY + BLOCK_DY[i] * SIMON_R;
}

// Which berry a tap lands on, or -1.
int simonBerryAt(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < 4; i++) {
    int bx, by;
    simonBerryPos(i, &bx, &by);
    int dx = x - bx, dy = y - by;
    if (dx * dx + dy * dy <= (SIMON_BERRY + 12) * (SIMON_BERRY + 12)) return i;
  }
  return -1;
}

static void simonPress(int16_t x, int16_t y, uint32_t now) {
  if (simonShowing) return;                          // watch first
  int b = simonBerryAt(x, y);
  if (b < 0) return;                                 // between berries: nothing
  simonLit = b;
  simonLitUntil = now + 250;
  if (b != simonSeq[simonPos]) {                     // wrong: the run is over
    sfxPlay(SFX_DENY);
    mgGrading(S_MISS, now);
    mgFinish(now);
    return;
  }
  sfxPlay(SFX_TAP);
  if (++simonPos < simonLen) return;
  mgScore = simonLen;                                // a whole run repeated
  mgPoints = (uint16_t)(mgScore * 2);
  if (simonLen >= SIMON_MAX) { mgFinish(now); return; }
  simonLen++;
  simonPos = 0;
  simonShowing = true;
  simonShowAt = now + SIMON_PAUSE_MS;
}

static void simonStep(uint32_t now) {
  if (!simonShowing) {
    if (now >= simonLitUntil) simonLit = -1;
    return;
  }
  if (now < simonShowAt) { simonLit = -1; return; }
  uint32_t k = (now - simonShowAt) / (SIMON_ON_MS + SIMON_GAP_MS);
  if (k >= simonLen) { simonShowing = false; simonLit = -1; return; }
  bool on = (now - simonShowAt) % (SIMON_ON_MS + SIMON_GAP_MS) < SIMON_ON_MS;
  simonLit = on ? simonSeq[k] : -1;
}

// --- Hide and Seek (joy + bond): the creature hides in one of three bushes,
// the bushes shuffle, and you point at where it went. Five rounds, faster each.
#define SEEK_ROUNDS 5
#define SEEK_Y 270
#define SEEK_GAP 120           // slot spacing
#define SEEK_HALF 52           // a bush's half-width, and its hit area
#define SEEK_SHOW_MS 1300
#define SEEK_REVEAL_MS 1100
#define SEEK_FIND_PTS 5
enum { SEEK_SHOW, SEEK_SHUFFLE, SEEK_PICK, SEEK_REVEAL };
uint8_t seekSlot[3] = { 0, 1, 2 };   // the slot (left..right) each bush stands in
uint8_t seekPet = 0;                 // the bush hiding the creature
uint8_t seekRound = 0, seekStage = SEEK_SHOW, seekSwapsLeft = 0, seekA = 0, seekB = 1;
uint8_t seekPicked = 0;
uint32_t seekStageAt = 0, seekSwapMs = 500;

static void seekNextRound(uint32_t now) {
  seekStage = SEEK_SHOW;
  seekStageAt = now;
  seekPet = random(3);
  seekSwapsLeft = 3 + seekRound;
  seekSwapMs = 520 - 60 * seekRound;
}

void startSeek() {
  if (!mgCanStart()) return;
  mgBegin(MG_SEEK, TR_JOY | TR_BOND);
  for (uint8_t i = 0; i < 3; i++) seekSlot[i] = i;
  seekRound = 0;
  seekNextRound(mgStart);
}

// Where bush i is drawn right now: mid-swap the two moving bushes travel on
// opposite arcs so they never pass through each other.
void seekBushPos(uint8_t i, uint32_t now, int *x, int *y) {
  float sx = CX + ((int)seekSlot[i] - 1) * SEEK_GAP;
  *x = (int)sx;
  *y = SEEK_Y;
  if (seekStage != SEEK_SHUFFLE || (i != seekA && i != seekB)) return;
  uint8_t other = i == seekA ? seekB : seekA;
  float tx = CX + ((int)seekSlot[other] - 1) * SEEK_GAP;
  float f = (float)(now - seekStageAt) / (float)seekSwapMs;
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  *x = (int)(sx + (tx - sx) * f);
  // The arc rises with the distance, so a swap of the two outer bushes clears
  // the one standing between them.
  int lift = 45 * abs((int)seekSlot[other] - (int)seekSlot[i]);
  *y = SEEK_Y + (int)((i == seekA ? -lift : lift) * sinf(f * 3.14159f));
}

// Which bush a tap lands on, or -1. Only asked while the bushes stand still.
int seekBushAt(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < 3; i++) {
    int bx, by;
    seekBushPos(i, 0, &bx, &by);
    if (abs(x - bx) <= SEEK_HALF && abs(y - by) <= 70) return i;
  }
  return -1;
}

static void seekPress(int16_t x, int16_t y, uint32_t now) {
  if (seekStage != SEEK_PICK) return;
  int b = seekBushAt(x, y);
  if (b < 0) return;
  seekPicked = b;
  bool right = b == seekPet;
  if (right) { mgScore++; sfxPlay(SFX_LEVEL); mgGrading(S_FOUND, now); }
  else       { sfxPlay(SFX_DENY); mgGrading(S_NOT_HERE, now); }
  mgPoints = (uint16_t)(mgScore * SEEK_FIND_PTS);
  seekStage = SEEK_REVEAL;
  seekStageAt = now;
}

static void seekStep(uint32_t now) {
  switch (seekStage) {
    case SEEK_SHOW:
      if (now - seekStageAt >= SEEK_SHOW_MS) {
        seekStage = SEEK_SHUFFLE;
        seekStageAt = now;
        seekA = random(3);
        seekB = (seekA + 1 + random(2)) % 3;
      }
      break;
    case SEEK_SHUFFLE:
      if (now - seekStageAt >= seekSwapMs) {
        uint8_t t = seekSlot[seekA];
        seekSlot[seekA] = seekSlot[seekB];
        seekSlot[seekB] = t;
        seekStageAt = now;
        if (--seekSwapsLeft == 0) { seekStage = SEEK_PICK; break; }
        seekA = random(3);
        seekB = (seekA + 1 + random(2)) % 3;
      }
      break;
    case SEEK_REVEAL:
      if (now - seekStageAt >= SEEK_REVEAL_MS) {
        if (++seekRound >= SEEK_ROUNDS) mgFinish(now);
        else seekNextRound(now);
      }
      break;
  }
}

static void drawBush(int x, int y) {
  uint16_t dark = C565(0x2e, 0x6e, 0x34), light = C565(0x4a, 0x9a, 0x4a);
  gfx->fillCircle(x - 26, y + 6, 30, dark);
  gfx->fillCircle(x + 26, y + 6, 30, dark);
  gfx->fillCircle(x, y - 14, 36, light);
  gfx->fillCircle(x - 12, y - 20, 12, C565(0x6a, 0xb8, 0x5a));
}

// Above this a tap leaves the game. In Block! that is above the top edge of
// an attack just launched from the top.
int mgLeaveStripY() {
  if (miniGame == MG_BLOCK) return CY - BLOCK_FROM - BLOCK_ATK_R;
  if (miniGame == MG_SIMON) return CY - SIMON_R - SIMON_BERRY - 12;   // above the top berry's hit area
  return 72;
}
// --- shared
void mgPress(int16_t x, int16_t y, uint32_t now) {
  if (mgOverUntil) return;
  if (miniGame == MG_ROCK) rockPress(now);
  else if (miniGame == MG_BLOCK) blockPress(x, y, now);
  else if (miniGame == MG_PUSH) pushPress(now);
  else if (miniGame == MG_SIMON) simonPress(x, y, now);
  else if (miniGame == MG_SEEK) seekPress(x, y, now);
}

// One frame of whichever game is open. Public so a test can drive it.
void mgStep(uint32_t now) {
  if (mgOverUntil) {
    if (now > mgOverUntil) miniGame = MG_NONE;
    return;
  }
  if (miniGame == MG_ROCK) rockStep(now);
  else if (miniGame == MG_BLOCK) blockStep(now);
  else if (miniGame == MG_TILT) tiltStep(now);
  else if (miniGame == MG_PUSH) pushStep(now);
  else if (miniGame == MG_SIMON) simonStep(now);
  else if (miniGame == MG_SEEK) seekStep(now);
  mgLast = now;
}

static void mgCentered(const char *s, int y, int size, uint16_t col) {
  gfx->setTextColor(col);
  gfx->setTextSize(size);
  gfx->setCursor(CX - (int)strlen(s) * 3 * size, y);
  gfx->print(s);
}

static void mgTimeBar(uint32_t now, uint32_t total, int y) {
  uint32_t left = mgUntil > now ? mgUntil - now : 0;
  int bw = 260, fw = (int)((uint64_t)bw * left / total);
  gfx->fillRoundRect(CX - bw / 2, y, bw, 14, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, y, fw, 14, 5, UI_BAR_OK);
}

static void renderMiniResult(uint16_t ink) {
  if (mgNoSensor) {
    mgCentered(T(S_TILT_NOSENSOR), 200, 2, ink);
    return;
  }
  char b[28];
  if (miniGame == MG_SEEK) snprintf(b, sizeof(b), T(S_SEEK_FMT), mgScore, (unsigned)SEEK_ROUNDS);
  else {
    StrId fmt = miniGame == MG_ROCK ? S_SMASH_FMT : miniGame == MG_BLOCK ? S_BLOCKED_FMT :
                miniGame == MG_TILT ? S_TILT_DONE_FMT : miniGame == MG_PUSH ? S_PUSHED_FMT : S_SIMON_FMT;
    snprintf(b, sizeof(b), T(fmt), mgScore);
  }
  mgCentered(b, 160, 3, ink);
  if (mgMask & TR_JOY) {
    char g[56];
    joyGainText(g, sizeof(g), mgJoyGain, mgBondGain);
    mgCentered(g, 220, 2, UI_BAR_BAD);
    return;
  }
  // The two stats the game trains, in the order its name gives them.
  static const StrId NAMES[3] = { S_TR_ATK, S_TR_DEF, S_TR_SPE };
  int first = miniGame == MG_ROCK || miniGame == MG_PUSH ? 0 : miniGame == MG_BLOCK ? 1 : 2;
  int second = miniGame == MG_TILT || miniGame == MG_PUSH ? 1 : 2;
  const uint8_t tr[3] = { pet.trAtk, pet.trDef, pet.trSpe };
  const uint8_t cap[3] = { pet.trMaxAtk(), pet.trMaxDef(), pet.trMaxSpe() };
  char a[24], c[24], g[52];
  gainPart(a, sizeof(a), mgGain[first], NAMES[first], tr[first] >= cap[first], false);
  gainPart(c, sizeof(c), mgGain[second], NAMES[second], tr[second] >= cap[second], false);
  snprintf(g, sizeof(g), "%s  %s", a, c);
  mgCentered(g, 220, 2, UI_BAR_BAD);
}

void renderMini() {
  uint32_t now = millis();
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  mgStep(now);
  if (!miniGame) return;
  if (mgOverUntil) {
    renderMiniResult(ink);
    gfx->flush();
    return;
  }
  bool hint = now - mgStart < 2500;
  if (miniGame == MG_ROCK) {
    // The rock cracks a little more with every smash.
    uint16_t stone = C565(0x8a, 0x82, 0x7a);
    gfx->fillCircle(CX, ROCK_CY, ROCK_R, stone);
    gfx->drawCircle(CX, ROCK_CY, ROCK_R, ink);
    for (uint8_t i = 0; i < mgScore && i < ROCK_BEATS; i++) {
      float a = i * 2.39996f;                        // golden angle: cracks spread evenly
      int x1 = CX + (int)(10 * cosf(a)), y1 = ROCK_CY + (int)(10 * sinf(a));
      int x2 = CX + (int)((ROCK_R - 4) * cosf(a + 0.3f)), y2 = ROCK_CY + (int)((ROCK_R - 4) * sinf(a + 0.3f));
      gfx->drawLine(x1, y1, x2, y2, ink);
    }
    if (rockBeat < ROCK_BEATS && now + rockPeriod >= rockBeatAt) {
      int32_t to = (int32_t)(rockBeatAt - now);
      int r = ROCK_R + (int)((int64_t)(ROCK_RING_R - ROCK_R) * (to > 0 ? to : 0) / (int32_t)rockPeriod);
      uint16_t col = to <= ROCK_PERFECT_MS && to >= -ROCK_PERFECT_MS ? UI_BAR_OK : UI_BAR_WARN;
      gfx->drawCircle(CX, ROCK_CY, r, col);
      gfx->drawCircle(CX, ROCK_CY, r + 1, col);
      gfx->drawCircle(CX, ROCK_CY, r + 2, col);
    }
    char c[12];
    snprintf(c, sizeof(c), "%u/%u", (unsigned)(rockBeat < ROCK_BEATS ? rockBeat + 1 : ROCK_BEATS),
             (unsigned)ROCK_BEATS);
    mgCentered(c, 400, 2, ink);
    if (hint) mgCentered(T(S_ROCK_HINT), 62, 2, ink);
  } else if (miniGame == MG_BLOCK) {
    const uint8_t *th = pet.isEgg() ? nullptr : thumbs.get(pet.speciesId);
    if (th) drawThumb(th, CX - thumbCell() / 2, CY - thumbCell() / 2, 3, false);
    bool locked = now < blockLockUntil;
    gfx->drawCircle(CX, CY, BLOCK_SHIELD, locked ? UI_BAR_BAD : ink);
    gfx->drawCircle(CX, CY, BLOCK_SHIELD + 1, locked ? UI_BAR_BAD : ink);
    for (int i = 0; i < BLOCK_MAX; i++) {
      int x, y;
      if (!blockAttackPos(i, now, &x, &y)) continue;
      gfx->fillCircle(x, y, BLOCK_ATK_R, C565(0xf0, 0x80, 0x30));
      gfx->fillCircle(x, y, BLOCK_ATK_R / 2, C565(0xff, 0xd8, 0x60));
      gfx->drawCircle(x, y, BLOCK_ATK_R, ink);
    }
    // The four lanes are the axes, so the score and the clock sit on the
    // diagonals, where nothing ever flies.
    char c[8];
    snprintf(c, sizeof(c), "%u", mgScore);
    gfx->setTextColor(ink);
    gfx->setTextSize(3);
    gfx->setCursor(CX - 120 - (int)strlen(c) * 9, CY - 132);
    gfx->print(c);
    uint32_t left = mgUntil > now ? (mgUntil - now + 999) / 1000 : 0;
    snprintf(c, sizeof(c), "%lu", (unsigned long)left);
    gfx->setTextColor(left <= 5 ? UI_BAR_BAD : ink);
    gfx->setCursor(CX + 120 - (int)strlen(c) * 9, CY - 132);
    gfx->print(c);
    if (hint) mgCentered(T(S_BLOCK_HINT), CY + 96, 2, ink);
  } else if (miniGame == MG_TILT) {
    gfx->drawCircle(CX, CY, TILT_ARENA, ink);
    gfx->drawCircle(CX, CY, TILT_ARENA + 1, ink);
    gfx->fillCircle((int)tiltGx, (int)tiltGy, TILT_GOAL, UI_BAR_OK);
    gfx->drawCircle((int)tiltGx, (int)tiltGy, TILT_GOAL, ink);
    for (int i = 0; i < TILT_HOLES; i++) gfx->fillCircle((int)tiltHx[i], (int)tiltHy[i], TILT_HOLE, RGB565_BLACK);
    gfx->fillCircle((int)tiltBx, (int)tiltBy, TILT_BALL, UI_WHITE);
    gfx->drawCircle((int)tiltBx, (int)tiltBy, TILT_BALL, ink);
    char c[8];
    snprintf(c, sizeof(c), "%u", mgScore);
    mgCentered(c, 22, 3, ink);
    if (hint) mgCentered(T(S_TILT_HINT), 120, 2, ink);
    mgTimeBar(now, TILT_MS, 428);
  } else if (miniGame == MG_PUSH) {
    // The slope, the boulder on it, and the creature shoving from below.
    const int x0 = 70, y0 = 360, x1 = 400, y1 = 190;
    gfx->fillTriangle(x0, y0 + 4, x1, y1 + 4, x1, y0 + 4, C565(0x8a, 0x6a, 0x4a));
    gfx->drawLine(x0, y0, x1, y1, ink);
    float f = 0.1f + 0.8f * (float)(pushSteps % PUSH_TOP) / PUSH_TOP;
    int bx = x0 + (int)((x1 - x0) * f), by = y0 + (int)((y1 - y0) * f) - 34;
    gfx->fillCircle(bx, by, 34, C565(0x8a, 0x82, 0x7a));
    gfx->drawCircle(bx, by, 34, ink);
    const uint8_t *th = pet.isEgg() ? nullptr : thumbs.get(pet.speciesId);
    float fc = f - 0.15f;                            // the creature, just behind it on the slope
    int cx = x0 + (int)((x1 - x0) * fc), cy = y0 + (int)((y1 - y0) * fc) - 30;
    if (th) drawThumb(th, cx - thumbCell() / 2, cy - thumbCell() / 2, 2, false);
    // The power bar along the top: yellow, green in the middle, and the needle.
    const int bw = 280, bh = 22, bxl = CX - bw / 2, byt = 84;
    gfx->fillRoundRect(bxl, byt, bw, bh, 6, UI_TRACK);
    int yw = (int)(bw / 2 * PUSH_YELLOW), gw = (int)(bw / 2 * PUSH_GREEN);
    gfx->fillRect(CX - yw, byt, 2 * yw, bh, UI_BAR_WARN);
    gfx->fillRect(CX - gw, byt, 2 * gw, bh, UI_BAR_OK);
    gfx->drawRoundRect(bxl, byt, bw, bh, 6, ink);
    int nx = CX + (int)(pushNeedle(now) * (bw / 2 - 4));
    gfx->fillRect(nx - 3, byt - 8, 6, bh + 16, now < pushLockUntil ? UI_BAR_BAD : ink);
    char c[8];
    snprintf(c, sizeof(c), "%u", (unsigned)(pushSteps / PUSH_TOP));   // boulders up the hill
    mgCentered(c, 30, 3, ink);
    if (hint) mgCentered(T(S_PUSH_HINT), 126, 2, ink);
    mgTimeBar(now, PUSH_MS, 420);
  } else if (miniGame == MG_SIMON) {
    for (uint8_t i = 0; i < 4; i++) {
      int x, y;
      simonBerryPos(i, &x, &y);
      bool lit = simonLit == (int8_t)i;
      gfx->fillCircle(x, y, SIMON_BERRY, lit ? UI_WHITE : SIMON_COL[i]);
      if (lit) gfx->fillCircle(x, y, SIMON_BERRY - 8, SIMON_COL[i]);
      gfx->drawCircle(x, y, SIMON_BERRY, ink);
      gfx->fillRect(x - 3, y - SIMON_BERRY - 10, 6, 12, C565(0x3a, 0x8a, 0x3a));
    }
    const uint8_t *th = pet.isEgg() ? nullptr : thumbs.get(pet.speciesId);
    if (th) drawThumb(th, CX - thumbCell() / 2, CY - thumbCell() / 2, 2, false);
    mgCentered(T(simonShowing ? S_SIMON_WATCH : S_SIMON_GO), 414, 2, ink);
    char c[8];
    snprintf(c, sizeof(c), "%u", mgScore);
    gfx->setTextColor(ink);
    gfx->setTextSize(3);
    gfx->setCursor(CX - 120 - (int)strlen(c) * 9, CY - 132);
    gfx->print(c);
  } else if (miniGame == MG_SEEK) {
    // The creature peeks out while it is shown and when it is found.
    bool peek = seekStage == SEEK_SHOW || (seekStage == SEEK_REVEAL);
    const uint8_t *th = pet.isEgg() ? nullptr : thumbs.get(pet.speciesId);
    for (uint8_t i = 0; i < 3; i++) {
      int x, y;
      seekBushPos(i, now, &x, &y);
      if (peek && i == seekPet && th) drawThumb(th, x - thumbCell() / 2, y - 60 - thumbCell() / 2, 2, false);
      drawBush(x, y);
      if (seekStage == SEEK_REVEAL && i == seekPicked && i != seekPet)
        gfx->drawCircle(x, y - 10, SEEK_HALF, UI_BAR_BAD);
    }
    char c[12];
    snprintf(c, sizeof(c), "%u/%u", (unsigned)(seekRound < SEEK_ROUNDS ? seekRound + 1 : SEEK_ROUNDS),
             (unsigned)SEEK_ROUNDS);
    mgCentered(c, 60, 2, ink);
    if (seekStage == SEEK_SHOW && seekRound == 0) mgCentered(T(S_SEEK_HINT), 110, 2, ink);
    if (seekStage == SEEK_PICK) mgCentered(T(S_SEEK_PICK), 110, 2, ink);
  }
  if (now < mgGradeUntil) {
    uint16_t col = mgGradeStr == S_PERFECT || mgGradeStr == S_FOUND ? UI_BAR_OK :
                   mgGradeStr == S_GOOD ? UI_BAR_WARN : UI_BAR_BAD;
    int gy = miniGame == MG_ROCK ? 330 : miniGame == MG_PUSH ? 150 : 360;
    mgCentered(T(mgGradeStr), gy, 3, col);
  }
  gfx->flush();
}

// ---------- the box ----------
// Storage past the six that fight. A creature is moved by picking a party slot
// and then a box slot, which swaps them -- so one gesture covers deposit,
// withdraw and exchange rather than needing three.
void renderBox() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  char head[32];
  snprintf(head, sizeof(head), T(S_BOX_FMT), party.boxCount(), BOX_SLOTS);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(head) * 6, 40);
  gfx->print(head);
  if (boxSwapFrom) {
    const PartyMon &p = party.slots[boxSwapFrom - 1];
    char sub[40];
    snprintf(sub, sizeof(sub), T(S_BOX_SWAP),
             p.empty() ? "-" : (p.nick[0] ? p.nick : DEX_TBL[p.dex].name));
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(sub) * 3, PSLOT_PROMPT_Y);
    gfx->print(sub);
  }
  for (uint8_t i = 0; i < BOX_PER_PAGE; i++) {
    uint8_t idx = boxPage * BOX_PER_PAGE + i;
    if (idx >= BOX_SLOTS) break;
    drawPartySlot(i, party.box[idx]);
  }
  {
    char pg[12];
    snprintf(pg, sizeof(pg), "%u/%u", (unsigned)(boxPage + 1),
             (unsigned)(BOX_SLOTS / BOX_PER_PAGE));
    drawPartyHub(pg, T(S_BOX_BTN), boxSwapFrom != 0);
  }
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_BACK)) * 6, 392);
  gfx->print(T(S_BACK));
  uiChrome();
  gfx->flush();
}

void boxTap(int16_t x, int16_t y) {
  // The sheet is checked first and is modal in the same way the party's is.
  if (boxDetail) {
    if (releaseConfirm) { monSheetConfirmTap(x, y, true); return; }
    if (monSheetBtn(x, y, true)) {          // TO PARTY
      int free = party.firstFree();
      if (free < 0) { sfxPlay(SFX_DENY); return; }
      party.swapPartyBox((uint8_t)free, boxDetail - 1);
      boxDetail = 0;
      boxSel = 0;
      sfxPlay(SFX_MEDAL);
      return;
    }
    if (monSheetBtn(x, y, false)) {         // RELEASE -- ask first, always
      releaseConfirm = true;
      sfxPlay(SFX_TAP);
      return;
    }
    boxDetail = 0;                          // anywhere else backs out
    releaseConfirm = false;
    sfxPlay(SFX_TAP);
    return;
  }
  for (uint8_t i = 0; i < BOX_PER_PAGE; i++) {
    uint8_t idx = boxPage * BOX_PER_PAGE + i;
    if (idx >= BOX_SLOTS) break;
    if (partySlotAt(x, y) != (int)i) continue;
    if (boxSwapFrom) {           // a party slot is waiting: complete the trade
      if (party.slots[boxSwapFrom - 1].empty() && party.box[idx].empty()) {
        sfxPlay(SFX_DENY);
        return;
      }
      party.swapPartyBox(boxSwapFrom - 1, idx);
      boxSwapFrom = 0;
      boxSel = 0;
      sfxPlay(SFX_MEDAL);
      return;
    }
    // Otherwise open its SHEET. It used to go straight to the party, which is a
    // lot to happen from one tap and left nowhere to put RELEASE; the sheet
    // offers TO PARTY explicitly and shows what you are about to move.
    if (party.box[idx].empty()) { sfxPlay(SFX_DENY); return; }
    if (party.firstFree() < 0) {
      boxSel = idx + 1;          // party is full: go choose who steps out
      boxOpen = false;
      sfxPlay(SFX_TAP);
      return;
    }
    boxDetail = idx + 1;
    releaseConfirm = false;
    sfxPlay(SFX_TAP);
    return;
  }
  boxOpen = false;               // anywhere else backs out
  boxSwapFrom = 0;
  boxDetail = 0;
  releaseConfirm = false;
}

// ---------- party ----------

void drawPartySlot(int i, const PartyMon &m) {
  int cx, cy;
  partySlotPos(i, &cx, &cy);
  gfx->fillCircle(cx, cy, PSLOT_R, m.empty() ? UI_TRACK : UI_WHITE);
  gfx->drawCircle(cx, cy, PSLOT_R, UI_INK);
  gfx->drawCircle(cx, cy, PSLOT_R - 1, UI_INK);
  if (m.empty()) {
    gfx->setTextColor(0x8410);
    gfx->setTextSize(2);
    gfx->setCursor(cx - (int)strlen(T(S_PARTY_EMPTY)) * 6, cy - 8);
    gfx->print(T(S_PARTY_EMPTY));
    return;
  }
  const uint8_t *th = thumbs.get(m.dex);
  if (th) drawThumb(th, cx - 24, cy - 44, 1, false);
  const DexEntry &d = DEX_TBL[m.dex];
  const char *nm = m.nick[0] ? m.nick : d.name;
  gfx->setTextColor(d.accent);
  gfx->setTextSize(1);
  gfx->setCursor(cx - (int)strlen(nm) * 3, cy + 8);
  gfx->print(nm);
  if (m.shiny) {
    gfx->setTextColor(UI_BAR_WARN);
    gfx->setCursor(cx + (int)strlen(nm) * 3 + 3, cy + 8);
    gfx->print("*");
  }
  char lv[12];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)m.level);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(cx - (int)strlen(lv) * 6, cy + 20);
  gfx->print(lv);
}

// The hub in the middle of the ring. On the party it is the BOX button; in the
// box it is the page. Either way the centre of a round panel is the easiest
// place on it to hit, so the primary action belongs there.
void drawPartyHub(const char *top, const char *bot, bool armed) {
  gfx->fillCircle(CX, PSLOT_CY, PSLOT_HUB_R, armed ? UI_BAR_WARN : UI_BG_DAY);
  gfx->drawCircle(CX, PSLOT_CY, PSLOT_HUB_R, UI_INK);
  gfx->drawCircle(CX, PSLOT_CY, PSLOT_HUB_R - 1, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(top) * 6, PSLOT_CY - 14);
  gfx->print(top);
  gfx->setTextSize(1);
  gfx->setCursor(CX - (int)strlen(bot) * 3, PSLOT_CY + 10);
  gfx->print(bot);
}

void renderParty() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);

  char head[24];
  snprintf(head, sizeof(head), T(S_PARTY_FMT), party.count());
  gfx->setTextColor(UI_INK);
  if (cordPick) {
    // The Linking Cord asking for KARRABLAST's or SHELMET's partner: the
    // question REPLACES the title. Under it, it collided with the size-3
    // header; the count is not what anybody is looking at here.
    gfx->setTextSize(2);
    gfx->setCursor(CX - (int)strlen(T(S_CORD_PICK)) * 6, 52);
    gfx->print(T(S_CORD_PICK));
  } else {
    gfx->setTextSize(3);
    gfx->setCursor(CX - (int)strlen(head) * 9, 42);
    gfx->print(head);
  }

  // the box lives behind the hub in the middle of the ring; it also shows how
  // full it is, so the player knows there is anything in there without opening
  if (!partyPick && !cordPick) {
    char bl[24];
    snprintf(bl, sizeof(bl), "%u/%u", party.boxCount(), BOX_SLOTS);
    drawPartyHub(bl, T(S_BOX_BTN), boxSwapFrom != 0);
  }

  if (boxSel) {
    const PartyMon &b = party.box[boxSel - 1];
    char sw[44];
    snprintf(sw, sizeof(sw), T(S_BOX_SWAP),
             b.nick[0] ? b.nick : DEX_TBL[b.dex].name);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(sw) * 3, PSLOT_PROMPT_Y);
    gfx->print(sw);
  }

  // when a newcomer is waiting, say so instead of the usual hint. The urgency
  // is carried by the red CLOSE button below, not by tinting the sentence:
  // UI_BAR_BAD as small text on cream is 3.2:1.
  if (partyPick) {
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(1);
    gfx->setCursor(CX - (int)strlen(T(S_PARTY_FULL)) * 3, PSLOT_PROMPT_Y);
    gfx->print(T(S_PARTY_FULL));
  }

  for (int i = 0; i < PARTY_SLOTS; i++) drawPartySlot(i, party.slots[i]);

  // exit: an explicit button, always in the same place
  const char *ex = partyPick ? T(S_PARTY_LETGO) : T(S_CLOSE);
  gfx->fillRoundRect(PARTYCLOSE_X, PARTYCLOSE_Y, PARTYCLOSE_W, PARTYCLOSE_H, 12,
                     partyPick ? UI_BAR_BAD : UI_TRACK);
  gfx->drawRoundRect(PARTYCLOSE_X, PARTYCLOSE_Y, PARTYCLOSE_W, PARTYCLOSE_H, 12, UI_INK);
  gfx->setTextColor(partyPick ? UI_WHITE : UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(ex) * 6, PARTYCLOSE_Y + 14);
  gfx->print(ex);
  uiChrome();
  gfx->flush();
}

// ---------- teclado para renombrar ----------

static const char KB_KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-";  // 28 + DEL + OK = 30
#define KB_COLS 6
#define KB_X 40
#define KB_Y 150
#define KB_W 64
#define KB_H 52

// The keyboard is shared, so it has to be told what it is naming. It used to
// hardcode pet.rename() on commit, which is why a second caller needed this.
void openKeyboardFor(uint8_t target) {
  kbTarget = target;
  kbOpen = true;
  const char *cur = (target == KB_TRAINER) ? pet.trainerName : pet.nick;
  strncpy(nameBuf, cur, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = 0;
  nameLen = strlen(nameBuf);
}
void openKeyboard() { openKeyboardFor(KB_PET); }

void renderKeyboard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_NAME)) * 6, 56);
  gfx->print(T(S_NAME));
  // buffer actual
  gfx->fillRoundRect(83, 84, 300, 40, 8, UI_WHITE);
  gfx->drawRoundRect(83, 84, 300, 40, 8, UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(95, 94);
  gfx->print(nameLen ? nameBuf : "_");

  for (int i = 0; i < 30; i++) {
    int x = KB_X + (i % KB_COLS) * KB_W, y = KB_Y + (i / KB_COLS) * KB_H;
    bool special = (i >= 28);
    gfx->fillRoundRect(x, y, KB_W - 6, KB_H - 6, 6, special ? UI_BAR_WARN : UI_WHITE);
    gfx->drawRoundRect(x, y, KB_W - 6, KB_H - 6, 6, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    if (i < 28) {
      gfx->setCursor(x + KB_W / 2 - 9, y + KB_H / 2 - 10);
      gfx->print(KB_KEYS[i]);
    } else {
      const char *lab = (i == 28) ? "<-" : "OK";
      gfx->setCursor(x + KB_W / 2 - 15, y + KB_H / 2 - 10);
      gfx->print(lab);
    }
  }
  gfx->flush();
}

void keyboardTap(int16_t x, int16_t y) {
  int col = (x - KB_X) / KB_W, row = (y - KB_Y) / KB_H;
  if (col < 0 || col >= KB_COLS || row < 0 || row >= 5) return;
  int i = row * KB_COLS + col;
  if (i >= 30) return;
  if (i == 28) {  // borrar
    if (nameLen) nameBuf[--nameLen] = 0;
  } else if (i == 29) {  // OK
    if (kbTarget == KB_TRAINER) pet.renameTrainer(nameBuf);
    else pet.rename(nameBuf);
    kbOpen = false;
  } else if (nameLen < sizeof(nameBuf) - 1) {
    nameBuf[nameLen++] = KB_KEYS[i];
    nameBuf[nameLen] = 0;
  }
}

// ---------- galeria pokedex ----------

#define GAL_X 73
#define GAL_Y 84
#define GAL_CELL 80

// The box drawThumb() centres a sprite in, for callers defined above GAL_CELL.
int thumbCell() { return GAL_CELL; }

// dibuja una miniatura centrada en su celda; sil=true la pinta en tinta
void drawThumb(const uint8_t *b, int x, int y, int s, bool sil) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int ox = x + (GAL_CELL - w * s) / 2;
  int oy = y + (GAL_CELL - h * s) / 2;
  for (int r = 0; r < h; r++) {
    for (int c = 0; c < w; c++) {
      uint8_t idx = d[r * w + c];
      if (idx == 0xFF) continue;
      uint16_t col = sil ? INK_K : (uint16_t)(pal[idx * 2] | (pal[idx * 2 + 1] << 8));
      gfx->fillRect(ox + c * s, oy + r * s, s, s, col);
    }
  }
}

void renderGallery() {
  if (galleryDetail) {  // vista detalle: se redibuja siempre (animada)
    gfx->fillScreen(RGB565_BLACK);
    gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
    const DexEntry &d = DEX_TBL[galleryDetail];
    bool reg = pet.isRegistered(galleryDetail);
    char head[24];
    snprintf(head, sizeof(head), "N.%03d %s%s", galleryDetail,
             pet.isShinyRegistered(galleryDetail) ? "*" : "", reg ? d.name : "???");
    gfx->setTextColor(reg ? d.accent : UI_INK);
    int glen = strlen(head);
    int gts = (glen <= 13) ? 3 : 2;  // auto-encoge nombres largos (no caben a t3)
    gfx->setTextSize(gts);
    gfx->setCursor(CX - glen * (gts == 3 ? 9 : 6), gts == 3 ? 56 : 60);
    gfx->print(head);
    if (galleryPmd.loaded) {
      // animado y a color si esta registrado; silueta estatica si no (estilo "?")
      drawPmdActM(galleryPmd, PMD_IDLE, CX, 300, reg ? millis() : 0, true, !reg, 6);
    } else {
      const uint8_t *t = thumbs.get(galleryDetail);
      if (t) drawThumb(t, CX - GAL_CELL, 135, 4, !reg);
    }
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(T(S_DETAIL_BACK)) * 6, 408);
    gfx->print(T(S_DETAIL_BACK));
    uiChrome();
    gfx->flush();
    return;
  }

  if (!galleryDirty) return;  // la rejilla es estatica
  galleryDirty = false;

  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  // the region's own name and its own tally: "how much of Johto have I seen"
  // is the question you are actually asking here
  char head[32];
  const RegionInfo &grg = REGIONS[galleryRegion % GAL_REGIONS];
  snprintf(head, sizeof(head), "%s %u/%u", grg.name,
           pet.registeredCountIn(grg.lo, grg.hi), (unsigned)GAL_SPAN);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(head) * 9, 36);
  gfx->print(head);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int16_t dex = GAL_LO + galleryPage * GAL_PER_PAGE + r * 4 + c;
      if (dex > GAL_HI || dex > DEX_COUNT) break;
      int x = GAL_X + c * GAL_CELL, y = GAL_Y + r * GAL_CELL;
      const uint8_t *t = thumbs.get(dex);
      if (t) {
        drawThumb(t, x, y, 2, !pet.isRegistered(dex));
        if (pet.isShinyRegistered(dex)) {
          gfx->setTextColor(UI_BAR_WARN);
          gfx->setTextSize(2);
          gfx->setCursor(x + 62, y + 4);
          gfx->print("*");
        }
      } else {
        char num[6];
        snprintf(num, sizeof(num), "%d", dex);
        gfx->setTextColor(UI_INK_SOFT);
        gfx->setTextSize(2);
        gfx->setCursor(x + 24, y + 32);
        gfx->print(num);
      }
    }
  }
  // A page number, not a row of dots. 25 dots do not fit across the bottom of
  // a round panel -- the chord at that height is only ~228 px -- and counting
  // them to find where you are is worse than reading the number.
  char pg[12];
  snprintf(pg, sizeof(pg), "%d/%d", galleryPage + 1, (int)GAL_PAGES);
  gfx->setTextColor(UI_INK_SOFT);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(pg) * 6, 410);
  gfx->print(pg);
  uiChrome();
  gfx->flush();
}

void galleryTap(int16_t x, int16_t y) {
  if (galleryDetail) {  // volver a la rejilla
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  if (y < 72) {  // tocar la cabecera = salir
    galleryOpen = false;
    galleryPmd.unload();
    return;
  }
  int c = (x - GAL_X) / GAL_CELL, r = (y - GAL_Y) / GAL_CELL;
  if (c < 0 || c > 3 || r < 0 || r > 3) return;
  int16_t dex = GAL_LO + galleryPage * GAL_PER_PAGE + r * 4 + c;
  if (dex > GAL_HI || dex > DEX_COUNT) return;
  galleryDetail = dex;
  galleryPmd.load(dex, pet.isShinyRegistered(dex));
}

void drawBattery() {
  int pc = batPercent();
  if (pc < 0) return;  // sin bateria conectada
  int x = CX - 14, y = 12, w = 24, h = 11;
  bool charging = batCharging();
  uint16_t col = charging ? UI_BAR_OK
                 : (pc >= 40) ? inkColor()
                 : (pc >= 15) ? UI_BAR_WARN
                              : UI_BAR_BAD;
  gfx->drawRoundRect(x, y, w, h, 2, col);
  gfx->fillRect(x + w, y + 3, 3, 5, col);  // borne
  if (charging) {
    // rayo de carga (zigzag) en vez de la barra de nivel
    uint16_t bolt = C565(0xff, 0xd9, 0x4a);
    int bx = x + w / 2;
    gfx->fillTriangle(bx + 3, y + 1, bx - 4, y + 6, bx + 1, y + 6, bolt);
    gfx->fillTriangle(bx - 1, y + 5, bx + 4, y + 5, bx - 3, y + 10, bolt);
  } else {
    int fw = (w - 4) * pc / 100;
    if (fw > 0) gfx->fillRect(x + 2, y + 2, fw, h - 4, col);
  }
}

// Drawn beside the battery when Pet::saveHealthy() goes false: NVS would not
// open, or it has refused three writes in a row.
//
// Deliberately not a dialog. The condition persists, so a modal would be
// dismissed once and then forgotten, and it is not actionable in the moment --
// what the player needs is to know before they put in another week, so they can
// EXPORT. The firmware could not say this at all before: the only symptom was a
// line on a serial port no player has attached.
void drawSaveWarning() {
  if (pet.saveHealthy()) return;
  const int x = CX - 40, y = 11, w = 13, h = 13;
  gfx->fillTriangle(x + w / 2, y, x, y + h, x + w, y + h, UI_BAR_BAD);
  gfx->fillRect(x + w / 2, y + 5, 2, 4, RGB565_BLACK);   // the "!"
  gfx->fillRect(x + w / 2, y + 10, 2, 2, RGB565_BLACK);
}

void drawHeader(const char *name, uint16_t nameColor, const char *msg) {
  drawBattery();
  drawSaveWarning();
  gfx->setTextColor(nameColor);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(name) * 9, HEADER_NAME_Y);
  gfx->print(name);
  gfx->setTextColor(inkColor());
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(msg) * 6, 90);
  gfx->print(msg);
}

// animacion de la ceremonia (10s): despedida = reverencia con corazones y se
// aleja caminando; escapada = se asusta y sale corriendo. Sustituye al idle.
void drawCeremony() {
  if (!pmd.loaded) { drawPet(); return; }  // respaldo si no hay sprite PMD
  uint32_t now = millis();
  float t = pet.ceremonyT();               // 0..1 a lo largo de los 10s
  bool panic = (pet.ceremony == CER_RUNAWAY);
  int x = CX, y = PET_GROUND;
  uint8_t act = PMD_IDLE;

  if (panic) {
    // final triste: penumbra azulada + lluvia
    for (int i = 0; i < 46; i++) {
      int rx = (i * 47 + now / 3) % 466;
      int ry = (i * 91 + now / 2) % 470;
      gfx->drawLine(rx, ry, rx - 3, ry + 12, C565(0x6a, 0x84, 0xb0));
    }
    bool fade = false;
    if (t < 0.30f) {                       // cabizbajo, temblando
      act = pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
      x = CX + (int)(4 * sinf(now * 0.04f));
    } else {                               // se aleja despacio y se desvanece
      act = pmd.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
      x = CX - (int)(((t - 0.30f) / 0.70f) * (CX + 120));
      fade = (t > 0.6f) && ((now / 160) % 2 == 0);  // parpadea hacia la silueta
    }
    drawPmdAct(act, x, y, now, true, fade, 5);  // fade=silueta: se difumina al irse
    // lagrima cayendo del bicho
    if (t < 0.55f) {
      int ty = y - 150 + (int)((now / 6) % 40);
      gfx->fillRect(x + 6, ty, 3, 6, C565(0x9a, 0xc4, 0xe8));
    }
    return;
  }

  // despedida epica: halo dorado pulsante + chispas y corazones que ascienden
  int gcy = PET_GROUND - 96;
  for (int k = 0; k < 4; k++) {
    int r = 60 + k * 34 + (int)(10 * sinf(now * 0.02f));
    gfx->drawCircle(CX, gcy, r, C565(0xff, 0xdf, 0x8a));
  }
  for (int i = 0; i < 16; i++) {
    int px = (i * 71 + 28) % 466;
    int py = 410 - (int)((now / 8 + i * 70) % 360);   // suben y reaparecen abajo
    if (py < 30) continue;
    if (i % 4 == 0) drawMap(SPR_HEART, 32, px - 8, py - 8, 1, false);  // corazoncito
    else gfx->fillRect(px, py, 4, 4, (i % 2) ? C565(0xff, 0xe7, 0x9f) : C565(0xff, 0x9a, 0xc0));
  }

  if (t < 0.45f) {                         // reverencia / pose de despedida
    act = pmd.has(PMD_POSE) ? PMD_POSE : (pmd.has(PMD_NOD) ? PMD_NOD : PMD_IDLE);
  } else {                                 // se aleja por la derecha
    act = pmd.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE;
    x = CX + (int)(((t - 0.45f) / 0.55f) * (CX + 140));
  }
  drawPmdAct(act, x, y, now, true, false, 5);
  if (pet.showHeart())                     // corazon grande siguiendo al bicho
    drawMap(SPR_HEART, 32, x + 50, y - 190, 2, false);
}

// dialogo de decision (2 botones apilados): evolucionar/mantener o despedirse/quedaros
// The two confirm buttons' hit areas, so a test can hold them to UI_TAP_MIN and
// prove they do not overlap without restating the numbers.
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot) {
  *b1Top = CONFIRM_B1_Y;
  *b1Bot = CONFIRM_B1_Y + CONFIRM_BTN_H;
  *b2Top = CONFIRM_B2_Y;
  *b2Bot = CONFIRM_B2_Y + CONFIRM_BTN_H;
}

// Draws the panel and its two buttons. sub1/sub2 are optional lines between the
// question and the buttons -- what the choice COSTS, which for anything
// irreversible has to be on screen before the tap, not after it.
void drawConfirmPanel(const char *q, const char *sub1, const char *sub2,
                      uint16_t subCol, const char *o1, uint16_t c1, uint16_t t1,
                      const char *o2, uint16_t c2, uint16_t t2) {
  gfx->fillRoundRect(CONFIRM_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 16, UI_WHITE);
  gfx->drawRoundRect(CONFIRM_X, CONFIRM_Y, CONFIRM_W, CONFIRM_H, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(q) * 6, 176);
  gfx->print(q);
  if (sub1 || sub2) {
    gfx->setTextSize(1);
    gfx->setTextColor(subCol);
    if (sub1) {
      gfx->setCursor(CX - (int)strlen(sub1) * 3, sub2 ? 188 : 194);
      gfx->print(sub1);
    }
    if (sub2) {
      gfx->setCursor(CX - (int)strlen(sub2) * 3, 197);
      gfx->print(sub2);
    }
    gfx->setTextSize(2);
    gfx->setTextColor(UI_INK);
  }
  gfx->fillRoundRect(CONFIRM_BTN_X, CONFIRM_B1_Y, CONFIRM_BTN_W, CONFIRM_BTN_H, 12, c1);
  gfx->setTextColor(t1);
  gfx->setCursor(CX - (int)strlen(o1) * 6, CONFIRM_B1_Y + 18);
  gfx->print(o1);
  gfx->fillRoundRect(CONFIRM_BTN_X, CONFIRM_B2_Y, CONFIRM_BTN_W, CONFIRM_BTN_H, 12, c2);
  gfx->setTextColor(t2);
  gfx->setCursor(CX - (int)strlen(o2) * 6, CONFIRM_B2_Y + 18);
  gfx->print(o2);
}

void drawChoiceDialog() {
  const char *q, *o1, *o2;
  const char *sub1 = nullptr, *sub2 = nullptr;
  uint16_t c1, c2, t1, t2;
  if (choiceKind == 1) {  // evolucion
    q = T(S_EVO_Q); o1 = T(S_EVO_TAP); o2 = T(S_EVO_KEEP);
    c1 = UI_BAR_BAD; t1 = UI_WHITE; c2 = UI_TRACK; t2 = UI_INK;
  } else if (choiceKind == 3) {   // retirada a peticion
    q = T(S_RETIRE_Q); o1 = T(S_FAR_GO); o2 = T(S_FAR_STAY);
    c1 = UI_BAR_WARN; t1 = UI_INK; c2 = UI_BAR_OK; t2 = UI_WHITE;
    // The price, spelled out, and only when there is one: retiring a creature
    // that has already earned its farewell costs nothing and must not claim to.
    // An EARLY one now costs TWO things and says both -- the creature is not
    // banked at all, which is the half a player would not otherwise discover
    // until the party screen came up empty.
    if (!pet.retireIsFree()) { sub1 = T(S_RETIRE_COST); sub2 = T(S_RETIRE_GONE); }
  } else {                // despedida
    q = T(S_FAR_Q); o1 = T(S_FAR_GO); o2 = T(S_FAR_STAY);
    c1 = UI_BAR_WARN; t1 = UI_INK; c2 = UI_BAR_OK; t2 = UI_WHITE;
  }
  drawConfirmPanel(q, sub1, sub2, UI_BAR_BAD, o1, c1, t1, o2, c2, t2);
}

// boton-CTA rojo y grande para evolucionar (pulsa para llamar la atencion)
void drawEvolveButton() {
  uint32_t now = millis();
  int p = (int)(5 * sinf(now * 0.006f));  // late: -5..5
  int x = EVO_BTN_X - p, y = EVO_BTN_Y - p, w = EVO_BTN_W + 2 * p, h = EVO_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 18, UI_BAR_BAD);
  gfx->drawRoundRect(x, y, w, h, 18, UI_WHITE);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 16, UI_WHITE);
  gfx->setTextColor(UI_WHITE);
  gfx->setTextSize(3);
  const char *t = T(S_EVO_TAP);
  gfx->setCursor(CX - (int)strlen(t) * 9, y + h / 2 - 11);
  gfx->print(t);
}

// boton-CTA dorado de despedida: "<nombre> quiere decirte algo..."
void drawFarewellButton() {
  uint32_t now = millis();
  int p = (int)(4 * sinf(now * 0.005f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, UI_BAR_WARN);
  gfx->drawRoundRect(x, y, w, h, 16, UI_INK);
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : DEX_TBL[pet.speciesId].name;
  snprintf(buf, sizeof(buf), T(S_FAREWELL_BTN), nm);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(buf) * 6, y + h / 2 - 8);
  gfx->print(buf);
}

// boton-CTA sombrio de escapada por abandono: "<nombre> se siente abandonado..."
// (final triste: azul-gris oscuro, latido lento y apagado)
void drawRunawayButton() {
  uint32_t now = millis();
  int p = (int)(3 * sinf(now * 0.003f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, C565(0x3a, 0x44, 0x5a));
  gfx->drawRoundRect(x, y, w, h, 16, C565(0x70, 0x80, 0x98));
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : DEX_TBL[pet.speciesId].name;
  snprintf(buf, sizeof(buf), T(S_RUNAWAY_BTN), nm);
  gfx->setTextColor(C565(0xc8, 0xd2, 0xe0));
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(buf) * 6, y + h / 2 - 8);
  gfx->print(buf);
}

// animacion epica de evolucion: halo radial + rayos giratorios + parpadeo del
// sprite acelerando + chispas que salen disparadas + fogonazo final
void drawEvolveFX(uint32_t now) {
  float t = pet.evolveT();          // 0..1
  int cx = CX, cy = PET_GROUND - 96;

  // halo radial que crece y pulsa
  int halo = 36 + (int)(t * 150) + (int)(8 * sinf(now * 0.02f));
  for (int k = 0; k < 4; k++) {
    int r = halo - k * 7;
    if (r > 0) gfx->drawCircle(cx, cy, r, UI_WHITE);
  }
  // rayos giratorios desde el centro del bicho
  float base = now * 0.004f;
  for (int i = 0; i < 12; i++) {
    float a = base + i * (float)(PI / 6);
    int len = 90 + (int)(70 * (0.5f + 0.5f * sinf(now * 0.012f + i)));
    gfx->drawLine(cx, cy, cx + (int)(cosf(a) * len), cy + (int)(sinf(a) * len), UI_WHITE);
  }
  // parpadeo entre la forma ANTERIOR y la NUEVA (siluetas), acelerando; al
  // final (t>0.9) se queda fija en la nueva para el fogonazo de revelado
  int period = 60 + (int)(220 * (1.0f - t));
  bool showOld = t < 0.9f && evoPmd.loaded && ((now / period) % 2) == 0;
  if (showOld) drawPmdActM(evoPmd, PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  else drawPmdAct(PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  // chispas que salen disparadas
  for (int i = 0; i < 10; i++) {
    float a = i * (float)(PI / 5) + t * 4.0f;
    int d = (int)((now / 14 + i * 33) % 200);
    int sx = cx + (int)(cosf(a) * d), sy = cy + (int)(sinf(a) * d);
    gfx->fillRect(sx - 2, sy - 2, 5, 5, (i & 1) ? C565(0xff, 0xe0, 0x70) : UI_WHITE);
  }
  // fogonazo final antes de revelar la forma nueva
  if (t > 0.9f) gfx->fillCircle(cx, cy, (int)(300 * (t - 0.9f) / 0.1f), UI_WHITE);
}

void drawPet() {
  if (pmd.loaded) {
    drawPetPMD();
    return;
  }
  if (mon.loaded) {
    drawPetSD();
    return;
  }
  int fi = flashIdxForDex(pet.speciesId);
  if (fi < 0) {
    // sin SD y sin sprite de flash: aviso claro de que faltan sprites
    gfx->setTextColor(inkColor());
    gfx->setTextSize(6);
    gfx->setCursor(CX - 18, PET_CY - 80);
    gfx->print("?");
    gfx->setTextSize(2);
    const char *l1 = T(S_NO_SPRITES);
    gfx->setCursor(CX - (int)strlen(l1) * 6, PET_CY - 4);
    gfx->print(l1);
    const char *l2 = T(S_LOAD_SPRITES);
    gfx->setCursor(CX - (int)strlen(l2) * 6, PET_CY + 20);
    gfx->print(l2);
    return;
  }
  const Species &sp = SPECIES[fi];
  int s = sp.scale;
  int x = CX - 16 * s;
  int y = PET_CY - 16 * s;

  // animacion de evolucion: alterna la silueta de la forma anterior y la nueva
  if (pet.evolving()) {
    bool flash = (millis() / 300) % 2;
    int16_t showDex = (flash && pet.prevSpeciesId >= 0) ? pet.prevSpeciesId : pet.speciesId;
    int sfi = flashIdxForDex(showDex);
    if (sfi >= 0) {
      const Species &show = SPECIES[sfi];
      drawMap(show.sprite, SPRITE_H, CX - 16 * show.scale, PET_CY - 16 * show.scale, show.scale, flash);
    }
    return;
  }

  PetMood m = pet.mood();
  if (m == MOOD_HAPPY && (millis() / 500) % 2) y -= 6;  // saltito

  drawMap(sp.sprite, SPRITE_H, x, y, s, false);

  // expresiones superpuestas usando las anclas de la especie
  bool blink = (millis() % 3500 < 300);
  if (m == MOOD_SLEEPING || blink) {
    overlayEye(sp, x, y, s, sp.eyeColL);
    overlayEye(sp, x, y, s, sp.eyeColR);
  }
  if (m == MOOD_EATING) overlayMouth(sp, x, y, s, true);
  else if (m == MOOD_SAD) overlayMouth(sp, x, y, s, false);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + 20 * s, y - 2 * s, 2, false);
}

// ---------- escena de bano ----------

void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return;
  bathUntil = millis() + 3000;
  bathPending = true;
  int cx = (int)beh.x;
  for (auto &b : bubbles) {
    b.x = cx - 70 + random(140);
    b.y = PET_GROUND - random(150);
    b.r = 8 + random(16);
    b.ph = random(64);
  }
}

void drawBath() {
  uint32_t now = millis();
  if (now > bathUntil) {
    bathUntil = 0;
    if (bathPending) {
      bathPending = false;
      pet.clean();
      // pose de alegria al quedar limpio
      if (pmd.has(PMD_POSE)) {
        beh.mode = 2;
        beh.act = PMD_POSE;
        beh.t0 = now;
        beh.until = now + pmdActTotalMs(pmd.acts[PMD_POSE]) * 2;
      }
    }
    return;
  }
  uint32_t left = bathUntil - now;
  if (left > 800) {
    // espuma: pompas meciendose y subiendo poco a poco
    float t = now / 220.0f;
    for (auto &b : bubbles) {
      int bx = b.x + (int)(sinf(t + b.ph) * 6);
      int by = b.y - (int)((3000 - left) / 90);
      gfx->fillCircle(bx, by, b.r, UI_WHITE);
      gfx->drawCircle(bx, by, b.r, 0x7E3D);
      gfx->fillCircle(bx - b.r / 3, by - b.r / 3, b.r / 4, UI_BG_DAY);
    }
  } else {
    // las pompas revientan: destellos
    for (int i = 0; i < 8; i++) {
      auto &b = bubbles[i];
      int sx = b.x + (i % 3) * 6 - 6, sy = b.y - 18;
      uint16_t col = (i % 2) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sx - 6, sy - 1, 13, 3, col);
      gfx->fillRect(sx - 1, sy - 6, 3, 13, col);
    }
  }
}

// ---------- mascota PMD: comportamiento ----------

uint32_t pmdActTotalMs(const PmdAct &a) {
  uint32_t t = 0;
  for (uint8_t i = 0; i < a.frames; i++) t += a.ms[i];
  return t ? t : 100;
}

uint8_t pmdFrameAt(const PmdAct &a, uint32_t t, bool loop) {
  uint32_t total = pmdActTotalMs(a);
  if (!loop && t >= total) return a.frames - 1;
  t %= total;
  uint8_t i = 0;
  while (t >= a.ms[i]) {
    t -= a.ms[i];
    i = (i + 1) % a.frames;
  }
  return i;
}

// dibuja una accion anclada por la base (centro-x, suelo) y devuelve su escala
// dibuja una accion de un PmdMon concreto (m); drawPmdAct usa el global pmd
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t sBase = m.acts[PMD_IDLE].h ? 170 / m.acts[PMD_IDLE].h : 5;
  if (sBase < 2) sBase = 2;
  if (sBase > maxS) sBase = maxS;
  uint8_t s = sBase;
  while (s > 2 && a.h * s > 250) s--;  // acciones con frame grande (ataque)
  uint8_t fi = pmdFrameAt(a, t, loop);
  const uint8_t *fr = a.data + (uint32_t)fi * a.w * a.h;
  // anclar por los pies (a.base), no por el alto del lienzo: asi las acciones
  // con padding distinto (Hurt, Eat...) quedan todas a la misma altura de suelo
  int x0 = cx - a.w * s / 2, y0 = groundY - (a.base ? a.base : a.h) * s;
  for (int r = 0; r < a.h; r++) {
    const uint8_t *row = fr + r * a.w;
    for (int c = 0; c < a.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x0 + c * s, y0 + r * s, s, s, sil ? INK_K : m.pal[idx]);
    }
  }
}
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  drawPmdActM(pmd, actId, cx, groundY, t, loop, sil, maxS);
}

// elige el siguiente capricho del bicho cuando esta contento
void behNext() {
  uint32_t now = millis();
  beh.t0 = now;
  int r = random(100);
  if (r < 35 && (pmd.has(PMD_WALKL) || pmd.has(PMD_WALKR))) {
    beh.mode = 1;  // paseo
    beh.targetX = 150 + random(176);
    beh.until = now + 15000;
  } else if (r < 60) {
    // gesto aleatorio entre los disponibles
    // (Hop fuera: salta demasiado alto; Sit fuera: mira hacia atras)
    static const uint8_t flair[] = { PMD_POSE, PMD_NOD, PMD_BREATH };
    uint8_t pick[3], n = 0;
    for (uint8_t f : flair)
      if (pmd.has(f)) pick[n++] = f;
    if (n) {
      beh.mode = 2;
      beh.act = pick[random(n)];
      beh.until = now + pmdActTotalMs(pmd.acts[beh.act]);
      return;
    }
    beh.mode = 0;
    beh.until = now + 2000 + random(3000);
  } else {
    beh.mode = 0;  // mirar al frente
    beh.until = now + 2000 + random(3000);
  }
}

void drawPetPMD() {
  uint32_t now = millis();

  if (pet.evolving()) {
    drawEvolveFX(now);
    return;
  }
  if (evoPmd.loaded) evoPmd.unload();  // termino la evolucion: libera la forma anterior

  PetMood m = pet.mood();
  uint8_t act;
  bool loop = true;
  if (m == MOOD_SLEEPING && pmd.has(PMD_SLEEP)) {
    act = PMD_SLEEP;
    beh.mode = 0;
  } else if (m == MOOD_EATING && pmd.has(PMD_EAT)) {
    act = PMD_EAT;
    beh.t0 = 0;
  } else if (m == MOOD_SAD && pmd.has(PMD_HURT)) {
    act = PMD_HURT;
  } else {
    // contento: el planificador decide (idle / paseo / gesto)
    if (now > beh.until) behNext();
    if (beh.mode == 1) {
      float d = beh.targetX - beh.x;
      if (fabsf(d) < 4) {
        behNext();
        act = PMD_IDLE;
      } else {
        beh.x += (d > 0 ? 3.0f : -3.0f);
        act = (d > 0) ? PMD_WALKR : PMD_WALKL;
      }
    } else {
      act = (beh.mode == 2) ? beh.act : PMD_IDLE;
      loop = false;
    }
    if (!pmd.has(act)) act = PMD_IDLE;
  }

  drawPmdAct(act, (int)beh.x, PET_GROUND, now - beh.t0, loop || act == PMD_IDLE, false, 5);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, (int)beh.x + 50, PET_GROUND - 190, 2, false);
}

// sprite animado desde la SD: zoom entero por pixel, frames a su ritmo
void drawPetSD() {
  int s = mon.scale;
  int w = mon.w * s, h = mon.h * s;
  int x = CX - w / 2;
  int y = PET_CY - h / 2;

  bool sil = false;
  if (pet.evolving()) {
    sil = (millis() / 300) % 2;
  } else if (pet.mood() == MOOD_HAPPY && (millis() / 500) % 2) {
    y -= 6;  // saltito
  }

  uint16_t fm = mon.frameMs ? mon.frameMs : 100;
  uint16_t fi = pet.sleeping ? 0 : (millis() / fm) % mon.frames;
  const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
  for (int r = 0; r < mon.h; r++) {
    const uint8_t *row = fr + r * mon.w;
    for (int c = 0; c < mon.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, sil ? INK_K : mon.pal[idx]);
    }
  }

  // emotes en vez de expresiones (los sprites importados no tienen anclas)
  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + w - 30, y - 50, 2, false);
}

// ojo cerrado: borra el ojo 3x4 y dibuja el parpado
void overlayEye(const Species &sp, int x, int y, int s, int col) {
  gfx->fillRect(x + col * s, y + sp.eyeRow * s, 3 * s, 4 * s, sp.bodyColor);
  gfx->fillRect(x + col * s, y + (sp.eyeRow + 2) * s, 3 * s, s, INK_K);
}

// borra la sonrisa base y pinta boca abierta (comer) o ceno (triste)
void overlayMouth(const Species &sp, int x, int y, int s, bool open) {
  int mc = sp.mouthCol, mr = sp.mouthRow;
  gfx->fillRect(x + (mc - 3) * s, y + mr * s, 7 * s, 2 * s, sp.bodyColor);
  if (open) {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, 2 * s, INK_K);
  } else {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, s, INK_K);
    gfx->fillRect(x + (mc - 3) * s, y + (mr + 1) * s, s, s, INK_K);
    gfx->fillRect(x + (mc + 3) * s, y + (mr + 1) * s, s, s, INK_K);
  }
}

void drawPoops() {
  for (int i = 0; i < pet.poops; i++) {
    drawMap(SPR_POOP, 32, 36 + i * 46, 244, 2, false);
  }
}

// The four care stats, on the panel's RIM.
//
// They were 2x2 bars across y 318..361 -- four 100 px tracks laid over the
// widest part of the circle, which is the one place a round panel has room to
// spare, while the home row below them had to make do with 60 px icons. An arc
// lives where no rectangular layout can put anything at all.
//
// Colour still means SEVERITY, exactly as the bars did; the label says which
// stat it is. The gap at 12 o'clock is for the name, and the arcs stop short of
// +-102 degrees so the ends stay clear of the poop row on the left.
#define CARE_R 216        // the track's radius
#define CARE_W 12         // and its thickness
#define CARE_LBL_R 190    // labels hug the track rather than floating mid-field
static const int16_t CARE_ARC[4][2] = {
  { -108, -82 }, { -78, -52 }, { 52, 78 }, { 82, 108 },
};
// One hue each. Four arcs in the same colour are four arcs you have to read the
// labels of; and the severity colours they used to share were 1.64:1 against
// their own groove, so the filled part barely differed from the empty part.
static const uint16_t CARE_COL[4] = {
  UI_CARE_FOOD, UI_CARE_JOY, UI_CARE_ENE, UI_CARE_HYG,
};
#define CARE_LOW 25       // below this the LABEL carries the warning

int uiCareArcTop(int i) {
  if (i < 0 || i >= 4) return -1;
  int top = 466;
  for (int a = CARE_ARC[i][0]; a <= CARE_ARC[i][1]; a++) {
    float rad = ((float)a - 90.0f) * 0.01745329f;
    int y = CY + (int)(sinf(rad) * CARE_R) - CARE_W / 2;
    if (y < top) top = y;
  }
  return top;
}

int uiHeaderNameBottom() { return HEADER_NAME_Y + HEADER_NAME_H; }

static void drawCareArc(int i, const char *label, uint8_t val) {
  int a0 = CARE_ARC[i][0], a1 = CARE_ARC[i][1];
  uiArc(CX, CY, CARE_R, a0, a1, CARE_W, UI_TRACK);
  if (val) uiArc(CX, CY, CARE_R, a0, a0 + (a1 - a0) * val / 100, CARE_W, CARE_COL[i]);
  float mid = ((float)(a0 + a1) / 2.0f - 90.0f) * 0.01745329f;
  int lx = CX + (int)(cosf(mid) * CARE_LBL_R);
  int ly = CY + (int)(sinf(mid) * CARE_LBL_R);
  gfx->setTextColor(val < CARE_LOW ? UI_BAR_BAD : inkColor());
  gfx->setTextSize(2);
  gfx->setCursor(lx - (int)strlen(label) * 6, ly - 8);
  gfx->print(label);
}

void drawBars() {
  drawCareArc(0, T(S_BAR_FOOD), pet.fullness);
  drawCareArc(1, T(S_BAR_JOY), pet.joy);
  drawCareArc(2, T(S_BAR_ENE), pet.energy);
  drawCareArc(3, T(S_BAR_HYG), pet.hygiene);
}

void drawButtons() {
  for (int i = 0; i < BTN_COUNT; i++) {
    bool off = uiButtonDisabled(i);   // durmiendo solo funciona LUZ
    int bx = buttons[i].cx - BTN_HALF, by = buttons[i].cy - BTN_HALF;
    if (!pet.sleeping) gfx->fillRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 16, UI_WHITE);
    gfx->drawRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 16, inkColor());
    if (!off) drawMap(buttons[i].icon, 16, buttons[i].cx - 16, buttons[i].cy - 16, 2, false);
  }
}

const char *eggMsg() {
  switch (pet.eggCracks()) {
    case 0: return T(S_EGG_TOUCH);
    case 1: return T(S_EGG_MOVES);
    default: return T(S_EGG_ALMOST);
  }
}

const char *statusMsg() {
  if (pet.evolving()) return T(S_EVOLVING);
  if (bathUntil) return "Splish splash!";  // onomatopeya universal
  if (pet.sleeping) return "Zzz...";
  if (pet.eating()) return T(S_EATING);
  if (pet.showHeart()) return T(S_LIKES);
  if (pet.fullness < 25) return T(S_HUNGRY);
  if (pet.hygiene < 25) return T(S_NEEDS_BATH);
  if (pet.energy < 25) return T(S_EXHAUSTED);
  if (pet.joy < 25) return T(S_SAD);
  if (pet.weight > 60) return T(S_CHUBBY);
  if (pet.shiny && pet.ageMinutes < 15) return T(S_IS_SHINY);
  return T(S_HAPPY);
}

// dibuja un mapa de n x n pixeles escalado; silhouette=true lo pinta en tinta
// An 8bpp indexed avatar, same shape as the badge art: 0xFF is transparent.
void drawAvatar(uint8_t which, int x, int y, int s) {
  drawArt(AVATARS[which % AVATAR_COUNT], x, y, s);
}

// Any of the overworld sprites: a player's avatar or a character like Mom.
static void drawArt(const AvatarArt &a, int x, int y, int s) {
  for (int r = 0; r < AVATAR_PX; r++)
    for (int c = 0; c < AVATAR_PX; c++) {
      uint8_t v = a.idx[r * AVATAR_PX + c];
      if (v == 0xFF) continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, a.pal[v]);
    }
}

void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette) {
  for (int r = 0; r < n; r++) {
    for (int c = 0; c < n; c++) {
      char ch = map[r][c];
      if (ch == '.') continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, silhouette ? INK_K : spriteColor(ch));
    }
  }
}
