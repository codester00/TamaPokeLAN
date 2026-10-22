#!/usr/bin/env python3
"""Real Game Boy music: pret/pokered (+ pokeyellow) audio/music/*.asm -> music.h

    python3 tools/gen_music.py

The .asm files are tracker sequences, not recordings:

    tempo 104
    duty_cycle 3
    note_type 12, 11, 3      ; speed, volume, fade
    octave 3
    note F#, 1
    sound_call .sub1
    sound_loop 2, .introloop

This EXECUTES them the way the game's sound engine does (v3.56) rather than
reading the lines top to bottom, which is what the first version did: it
ignored sound_call (so a subroutine's notes landed wherever its text sat),
played a "sound_loop 2" passage once, and looped every tune from its very
first note, intro included. Each channel comes out as a flat list of note
events plus the index the song loops back to (the target of its
"sound_loop 0"), or none for a one-shot jingle.

TIMING is the engine's own (audio/engine_1.asm, Audio1_note_length): a note
lasts length * speed * tempo / 256 frames, the remainder carried to the next
note, at the Game Boy's 59.7275 frames a second. The first version used a
hand-tuned 18 ms a unit, which put the battle themes at about 40% speed.

CHANNEL 3 is the wave channel -- the bass line. Its note_type is speed,
output level (0 mute, 1 full, 2 half, 3 quarter) and wave instrument; the
instruments are the 32-nibble shapes in audio/wave_samples.asm, emitted as
MUSIC_WAVES. Channel 4 (drums) is not used by any tune taken here.

FREQUENCY. audio/notes.asm holds twelve 16-bit values; the period is
65536 - value, shifted right by (octave - 1), and the Game Boy register is
2048 - period. That lands C at octave 3 on 261.6 Hz -- middle C. The wave
channel plays an octave below a pulse on the same register, as on hardware
(gbsynth.cpp).

Source: Nintendo / Game Freak / Creatures, via the pokered and pokeyellow
disassemblies. See CREDITS.md -- the same standing as the trainer sprites.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'music.h')
RAW = 'https://raw.githubusercontent.com/pret/%s/master/%s'

# (repo, file, prefix in music.h, label prefix in the .asm). The ORDER is the
# track numbering audio.cpp maps Music ids onto (MUSIC_TRK_* below), so append.
TUNES = [
    ('pokered',   'gymleaderbattle',   'GYM',     'Music_GymLeaderBattle'),
    ('pokered',   'trainerbattle',     'TRAINER', 'Music_TrainerBattle'),
    ('pokered',   'wildbattle',        'WILD',    'Music_WildBattle'),
    ('pokered',   'defeatedgymleader', 'WIN_GYM', 'Music_DefeatedGymLeader'),
    ('pokered',   'finalbattle',       'FINAL',   'Music_FinalBattle'),
    ('pokered',   'defeatedtrainer',   'WIN_TRAINER', 'Music_DefeatedTrainer'),
    ('pokered',   'defeatedwildmon',   'WIN_WILD', 'Music_DefeatedWildMon'),
    ('pokered',   'gamecorner',        'GAMECORNER', 'Music_GameCorner'),
    ('pokeyellow', 'surfingpikachu',   'SURF',    'Music_SurfingPikachu'),
]

NOTE_NAMES = ['C_', 'C#', 'D_', 'D#', 'E_', 'F_', 'F#', 'G_', 'G#', 'A_', 'A#', 'B_']
FPS = 59.7275          # the Game Boy's frame rate; the engine counts in frames
MAX_EVENTS = 3000      # per channel; a runaway interpreter fails loudly instead


def fetch(repo, path):
    r = subprocess.run(['curl', '-fsSL', RAW % (repo, path)], capture_output=True)
    if r.returncode != 0:
        raise SystemExit('fetch failed: %s %s' % (repo, path))
    return r.stdout.decode('utf-8', 'replace')


def note_table():
    """The twelve base periods from audio/notes.asm."""
    txt = fetch('pokered', 'audio/notes.asm')
    vals = [int(m, 16) for m in re.findall(r'dw \$([0-9A-Fa-f]{4})', txt)][:12]
    if len(vals) != 12:
        raise SystemExit('could not read the note table')
    return [65536 - v for v in vals]      # period at octave 1


def wave_table():
    """The channel-3 instruments: 32 nibbles each, in the order the game indexes them."""
    txt = fetch('pokered', 'audio/wave_samples.asm')
    order = re.findall(r'dw \.(wave\d)', txt)
    shapes = {}
    for m in re.finditer(r'\.(wave\d)\s*\n\s*dn\s+([^\n]+)', txt):
        shapes[m.group(1)] = [int(x) for x in m.group(2).split(',')]
    # .wave5 has no data of its own -- the game reads whatever bytes follow (the
    # file says so), and only Lavender Town and Pokemon Tower use it. The
    # instruments are the defined ones, in order; run_channel() refuses a tune
    # that asks for one past them.
    waves = []
    for n in order:
        if n not in shapes:
            break
        if shapes[n] not in waves or len(waves) == order.index(n):
            waves.append(shapes[n])
    if len(waves) < 5 or any(len(w) != 32 for w in waves):
        raise SystemExit('could not read the wave instruments')
    return waves


def gb_freq(periods, name, octave):
    if name not in NOTE_NAMES:
        raise SystemExit('unknown note %r' % name)
    p = periods[NOTE_NAMES.index(name)] >> max(0, octave - 1)
    f = 2048 - max(p, 1)
    return f if 0 <= f < 2048 else 0


def program(text):
    """The .asm as a list of (label-or-None, command, args) with label positions.

    Global labels end in '::' (or ':' for a plain one); local '.name:' labels are
    scoped to the last global one, as rgbasm scopes them, so 'Ch2.sub2' and a bare
    '.sub2' inside Ch2 name the same place.
    """
    lines, labels, scope = [], {}, ''
    for raw in text.splitlines():
        s = raw.split(';')[0].strip()
        if not s:
            continue
        m = re.match(r'^([A-Za-z_][\w]*)::?$', s)
        if m:
            scope = m.group(1)
            labels[scope] = len(lines)
            continue
        m = re.match(r'^\.(\w+):?$', s)
        if m:
            labels[scope + '.' + m.group(1)] = len(lines)
            continue
        parts = s.split(None, 1)
        args = [a.strip() for a in parts[1].split(',')] if len(parts) > 1 else []
        lines.append((scope, parts[0], args))
    return lines, labels


def resolve(labels, scope, target):
    key = scope.split('.')[0] + target if target.startswith('.') else target
    if key not in labels:
        raise SystemExit('unknown label %s (from %s)' % (target, scope))
    return labels[key]


def run_channel(lines, labels, start, ch, periods, state):
    """Execute one channel. Returns (events, loop_index or None).

    An event is (freq, duty_or_wave, vol, envDir, envPeriod, ms). `state` carries
    the tempo, which the song sets once on its first channel and all share.
    """
    pc = labels[start]
    octave, duty, speed, vol, fade, wave = 4, 2, 12, 15, 0, 0
    perfect = False
    frac = 0                 # the engine's fractional frame carry, per channel
    frames_total = 0         # where this channel is, in frames, for exact ms
    stack, counters = [], {}
    label_at = {}            # line index of a label -> first event index there
    rev = {}
    for k, v in labels.items():
        rev.setdefault(v, []).append(k)
    out = []

    def emit(f, d, v, ed, ep, length):
        nonlocal frac, frames_total
        delay = frac + length * speed * state['tempo']
        frames, frac = delay >> 8, delay & 0xFF
        ms0 = round(frames_total * 1000 / FPS)
        frames_total += frames
        out.append((f, d, v, ed, ep, round(frames_total * 1000 / FPS) - ms0))

    steps = 0
    while True:
        steps += 1
        if steps > 200000 or len(out) > MAX_EVENTS:
            raise SystemExit('%s: runaway channel (no loop found)' % start)
        if pc in rev and pc not in label_at and not stack:
            label_at[pc] = len(out)
        if pc >= len(lines):
            return out, None
        scope, cmd, args = lines[pc]
        pc += 1
        if cmd == 'tempo':
            state['tempo'] = int(args[0])
        elif cmd == 'octave':
            octave = int(args[0])
        elif cmd in ('duty_cycle', 'duty_cycle_pattern'):
            duty = int(args[0]) & 3
        elif cmd == 'toggle_perfect_pitch':
            perfect = not perfect
        elif cmd == 'note_type':
            p = [int(a) for a in args]
            speed = p[0]
            if len(p) > 1: vol = p[1]
            if len(p) > 2:
                if ch == 3: wave = p[2]
                else: fade = p[2]
        elif cmd == 'rest':
            emit(0, duty, 0, 0, 0, int(args[0]))
        elif cmd == 'note':
            f = gb_freq(periods, args[0], octave)
            if perfect and f: f += 1
            length = int(args[1])
            if ch == 3:
                if wave >= state['waves']:
                    raise SystemExit('%s: wave instrument %d is not defined' % (start, wave))
                # output level 0 mute, 1 full, 2 half, 3 quarter
                emit(f if vol else 0, wave, vol, 0, 0, length)
            else:
                # fade > 0 decays every `fade` steps; < 0 swells; 0 holds
                d, per = (-1, fade) if fade > 0 else (1, -fade) if fade < 0 else (0, 0)
                emit(f, duty, vol, d, per & 7, length)
        elif cmd == 'sound_call':
            stack.append(pc)
            pc = resolve(labels, scope, args[0])
        elif cmd == 'sound_ret':
            if not stack:
                return out, None          # the end of a one-shot
            pc = stack.pop()
        elif cmd == 'sound_loop':
            n, target = int(args[0]), resolve(labels, scope, args[1])
            if n == 0:
                if target not in label_at:
                    raise SystemExit('%s: loops to a label it never passed' % start)
                return out, label_at[target]
            key = pc - 1
            left = counters.get(key, n - 1)
            if left > 0:
                counters[key] = left - 1
                pc = target
            else:
                counters.pop(key, None)
        elif cmd in ('vibrato', 'volume', 'execute_music', 'stereo_panning', 'pitch_slide'):
            pass                          # not reproduced: see the module docstring
        else:
            raise SystemExit('%s: unhandled command %r' % (start, cmd))


def loop_ms(ev, loop):
    if loop is None:
        return None
    return sum(e[5] for e in ev[loop:])


def main():
    periods = note_table()
    waves = wave_table()
    c3 = 131072 // (2048 - gb_freq(periods, 'C_', 3))
    sys.stderr.write('middle C check: octave 3 C_ -> %d Hz\n' % c3)
    if not 255 <= c3 <= 268:
        raise SystemExit('note table is off')
    lines = ['// GENERATED by tools/gen_music.py from pret/pokered and pret/pokeyellow - do not edit',
             '#pragma once', '#include <stdint.h>', '',
             '// Real Game Boy music, as note events for gbsynth.cpp, executed from the',
             "// disassemblies' own sequences (calls, loops, the engine's tempo).",
             '// Nintendo / Game Freak / Creatures -- see CREDITS.md.',
             '',
             'struct MusicNote {',
             '  uint16_t freq;   // Game Boy frequency register, 0 = rest',
             '  uint8_t duty;    // pulse: duty 0..3; wave channel: the instrument',
             '  uint8_t vol;     // pulse: 0..15; wave channel: output level 0..3',
             '  int8_t envDir;',
             '  uint8_t envPeriod;',
             '  uint16_t ms;',
             '};', '',
             '#define MUSIC_NO_LOOP 0xFFFF',
             '#define MUSIC_WAVE_COUNT %d' % len(waves),
             '// The wave channel\'s instruments (audio/wave_samples.asm), 32 nibbles each.',
             'static const uint8_t MUSIC_WAVES[MUSIC_WAVE_COUNT][32] = {']
    for w in waves:
        lines.append('  { %s },' % ', '.join(str(x) for x in w))
    lines += ['};', '']
    index = []
    for repo, fn, pre, sym in TUNES:
        txt = fetch(repo, 'audio/music/%s.asm' % fn)
        prog, labels = program(txt)
        state = {'tempo': 256, 'waves': len(waves)}
        chans = []
        for ch in (1, 2, 3):
            start = '%s_Ch%d' % (sym, ch)
            if start not in labels:
                chans.append(([(0, 0, 0, 0, 0, 100)], None))
                sys.stderr.write('%-18s Ch%d: absent\n' % (fn, ch))
                continue
            ev, loop = run_channel(prog, labels, start, ch, periods, state)
            if not ev:
                ev = [(0, 0, 0, 0, 0, 100)]
            chans.append((ev, loop))
            sys.stderr.write('%-18s Ch%d: %4d events, %s, %.1f s\n' % (
                fn, ch, len(ev), 'loops at %d (%.1f s a loop)' % (loop, loop_ms(ev, loop) / 1000)
                if loop is not None else 'one-shot', sum(e[5] for e in ev) / 1000))
        # every channel of a looping song must loop over the same length of time,
        # or the parts drift apart a little more on every pass
        lens = {loop_ms(ev, lp) for ev, lp in chans if lp is not None}
        if len(lens) > 1 and max(lens) - min(lens) > 20:
            raise SystemExit('%s: channels loop over different lengths %s' % (fn, sorted(lens)))
        for i, (ev, loop) in enumerate(chans, 1):
            name = 'MUSIC_%s_CH%d' % (pre, i)
            lines.append('static const MusicNote %s[%d] = {' % (name, len(ev)))
            for f, d, v, ed, ep, ms in ev:
                lines.append('  { %d, %d, %d, %d, %d, %d },' % (f, d, v, ed, ep, ms))
            lines.append('};')
            lines.append('')
        index.append((pre, fn, [lp for _, lp in chans]))
    lines += ['struct MusicTrack {',
              '  const MusicNote *ch[3];',
              '  uint16_t n[3];',
              '  uint16_t loop[3];   // the event each channel loops back to, or MUSIC_NO_LOOP',
              '  const char *name;',
              '};',
              '#define MUSIC_TRACKS %d' % len(index)]
    for i, (pre, fn, _) in enumerate(index):
        lines.append('#define MUSIC_TRK_%s %d' % (pre, i))
    lines.append('static const MusicTrack MUSIC_TBL[MUSIC_TRACKS] = {')
    for pre, fn, loops in index:
        ch = ', '.join('MUSIC_%s_CH%d' % (pre, i) for i in (1, 2, 3))
        n = ', '.join('sizeof(MUSIC_%s_CH%d)/sizeof(MusicNote)' % (pre, i) for i in (1, 2, 3))
        lp = ', '.join(str(x) if x is not None else 'MUSIC_NO_LOOP' for x in loops)
        lines.append('  { { %s }, { %s }, { %s }, "%s" },' % (ch, n, lp, fn))
    lines.append('};')
    lines.append('')
    open(OUT, 'w').write('\n'.join(lines))
    sys.stderr.write('wrote %s\n' % os.path.normpath(OUT))


if __name__ == '__main__':
    main()
