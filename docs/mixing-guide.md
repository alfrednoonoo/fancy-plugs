# Mixing with Fancy Plugs

How to treat drums, bass, guitars, vocals, sermons, podcasts and voiceovers with the five plugins in REAPER.

Every number here is a starting point for a typical recording, not a rule. Trust your ears over the table, and compare with the plugin bypassed at matched level, because louder nearly always sounds better at first.

## Before you start

**Set levels first.** Aim for track peaks around -12 to -6 dBFS before any plugin. The Threshold values below assume that. If your track is louder or quieter, ignore the Threshold number and go by the gain reduction (GR) target, which is the number that matters.

**Know each plugin's job.**

| Plugin | Use it to | Don't expect it to |
|---|---|---|
| Fancy EQ | Remove problems and shape tone | Fix a bad mic position or a noisy room |
| Fancy Comp | Control level, add punch, duck one track under another | Remove noise. Compression brings room noise up |
| Fancy Kicker | Add weight at one low frequency | Clean up muddy lows |
| Fancy Exciter | Add brightness that wasn't there | Replace a dull recording completely. It also brightens hiss and sibilance |
| Fancy Verb | Add space, or short echoes | Remove the room you recorded in |

**What the set doesn't have.** There's no gate, noise reducer, multiband de-esser or loudness meter. REAPER covers the gaps:

| Need | REAPER tool |
|---|---|
| Gate | ReaGate |
| Noise reduction | ReaFIR in Subtract mode |
| Loudness reading | JS: Loudness Meter, or the render dialog's dry run |
| Clip gain | Item volume handle, or item envelopes |

**Controls that catch people out:**

- **Fancy Exciter does nothing until Harmonics is above 0 %.**
- **Fancy Comp's Threshold starts at 0 dB,** so nothing compresses until you pull it down.
- **Comp attack and release are time constants.** Tick **Attack = 90 % rise** in EDIT if you're copying settings from a compressor that quotes rise time, or they'll seem about 2.3 times faster.
- **Comp's limiter ceiling is fixed at 0 dBFS,** and its Output knob comes before the limiter. To land lower, pull down the track fader, which comes after the plugins. See [Delivery](#delivery).
- **Fancy Verb's module settings can't be automated.** Pre-delay, Center, Width, Wet, Dry and Output can.
- **Loading a Verb factory preset resets Dry to 0 dB.** On a send track, pull Dry back to -inf afterwards.
- **Set latency-changing options before you play.** Comp's Pre-comp, Oversampling and Limit output, and Exciter's Oversampling, cause a short dropout when changed.

## Using Fancy Verb as a send

Use one or two shared reverbs rather than one per track, so everything sits in the same space.

1. Add a track called `Verb` and put Fancy Verb on it.
2. Pick a factory preset, or load an IR with a File module.
3. Set **Wet 0 dB, Dry -inf**.
4. Send other tracks to it, and set the amount with each send's level.

Add a Fancy EQ after the Verb to thin out the return: high pass at 150–300 Hz, low pass at 6–10 kHz. Or use the LP/HP module inside the Verb, which changes the IR itself.

| Preset | Good for |
|---|---|
| Small Room | Drums, guitars, gluing a dry vocal into a mix |
| Plate | Vocals, snare |
| Hall | Ballads, strings, big choruses |
| Long Tail | Ambient and special effects |
| Slapback | Rock and country vocals, rhythm guitar |

## Drums

### Kick

**Chain:** Fancy EQ → Fancy Comp → Fancy Kicker

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 30 Hz, 24 dB/oct | Removes rumble you can't hear but the limiter can |
| EQ | Band, cut | 300–500 Hz, -3 to -6 dB, Q 2 | The cardboard "box" sound |
| EQ | Band, boost | 3–5 kHz, +2 to +4 dB, Q 1.5 | The beater's click |
| Comp | Ratio, Attack, Release | 4:1, 20–30 ms, 80–120 ms | A long attack lets the click through |
| Comp | RMS size | 0 ms | Peak detection, to catch each hit |
| Comp | GR target | 3–6 dB | |
| Kicker | Freq | 50–60 Hz; 45 Hz for sub-heavy styles | Sweep it to find where the kick already has its note |
| Kicker | Wet, Dry | -12 to -6 dB, 0 dB | Start low. Listen on small speakers as well |
| Kicker | Filter Q, in EDIT | 2–3 | Higher rings longer and starts to sound like a tone |

