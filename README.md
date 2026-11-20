# TENRINGS
v1.2 camera hand instrument (VST3) for FL Studio on Mac: Drum (orange), Keyboard (turquoise), Lead (purple), Strings (green), on a beige look.
See PLAN.md and scripts/INSTALL.txt. Engine tests: `tests/test_engine.cpp` (pure C++), plugin tests run in CI.

Keyboard: hands flat, palms down, camera angled down. Hover at rest, press individual fingertips toward the desk, lift to release. Lead: move and briefly settle on the destination to skip intervening notes. v1.1 includes warmer built-in keys and lead. See INSTALL.txt in the package.

Strings (v1.2): touch both index fingertips, pull apart, hold still = a new air string (up to 8). Swipe any fingertip through a string to pluck it; sweep a whole hand to strum. Each new string takes the next note of the chosen key (or its length sets the pitch). Built-in warm pluck plus MIDI out. Undo / Clear buttons; strings are saved with the project. "Beige only" hides the camera picture and draws hand outlines instead.
