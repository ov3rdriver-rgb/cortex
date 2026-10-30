CORTEX {{VERSION}} - TESTER PACKAGE (WINDOWS)
=======================================

1. WHAT CORTEX IS
-----------------
Cortex is an audio plug-in. It changes the sound of an input.

Cortex has two "brains". A brain is a set of audio samples that
Cortex analyses.

Cortex cuts the input into short blocks. Each block is
approximately 20 ms long. For each block of the input, each brain
finds the block in its own samples that sounds most similar.

The two brains do not use data from each other. Both brains use
only the input.

Cortex then mixes three sources:
 - the Input
 - Brain 1
 - Brain 2

Cortex divides the sound of each source into two parts:
 - The ENVELOPE. This part gives the colour of the sound (the timbre).
 - The DETAIL. This part gives the notes and the texture.

You set the mix of the envelopes and the mix of the details
separately. Thus, you can use the notes of one source with the
colour of a different source.


2. CONTENTS OF THIS PACKAGE
---------------------------
VST3\Cortex.vst3               The effect plug-in.
VST3\Cortex Instrument.vst3    The instrument plug-in.
Standalone\Cortex.exe          The effect as a stand-alone program.
Standalone\Cortex Instrument.exe
                               The instrument as a stand-alone program.
README.txt                     This document.
INSTALL.txt                    How to install and remove Cortex.
QUICK-START.txt                How to make a first sound.
KNOWN-ISSUES.txt               Problems that we know about.
FEEDBACK.txt                   How to send a report.
LICENSE.txt                    The licence (GNU AGPL 3.0).
CREDITS.md                     The people and projects that Cortex uses.
CHANGELOG.md                   The changes in each version.
Inter-OFL.txt, Barlow-OFL.txt  The licences of the fonts in Cortex.
Source\cortex-{{VERSION}}-source.zip The source code of this version.


3. THE TWO PLUG-INS
-------------------
CORTEX (EFFECT)
  Put Cortex on an audio track. The Input is the audio on the track.
  You can also use a sidechain as the Input.

CORTEX INSTRUMENT
  Put Cortex Instrument on an instrument (MIDI) track. The Input is
  an oscillator. You play the oscillator with MIDI notes. The
  oscillator plays one note at a time.


4. REQUIREMENTS
---------------
 - Windows 10 or Windows 11, 64-bit.
 - A host program (DAW) that loads VST3 plug-ins. Alternatively, use
   the stand-alone programs.
 - Folders of audio samples (WAV, AIFF, FLAC or OGG) for the brains.

For macOS, use the package "Cortex-{{VERSION}}-macOS.zip".


5. VERSION
----------
Version {{VERSION}}, build {{HASH}}.
The version and the build show at the top right of the Cortex window.
Make sure that this version shows before you send a report.


6. LICENCE AND SOURCE CODE
--------------------------
Cortex is free software under the GNU Affero General Public License,
version 3. Refer to LICENSE.txt.

The source code of this version is in the Source folder.

Cortex uses code from Samplebrain by Then Try This and Aphex Twin.
Refer to CREDITS.md.