Tune Kicker's Freq to the song's key if you can, because a kick that fights the bass note sounds weak.

### Snare

**Chain:** Fancy EQ → Fancy Comp

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 80–100 Hz, 12 dB/oct | Removes kick bleed |
| EQ | Band, boost | 180–220 Hz, +2 to +3 dB, Q 1.5 | Body |
| EQ | Band, cut | 400–800 Hz, -2 to -4 dB, Q 3 | Honk. Find it by sweeping a narrow boost, then cut |
| EQ | Notch, if needed | 300–900 Hz | A ringing drum head |
| EQ | High shelf | 6–8 kHz, +2 dB | Snap |
| Comp | Ratio, Attack, Release | 4:1, 10–20 ms, 100 ms | Longer attack gives more crack |
| Comp | GR target | 3–5 dB | |

Send to Plate at about -18 dB.

### Toms

**Chain:** Fancy EQ → Fancy Comp

Use high pass 60–80 Hz, Band boost 80–120 Hz for floor toms or 150–250 Hz for rack toms, and Band cut -3 to -5 dB at 300–500 Hz. Compress 4:1, attack 15 ms, release 150 ms, with 3–6 dB GR. Cut the silence between hits with item edits rather than a gate, because that's cleaner and costs nothing.

### Overheads and room

**Chain:** Fancy EQ → Fancy Comp (room only)

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 100–200 Hz, 12 dB/oct | Leaves the low end to the close mics |
| EQ | Band, cut | 300–500 Hz, -2 to -3 dB, Q 1 | Boxiness |
| EQ | High shelf | 10 kHz, +1 to +2 dB | Air, if the cymbals aren't already harsh |
| Comp, room | Ratio, Attack, Release | 8:1, 1 ms, 100 ms | Crushed on purpose; blend it in with the track fader |

Set EQ to **Linear** phase in EDIT on multi-mic drums if a cut changes the phase relationship between mics. It adds latency, which REAPER compensates for.

### Drum bus

**Chain:** Fancy EQ → Fancy Comp → Fancy Exciter (optional)

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| Comp | Preset | Gentle Glue | Glue |
| Comp | Ratio, Attack, Release | 2:1, 30 ms, 200–300 ms | Set release so GR recovers between hits, in time with the song |
| Comp | Highpass (detector) | 80–100 Hz | Stops the kick driving all the compression |
| Comp | GR target | 1–3 dB | |
| Exciter | Freq, Clip Boost, Harmonics | 6–8 kHz, 3 dB, 10–20 % | Life on dull cymbals |

For parallel punch, load **Parallel Crush** on the bus (Wet -6 dB, Dry 0 dB, 10:1, attack 1 ms, release 80 ms). Set Threshold for 10–15 dB GR, then bring Wet up until you notice it, and back off a touch.

## Bass

**Chain:** Fancy EQ → Fancy Comp → Fancy Kicker (optional) → Fancy Exciter (optional)

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 30–40 Hz, 24 dB/oct | Tightens the low end |
| EQ | Band, cut | 200–300 Hz, -2 to -4 dB, Q 1.5 | Mud |
| EQ | Band, boost | 700 Hz–1 kHz, +2 dB, Q 1.5 | Growl, so it reads on small speakers |
| EQ | Band, boost | 2–3 kHz, +1 to +3 dB, Q 2 | Finger or pick attack |
| Comp | Ratio, Attack, Release | 4:1, 10–30 ms, 150–250 ms | Even notes without losing the pluck |
| Comp | Auto | On for fast or uneven playing | Program-dependent release |
| Comp | GR target | 4–8 dB | Bass can take more than most |
| Kicker | Freq | 60–100 Hz, above the kick's frequency | Gives kick and bass their own places |
| Kicker | Wet | -15 to -9 dB | |
| Kicker | Mono below Freq, in EDIT | On for stereo bass | Keeps the low end mono-compatible |
| Exciter | Freq, Clip Boost, Harmonics | 800 Hz–1.5 kHz, 6 dB, 20–30 % | Makes notes audible on phone speakers |
| Exciter | Character, in EDIT | 20–40 % | Warmer, even harmonics |

