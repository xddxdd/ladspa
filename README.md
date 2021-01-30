Lan Tian's LADSPA plugins
=========================

A collection of LADSPA sound effect plugins I created for myself.

Current list of plugins:

- LT Stereo Frequency Splitter
- LT Stereo Frequency Splitter (w/ Dynamic Boost)

Table of Contents
-----------------

- [Lan Tian's LADSPA plugins](#lan-tians-ladspa-plugins)
  - [Table of Contents](#table-of-contents)
  - [LT Stereo Frequency Splitter](#lt-stereo-frequency-splitter)
  - [LT Stereo Frequency Splitter (w/ Dynamic Boost)](#lt-stereo-frequency-splitter-w-dynamic-boost)

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
- Pass the signal through a low-pass filter to one channel (randomly selected, with an optional gain)
- ... and a high-pass filter to the other channel

This plugin supports randomly selecting the bass channel (speaker) on startup, to avoid damage to one of the speakers in case the bass gain is set too high for too long time. It also supports optional "wear leveling", which switches channel during playback. Note that switching during playback may make the music sound weird.

Credits:

- [LADSPA SDK](https://www.ladspa.org/ladspa_sdk/download.html)
- Richard W.E. Furse for his high-pass filter, low-pass filter and amp LADSPA module examples

LT Stereo Frequency Splitter (w/ Dynamic Boost)
-----------------------------------------------

This is the stereo frequency splitter plugin, combined with dynamic gains & caps for bass/treble channels.

Without dynamic gain, you need to set a very high gain for music with little to no bass, and you may forget it later, and possibly break your speaker with some rock music.

Even if your speakers isn't breaking, hearing distorted (physically limited) sound isn't pleasing either.

Now with a dynamic gain, you can avoid having to adjust gains from music to music.

The dynamic boost version of the plugin will (by default) amplify both bass & treble channel to the 0 dB baseline, which is a safe value to make sure that your speakers won't break, while you can enjoy strong beats at the same time.
