Build an iPhone app called TENRINGS. This is a standalone camera instrument, not a VST plugin and not a Mac app. Do not build a plugin. Build a native iOS app (iPhone first, iPad is fine if it comes along easily).

I will perform in front of the front camera with my hands. The app watches my hands and turns them into music in three modes: Drum, Keyboard, and Lead. I hit Record, play, hit Stop, then share a file to my Mac so I can drop it into FL Studio.

Speed is the top priority. Whatever movement I do should turn into sound as fast as you can make it. If a larger app size or more CPU makes the hands feel instant, that is worth it. It has to work fast and efficiently. Use the highest camera frame rate you can (60 fps if the camera supports it). Detect hits at the start of the motion, not after my hand stops. The live camera preview must never delay the sound.

Test every single feature before you call it done. Check all three modes. Check every knob, button, and switch — nothing should be a dead control. Switching modes must immediately silence whatever the previous mode was playing. Report every problem you find at once.

HARD RULES
- The camera view is mirrored, like a mirror, so my left hand matches the left side of the screen.
- If a hand leaves the frame, that hand plays nothing. Any notes it was holding cut off immediately. No stuck hats, no stuck keys, no leftover lead tone.
- If neither hand is visible, the app is completely silent. My face, chest, and empty air never trigger anything.
- The center of the camera is a dead zone (where my face sits). Pads and note picking live on the sides, not on my face.
- One hand in frame can still play that hand’s part, except in Lead mode, which needs both hands. If either hand leaves in Lead, it goes silent.
- The camera stays on the phone. Do not upload video or audio anywhere. No accounts, no cloud sync required.
- All names, code, sounds, and looks must be original. Do not copy a commercial app’s UI or sounds.

COLOURS AND UI
- Dark background, strong contrast, big obvious controls.
- Mode colours: Drum = orange, Keyboard = turquoise, Lead = purple.
- The active mode should recast the chrome in that colour so I can tell which mode I am in at a glance.
- Full-screen live camera with the pads / fingertip labels / lead ribbon drawn on top.
- Big mode switch, big Record / Stop button, and a clear status if the camera permission is missing or no hands are in frame.
- First launch: ask for Camera and Microphone permissions (mic only if you need it; playback and recording of the app’s own sound should work without the mic). If camera permission is off, explain that and still let me tap Drum pads to test the sounds.

SHARED CONTROLS (every one must work)
- Mode: Drum / Keyboard / Lead
- Camera on/off (off = no tracking; Drum pads can still be tapped on screen)
- Built-in sound on/off
- Key and scale picker (used by Keyboard and Lead), for example C minor, C major, and other common keys
- Easy 5-note scale option (so every finger on one hand sounds good together)
- Octave up/down
- Sensitivity (how hard a jab or tap needs to be)
- Tempo in BPM that I type (the phone is not inside FL Studio, so hats and any grid follow this tempo). Include a simple metronome click I can turn on while recording.
- After recording: preview playback, then Share

DRUM MODE (orange)
Right hand = drums. Left hand = percussion. Hits are jabs: a quick move into a zone. Do not use “swipe up = snare, swipe down = kick.” If you did that, every snare would also trigger a kick when the hand comes back down.

Layout, mirrored like a mirror:

 LEFT HAND                         RIGHT HAND
 percussion                        drums

  open hat (jab up)                 snare (jab, upper right)

  far left: hat roll (hold)         face stays quiet in the center
  toward the middle: perc (jab)
                                    kick (jab, lower right)

How it plays:
- Jab high on the right = snare
- Jab low on the right = kick
- Hold the left hand at the far left edge = closed-hat roll, timed to the typed tempo. Leaving that edge or leaving the frame stops the roll.
- Move the left hand inward and jab = perc (clap / rim / shaker)
- Jab up on the left = open hat
- How hard I jab changes how loud the hit is
- The two right-hand pads stay big so I can hit kick and snare without staring at the screen
- On-screen pads must also be tappable with a finger, so I can test without waving at the camera
- Hat-roll speed control: 1/8 or 1/16 (synced to the typed BPM)
- Do not add crash/tom corner pads in this version