**Ducking bass under the kick:** this gives each kick room when they share a frequency.

1. Send the kick track to channels 3/4 of the bass track.
2. On a Fancy Comp on the bass, set Detector input to **Sidechain L+R**.
3. Check inputs 3 and 4 reach the sidechain pins in the pin connector.
4. Use ratio 4:1, attack 1–5 ms, release 60–120 ms, and 2–4 dB GR on each kick.

## Guitars

### Electric rhythm

**Chain:** Fancy EQ → Fancy Comp (optional)

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 80–120 Hz, 24 dB/oct | Leaves room for bass and kick. Go higher in a dense mix |
| EQ | Low pass | 8–10 kHz, 12 dB/oct | Removes fizz from distortion |
| EQ | Band, cut | 200–400 Hz, -2 to -3 dB, Q 1 | Mud and boxiness |
| EQ | Band, cut | 2.5–4 kHz, -2 dB, Q 3 | Harshness, if it fights the vocal |
| Comp | Ratio, GR | 2:1, 1–2 dB | Distorted guitars are already compressed, so use little or none |

Double-tracked guitars panned hard left and right sound wider than any effect. Send to Small Room at -20 dB or lower, or use **Slapback** for a dry-but-big sound.

### Electric lead

**Chain:** Fancy EQ → Fancy Comp → Fancy Exciter → send to Verb

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 100–150 Hz | |
| EQ | Band, boost | 1–2.5 kHz, +2 to +3 dB, Q 1 | Pushes it forward |
| Comp | Ratio, Attack, Release | 3:1, 15 ms, 150 ms | Sustain |
| Comp | GR target | 3–6 dB | |
| Exciter | Freq, Clip Boost, Harmonics | 3 kHz, 3–6 dB, 10–20 % | Bite. Stop if it turns fizzy |

Send to Plate or Hall with Pre-delay at 40–80 ms, so the note stays clear in front of the tail. Automate the Verb send up at phrase ends.

### Acoustic

**Chain:** Fancy EQ → Fancy Comp → Fancy Exciter → send to Verb

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 80–120 Hz, 24 dB/oct | Removes body boom and mic stand noise |
| EQ | Band, cut | 150–250 Hz, -3 to -5 dB, Q 1.5 | Boom |
| EQ | Band, cut | 800 Hz–1 kHz, -2 dB, Q 2 | Nasal, papery tone |
| EQ | High shelf | 8–10 kHz, +2 to +3 dB | Sparkle |
| Comp | Ratio, Attack, Release | 3:1, 15–25 ms, 150 ms | Lets the pick through |
| Comp | Knee | 6 dB | Hides the compression |
| Comp | GR target | 2–5 dB | |
| Exciter | Freq, Clip Boost, Harmonics | 6–8 kHz, 3 dB, 10–20 % | Airy shimmer on strummed parts |

## Vocals

### Lead vocal chain

Run the plugins in this order, because each stage gets a cleaner signal to work with:

**Fancy EQ (cleanup) → Fancy Comp (peaks) → Fancy Comp (level) → Fancy EQ (tone) → Fancy Exciter → send to Verb**

**Fix big level jumps first.** Ride item volume or clip gain on phrases that jump more than about 6 dB, so the compressors don't have to.

**1. Cleanup EQ**

| Setting | Value | Focus on |
|---|---|---|
| High pass | 70–90 Hz for low voices, 100–130 Hz for high voices, 18–24 dB/oct | Plosives and rumble. Go up until the voice thins, then back off |
| Band, cut | 200–350 Hz, -2 to -4 dB, Q 1.5 | Mud |
| Band, cut | 400–600 Hz, -2 dB, Q 2 | Boxiness from a small room |
| Notch or narrow Band | Wherever a resonance rings | Sweep a narrow boost to find it, then cut. Use as few as you can |

