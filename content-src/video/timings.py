"""Word timings for the voice lines, read by hand into film.py's caption times.

    .deps/tts-venv/Scripts/python content-src/video/timings.py

Writes <cut>/timing/<line>.json for every wav in <cut>/assets/voice. Needs faster-whisper and soundfile.
"""
import json

import numpy as np
import soundfile as sf
from pathlib import Path

from faster_whisper import WhisperModel

HERE = Path(__file__).resolve().parent
model = WhisperModel("base.en", device="cpu", compute_type="int8")
for cut in ("wide", "tall"):
    for wav in sorted((HERE / cut / "assets/voice").glob("*.wav")):
        # Decoded here, because faster-whisper's own decoder fails with the PyAV in this environment.
        audio, rate = sf.read(str(wav), dtype="float32", always_2d=True)
        audio = audio.mean(axis=1)
        audio = np.interp(np.arange(0, len(audio), rate / 16000), np.arange(len(audio)), audio).astype("float32")
        segs, _ = model.transcribe(audio, word_timestamps=True)
        words = [dict(text=w.word.strip(), start=round(w.start, 2), end=round(w.end, 2)) for s in segs for w in s.words]
        (HERE / cut / "timing" / f"{wav.stem}.json").write_text(json.dumps(words), encoding="utf-8", newline="\n")
        print(cut, wav.stem, " ".join(f"{w['text']}@{w['start']}-{w['end']}" for w in words))
