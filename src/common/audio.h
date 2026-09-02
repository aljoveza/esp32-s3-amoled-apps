#pragma once
#include <Arduino.h>

// =========================================================================
//  Audio — ES8311 codec + I2S, for short tones only
//
//  This board's audio path (ES8311 codec, I2S bus, speaker amp on the PA
//  pin) had no runtime on this project before this module. It is brought up
//  defensively: audioBegin() returns false rather than blocking anything if
//  any step fails, and audioBeep() is always safe to call -- it is simply a
//  no-op when the hardware never came up. See docs/HARDWARE.md.
//
//  This is a tone generator, not a media player: audioBeep() synthesizes a
//  sine wave on the fly. There is no file playback and no mixing -- for a
//  short phase-change chime, that is all this board needs.
// =========================================================================

bool audioBegin();
bool audioAvailable();

// Blocks for durationMs while it streams the tone out. Calls this short (a
// couple hundred ms at most) since nothing else runs on this core meanwhile.
// A no-op, safe to call unconditionally, when the hardware never came up.
void audioBeep(uint16_t freqHz, uint16_t durationMs, uint8_t volumePercent = 70);