**2. Peak compressor.** Quick, catching only the loudest syllables.

| Setting | Value | Focus on |
|---|---|---|
| Ratio | 4:1 to 6:1 | |
| Attack, Release | 2–5 ms, 60–100 ms | |
| RMS size | 0–5 ms | |
| Highpass (detector) | 100 Hz | Keeps plosives from triggering it |
| GR target | 3–6 dB on peaks, 0 between them | If it never returns to 0, Threshold is too low |

**3. Level compressor.** Slow, and holds the whole performance forward.

| Setting | Value | Focus on |
|---|---|---|
| Preset | Vocal Control is a good start | |
| Ratio | 2:1 to 3:1 | |
| Attack, Release | 15–30 ms, 150–300 ms | The attack keeps consonants |
| Knee | 6 dB | |
| GR target | 2–4 dB, moving steadily | |

**4. Tone EQ**

| Setting | Value | Focus on |
|---|---|---|
| Band, boost | 2.5–5 kHz, +1 to +3 dB, Q 1 | Presence and clarity |
| High shelf | 10–12 kHz, +1 to +3 dB | Air |
| Band, boost | 150–220 Hz, +1 dB, Q 1 | Warmth, only if it sounds thin |

**5. Exciter**

| Setting | Value | Focus on |
|---|---|---|
| Freq | 5–8 kHz | Lower sounds forward, higher sounds airy |
| Clip Boost | 3–6 dB | |
| Harmonics | 10–25 % | Stop before S sounds spit |
| Mix Back | -12 to -6 dB | |
| Character, in EDIT | 30–50 % | Warmer at the low end |

**6. Reverb and delay.** Use a Verb send with the Plate preset at -18 to -12 dB, and Pre-delay 20–60 ms so the words stay ahead of the tail. Add an Echo generator for a rhythmic delay. Its Spacing is `60000 ÷ BPM` in ms for a quarter note, or half that for an eighth. So at 120 BPM it's 500 ms or 250 ms. Lower End Vol. to shorten the tail.

### Handling sibilance

There's no dedicated de-esser, so use a Fancy Comp as a broadband one. It turns the whole voice down during an S, not only the S.

1. Put a Fancy Comp before the Exciter.
2. Tick **Preview filter**, and sweep Highpass (detector) from 4 to 9 kHz until you hear mostly S. Then untick it.
3. Use ratio 4:1 to 6:1, attack 0.5–1 ms, release 40–60 ms, RMS size 0 ms.
4. Set Threshold so each S gets 3–6 dB GR, and normal words get none.

If it dulls the voice or you hear it pumping, use less and fix the worst S with item clip gain. If the Exciter is making it worse, lower Harmonics or raise its Freq.

### Backing vocals

**Chain:** Fancy EQ → Fancy Comp → send to Verb

Use a higher high pass than the lead, at 120–200 Hz, and cut 2–4 dB at 2.5–4 kHz, where the lead lives. Compress harder at 4:1 with 6–8 dB GR so the voices blend, and add more reverb than the lead so they sit behind it.

### Ducking the music under the vocal

To make room for a vocal without a static EQ cut, put a Fancy Comp on the music bus and route the vocal to its sidechain. Use ratio 2:1, attack 10 ms, release 200 ms and 1–2 dB GR.

## Speech

Speech asks for less processing than music. Listeners notice harshness, hiss and uneven level before anything else, and reverb is rarely wanted.

**Editing comes first:**

1. Cut mistakes, long pauses and false starts. Ripple editing in REAPER saves time.
2. Keep breaths, but turn distracting ones down 6–10 dB rather than cutting them. Silent gaps sound unnatural.
3. Put 5–10 ms crossfades on every cut to avoid clicks.
4. Even out phrases and speakers with item volume before any compression.
5. Reduce steady noise with ReaFIR before compressing, because compression raises it.

### Podcasts

**Voice chain, one per speaker:** ReaFIR if needed → Fancy EQ → Fancy Comp → Fancy Comp (optional sibilance) → Fancy Exciter (optional)

