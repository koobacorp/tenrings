# TENRINGS — camera hand instrument for FL Studio
(v1.0 built Nov 7, 2026 — see section 15)

Status: planning. Nothing gets built, pushed, or installed on your Mac until you say go.

Name: **TENRINGS** (chosen). Ten fingers, like ten rings — one on each fingertip for Keyboard mode, and the same two hands for Drum and Lead.

---

## 1. Name

**TENRINGS** — chosen on Nov 7, 2026.

Ten fingers, like ten rings. It fits Keyboard mode (each fingertip is a key) and still works for Drum and Lead, which use those same two hands. The out-of-frame rule stays: if your hands leave the camera, nothing plays.

Earlier name ideas that were not used: INFRAME, CHROMA, PRISM, FLICK, NEON.

All names, code, sounds, and looks will be our own originals.

---

## 2. What it does (plain words)

TENRINGS is a VST3 instrument you load in FL Studio. It uses your Mac camera to watch your hands and turns them into music.

Three modes, one plugin, one camera:

- **Drum** — play kick, snare, hats, and perc in the air.
- **Keyboard** — each fingertip is a key, so you can play melodies like a piano.
- **Lead** — play a singing line that stays in key, with slides, vibrato, and bends, like a guitar or violin.

It makes sound right away with a small built-in kit and a simple keyboard/lead sound. It also sends MIDI, so the notes can land in your piano roll and later drive another plugin, like Serum.

The picture is mirrored, like a mirror, so your left hand matches the left side of the screen. The window is resizable and can be minimized. Contrast stays high so the active mode colour is obvious.

---

## 3. Hard rules

1. **If a hand is out of the frame, that hand plays nothing.** Notes it was holding cut off immediately. No stuck hats, no stuck keys, no leftover lead tone.
2. **If neither hand is visible, the plugin is completely silent.** Your face, chest, and empty air never trigger anything.
3. **The center of the camera is a dead zone** (where your face sits). Pads and note picking live on the sides, not on your face.
4. **One hand in frame can still play that hand’s part**, except in Lead mode, which needs both hands (one for the notes, one for volume). If either hand leaves in Lead, it goes silent.
5. **Nothing is installed on your Mac, and nothing goes public, without your OK.**

---

## 4. Drum mode (orange)

Right hand = drums. Left hand = percussion. Hits are jabs (a quick move in a zone), not “swipe up vs swipe down.” If an upward move were the snare and the downward move were the kick, every snare would also trigger a kick when your hand came back down.

```
         your view (mirrored, like a mirror)

 LEFT HAND                         RIGHT HAND
 percussion                        drums

 ┌─ open hat (jab up) ──┐         ┌─ snare (jab) ──┐
 │                      │         │                │
 │  far left:           │  face   │                │
 │  hat roll (hold)     │  stays  │                │
 │                      │  quiet  │                │
 │  toward the middle:  │         │  kick (jab)    │
 │  perc (jab)          │         └────────────────┘
 └──────────────────────┘
```

How you play it:

- Jab high on the right for **snare**.
- Jab low on the right for **kick**.
- Hold your left hand at the far left edge for a **closed-hat roll**, timed to your project’s tempo in FL. Leaving that edge, or leaving the frame, stops the roll.
- Move that same hand inward and jab for **perc** (clap / rim / shaker).
- Jab up on the left for **open hat**.
- How hard you jab changes how loud the hit is.

The two right-hand pads stay big on purpose, so you can hit kick and snare without staring at the screen. Hats keep time by holding, not by trying to hit every 16th note in the air.

A crash or tom can sit in a corner later without crowding these main pads. They are not in the first version.

Built-in sound: a small electronic kit (kick, snare, closed hat, open hat, perc). You can also send the same hits as MIDI notes to your own drums in FL.

On-screen pads can be clicked with the mouse, so you can test without waving at the camera.

UI colour: **orange**. Orange pads, orange mode button, dark background so the pads pop.

---

## 5. Keyboard mode (turquoise)

Notes live on your fingers, not on spots you have to chase around the screen. Moving your whole hand to each note would be too slow for piano-style melodies.