KEYBOARD MODE (turquoise)
Notes live on my fingertips, not on spots I have to chase around the screen. Moving my whole hand to each note is too slow.

- Keep my hand in one place and tap a finger down like a piano key, then lift to let go
- Right hand thumb through pinky = five notes next to each other in the key I picked
- Left hand = the five notes below that, like two hands on a real keyboard
- Sliding a whole hand left or right shifts to a new set of notes (like changing position on a piano)
- Label each fingertip with its note name on the camera view
- How hard I tap changes how loud the note is
- This needs good lighting and palms generally facing the camera. Tune tracking so fast lines are possible, not just slow taps.
- The easy 5-note scale option puts a 5-note scale on one hand so every finger sounds good together

LEAD MODE (purple)
This is for slower singing lines, not a replacement for the finger keyboard. It stays in key, and it can still bend and shake like a guitar or violin.

Do not build a pure theremin (any pitch, free-floating). That is hard to play in tune and the camera shake makes notes sound drunk. Snap to notes in the chosen key, then add expression.

- Right hand is the string:
  - Move left and right to move through the notes in key
  - Each note sticks until I clearly move to the next one, so a small shake will not change the note
  - That small shake is vibrato
  - Move farther and the pitch slides into the next note (violin / guitar slide)
  - Lift the right hand up to bend the note, like pulling a string. The bend aims toward the next note in key, not out of tune
- Left hand is the bow:
  - Up is louder, down is quieter
  - A dip of the left hand retriggers a note so I can phrase a line
- If either hand leaves the frame, Lead goes silent
- Controls that must work: slide amount, vibrato strength, bend amount (capped toward the next in-key note)

SOUND
- Include a small original electronic drum kit (kick, snare, closed hat, open hat, perc) and a simple keyboard tone and a simple singing lead tone so the app makes sound immediately
- Sounds must be original, not ripped from another product
- Velocity from jab/tap speed

RECORDING AND EXPORT (this is the point of the phone app)
- Big Record / Stop
- While recording, show a timer
- Record what I play as a stereo WAV, 48 kHz (16-bit is fine), so I can drop it on the FL Studio playlist
- Also save a MIDI file (.mid) of the same performance (notes, velocity, and for Lead: pitch bend plus expression/volume) so I can drop notes onto the FL piano roll and edit them
- After Stop: let me preview the audio, then a Share button that uses the iOS share sheet so I can AirDrop the files to my Mac, Save to Files, or save to iCloud Drive
- I cannot drag from the iPhone screen onto FL. The path is: share/AirDrop → on the Mac, drag the WAV onto the playlist and/or the MIDI onto the piano roll. The share flow should make that obvious (clear file names like TENRINGS-Drum-2026-11-07-1200.wav and the matching .mid)
- Do not require iCloud login to function; share sheet + local Files is enough
- Hands-out-of-frame silence also applies while recording (do not write phantom notes)

TECH NOTES
- Use on-device hand and fingertip tracking (Apple Vision human hand pose or better if you have something faster). No cloud tracking.
- Front camera, mirrored preview
- Trigger audio with the lowest latency you can get on iOS (use the hardware playback path meant for instruments, not a high-latency media player)
- Prevent stuck notes on: finger lift, hand leaving the frame, mode change, Record stop, app backgrounding
- If tracking is lost, silence immediately
- Original code and look

WHAT DONE MEANS
- I can pick Drum, Keyboard, or Lead, play with my hands, hear sound immediately, record a take, preview it, and share a WAV and a MIDI file to my Mac
- Every control in every mode does something real
- Movement feels as instant as the iPhone camera will allow
- You have double-checked all three modes and every knob/button and told me anything that is still weak
