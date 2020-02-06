Lan Tian's LADSPA plugins
=========================

A collection of LADSPA sound effect plugins I created for myself.

Currently only includes one plugin:

- LT Stereo Frequency Splitter

LT Stereo Frequency Splitter
----------------------------

This plugin is intended to be used with users that fit the following description:

- Has a portable speaker that:
  - has stereo outputs (two dedicated speakers)
  - but has no dedicated bass outputs (no bass units of any kind)
- But want to enjoy high quality bass-amplified music

If you increase the bass gain with an equalizer on your low-range to mid-range portable speaker (has two speakers (stereo), but without a dedicated bass speaker), you may notice the volume start fluctuating, and the sound quality is impacted. 

Because of mechanical limits, speaker units will have difficulty recreating the high-frequency parts of the sound, when the low-frequency signal is too high.

This plugin splits the bass portion into a dedicated channel (speaker), and the rest of the sound into the other channel (speaker). In this way you can enjoy amplified bass and unimpacted high frequency tune at the same time.

Processing steps:

- Mixes input signal down from stereo to mono
- Pass the signal through a low-pass filter to the left channel (with an optional gain)
- ... and a high-pass filter to the right channel

Credits:

- [LADSPA SDK](https://www.ladspa.org/ladspa_sdk/download.html)
- Richard W.E. Furse for his high-pass filter, low-pass filter and amp LADSPA module examples
