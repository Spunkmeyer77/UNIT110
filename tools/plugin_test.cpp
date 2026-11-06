// End-to-end check of the plug-in: host-synced sequencer, pattern queueing, keyboard recording,
// state save/restore, and an editor snapshot.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/MidiExport.h"
#include "../Source/PluginEditor.h"
using tross110::slotName;

struct FakeHost : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        return p;
    }
};

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory();
    const double sr = 48000.0;
    const int block = 480;   // 10 ms; a 16th at 120 bpm = 125 ms = 6000 samples

    Tross110Processor proc;
    FakeHost host;
    proc.setPlayHead (&host);
    proc.setRateAndBufferSizeDetails (sr, block);
    proc.prepareToPlay (sr, block);
    std::printf ("params: %d\n", (int) proc.getParameters().size());
    auto pv = [&] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    auto near = [] (float a, float b) { return std::abs (a - b) < 0.01f; };
    check (near (pv ("bd_decay"), 27.8f) && near (pv ("bd_lvl"), -8.4f) && near (pv ("sd_decay"), 43.9f) && near (pv ("sd_snappy"), 60.0f)
           && near (pv ("cp_tune"), 2.6f) && near (pv ("cp_decay"), 30.0f) && near (pv ("cp_lvl"), -3.6f) && near (pv ("cp_tail"), 15.0f)
           && ! proc.showHelp.load(),
           "defaults: BD Decay 27.8 %, Level -8.4 dB; SD Decay 43.9 %, Snappy 60 %; CP Tune 2.6 st, Decay 30 %, Level -3.6 dB, Tail 15 %; HELP off");
    check (proc.apvts.getParameter ("pattern")->getNumSteps() == 32, "32 pattern slots (banks A-D x 8)");
    check (proc.apvts.getRawParameterValue ("rec")->load() < 0.5f && proc.getMode() == Tross110Processor::PlayPattern, "starts in PLAY PATTERN with REC off");
    proc.setMode (Tross110Processor::WriteTap);
    check (proc.apvts.getRawParameterValue ("rec")->load() > 0.5f, "WRITE TAP turns REC on");
    std::printf ("BD Decay shows as \"%s\"\n", proc.apvts.getParameter ("bd_decay")->getCurrentValueAsText().toRawUTF8());

    auto& pats = proc.patterns;
    auto put = [&] (int slot, int row, const char* s) { for (int i = 0; i < 16; ++i) pats.setCell (slot, row, i, s[i] == 'x'); };
    put (0, 0, "x.......x.......");
    put (0, 1, "x...x...x...x...");
    put (0, 2, "....x.......x...");
    put (0, 4, "x.x.x.x.x.x.x.xx");
    put (0, 5, "..............x.");
    put (1, 1, "x..x..x...x..x..");
    put (1, 3, "....x.......x...");
    put (1, 6, "x...............");
    put (11, 1, "x.x.x.x.x.x.x.x.");

    const int totalOuts = proc.getTotalNumOutputChannels();
    std::printf ("output buses: %d, channels: %d\n", proc.getBusCount (false), totalOuts);
    juce::AudioBuffer<float> buf (totalOuts, block);
    juce::AudioBuffer<float> all (2, (int) sr * 9);
    int written = 0, stepsSeen = 0, lastStep = -1, bad = 0;
    float peak = 0;
    std::vector<int> slotAtStep0;

    auto run = [&] (int numBlocks, std::function<void (int, juce::MidiBuffer&)> perBlock = {})
    {
        for (int b = 0; b < numBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (perBlock) perBlock (b, midi);
            proc.processBlock (buf, midi);
            if (host.playing) host.ppq += block / sr * host.bpm / 60.0;
            const int st = proc.getPlayStep();
            if (st != lastStep && st >= 0)
            {
                ++stepsSeen;
                if (st == 0) slotAtStep0.push_back (proc.getCurrentSlot());
            }
            lastStep = st;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) ++bad;
                    peak = juce::jmax (peak, std::abs (v));
                }
            if (written + block <= all.getNumSamples())
            {
                for (int c = 0; c < 2; ++c) all.copyFrom (c, written, buf, c, 0, block);
                written += block;
            }
        }
    };

    // 1. stopped: nothing from the sequencer, keyboard still plays
    run (20);
    check (peak == 0.0f && proc.getPlayStep() == -1, "silent and no steps while the transport is stopped");
    run (50, [] (int b, juce::MidiBuffer& m) { if (b == 0) m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 110), 0); });
    check (peak > 0.05f, "keyboard note C1 plays the bass drum while stopped");

    // 2. play two bars of pattern 1
    host.playing = true; host.ppq = 0.0;
    stepsSeen = 0; slotAtStep0.clear();
    run (400);   // 4 s = 2 bars
    check (stepsSeen == 32, ("32 steps in two bars (got " + juce::String (stepsSeen) + ")").toRawUTF8());

    // 3. pick pattern 2 in the middle of a bar: it must wait for step 1
    auto* patParam = proc.apvts.getParameter ("pattern");
    host.ppq = 1.5;   // halfway through bar 1
    lastStep = -1; slotAtStep0.clear();
    patParam->setValueNotifyingHost (patParam->convertTo0to1 (1.0f));
    run (10);
    check (proc.getCurrentSlot() == 0 && proc.getPendingSlot() == 1, "pattern 2 queued while pattern 1 finishes");
    run (300);
    check (! slotAtStep0.empty() && slotAtStep0.front() == 1 && proc.getPendingSlot() == -1, "pattern 2 starts on step 1");

    // 4. LEN = 12: steps wrap at 12
    auto* lenParam = proc.apvts.getParameter ("seqlen");
    lenParam->setValueNotifyingHost (lenParam->convertTo0to1 (12.0f));
    int maxStep = 0;
    run (20);   // let the step that was playing finish
    for (int b = 0; b < 300; ++b) { run (1); maxStep = juce::jmax (maxStep, proc.getPlayStep()); }
    check (maxStep == 11, ("LEN 12 plays steps 1..12 only (highest " + juce::String (maxStep + 1) + ")").toRawUTF8());
    lenParam->setValueNotifyingHost (lenParam->convertTo0to1 (16.0f));

    // 5. record: snare (D1) hit just before step 7 at velocity 120 -> SD + accent on step 7 of pattern 2
    host.ppq = 0.0;
    run (1);
    const int samplesTo7 = (int) std::round ((6.0 / 4.0 - 0.06) * 60.0 / host.bpm * sr);   // 6th 16th minus a bit
    const int hitBlock = samplesTo7 / block;
    run (hitBlock + 2, [&] (int b, juce::MidiBuffer& m)
    {
        if (b == hitBlock - 1) m.addEvent (juce::MidiMessage::noteOn (10, 38, (juce::uint8) 120), samplesTo7 % block);
    });
    check (pats.cell (1, 2, 6) && pats.cell (1, 0, 6), "recorded snare + accent into the nearest step (7)");
    check (! pats.cell (0, 2, 6), "recording went into the playing pattern only");

    // 6. stop: a queued slot switches at once
    host.playing = false;
    patParam->setValueNotifyingHost (patParam->convertTo0to1 (0.0f));
    run (2);
    check (proc.getCurrentSlot() == 0, "pattern change while stopped is immediate");

    // 6b. multi-out: BD on its own output, unaffected by Volume; back on MAIN when switched off
    check (proc.getBusCount (false) == 7 && totalOuts == 8, "main stereo out + 6 mono drum outs");
    {
        auto* own = proc.apvts.getParameter ("bd_out");
        auto* vol = proc.apvts.getParameter ("master");
        const int bdCh = proc.getChannelIndexInProcessBlockBuffer (false, 1, 0);
        auto hitBd = [&] (float& mainPeak, float& bdPeak, float& sdPeak)
        {
            mainPeak = bdPeak = sdPeak = 0;
            for (int b = 0; b < 30; ++b)
            {
                juce::MidiBuffer m;
                if (b == 0) m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0);
                proc.processBlock (buf, m);
                for (int i = 0; i < block; ++i)
                {
                    mainPeak = juce::jmax (mainPeak, std::abs (buf.getSample (0, i)));
                    bdPeak   = juce::jmax (bdPeak, std::abs (buf.getSample (bdCh, i)));
                    sdPeak   = juce::jmax (sdPeak, std::abs (buf.getSample (bdCh + 1, i)));
                }
            }
        };
        float mainPeak, bdPeak, sdPeak;
        run (600);   // let earlier sounds (cymbal, clap tail) die away
        own->setValueNotifyingHost (1.0f);
        vol->setValueNotifyingHost (0.0f);   // Volume all the way down
        hitBd (mainPeak, bdPeak, sdPeak);
        check (mainPeak < 1e-4f && bdPeak > 0.05f && sdPeak == 0.0f,
               ("BD Own Out: kick only on the BD output, ignores Volume (main " + juce::String (mainPeak, 5)
                + ", BD out " + juce::String (bdPeak, 3) + ")").toRawUTF8());
        own->setValueNotifyingHost (0.0f);
        vol->setValueNotifyingHost (vol->getDefaultValue());
        run (200);
        hitBd (mainPeak, bdPeak, sdPeak);
        check (mainPeak > 0.05f && bdPeak == 0.0f, "BD back on MAIN: kick in the main mix, BD output silent");
    }

    // 6c. name-strip pads: play when stopped (no recording); record (+ accent on Shift) while playing with REC on
    {
        run (200);
        juce::uint16 before = pats.get (proc.getCurrentSlot(), 3);
        proc.panelHit (2, false);   // clap, stopped
        float p = 0;
        for (int b = 0; b < 20; ++b) { juce::MidiBuffer m; proc.processBlock (buf, m); for (int i = 0; i < block; ++i) p = juce::jmax (p, std::abs (buf.getSample (0, i))); }
        check (p > 0.02f && pats.get (proc.getCurrentSlot(), 3) == before, "clap pad plays while stopped and leaves the pattern alone");

        const int slot = proc.getCurrentSlot();
        pats.clear (slot);
        host.playing = true; host.ppq = 0.0;
        run (1);
        host.ppq = 2.0;   // exactly step 9
        proc.panelHit (1, true);    // snare, Shift-click
        proc.panelHit (3, false);   // closed hat, normal click
        run (1);
        check (pats.cell (slot, 2, 8) && pats.cell (slot, 0, 8) && pats.cell (slot, 4, 8),
               "pad clicks while playing: snare + accent and closed hat recorded on step 9");
        auto* recP = proc.apvts.getParameter ("rec");
        recP->setValueNotifyingHost (0.0f);
        proc.panelHit (0, false);   // bass drum
        run (1);
        check (! pats.cell (slot, 1, 8) && ! pats.cell (slot, 1, 9), "with REC off a pad click only plays");
        recP->setValueNotifyingHost (1.0f);
        host.playing = false;
        run (2);
    }

    // 6d. MIDI export: write a known pattern, read the file back, check notes, velocities, timing, length
    {
        tross110::PatternMemory pm;
        auto put2 = [&] (int slot, int row, const char* s) { for (int i = 0; i < 16; ++i) pm.setCell (slot, row, i, s[i] == 'x'); };
        put2 (2, 0, "x...............");   // accent on step 1
        put2 (2, 1, "x.......x.......");   // BD
        put2 (2, 2, "....x.......x...");   // SD
        put2 (2, 4, "x.x.x.x.x.x.x.x.");   // CH
        put2 (7, 6, "x...............");   // CY in slot 8
        auto f = outDir.getChildFile ("export_test.mid");
        check (tross110::MidiExport::write (tross110::MidiExport::make (pm, { 2 }, 16, 128.0, "UNIT 110 Pattern 3"), f), "MIDI file written");
        juce::MidiFile back;
        juce::FileInputStream in (f);
        const bool read = in.openedOk() && back.readFrom (in);
        int ons = 0, accented = 0, bdOn = 0, chOn = 0, ch10 = 0;
        double lastTick = 0, tempo = 0;
        if (read)
            for (auto* e : *back.getTrack (0))
            {
                const auto& m = e->message;
                lastTick = juce::jmax (lastTick, m.getTimeStamp());
                if (m.isTempoMetaEvent()) tempo = 60.0 / m.getTempoSecondsPerQuarterNote();
                if (! m.isNoteOn()) continue;
                ++ons; if (m.getChannel() == 10) ++ch10;
                if (m.getVelocity() == 127) ++accented;
                if (m.getNoteNumber() == 36) ++bdOn;
                if (m.getNoteNumber() == 42) ++chOn;
            }
        std::printf ("MIDI export: %d notes, %d accented, tempo %.1f, last tick %.0f, ticks/quarter %d\n",
                     ons, accented, tempo, lastTick, (int) back.getTimeFormat());
        check (read && ons == 12 && bdOn == 2 && chOn == 8 && accented == 2 && ch10 == 12, "exported notes: BD x2, SD x2, CH x8, accent on step 1 (BD + CH at 127), channel 10");
        check (std::abs (tempo - 128.0) < 0.01 && std::abs (lastTick - 16 * 24) < 0.5, "exported tempo 128 BPM and clip length 16 steps");
        auto row = tross110::MidiExport::make (pm, { 2, 7 }, 12, 120.0, "row");
        double endTick = 0;
        for (auto* e : *row.getTrack (0)) endTick = juce::jmax (endTick, e->message.getTimeStamp());
        check (std::abs (endTick - 2 * 12 * 24) < 0.5, "two patterns in a row at LENGTH 12 = 24 steps");
        check (tross110::MidiExport::isEmpty (pm, 0) && ! tross110::MidiExport::isEmpty (pm, 7), "empty patterns are detected");
    }

    // 6e. copy + chain: copy pattern 1 to 4, chain 4,2 locked to the song position, saved with the state
    {
        const int len = 16;
        pats.copySlot (0, 3);
        bool same = true;
        for (int r = 0; r < tross110::kNumRows; ++r) same &= pats.get (3, r) == pats.get (0, r);
        check (same && ! pats.isEmpty (3), "Copy to: pattern 1 copied into pattern 4");

        proc.setMode (Tross110Processor::PlaySong);
        proc.chainAppend (3); proc.chainAppend (1);
        check (proc.getChainLength() == 2, "chain built: 4, 2");
        host.playing = true; host.ppq = 0.0;
        std::vector<int> barSlots;
        int prevStep = -1;
        for (int b = 0; b < 1600 && barSlots.size() < 4; ++b)
        {
            run (1);
            const int st = proc.getPlayStep();
            if (st == 0 && prevStep != 0) barSlots.push_back (proc.getCurrentSlot());
            prevStep = st;
        }
        check (barSlots.size() == 4 && barSlots[0] == 3 && barSlots[1] == 1 && barSlots[2] == 3 && barSlots[3] == 1,
               "chain plays 4, 2, 4, 2 bar by bar");
        host.ppq = 4.0 * 5 + 0.5;   // jump into bar 6 -> chain step 2 -> pattern 2
        run (60);
        check (proc.getCurrentSlot() == 1 && proc.getChainPos() == 1, "after a jump the chain lands on the right pattern");

        proc.showHelp = true;
        juce::MemoryBlock st; proc.getStateInformation (st);
        Tross110Processor other;
        other.setStateInformation (st.getData(), (int) st.getSize());
        check (other.getChainLength() == 2 && other.getChain() == std::vector<int> ({ 3, 1 }), "chain saved and restored with the project");
        check (other.showHelp.load(), "HELP on is saved with the project");
        proc.showHelp = false;

        auto chainMidi = tross110::MidiExport::make (pats, proc.getChain(), len, 120.0, "chain");
        double endTick = 0;
        for (auto* e : *chainMidi.getTrack (0)) endTick = juce::jmax (endTick, e->message.getTimeStamp());
        check (std::abs (endTick - 2 * 16 * 24) < 0.5, "chain export = 2 patterns in a row");

        proc.chainClear();
        check (proc.getChainLength() == 0, "chain cleared");
        proc.setMode (Tross110Processor::WriteTap);
        host.playing = false;
        run (2);
        pats.clear (3);
    }

    // 6f. parameter locks: a BD step with Level locked to -inf is silent, the next one plays; saved and copied
    {
        using namespace tross110;
        const int slot = proc.getCurrentSlot();
        pats.clear (slot);
        pats.setCell (slot, 1, 0, true); pats.setCell (slot, 1, 4, true);   // BD on steps 1 and 5
        pats.setLock (slot, BD, LLevel, 0, -70.0f);
        pats.setLock (slot, BD, LTune, 4, 7.5f);
        check (pats.stepHasLock (slot, BD, 0) && pats.hasAnyLock (slot), "locks stored on BD steps 1 and 5");
        run (600);   // quiet
        host.playing = true; host.ppq = 0.0;
        float p1 = 0, p2 = 0;
        for (int b = 0; b < 200; ++b)   // 2 s: step 1 at 0 s, step 5 at 0.5 s
        {
            juce::MidiBuffer m; proc.processBlock (buf, m);
            host.ppq += block / sr * host.bpm / 60.0;
            for (int i = 0; i < block; ++i)
            {
                const float v = std::abs (buf.getSample (0, i));
                if (b < 45) p1 = juce::jmax (p1, v); else if (b >= 50 && b < 95) p2 = juce::jmax (p2, v);
            }
        }
        host.playing = false; run (2);
        check (p1 < 1e-4f && p2 > 0.02f, ("Level lock -inf silences step 1 only (step 1 " + juce::String (p1, 5) + ", step 5 " + juce::String (p2, 3) + ")").toRawUTF8());

        juce::MemoryBlock st; proc.getStateInformation (st);
        Tross110Processor other;
        other.setStateInformation (st.getData(), (int) st.getSize());
        check (std::abs (other.patterns.getLock (slot, BD, LTune, 4) - 7.5f) < 0.01f && other.patterns.getLock (slot, BD, LLevel, 0) <= -69.9f
               && ! isLock (other.patterns.getLock (slot, BD, LTune, 0)), "locks saved and restored with the project");
        pats.copySlot (slot, 9);
        check (std::abs (pats.getLock (9, BD, LTune, 4) - 7.5f) < 0.01f, "Copy to copies the locks too");
        pats.clear (9);
        check (! pats.hasAnyLock (9), "Clear removes the locks");
        pats.clearLock (slot, BD, LLevel, 0);   // keep step 5's tune lock for the snapshot
    }

    // 7. state round trip
    juce::MemoryBlock state; proc.getStateInformation (state);
    {
        Tross110Processor other;
        other.setStateInformation (state.getData(), (int) state.getSize());
        bool same = true;
        for (int s = 0; s < tross110::kNumSlots; ++s)
            for (int r = 0; r < tross110::kNumRows; ++r)
                same &= other.patterns.get (s, r) == pats.get (s, r);
        check (same, "all 12 patterns restored from the saved state");
    }
    proc.apvts.getParameter ("bd_tune")->setValueNotifyingHost (0.25f);   // -6 st
    proc.setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (proc.apvts.getRawParameterValue ("bd_tune")->load() - 0.0f) < 0.01f, "parameters restored");

    check (bad == 0, "no NaN / inf in the output");
    std::printf ("peak %.3f\n", peak);

    juce::WavAudioFormat wav;
    auto file = outDir.getChildFile ("sequencer.wav");
    file.deleteFile();
    if (auto os = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
        if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (16)))
            w->writeFromAudioSampleBuffer (all, 0, written);

    // ---- extra UNIT 110 controls: balance, power, START/STOP on the tempo knob
    {
        host.playing = false; run (2);
        auto setP = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        host.bpm = 150.0;
        proc.internalPlay = true;
        int steps = 0, prev = -1;
        for (int b = 0; b < 200; ++b) { run (1); const int st = proc.getPlayStep(); if (st >= 0 && st != prev) { ++steps; prev = st; } }
        // 2 s at 150 BPM = 20 sixteenths
        check (steps >= 19 && steps <= 21 && std::abs (proc.currentBpm.load() - 150.0) < 0.01,
               ("START with the DAW stopped runs at the DAW's tempo (" + juce::String (steps) + " steps in 2 s at 150 BPM)").toRawUTF8());
        proc.internalPlay = false; run (2); host.bpm = 120.0;
        // SWING 75 %: the second 16th of each pair lands half a 16th late
        {
            setP ("swing", 75.0f);
            for (int r = 0; r < tross110::kNumRows; ++r) proc.patterns.set (proc.getCurrentSlot(), r, 0);
            host.ppq = 0.0; host.playing = true;
            std::vector<double> at;
            int last = -1;
            for (int b = 0; b < 100; ++b)
            {
                run (1);
                const int st = proc.getPlayStep();
                if (st != last && st >= 0 && st <= 3) at.push_back (host.ppq * 4.0);
                last = st;
            }
            host.playing = false; run (2);
            // steps 0..3 at 16th positions 0, 1.5, 2, 3.5 (one block of slack)
            const double slack = block / sr * host.bpm / 60.0 * 4.0 + 1e-6;
            check (at.size() >= 4 && std::abs (at[1] - 1.5) <= slack && std::abs (at[2] - 2.0) <= slack && std::abs (at[3] - 3.5) <= slack,
                   "SWING 75 % delays every second 16th by half a 16th");
            setP ("swing", 50.0f);
            auto mf = tross110::MidiExport::make (proc.patterns, { 0 }, 16, 120.0, "x", 75.0);
            juce::ignoreUnused (mf);
        }
        host.playing = true; run (2);
        check (! proc.internalPlay.load() && proc.isHostPlaying(), "host playback takes over from START");
        host.playing = false; run (2);
        setP ("power", 0.0f);
        proc.panelHit (tross110::BD, true);
        juce::AudioBuffer<float> buf (2 + tross110::kNumVoices, block); juce::MidiBuffer mb;
        float peak = 0.0f;
        for (int b = 0; b < 10; ++b) { proc.processBlock (buf, mb); peak = juce::jmax (peak, buf.getMagnitude (0, 0, block)); }
        check (peak == 0.0f, "POWER off = silence");
        setP ("power", 1.0f); run (20);
        setP ("balance", 5.0f); run (30);
        proc.panelHit (tross110::BD, false);
        peak = 0.0f;
        for (int b = 0; b < 10; ++b) { proc.processBlock (buf, mb); peak = juce::jmax (peak, buf.getMagnitude (0, 0, block)); }
        check (peak < 1.0e-4f, "BALANCE fully right silences the bass drum");
        setP ("balance", 0.0f); setP ("tempo", 120.0f);
        juce::MemoryBlock st; proc.setMode (Tross110Processor::WriteStep); proc.getStateInformation (st);
        Tross110Processor other; other.setStateInformation (st.getData(), (int) st.getSize());
        check (other.getMode() == Tross110Processor::WriteStep, "panel mode saved with the project");
        proc.setMode (Tross110Processor::PlayPattern);
    }

    // editor snapshots: front panel at 1x / 2x, the open case, About
    pats.setCell (0, 3, 10, true);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    std::printf ("editor size %d x %d\n", ed->getWidth(), ed->getHeight());
    juce::PNGImageFormat png;
    auto snap = [&] (const juce::String& name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile (name); f.deleteFile();
        if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
    };
    const int baseW = ed->getWidth(), baseH = ed->getHeight();
    check (baseW == unit110ui::kDesignW * 3 / 4, "editor opens at 75 % of the design size");
    auto* te = dynamic_cast<Tross110Editor*> (ed.get());
    ed->setSize (unit110ui::kDesignW, unit110ui::kDesignH);
    snap ("editor_1x.png");
    ed->setSize (baseW, baseH);
    if (te != nullptr)
    {
        auto& fp = te->getFront();
        // SHIFT + 4 = WRITE STEP, then SNARE selects the row and keys 1, 5 toggle steps 1 and 5
        proc.selectSlot (2); run (1);
        fp.pressKey (FrontPanel::Shift, false); fp.pressKey (FrontPanel::N4, false);
        check (proc.getMode() == Tross110Processor::WriteStep, "SHIFT + 4 = WRITE STEP");
        fp.pressKey (FrontPanel::KSD, false); fp.pressKey (FrontPanel::N1, false); fp.pressKey (FrontPanel::N5, false);
        check (pats.cell (2, 2, 0) && pats.cell (2, 2, 4) && fp.getStepRow() == 2, "WRITE STEP: SNARE + 1 + 5 sets snare steps 1 and 5");
        fp.pressKey (FrontPanel::Bank, false); fp.pressKey (FrontPanel::N1, false);
        check (pats.cell (2, 2, 8), "I-II key switches to steps 9-16");
        // SHIFT + 3 = WRITE SONG, keys add measures, D.C. (C.HIHAT) takes the last one back
        proc.chainClear();
        fp.pressKey (FrontPanel::N3, true);
        fp.pressKey (FrontPanel::N1, false); fp.pressKey (FrontPanel::N2, false); fp.pressKey (FrontPanel::N3, false);
        fp.pressKey (FrontPanel::KCH, false);
        check (proc.getChain() == std::vector<int> ({ 0, 1 }), "WRITE SONG: 1, 2, 3 then D.C. -> song A1, A2");
        fp.pressKey (FrontPanel::N1, true);
        check (proc.getMode() == Tross110Processor::PlaySong, "SHIFT + 1 = PLAY SONG");
        fp.pressKey (FrontPanel::N7, true);
        check (proc.patterns.isEmpty (proc.getCurrentSlot()), "SHIFT + 7 clears the pattern");
        fp.pressKey (FrontPanel::N8, true);
        check (proc.getChainLength() == 0, "SHIFT + 8 clears the song");
        fp.pressKey (FrontPanel::N6, true);
        check ((int) proc.apvts.getRawParameterValue ("seqlen")->load() == 12, "SHIFT + 6 = 12 steps");
        fp.pressKey (FrontPanel::N6, true);
        fp.pressKey (FrontPanel::N2, true);
        // bank B, key 3 = pattern B3 (slot 10)
        fp.pressKey (FrontPanel::Bank, false); fp.pressKey (FrontPanel::N3, false); run (1);
        check (proc.getCurrentSlot() == 10, "bank B + key 3 = pattern B3");
        fp.pressKey (FrontPanel::Bank, false); fp.pressKey (FrontPanel::Bank, false); fp.pressKey (FrontPanel::Bank, false);
        fp.pressKey (FrontPanel::N1, false); run (1);
        check (proc.getCurrentSlot() == 0, "back to A1");
        // copy / paste and the parameter-lock screen
        proc.patterns.setCell (0, 1, 0, true); proc.patterns.setCell (0, 1, 8, true);
        fp.copyPattern();
        fp.pressKey (FrontPanel::N4, false); run (1);
        check (fp.pastePattern() && pats.cell (3, 1, 0) && pats.cell (3, 1, 8), "copy A1, paste into A4");
        fp.pressKey (FrontPanel::N1, false); run (1);
        // SHIFT + drum = mute, Option + drum = solo
        fp.pressKey (FrontPanel::KSD, true);
        check (proc.apvts.getRawParameterValue ("sd_mute")->load() > 0.5f, "SHIFT + SNARE mutes the snare");
        fp.pressKey (FrontPanel::KSD, true);
        fp.pressKey (FrontPanel::KCP, false, true);
        check (proc.apvts.getRawParameterValue ("cp_solo")->load() > 0.5f && proc.apvts.getRawParameterValue ("sd_mute")->load() < 0.5f,
               "Option + CLAP solos the clap; SHIFT + SNARE again unmutes");
        fp.pressKey (FrontPanel::KCH, true);
        snap ("editor_mute_solo.png");
        fp.pressKey (FrontPanel::KCH, true);
        fp.pressKey (FrontPanel::KCP, false, true);
        fp.toggleLockView();
        check (fp.isLockView(), "double-click SHIFT shows the P-LOCK screen");
        fp.pressKey (FrontPanel::KBD, false);
        fp.lockParam = tross110::LTune;
        fp.setLockFromY (0, FrontPanel::lockLane().getY());   // top = +12 st
        check (std::abs (pats.getLock (0, tross110::BD, tross110::LTune, 0) - 12.0f) < 0.01f, "dragging to the top locks BD step 1 tune to +12");
        fp.setLockFromY (8, FrontPanel::lockLane().getCentreY());
        check (std::abs (pats.getLock (0, tross110::BD, tross110::LTune, 8)) < 0.6f, "the middle of the tune lane = 0 st");
        proc.internalPlay = true; run (30);
        ed->setSize (unit110ui::kDesignW, unit110ui::kDesignH);
        snap ("editor_locks.png");
        fp.toggleLockView();
        snap ("editor_playing.png");
        proc.internalPlay = false; run (2);
        te->openCase (true, false);
        check (te->isCaseOpen(), "a case screw opens the unit");
        snap ("editor_pcb.png");
        te->openCase (false, false);
        te->showAbout();
        snap ("editor_about.png");
    }
    ed.reset();

    std::printf ("%s\n", failures ? "SOME TESTS FAILED" : "all tests passed");
    return failures ? 1 : 0;
}
