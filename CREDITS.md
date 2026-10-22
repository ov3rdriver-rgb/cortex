# Credits & notices

Cortex is free software released under the **GNU Affero General Public License v3.0** (see `LICENSE`).

## Samplebrain
The brain engine (`Source/brain/`) is ported from **Samplebrain** by **Then Try This** and **Aphex Twin**
(`third_party/samplebrain`, from https://github.com/fxbeaulieu/SampleBrain).
Copyright (C) 2022 Then Try This. Licensed GPL-2.0-or-later, used here under GPL-3.0 terms.
Original copyright headers are kept on all ported files.

## Aquila
Samplebrain computes MFCCs with the **Aquila** DSP library (Copyright (c) Zbigniew Siciarz, MIT License).
Cortex reimplements the mel filterbank and DCT itself so analysis is realtime-safe; no Aquila code is compiled in.

## Cepstral morph concept
The cepstral envelope/detail morph is a reimplementation of the idea behind
`ap_cepstral_morph.amxd` by **Alexander Panos**. No part of the original patch is included.

## JUCE
Built with **JUCE** (https://juce.com), used under its AGPLv3 licensing option.