**Master chain:** Fancy Comp as a limiter, with loudness set in REAPER's render dialog.

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 80 Hz for low voices, 100–120 Hz for high voices, 24 dB/oct | Plosives, desk thumps, rumble |
| EQ | Band, cut | 200–400 Hz, -2 to -4 dB, Q 1.5 | Boxy room and close-mic boom |
| EQ | Band, boost | 3–5 kHz, +2 dB, Q 1 | Intelligibility |
| EQ | High shelf | 10 kHz, +1 to +2 dB | Air. Skip it if there's hiss |
| Comp | Ratio, Attack, Release | 3:1, 10 ms, 150–250 ms | Natural, with no pumping |
| Comp | Knee | 6 dB | |
| Comp | Highpass (detector) | 100 Hz | |
| Comp | GR target | 3–6 dB during speech, 0 in gaps | |
| Exciter | Freq, Clip Boost, Harmonics | 6–8 kHz, 3 dB, 10–15 % | Only for a dull mic |

**Music under speech:** put a Fancy Comp on the music track, send the voice to its sidechain, and set Detector input to **Sidechain L+R**.

| Setting | Value |
|---|---|
| Ratio | 4:1 |
| Attack, Release | 10–20 ms, 300–500 ms |
| GR target | 10–15 dB while someone speaks |

A slow release stops the music jumping up between words.

**Remote guests:** process each track separately. For a thin laptop or phone recording, try a Band boost of +2 dB at 180 Hz and a Low pass at 12 kHz to hide codec artifacts, and go easy on the Exciter, because it brightens those artifacts too. Add a very low Small Room send, around -30 dB, only if the voices need to feel like one space.

### Sermons

Sermons usually come from a lapel, headset or pulpit mic in a large reverberant room, sometimes with PA bleed. The hard parts are a wide dynamic range and room sound you can't remove.

**Chain:** ReaFIR if there's HVAC hum or hiss → Fancy EQ → Fancy Comp (slow) → Fancy Comp (peaks) → master limiter

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 90–120 Hz, 24 dB/oct | Handling noise, stage rumble, HVAC |
| EQ | Notch | 50 or 60 Hz and harmonics | Mains hum, if you have it |
| EQ | Band, cut | 250–500 Hz, -3 to -5 dB, Q 1 | Room buildup and chest-mounted lapel boom |
| EQ | Band, boost | 2–4 kHz, +2 to +3 dB, Q 1 | Words carry better over a phone or car speaker |
| EQ | Low pass | 12–14 kHz, 12 dB/oct | Removes hiss, and old recordings often have little above that |
| Comp, slow | Ratio, Attack, Release | 2.5:1, 30 ms, 400–600 ms | Levels whispers against shouts over whole sentences |
| Comp, slow | Knee | 12 dB | |
| Comp, slow | GR target | 3–8 dB | |
| Comp, peaks | Ratio, Attack, Release | 6:1, 3 ms, 80 ms | Catches sudden emphasis |
| Comp, peaks | GR target | 3–6 dB on shouts only | |

**Focus on:**

- **Do the big moves by hand.** Sermons can swing 20 dB between a quiet aside and a climax. Ride those with item volume or an envelope first, or the compressors will pump and raise the room between phrases.
- **Don't add reverb.** The room is already there. Fancy Verb has no use here.
- **Watch the room.** If it gets louder as you compress, reduce GR and do more by hand.
- **Skip the Exciter on most sermons.** It brings up hiss and room tone. Use it only on a clean headset mic, at Freq 5 kHz and Harmonics under 10 %.
- **Cut congregation noise and long gaps.** A congregational response or song in the recording will need its own level, so split it to its own item.
- **Keep one preset.** Save your chain with `+ New`, so every week starts from the same sound.

### Voiceovers and audiobooks

Voiceover needs a clean, close, consistent sound. It's heard exposed, often over music, so noise and mouth clicks stand out.

**Chain:** Edit cleanup → Fancy EQ → Fancy Comp → Fancy Comp (sibilance) → Fancy Exciter (optional) → Fancy Comp as limiter