How you play it:

- Keep your hand in one place and **tap a finger down** like a piano key, then lift to let go.
- Right hand thumb through pinky = five notes next to each other in the key you pick.
- Left hand = the five notes below that, like two hands on a real keyboard.
- Slide a whole hand left or right only when you want a new set of notes, the same way you shift position on a piano.
- The camera labels each fingertip with its note name while you learn.
- How hard you tap changes how loud the note is.

There is also an **easy scale** option that puts a 5-note scale on one hand, so every finger sounds good together.

You pick the key and scale (for example C minor). An octave control shifts the whole range up or down.

This asks more of the camera than the drum jabs. It works best with good lighting and your palms generally facing the camera. Missed or double hits can happen if you play extremely fast. We will test fingertip tracking on your Mac and tune it so fast lines are possible, not just slow taps.

Built-in sound: a simple keyboard/synth so you hear yourself immediately. You can also send the notes as MIDI to Serum or any other instrument.

UI colour: **turquoise**. Turquoise keys and fingertip labels, dark background, strong contrast.

---

## 6. Lead mode (purple)

Lead mode is for slower, singing lines — not a replacement for the finger keyboard. It stays in key, and it can still bend and shake like a guitar or violin.

A pure theremin (any pitch, hand floating in space) would work against you: it is hard to play in tune, especially if you have never used one, and the camera adds a little shake that can make notes sound wobbly. So Lead mode snaps to the notes in a key you pick, then adds expression on top.

How you play it:

- **Right hand** is the string:
  - Move left and right to move through the notes in key.
  - Each note **sticks** until you clearly move to the next one, so a small shake will not change the note.
  - That small shake is **vibrato** (the sing).
  - Move farther and the pitch **slides** into the next note, like a violin or a guitar slide.
  - Lift that hand up to **bend** the note, like pulling a string. The bend aims toward the next note in key, not out of tune.
- **Left hand** is the bow:
  - Up is louder, down is quieter.
  - A dip of the left hand retriggers a note, so you can phrase a line instead of one endless sound.
- If either hand leaves the frame, Lead goes silent.

You pick the same kind of key/scale as Keyboard mode.

Built-in sound: a simple singing lead tone with glide, so the mode is usable on its own. The MIDI it sends includes the notes, the pitch bends (slides, vibrato, string-style bends), and the volume expression, so a plugin like Serum can replay the same performance if its bend range is matched.

UI colour: **purple**. Purple note ribbon and mode button, dark background, strong contrast.

---

## 7. Recording: notes on the piano roll

TENRINGS acts like a MIDI keyboard, not like something that dumps audio clips on its own.

You hit record in FL, play with your hands, and the notes should land in the piano roll the same way they do when you play a regular keyboard. Then you can:

- Slide the whole take a little earlier to make up for the camera delay.
- Snap drums to the grid.
- Fix a wrong note.
- Send those same notes to another plugin, like Serum.

What gets recorded in each mode:

- **Drum:** one MIDI note per hit (kick, snare, closed hat, open hat, perc), including how hard you hit. The hat roll writes a stream of hat notes at the project tempo.
- **Keyboard:** normal note on/off for each finger tap, including how hard you tapped.
- **Lead:** the notes in key, plus pitch-bend movement (slides, vibrato, bends) and left-hand volume as expression. When you play it back, the line can still sing.

Recording audio stays optional. You would only record the mixer track if you want the exact built-in sound printed, like recording a vocal. For almost everything, MIDI is better.

### Honest note about FL Studio

FL can be picky about notes that are born inside a plugin, compared with notes that arrive from a normal keyboard. “It will definitely just appear, no matter what” is not a promise I can make until we test it in your copy of FL.

The plan to make sure it really lands on your piano roll:

