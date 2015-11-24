# sidsynth
Simple SID synth using resid and libsidplay2, visuals using OpenGL/GLUT. Project files for Mac OS X.

There're two modes: synth mode and .sid playback mode. You can switch between them in sid.cpp (look for #define SYNTH_MODE 1).

You can connect a midi keyboard and basic controls should work. In addition I've routed input from my Keylab midi controller
to some of the knobs. Keyboard and mouse can be used for tweaking the knobs, but that's quite tedious. Look up the keys from
the code.

Playback mode loads a hardcoded set of .sids (list is in sid.cpp).

I'm using a slightly tweaked resid for SID emulation. I basically added support for more than 3 voices (currently at 8, which is
what my laptop can handle without underruns, see NUM_VOICES in siddefs.h). Plus there's some feeble attempts at adding
digital filters like bass and treble boost, harmonics, and fuzz. The effects are combined in sid.cc:SID::clock().

For visualization, I draw the final waveform and Fourier spectrum. The spectrum is computed at AudioCoreDriver::fillBuffer().