| Plugin | Setting | Value | Focus on |
|---|---|---|---|
| EQ | High pass | 70–90 Hz for low voices, 100 Hz for high voices, 24 dB/oct | Keep it transparent |
| EQ | Band, cut | 200–300 Hz, -2 to -3 dB, Q 1.5 | Proximity boom |
| EQ | Band, boost | 3–5 kHz, +1 to +2 dB, Q 1 | Clarity. Add it lightly, or it tires listeners |
| EQ | High shelf | 10–12 kHz, +1 to +2 dB | Polish on commercials |
| Comp | Ratio, Attack, Release | 3:1, 5–10 ms, 100–150 ms | Consistent, close and forward |
| Comp | GR target | 3–5 dB; 6–8 dB for commercials | |
| Exciter | Freq, Clip Boost, Harmonics | 7–10 kHz, 3 dB, 5–15 % | Commercial sheen only |

**By type:**

| Type | Treatment |
|---|---|
| Commercial | Stronger compression and presence. Use a Small Room send only if the client wants one |
| Narration, documentary | Natural; 2–4 dB GR and no Exciter |
| Audiobook | The lightest processing, and identical settings across every chapter |
| Explainer or e-learning | Clear and even; 3–5 dB GR |
| Phone or radio effect | EQ with High pass 300–500 Hz and Low pass 3–4 kHz, both 24 dB/oct. Add Exciter at 1 kHz, Clip Boost 12 dB, Harmonics 40 % for grit |

**Effects:** for a hall or announcement sound, use the Slapback or Hall preset on a send, and automate the send so it opens only on the phrase you want. A reversed swell comes from a Verb chain of Reverb Generator → Reverse, with Pre-delay set by ear.

Record room tone, and fill gaps with it rather than with silence. Silence sounds like a dropout after a voice with any noise floor.

## Delivery

Put the limiter last on the master chain:

1. Use a Fancy Comp with **Limit output** ticked. Its ceiling is 0 dBFS.
2. Raise Output or Wet until the limiter works on only the loudest peaks.
3. Pull the master fader down to your peak target. The fader comes after the plugins, and this is the only way to land under 0 dBFS with Fancy Comp's limiter.
4. Check the result with JS: Loudness Meter, or the dry run in REAPER's render dialog.

Fancy Comp's limiter works on sample peaks. It doesn't look at true peak, which can overshoot between samples, so leave the margin below and check the true-peak reading.

Check each platform's current spec before delivering, because these change:

| Destination | Loudness | Peak |
|---|---|---|
| Podcast, stereo | -16 LUFS | -1 dBTP |
| Podcast, mono | -19 LUFS | -1 dBTP |
| Sermon online | -16 LUFS | -1 dBTP |
| Streaming music | -14 LUFS as a guide | -1 dBTP |
| YouTube | -14 LUFS as a guide | -1 dBTP |
| Broadcast, EBU R128 | -23 LUFS | -1 dBTP |
| Broadcast, ATSC A/85 | -24 LKFS | -2 dBTP |
| Audiobook, ACX | -23 to -18 dB RMS, noise floor under -60 dB RMS | -3 dB |

For a single voice, consider rendering mono, which halves the file size. A podcast at -19 LUFS mono plays back about as loud as -16 LUFS stereo.

## Quick problem guide

| Problem | Try |
|---|---|
| Muddy | EQ cut at 200–400 Hz |
| Boxy | EQ cut at 400–600 Hz |
| Harsh | EQ cut at 2.5–4 kHz, narrow |
| Thin | EQ boost at 150–250 Hz, or less high pass |
| Dull | EQ high shelf, then Exciter |
| Lost in the mix | EQ boost at 2–5 kHz before turning it up |
| Pumping | Longer Comp release, or less GR |
| Lost attack | Longer Comp attack |
| Noise rising in gaps | Less GR, higher Threshold, or ReaFIR first |
| Spitty S | Comp as a de-esser, or lower Exciter Harmonics |
| Weak low end on small speakers | Exciter on bass, around 800 Hz–1.5 kHz |
| Weak kick | Kicker at the kick's own note |
| Reverb washing over words | More Pre-delay, or a lower send |