1. The plugin creates real MIDI (note on, note off, how hard you hit, pitch bend, expression).
2. It can also play its own built-in sound so you hear yourself while you perform.
3. We will test the normal FL path first: record with **Notes** checked, like you would with a keyboard.
4. If FL does not capture plugin-created notes reliably, we use a backup the Mac already understands: a virtual MIDI cable named after the plugin. FL then sees your hands as a regular MIDI keyboard, which is the path that actually writes to the piano roll. If we need that, you get a short setup (turn that input on in FL’s MIDI settings).
5. Putting notes on your piano roll is a must-pass test on your Mac before we call the first version done.
6. Playback: the piano roll can play the plugin back, so you are not stuck only hearing a one-time performance. There will be a camera on/off control so playback and a live take do not fight each other.

The camera delay is usually pretty even (cameras run around 30 frames a second). After you record, slide the clip a little earlier, or snap it to the grid if you want it tight. You do not have to play perfectly in time with what you hear.

---

## 8. Window and colours

- One window, with a live mirrored camera view and a clear mode switch: **Drum (orange)**, **Keyboard (turquoise)**, **Lead (purple)**. Switching mode immediately silences whatever the previous mode was playing.
- The whole chrome of the active mode uses that mode’s colour, so you can tell which one you are in at a glance.
- Dark background, strong contrast, big controls. The main controls should be obvious.
- The window is resizable and can be minimized.
- Shared controls: camera on/off, built-in sound on/off, send MIDI on/off, key/scale (Keyboard and Lead), octave, sensitivity (how hard a jab/tap needs to be), and a status line if the camera is missing permission or no hands are in frame.
- Drum extra: hat-roll speed (for example 1/8 or 1/16).
- Lead extra: how much slide, how strong the vibrato shake is, and how far a bend goes (capped toward the next note in key).
- First time in FL: macOS will ask for camera permission for FL Studio. If permission is off, the plugin explains that and still lets you click Drum pads to test.

The camera stays on your Mac. Nothing is uploaded.

---

## 9. What the first version includes

When you say go, this is built as one finished first version (not a chain of half-builds), then you test it in FL and tell me what feels off.

Included:

- Mac camera + tracking for two hands and ten fingertips.
- The out-of-frame silence rule, the center dead zone, and mirrored view.
- Drum, Keyboard, and Lead modes as described above.
- Mode colours: orange, turquoise, purple.
- Built-in sounds for all three modes. Speed matters more than keeping the file tiny.
- MIDI out, with piano-roll recording treated as a must-pass check.
- Velocity (how hard you jab or tap).
- Hat roll synced to FL’s project tempo.
- Key/scale picker, easy 5-note scale option, octave shift.
- Camera on/off, clickable Drum pads as a fallback.
- Resizable / minimizable high-contrast window.
- Automated tests on the music math: zone hits, no double kick after a snare, finger note on/off with no stuck notes, out-of-frame = silence, scale maps, hat-roll timing, lead note-stick vs vibrato vs slide vs bend-in-key, MIDI event pairing.
- A full pass that checks every mode and every knob or button, so no control is dead. Report every problem at once.

Not in the first version:

- Extra drum pads (crash, tom) in the corners.
- Loading your own sample kit (MIDI out covers “use my own drums” for now).
- A true free-pitch theremin (any pitch, out of key).
- Per-finger tracking used in Drum mode (Drum stays whole-hand jabs, which is more reliable for hits).
- Selling, Apple notarization, or a licence system.

---

## 10. Honest limits

- Camera tracking always has some delay, but speed is the top priority. The first version should translate movement into sound as fast as the Mac camera and FL will allow (prefer 60 frames a second if the camera can do it, trigger a hit as soon as the motion starts, and never let the pretty camera picture slow down the notes). It still will not feel quite as tight as real pads, and you can fix leftover delay after you record.
- Drum mode works best with good lighting and both hands clearly in frame (or just the hand you are using).
- Keyboard mode needs clearer finger visibility. Palms toward the camera, not fully edge-on.
- Lead mode is better for sustained, expressive lines than for very fast runs. Fast melodies belong in Keyboard mode.
- Apple’s hand tracking is built into the Mac, so we do not bundle a huge extra file. That keeps the download small, but it also means this is a Mac plugin (which matches how you use FL).
- Camera tracking cannot be fully proven on the Linux computer I build on. Hit math, note maps, and “no stuck notes” get automated tests there. The live camera feel has to be tested by you in FL on your Mac.
- FL Studio must be given camera permission in System Settings the first time.

