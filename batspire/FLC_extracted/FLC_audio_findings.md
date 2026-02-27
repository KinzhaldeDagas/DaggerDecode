# FLC Audio Findings

## Question
Why do Battlespire conversation `.FLC` files show animated portraits but no spoken audio?

## Findings

1. The UI "Speak" path for dialogue lines resolves and launches a `.flc` animation clip only; it does not attempt to also fetch/play a matching audio file in the same action path. (`CmdSpeakBsaDialogueLine` reads an FLC entry and calls `WriteTempFlcAndLaunch`.)
2. The BSA tree logic treats `FLC.BSA` as containing dialogue **voice clips** named like `xxNN.voc`/`xxNN.wav`, i.e., voice is modeled as separate assets, not embedded in `.flc` animations.
3. A direct binary scan of all extracted `.FLC` files (`batspire/FLC_extracted/*.FLC`) found only standard FLIC frame/chunk types and no audio-related chunk IDs.

## Repro scan summary

Python scan over `batspire/FLC_extracted/*.FLC`:

- Files scanned: 163
- Files with valid FLC magic: 163/163
- Top-level chunk types seen: `0xAF12` (FLC file header marker context), `0xF1FA` (frame chunks)
- Frame subchunk types seen: `0x0007`, `0x0012`, `0x0004`, `0x000F`
- Audio-like subchunk IDs detected: 0

## Conclusion

The conversation portrait `.FLC` files in this repo are animation-only (video frames). Spoken dialogue is expected to live in separate `.VOC/.WAV` assets in `FLC.BSA` (as implied by the UI categorization). Therefore, playing only the `.FLC` files results in silent animation.
