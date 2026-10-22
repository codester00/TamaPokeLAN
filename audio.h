#pragma once
#include <stdint.h>

// Efectos de sonido del juego (cola, no bloqueante). El orden coincide con la
// tabla SFX de audio.cpp.
enum Sfx : uint8_t {
  SFX_TAP = 0,  // tocar / boton
  SFX_EAT,      // comer
  SFX_PLAY,     // punto del minijuego / golpe
  SFX_HEART,    // le gusta / mimo
  SFX_HATCH,    // eclosion
  SFX_EVOLVE,   // evolucion
  SFX_MEDAL,    // medalla / hito
  SFX_DENY,     // accion no permitida
  SFX_BYE,      // despedida
  SFX_LEVEL,    // sube de nivel
  // battle cues: a fight in silence is what made it feel flat
  SFX_HIT,      // a physical move landing
  SFX_BEAM,     // a special move
  SFX_STATUS,   // a status move or an ailment taking hold
  SFX_SUPER,    // super effective
  SFX_FAINT,    // something goes down
  SFX_VICTORY,  // the fight is won
  SFX_COUNT
};

// Music: the real Game Boy themes (music.h, tools/gen_music.py), played by the
// sequencer in musicplay.cpp on two pulse voices and the wave channel's bass.
// An effect takes the first pulse voice for as long as it lasts and the music
// carries on underneath -- an effect always cuts through, which is what you want.
// Each id names a moment, not a file: musicTrackFor() maps it onto a track.
enum Music : uint8_t {
  MUS_NONE = 0,
  MUS_GYM,          // a gym leader or the Elite Four (Red: the gym leader battle)
  MUS_WIN_GYM,      // ... beaten
  MUS_WILD,         // a wild encounter
  MUS_TRAINER,      // a trainer who is not a leader -- and a LAN rival
  MUS_FINAL,        // the champion (Red: the final battle)
  MUS_WIN_TRAINER,  // a trainer or a rival beaten
  MUS_WIN_WILD,     // a wild creature beaten or caught
  MUS_GAMECORNER,   // inside a Game Corner
  MUS_SURF,         // Surfing Pikachu (Yellow's own theme)
  MUS_COUNT
};
void audioMusic(uint8_t id);

// 0..10. Stored, and applied as the square wave's amplitude; 0 is silence
// without disabling the system, which is what a mute expects to do.
void audioSetVolume(uint8_t v);
uint8_t audioVolume();

void audioBegin();          // init ES8311 + I2S + amplificador + tarea de audio
void sfxPlay(uint8_t id);   // encola un efecto (no bloquea el loop)
void audioSetEnabled(bool on);
bool audioEnabled();