---

## 11. How it’s built and delivered

- VST3 only (no AU), for FL Studio on your Mac.
- Same path as your other plugins: written on the shared computer, Mac build through GitHub Actions in a private koobacorp repo after you approve creating it.
- Universal Mac build (Apple Silicon and Intel).
- Fast and efficient: tracking runs at camera speed on a high-priority path; the picture on screen must not delay the sound. If a larger plugin file or a bit more CPU makes the hands feel instant, that is worth it.
- You get a zip in chat and in `REP PLUG`, like the others. I only install it if you ask.
- Typical install: put the `.vst3` in `Library/Audio/Plug-Ins/VST3`, then FL Options → Manage plugins → Find plugins.


---

## 12. Speed and full testing (added Nov 7, 2026)

Priorities for the Mac plugin:

1. **Speed first.** Whatever movement he does should turn into sound as fast as possible. That is more important than keeping the plugin small. A larger file is OK if it helps.
2. **Everything must work.** Every mode, and every knob and button, gets checked. No dead controls. After the full pass, report every issue at once — not a drip of half-fixes.
3. Practical speed choices: use the highest camera frame rate available (60 fps when the camera supports it); detect drum jabs and key taps at the start of the motion, not after the hand stops; keep hand tracking off the audio thread but with the shortest possible path to triggering a note; recommend a smaller audio buffer in FL if that makes live playing feel tighter.

You can also point your iPhone at yourself and use it as the Mac's camera (Continuity Camera). That often gives a better angle and a higher frame rate without building a separate app. The phone app below is different: it records on the phone, then you bring the file into FL.

---

## 13. Phone app (separate product, not this plugin)

Yes, TENRINGS can also be an iPhone app, but it is not the VST running on a phone. It would be its own app: you perform using the phone camera, hit record, then get a file onto the Mac and drop it into FL.

You cannot drag from the iPhone screen straight onto FL. The real path is: record → share the file (AirDrop, Save to Files, or iCloud) → on the Mac, drag that file onto FL.

The phone app should export:

- A **WAV audio clip** of what you played (what was asked for).
- A **MIDI file** as well, so he can drop notes onto the piano roll and still edit them.

Hats on the phone would follow a tempo you type, since the phone is not inside the FL project.

This phone app is for **Grok Build** to make. The Mac VST3 stays with Plugin Lab. A paste-ready prompt lives in `GROK-BUILD-PHONE-APP-PROMPT.md`.

---

## 14. Waiting on you

Nothing starts on the Mac plugin until you say go.

The name is TENRINGS. The layouts, colours, piano-roll goal, speed-first rule, and full-control testing stay as written here unless you change them.

---

## 15. v1.0 built (Nov 7, 2026)

- Built as one VST3 instrument (universal arm64 + x86_64), private repo koobacorp/tenrings, GitHub Actions macOS build.
- Drum (orange), Keyboard (turquoise), Lead (purple) as planned. Camera runs at up to 60 fps (720p preferred), and Apple Vision tracks 2 hands on a high-priority queue. Events go to audio through a lock-free queue, and the preview picture is drawn after the notes.
- Live Serum: plugin MIDI out (FL wrapper MIDI output port → Serum MIDI input port), or a "TENRINGS" virtual MIDI port FL sees as a keyboard. Built-in sound can be turned off. Incoming MIDI is never echoed back out, so there are no feedback loops. Piano-roll clips play the built-in sounds.
- Lead pitch-bend range: 2 semitones (3 with Easy 5-note + Minor). Volume is CC11.
- Tests: 125 engine + 98 plugin checks pass in CI, and pluginval strictness 10 passes.
- Still to prove on the Mac: live camera feel and latency, FL camera permission, notes landing on the piano roll, and the Serum route.


