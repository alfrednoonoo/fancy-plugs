# Fancy TS: REAPER checklist

Install with `scripts\get-latest.ps1`, then in REAPER:

- [ ] Fancy TS appears under VST3 as "Fancy TS"; the window matches mockup 50 (Contour, Detector, display, ATTACK, SUSTAIN, DETECT cards, footer, Out/Wet/Dry).
- [ ] At its defaults on any track it changes nothing (flip bypass: no difference).
- [ ] Snare loop, Attack +8: the hits snap; the display's white line jumps up at each hit and the dots line up with them. Attack -8: the hits soften.
- [ ] Sustain -8, Length 300 ms on a roomy drum loop: the room shrinks; Contour Sharp, Medium, Smooth change how it lets go; the noise between hits doesn't rise with Sustain +8.
- [ ] Pump 60 %: the level dips just after each hit.
- [ ] A quiet and a loud take of the same part shape alike.
- [ ] Detector Sidechain with a kick on pins 3/4 shaping a pad: the pad's attack follows the kick. Unroute it: the red "No sidechain signal" note appears.
- [ ] EDIT > Lookahead 5 ms: REAPER's PDC follows, and the dry fader stays lined up (Wet and Dry together don't comb). Clip output with Oversampling 4x: latency rises while ticked and falls when unticked.
- [ ] Listen to change plays only the difference, and turns off when the window closes.
- [ ] Every preset loads; Save, reopen: the settings come back.
- [ ] Render offline twice: the renders match.