## 16. v1.1 update (Nov 7, 2026)
- Keyboard is now a flat-hand air piano: palms down, camera angled down toward the desk. This supersedes the earlier palms-facing-camera instructions. Hover in a comfortable rest pose, then press a fingertip down and lift to release.
- Press model: project each tip's MCP/PIP reach onto the hand-local wrist-to-middle-knuckle axis, subtract its own learned rest plus the median common motion of the fingers. Translation, in-plane rotation and scale cancel; common rocking/drops do not trigger siblings. Rest adapts only while up; speed gives velocity. Missing joints release the finger. The independent turquoise piano row and fingertip lights share the exact same down state.
- This is 2D Vision, not depth sensing: a slightly angled view is required. Fully overhead presses can be invisible. Median rejection conservatively favors one/two independent finger presses; three-or-more simultaneous presses may be rejected as whole-hand motion. Real-camera feel still needs testing on the real setup.
- Keys are rounder, softly attacked sine-based notes through a gentle two-pole lowpass. Lead is a soft sine/triangle singing tone with stronger two-pole filtering; drums are unchanged. All synthesis is original code.
- Lead commits a new note only after horizontal motion settles for 35 ms (usually three camera frames at 60 fps), so a sweep plays start -> destination, not every crossed scale note. Hysteresis, glide, wobble vibrato, lift-bend, left-hand expression and dip retrigger remain.
- v1.1 universal macOS package: TENRINGS-macOS-universal-v1.1.zip. Same plugin identity/settings and Serum MIDI hookup as v1.0.

## 17. v1.2 update (Nov 20, 2026): Strings mode + beige look
- New 4th mode **Strings** (green). Touch both index fingertips together for 0.15 s (dotted line + hold ring), pull apart, hold still 0.3 s at >= 0.12 picture-heights long = the string is set (solid neon, tick marks), then move hands away; it stays anchored. If a hand leaves mid-pull the string is kept when long enough, cancelled when too short. Up to 8 strings; tips must separate before another string can start.
- Pitch: each new string takes the lowest unused scale step from the root of Key/Scale/Octave (E minor: E4 F#4 G4 A4 B4 C5 D5 E5); Undo frees its note. "Length sets pitch": ~0.9 picture-heights = root, every 0.09 shorter = one step up (longer = lower), clamped -7..+9 steps, always in key. Changing key/scale/octave retunes existing strings.
- Pluck: any seen fingertip whose path between two camera frames crosses a string (segment intersection, string ends excluded) plays it. One crossing = one note: a finger must move 0.025 away from a string before it can pluck it again, and one string never fires twice inside 90 ms (so a whole-hand sweep strums each string once, in crossing order). Slower than the sensitivity-based minimum speed = no note; speed -> velocity. New strings ignore plucks for 0.3 s; strings cannot be plucked while one is being made. Tracking jumps over 0.45 picture-heights in one frame (left/right hands swapping labels) are ignored.
- Sound: original warm plucked string (Karplus-Strong loop, velocity-bright excitation, pick-position comb, small wooden body resonance, gentle top roll-off), 12 voices, rings 1-5 s by pitch; a re-pluck damps the old ring. MIDI: note-on with a 0.6 s auto note-off (a re-hit inside that time sends off then on, one note-off total). Piano-roll note-offs soften the ring.
- Strings (positions, scale steps, colours) are saved in the plugin state; old v1.1 projects load with none. Undo string / Clear strings buttons. Mouse: click a string = pluck, drag across = strum, Shift- or right-drag = draw a string.
- Beige look for all modes: beige #E8DCC4 chrome, ink #1F1A12 text (12.7:1). Deepened mode colours, each >= 4.7:1 on beige with white text >= 6.4:1: Drum #9A3E00, Keyboard #006A61, Lead #6A2FB0, Strings #276B19. String colours each >= 3.4:1 on beige (brightened over the camera picture). "Beige only" hides the camera picture and draws hand outlines (palm-knuckle-finger skeleton).
- Safety: built-in output passes a soft ceiling at 0.985 (-0.13 dBFS) in every mode, NaN/Inf guarded. Latency reported 0 samples (nothing is delayed). Host bypass: silent output, held notes released once, no orphan note-offs.
- Package: TENRINGS-macOS-universal-v1.2.zip. Same plugin identity and settings; v1.1 projects open with all their settings.
