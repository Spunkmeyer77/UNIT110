#include "PluginEditor.h"
#include "MidiExport.h"
#include "BinaryData.h"

using namespace tross110;
using namespace unit110ui;

namespace
{
juce::Font font (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
}

const juce::Colour lcdInk (0xff252d29), lcdGlass (0xffc9dccf);
// SHIFT + 1..5 in the order printed on the panel -> processor modes
const int kShiftMode[] = { Tross110Processor::PlaySong, Tross110Processor::PlayPattern, Tross110Processor::WriteSong,
                           Tross110Processor::WriteStep, Tross110Processor::WriteTap };
juce::String modeName (int m)
{
    switch (m)
    {
        case Tross110Processor::PlaySong:  return "PLAY SONG";
        case Tross110Processor::WriteSong: return "WRITE SONG";
        case Tross110Processor::WriteStep: return "WRITE STEP";
        case Tross110Processor::WriteTap:  return "WRITE TAP";
        default:                           return "PLAY PATTERN";
    }
}
const char* kRowNames[kNumRows] = { "ACCENT", "BASS DR", "SNARE", "CLAP", "C.HIHAT", "O.HIHAT", "CYMBAL" };

int keyVoice (int key)
{
    switch (key)
    {
        case FrontPanel::KBD: return BD;
        case FrontPanel::KSD: return SD;
        case FrontPanel::KOH: return OH;
        case FrontPanel::KCH: return CH;
        case FrontPanel::KCY: return CY;
        case FrontPanel::KCP: return CP;
        default:              return -1;
    }
}

juce::File exportStartFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    return d.isDirectory() ? d : juce::File::getSpecialLocation (juce::File::userHomeDirectory);
}

constexpr float kKnobStart = juce::MathConstants<float>::pi * 1.25f, kKnobEnd = juce::MathConstants<float>::pi * 2.75f;
} // namespace

// =================================================================================================
// knobs and trimmers
void KnobLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float angle = start + pos * (end - start);
    const auto c = r.getCentre();
    const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f - (kind == FrontKnob ? 9.0f : 3.0f);

    if (kind == FrontKnob)
    {
        // contact shadow on the panel, then the photographed cap, turned
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c.translated (2.5f, 4.0f)).expanded (1.5f));
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c.translated (4.0f, 7.0f)).expanded (3.0f));
        if (knob.isValid())
        {
            g.setOpacity (1.0f);
            const auto t = juce::AffineTransform::translation (-knob.getWidth() * 0.5f, -knob.getHeight() * 0.5f)
                               .rotated (angle)
                               .scaled (rad * 2.0f / (float) knob.getWidth())
                               .translated (c.x, c.y);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImageTransformed (knob, t);
        }
        // the light comes from above: keep the highlight still while the cap turns
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.06f), c.x, c.y - rad, juce::Colours::transparentWhite, c.x, c.y + rad * 0.2f, false);
        g.setGradientFill (sheen);
        g.fillEllipse (juce::Rectangle<float> (rad * 1.7f, rad * 1.7f).withCentre (c));
        return;
    }

    // a cream PCB trimmer with a cross slot, seen from above
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c.translated (1.5f, 2.5f)));
    juce::ColourGradient body (juce::Colour (0xfff7f0dc), c.x - rad * 0.5f, c.y - rad * 0.6f, juce::Colour (0xffd9cba8), c.x + rad * 0.6f, c.y + rad, true);
    g.setGradientFill (body);
    g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0xffb5a585));
    g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c).reduced (0.5f), 1.0f);
    // knurled edge
    g.setColour (juce::Colour (0x30000000));
    for (int i = 0; i < 36; ++i)
    {
        const float a = (float) i / 36.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::sin (a) * rad * 0.86f, c.y - std::cos (a) * rad * 0.86f, c.x + std::sin (a) * rad * 0.98f, c.y - std::cos (a) * rad * 0.98f, 0.7f);
    }
    // raised collar + cross slot, turning
    const float cr = rad * 0.52f;
    g.setColour (juce::Colour (0xffe6dbbf));
    g.fillEllipse (juce::Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0xff9c8c6a));
    g.drawEllipse (juce::Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c), 1.0f);
    juce::Path slot;
    const float sw = juce::jmax (1.6f, rad * 0.16f), sl = cr * 0.92f;
    slot.addRoundedRectangle (-sw * 0.5f, -sl, sw, sl * 2.0f, sw * 0.4f);
    slot.addRoundedRectangle (-sl, -sw * 0.5f, sl * 2.0f, sw, sw * 0.4f);
    slot.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    g.setColour (juce::Colour (0xff5a4c34));
    g.fillPath (slot);
    // the long arm of the cross marks the setting
    juce::Path mark;
    mark.addRoundedRectangle (-sw * 0.5f, -sl, sw, sl, sw * 0.4f);
    mark.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    g.setColour (juce::Colour (0xff2d2418));
    g.fillPath (mark);
}

juce::Font KnobLook::getSliderPopupFont (juce::Slider&) { return font (15.0f); }

void KnobLook::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xee1d2023));
    g.fillRoundedRectangle (body, 5.0f);
    g.setColour (juce::Colour (0xff6fb3d2));
    g.drawRoundedRectangle (body.reduced (0.5f), 5.0f, 1.0f);
    setColour (juce::BubbleComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
}

Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, KnobLook& look, float a0, float a1)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (a0, a1, true);
    setMouseDragSensitivity (180);
    setLookAndFeel (&look);
    setPopupDisplayEnabled (true, false, nullptr, 1500);
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    if (auto* p = s.getParameter (id))
    {
        attachment = std::make_unique<juce::SliderParameterAttachment> (*p, *this, nullptr);
        setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }
}

// =================================================================================================
// About
AboutBox::AboutBox() { setVisible (false); setInterceptsMouseClicks (true, false); }

void AboutBox::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.62f));
    card = getLocalBounds().withSizeKeepingCentre (700, 420);
    const auto c = card.toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (c.translated (0, 10).expanded (6), 16.0f);
    juce::ColourGradient metal (juce::Colour (0xffd9dcd8), c.getX(), c.getY(), juce::Colour (0xffa9aea9), c.getX(), c.getBottom(), false);
    metal.addColour (0.5, juce::Colour (0xffc6cac5));
    g.setGradientFill (metal);
    g.fillRoundedRectangle (c, 12.0f);
    // brushed lines
    juce::Random rnd (110);
    for (int y = (int) c.getY() + 3; y < (int) c.getBottom() - 3; y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha (rnd.nextFloat() * 0.06f));
        g.drawHorizontalLine (y, c.getX() + 4, c.getRight() - 4);
    }
    // black top band like the LCD surround
    auto band = c.withHeight (150.0f);
    g.setColour (juce::Colour (0xff17191b));
    g.fillRoundedRectangle (band, 12.0f);
    g.fillRect (band.withTrimmedTop (20));

    // logo
    auto hex = juce::Rectangle<float> (46, 46).withPosition (c.getX() + 34, c.getY() + 34);
    juce::Path hp;
    hp.addPolygon (hex.getCentre(), 6, 23.0f, juce::MathConstants<float>::pi / 6.0f);
    g.setColour (juce::Colour (0xffd4d7d3));
    g.strokePath (hp, juce::PathStrokeType (3.0f));
    g.setFont (font (17.0f));
    g.drawText ("TR", hex, juce::Justification::centred);
    g.setFont (font (22.0f, false));
    g.drawText ("TRANSISTOR LABS", (int) c.getX() + 92, (int) c.getY() + 40, 300, 34, juce::Justification::centredLeft);
    g.setFont (font (64.0f));
    g.drawText ("UNIT 110", (int) c.getX() + 34, (int) c.getY() + 82, 420, 64, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff6fb3d2));
    g.setFont (font (18.0f));
    g.drawText ("v" JucePlugin_VersionString, (int) c.getRight() - 234, (int) c.getY() + 104, 200, 30, juce::Justification::centredRight);
    g.setColour (juce::Colour (0xff8c9196));
    g.setFont (font (13.0f, false));
    g.drawText (juce::String::fromUTF8 ("RHYTHM COMPOSER  \xc2\xb7  VST3 / AU / LV2"), (int) c.getRight() - 334, (int) c.getY() + 40, 300, 30, juce::Justification::centredRight);

    // body
    auto b = c.reduced (34.0f, 0).withTrimmedTop (170.0f).toNearestInt();
    g.setColour (juce::Colour (0xff222527));
    g.setFont (font (14.0f, false));
    const juce::String lines[] = {
        "SHIFT + 1..5   modes (play song / pattern, write song / step / tap)",
        "SHIFT + 6   12 / 16 steps     SHIFT + 7   clear pattern     SHIFT + 8   clear song",
        "A|B|C|D   bank of 8 patterns     START / STOP   run without the host",
        "LCD: click steps to edit, right-click for MIDI export and copy",
        "Case screws (bottom corners): open the unit to reach the trimmers"
    };
    int y = b.getY();
    for (auto& l : lines) { g.drawText (l, b.getX(), y, b.getWidth(), 22, juce::Justification::centredLeft); y += 25; }

    y += 12;
    g.setColour (juce::Colour (0x40000000));
    g.drawHorizontalLine (y, (float) b.getX(), (float) b.getRight());
    y += 14;
    g.setFont (font (15.0f));
    g.setColour (juce::Colour (0xff1f5f80));
    linkStore = { b.getX(), y, 300, 24 };
    g.drawText ("ticcoross.gumroad.com", linkStore, juce::Justification::centredLeft);
    linkMail = { b.getRight() - 300, y, 300, 24 };
    g.drawText ("ticcoross@gmail.com", linkMail, juce::Justification::centredRight);
    g.setColour (juce::Colour (0xff55595d));
    g.setFont (font (12.0f, false));
    g.drawText ("Click anywhere to close", card.withTrimmedTop (card.getHeight() - 30), juce::Justification::centred);
}

void AboutBox::mouseUp (const juce::MouseEvent& e)
{
    if (linkStore.contains (e.getPosition())) { juce::URL ("https://ticcoross.gumroad.com/").launchInDefaultBrowser(); return; }
    if (linkMail.contains (e.getPosition()))  { juce::URL ("mailto:ticcoross@gmail.com").launchInDefaultBrowser(); return; }
    setVisible (false);
}

// =================================================================================================
// the open case
namespace
{
// board-image pixels: first trimmer of group g at (112 + 196 g, 933), 52 px apart.
// groups: ACC, BD, SD, OH, CH, CY, CP
const char* kGroupIds[7] = { "acc", "bd", "sd", "oh", "ch", "cy", "cp" };
const juce::Point<float> kBoardScrews[] = { { 119, 128 }, { 1387, 123 }, { 102, 783 }, { 1384, 783 } };
}

PcbView::PcbView (Tross110Processor& p, Tross110Editor& ed) : proc (p), editor (ed)
{
    board = juce::ImageCache::getFromMemory (BinaryData::pcb_png, BinaryData::pcb_pngSize);
    trimLook.kind = KnobLook::Trimmer;
    smallTrimLook.kind = KnobLook::SmallTrimmer;
    scale = (float) kDesignH / (float) board.getHeight();
    xOff = ((float) kDesignW - board.getWidth() * scale) * 0.5f;

    const float a0 = juce::MathConstants<float>::pi * 1.2f, a1 = juce::MathConstants<float>::pi * 2.8f;
    auto add = [&] (const juce::String& id, float bx, float by, bool small)
    {
        auto* k = trims.add (new Knob (proc.apvts, id, small ? smallTrimLook : trimLook, a0, a1));
        const float d = (small ? 30.0f : 44.0f) * scale;
        const auto c = toView (bx, by);
        k->setBounds (juce::Rectangle<float> (d + 6, d + 6).withCentre (c).toNearestInt());
        addAndMakeVisible (k);
    };
    for (int g = 0; g < 7; ++g)
    {
        const float x0 = 112.0f + 196.0f * (float) g, y = 933.0f;
        const juce::String id = kGroupIds[g];
        if (g == 0) { add ("acc_drive", x0, y, false); add ("acc_decay", x0 + 52, y, false); add ("accent", x0 + 104, y, false); }
        else        { add (id + "_tune", x0, y, false); add (id + "_decay", x0 + 52, y, false); add (id + "_lvl", x0 + 104, y, false); }
    }
    add ("bd_attack", 112.0f + 196.0f * 1 + 112.0f, 857.0f, true);
    add ("sd_snappy", 112.0f + 196.0f * 2 + 112.0f, 857.0f, true);
    add ("cy_tone",   112.0f + 196.0f * 5 + 112.0f, 857.0f, true);
    add ("cp_tail",   112.0f + 196.0f * 6 + 112.0f, 857.0f, true);
}

juce::Point<float> PcbView::toView (float x, float y) const { return { xOff + x * scale, y * scale }; }

void PcbView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (board, juce::Rectangle<float> (xOff, 0.0f, board.getWidth() * scale, (float) kDesignH));
}

bool PcbView::overScrew (juce::Point<float> p) const
{
    for (auto s : kBoardScrews) if (p.getDistanceFrom (toView (s.x, s.y)) < 16.0f) return true;
    return false;
}

void PcbView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (overScrew (e.position) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void PcbView::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || overScrew (e.position)) editor.openCase (false);
}

// =================================================================================================
// front panel
juce::Rectangle<int> FrontPanel::keyRect (int key)
{
    static const int numX[8] = { 354, 445, 536, 627, 717, 808, 899, 990 };
    switch (key)
    {
        case Shift:   return { 95, 546, 100, 76 };
        case Bank:    return { 263, 545, 46, 76 };
        case Start:   return { 1096, 520, 101, 98 };
        case Stop:    return { 1265, 519, 102, 98 };
        case KAccent: return { 96, 687, 131, 83 };
        case KBD:     return { 287, 686, 129, 83 };
        case KSD:     return { 476, 685, 128, 84 };
        case KOH:     return { 664, 685, 129, 83 };
        case KCH:     return { 854, 684, 129, 83 };
        case KCY:     return { 1043, 683, 130, 84 };
        case KCP:     return { 1234, 682, 132, 84 };
        default:      break;
    }
    if (key >= N1 && key <= N8) return { numX[key - N1], 544, 46, 76 };
    return {};
}

namespace
{
constexpr float kLabelW = 76.0f, kTopRow = 24.0f, kRowH = 21.5f;
const juce::Point<float> kFrontScrews[] = { { 36, 808 }, { 1419, 808 } };
const juce::Rectangle<int> kLogo { 100, 50, 345, 70 }, kPowerHit { 1205, 0, 100, 48 }, kPowerTab { 1223, 13, 37, 10 };
}

juce::Rectangle<int> FrontPanel::lcdCell (int row, int step)
{
    const auto glass = lcdRect().toFloat();
    const float x0 = glass.getX() + kLabelW, cw = (glass.getWidth() - kLabelW - 8.0f) / 16.0f;
    return juce::Rectangle<float> (x0 + step * cw, glass.getY() + kTopRow + row * kRowH, cw, kRowH).toNearestInt();
}

FrontPanel::FrontPanel (Tross110Processor& p, Tross110Editor& ed) : proc (p), editor (ed)
{
    panel = juce::ImageCache::getFromMemory (BinaryData::panel_png, BinaryData::panel_pngSize);
    knobLook.knob = juce::ImageCache::getFromMemory (BinaryData::knob_png, BinaryData::knob_pngSize);
    const char* ids[4] = { "tempo", "balance", "accent", "master" };
    const int cx[4] = { 880, 1032, 1185, 1338 };
    for (int i = 0; i < 4; ++i)
    {
        auto* k = knobs.add (new Knob (proc.apvts, ids[i], knobLook, kKnobStart, kKnobEnd));
        k->setBounds (juce::Rectangle<int> (124, 124).withCentre ({ cx[i], 283 }));
        addAndMakeVisible (k);
    }
    bank = proc.getCurrentSlot() / 8;
    startTimerHz (30);
}

FrontPanel::~FrontPanel() { stopTimer(); }

bool FrontPanel::powered() const { return proc.apvts.getRawParameterValue ("power")->load() > 0.5f; }
int  FrontPanel::len() const     { return juce::jlimit (1, kNumSteps, (int) proc.apvts.getRawParameterValue ("seqlen")->load()); }

void FrontPanel::flash (const juce::String& m) { message = m; messageUntil = juce::Time::getMillisecondCounter() + 1600; repaint (lcdRect()); }

void FrontPanel::timerCallback()
{
    ++blink;
    const int v = proc.patterns.getVersion(), st = proc.getPlayStep(), sl = proc.getCurrentSlot();
    const bool msgOn = message.isNotEmpty() && juce::Time::getMillisecondCounter() < messageUntil;
    if (! msgOn && message.isNotEmpty()) message.clear();
    // the LCD is cheap to draw; refresh it when something changed and a few times a second for blinking
    if (v != lastVersion || st != lastStep || sl != lastSlot || blink % 8 == 0 || msgOn)
    {
        lastVersion = v; lastStep = st; lastSlot = sl;
        repaint (lcdRect().expanded (2));
    }
    if (blink % 15 == 0) repaint (kPowerHit);
}

void FrontPanel::drawScrew (juce::Graphics& g, juce::Point<float> c) const
{
    const float r = 9.0f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (r * 2 + 3, r * 2 + 3).withCentre (c.translated (1, 1.5f)));
    juce::ColourGradient gr (juce::Colour (0xffeef0ee), c.x - r * 0.5f, c.y - r * 0.6f, juce::Colour (0xff7d8285), c.x + r * 0.7f, c.y + r, true);
    g.setGradientFill (gr);
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
    g.setColour (juce::Colour (0xff5d6164));
    g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c), 0.8f);
    juce::Path x;
    x.addRectangle (-1.1f, -r * 0.62f, 2.2f, r * 1.24f);
    x.addRectangle (-r * 0.62f, -1.1f, r * 1.24f, 2.2f);
    x.applyTransform (juce::AffineTransform::rotation (0.35f).translated (c.x, c.y));
    g.setColour (juce::Colour (0xff3c4043));
    g.fillPath (x);
}

bool FrontPanel::overScrew (juce::Point<int> p) const
{
    for (auto s : kFrontScrews) if (p.toFloat().getDistanceFrom (s) < 14.0f) return true;
    return false;
}

void FrontPanel::drawKeyPressed (juce::Graphics& g, juce::Rectangle<int> r)
{
    // the key sinks: redraw it a little lower and darker, with a shadow along the top edge
    g.setColour (juce::Colour (0xff121416));
    g.fillRoundedRectangle (r.toFloat(), 4.0f);
    g.drawImage (panel, r.getX(), r.getY() + 2, r.getWidth(), r.getHeight() - 2, r.getX(), r.getY(), r.getWidth(), r.getHeight() - 2);
    g.setColour (juce::Colour (0x2a002a40));
    g.fillRoundedRectangle (r.toFloat().reduced (2.0f), 4.0f);
    juce::ColourGradient top (juce::Colours::black.withAlpha (0.35f), 0, (float) r.getY(), juce::Colours::transparentBlack, 0, (float) r.getY() + 9.0f, false);
    g.setGradientFill (top);
    g.fillRoundedRectangle (r.toFloat().reduced (1.0f), 4.0f);
}

void FrontPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageAt (panel, 0, 0);

    // POWER: the blue slider tab sits left (ON) or right (OFF)
    if (! powered())
    {
        const auto tab = panel.getClippedImage (kPowerTab);
        g.setColour (juce::Colours::black);
        g.fillRect (kPowerTab.withTrimmedBottom (3));
        g.drawImageAt (tab, kPowerTab.getX() + 30, kPowerTab.getY());
    }

    drawLcd (g);

    if (heldKey >= 0) drawKeyPressed (g, keyRect (heldKey));
    if (shiftLatched && heldKey != Shift) drawKeyPressed (g, keyRect (Shift));
    if (accentArmed && heldKey != KAccent) drawKeyPressed (g, keyRect (KAccent));

    for (auto s : kFrontScrews) drawScrew (g, s);
}

void FrontPanel::drawLcd (juce::Graphics& g)
{
    const auto glass = lcdRect().toFloat();
    if (! powered()) return;   // a dark (blank) display

    const int slot = proc.getCurrentSlot(), playStep = proc.getPlayStep(), length = len();
    const int mode = proc.getMode();
    const float x0 = glass.getX() + kLabelW, cw = (glass.getWidth() - kLabelW - 8.0f) / 16.0f;
    const auto ghost = lcdInk.withAlpha (0.055f);
    auto ink = [] (float a = 0.86f) { return lcdInk.withAlpha (a); };

    // step numbers; the playing step is shown reversed
    for (int s = 0; s < kNumSteps; ++s)
    {
        auto r = juce::Rectangle<float> (x0 + s * cw, glass.getY() + 6.0f, cw, 15.0f);
        const bool on = s == playStep;
        if (on) { g.setColour (ink()); g.fillRect (r.reduced (1.0f, 0)); }
        g.setColour (on ? lcdGlass : ink (s < length ? 0.86f : 0.25f));
        g.setFont (font (10.5f));
        g.drawText (juce::String (s + 1), r, juce::Justification::centred);
        if (mode == Tross110Processor::WriteStep && (s / 8) == stepHalf)
        {
            g.setColour (ink());
            g.fillRect (r.getX() + 3.0f, r.getBottom() + 0.5f, r.getWidth() - 6.0f, 1.6f);
        }
    }

    // grid
    const auto gridArea = juce::Rectangle<float> (glass.getX() + 6.0f, glass.getY() + kTopRow, glass.getWidth() - 14.0f, kRowH * kNumRows);
    g.setColour (ink (0.75f));
    g.drawRect (gridArea, 1.4f);
    for (int r = 0; r < kNumRows; ++r)
    {
        const float y = gridArea.getY() + r * kRowH;
        if (r > 0) g.drawHorizontalLine ((int) y, gridArea.getX(), gridArea.getRight());
        g.setFont (font (10.0f));
        const bool sel = mode == Tross110Processor::WriteStep && r == stepRow;
        g.setColour (ink());
        if (sel)
        {
            juce::Path tri;
            tri.addTriangle (gridArea.getX() + 3, y + 6, gridArea.getX() + 3, y + kRowH - 6, gridArea.getX() + 8, y + kRowH * 0.5f);
            g.fillPath (tri);
        }
        g.drawText (kRowNames[r], juce::Rectangle<float> (gridArea.getX() + 10.0f, y, kLabelW - 14.0f, kRowH), juce::Justification::centredLeft);
        for (int s = 0; s < kNumSteps; ++s)
        {
            const auto cell = juce::Rectangle<float> (x0 + s * cw, y, cw, kRowH);
            const bool on = proc.patterns.cell (slot, r, s);
            g.setColour (on ? ink (s < length ? 0.88f : 0.3f) : ghost);
            g.fillRect (cell.reduced (3.5f, 4.0f));
            if (on && r > 0 && proc.patterns.stepHasLock (slot, r - 1, s))
            { g.setColour (lcdGlass); g.fillRect (cell.getRight() - 7.0f, cell.getY() + 5.0f, 2.0f, 2.0f); }
        }
    }
    g.setColour (ink (0.4f));
    g.drawVerticalLine ((int) x0, gridArea.getY(), gridArea.getBottom());
    for (int s = 1; s < kNumSteps; ++s)
        g.drawVerticalLine ((int) (x0 + s * cw), gridArea.getY(), gridArea.getBottom());
    if (playStep >= 0)
    {
        g.setColour (ink (0.12f));
        g.fillRect (juce::Rectangle<float> (x0 + playStep * cw, gridArea.getY(), cw, gridArea.getHeight()));
    }

    // lower boxes: MODE | PATTERN | TEMPO
    const float by = gridArea.getBottom() + 7.0f, bh = glass.getBottom() - 6.0f - by;
    const juce::Rectangle<float> boxes[3] = { { glass.getX() + 6.0f, by, 236.0f, bh },
                                              { glass.getX() + 248.0f, by, 112.0f, bh },
                                              { glass.getX() + 366.0f, by, glass.getWidth() - 374.0f, bh } };
    const char* titles[3] = { "MODE", "PATTERN", "TEMPO" };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (ink (0.75f));
        g.drawRoundedRectangle (boxes[i], 3.0f, 1.4f);
        g.setFont (font (8.5f));
        g.drawText (titles[i], boxes[i].withHeight (13.0f).withTrimmedLeft (5.0f), juce::Justification::centredLeft);
        g.drawHorizontalLine ((int) (boxes[i].getY() + 13.0f), boxes[i].getX(), boxes[i].getRight());
    }
    const bool msgOn = message.isNotEmpty();
    g.setColour (ink());
    auto mb = boxes[0].withTrimmedTop (14.0f).reduced (6.0f, 2.0f);
    g.setFont (font (msgOn ? 16.0f : 20.0f));
    g.drawFittedText (msgOn ? message : modeName (mode), mb.withTrimmedBottom (16.0f).toNearestInt(), juce::Justification::centredLeft, 2);
    g.setFont (font (10.0f));
    juce::String status;
    if (shiftLatched) status << "SHIFT  ";
    if (accentArmed) status << "ACC  ";
    if (mode == Tross110Processor::WriteStep) status << "EDIT " << kRowNames[stepRow] << "  " << (stepHalf ? "9-16" : "1-8");
    else if (mode == Tross110Processor::WriteSong || mode == Tross110Processor::PlaySong)
    {
        const auto ch = proc.getChain();
        status << "SONG " << (int) ch.size() << " MEAS";
        if (mode == Tross110Processor::PlaySong && proc.getChainPos() >= 0 && ! ch.empty()) status << "  NOW " << proc.getChainPos() + 1;
    }
    else if (mode == Tross110Processor::WriteTap) status << "REC LIVE HITS";
    g.drawText (status, mb.withTrimmedTop (mb.getHeight() - 15.0f), juce::Justification::centredLeft);

    // pattern: bank letter + number, the queued one blinks next to it
    auto pb = boxes[1].withTrimmedTop (14.0f);
    g.setFont (font (34.0f));
    g.drawText (slotName (slot), pb.withTrimmedBottom (14.0f), juce::Justification::centred);
    g.setFont (font (10.0f));
    const int pend = proc.getPendingSlot();
    juce::String sub = "BANK " + juce::String::charToString ((juce::juce_wchar) ('A' + bank));
    if (pend >= 0) sub = (blink / 8) % 2 ? "NEXT " + slotName (pend) : juce::String();
    g.drawText (sub, pb.withTrimmedTop (pb.getHeight() - 15.0f), juce::Justification::centred);

    // tempo + length
    auto tb = boxes[2].withTrimmedTop (14.0f);
    g.setFont (font (28.0f));
    g.drawText (juce::String (juce::roundToInt (proc.currentBpm.load())), tb.withTrimmedBottom (14.0f), juce::Justification::centred);
    g.setFont (font (10.0f));
    juce::String t = juce::String (length) + " STEPS";
    if (proc.isHostPlaying()) t << "  HOST";
    else if (proc.internalPlay.load()) t << "  RUN";
    g.drawText (t, tb.withTrimmedTop (tb.getHeight() - 15.0f), juce::Justification::centred);
}

void FrontPanel::selectSlot (int slot)
{
    if (auto* p = proc.apvts.getParameter ("pattern"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) slot));
    proc.selectSlot (slot);
}

void FrontPanel::pressKey (int key, bool shiftHeld)
{
    const bool shift = shiftHeld || shiftLatched;
    if (key == Shift) { shiftLatched = ! shiftLatched; repaint(); return; }
    shiftLatched = false;   // a latched SHIFT applies to one key
    if (! powered()) return;
    const int mode = proc.getMode();

    if (key == Start)
    {
        if (proc.isHostPlaying()) flash ("HOST IS PLAYING");
        else proc.internalPlay = true;
    }
    else if (key == Stop)
    {
        proc.internalPlay = false;
    }
    else if (key == Bank)
    {
        if (shift || mode == Tross110Processor::WriteStep) { stepHalf = 1 - stepHalf; flash (stepHalf ? "STEPS 9-16" : "STEPS 1-8"); }
        else
        {
            bank = (bank + 1) % 4;
            if (mode != Tross110Processor::WriteSong) selectSlot (bank * 8 + proc.getCurrentSlot() % 8);
        }
    }
    else if (key >= N1 && key <= N8)
    {
        const int n = key - N1;
        if (shift)
        {
            if (n < 5) { proc.setMode (kShiftMode[n]); flash (modeName (kShiftMode[n])); }
            else if (n == 5)
            {
                auto* p = proc.apvts.getParameter ("seqlen");
                const int nl = len() == 12 ? 16 : 12;
                p->setValueNotifyingHost (p->convertTo0to1 ((float) nl));
                flash (juce::String (nl) + " STEPS");
            }
            else if (n == 6) { proc.clearCurrentPattern(); flash ("PATTERN " + slotName (proc.getCurrentSlot()) + " CLEARED"); }
            else             { proc.chainClear(); flash ("SONG CLEARED"); }
        }
        else if (mode == Tross110Processor::WriteStep)
        {
            const int s = stepHalf * 8 + n;
            const int slot = proc.getCurrentSlot();
            proc.patterns.setCell (slot, stepRow, s, ! proc.patterns.cell (slot, stepRow, s));
        }
        else if (mode == Tross110Processor::WriteSong)
        {
            if (proc.getChainLength() >= Tross110Processor::kMaxChain) flash ("SONG FULL");
            else { proc.chainAppend (bank * 8 + n); flash ("MEAS " + juce::String (proc.getChainLength()) + " = " + slotName (bank * 8 + n)); }
        }
        else
        {
            if (mode == Tross110Processor::PlaySong) { proc.setMode (Tross110Processor::PlayPattern); }
            selectSlot (bank * 8 + n);
        }
    }
    else if (key == KAccent)
    {
        if (mode == Tross110Processor::WriteStep) stepRow = 0;
        else accentArmed = ! accentArmed;
    }
    else if (const int v = keyVoice (key); v >= 0)
    {
        if (mode == Tross110Processor::WriteStep) stepRow = v + 1;
        else if (mode == Tross110Processor::WriteSong && key == KOH)   // ENTER: the playing pattern
        {
            proc.chainAppend (proc.getCurrentSlot());
            flash ("MEAS " + juce::String (proc.getChainLength()) + " = " + slotName (proc.getCurrentSlot()));
        }
        else if (mode == Tross110Processor::WriteSong && key == KCH)   // D.C.: take back the last measure
        {
            proc.chainRemoveLast();
            flash ("SONG " + juce::String (proc.getChainLength()) + " MEAS");
        }
        else
        {
            proc.panelHit (v, accentArmed);
            accentArmed = false;
        }
    }
    repaint();
}

void FrontPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (kPowerHit.contains (p))
    {
        auto* pw = proc.apvts.getParameter ("power");
        pw->setValueNotifyingHost (powered() ? 0.0f : 1.0f);
        repaint();
        return;
    }
    if (overScrew (p)) { editor.openCase (true); return; }
    if (kLogo.contains (p)) { editor.showAbout(); return; }

    if (lcdRect().contains (p) && powered())
    {
        if (e.mods.isPopupMenu()) { showLcdMenu(); return; }
        for (int r = 0; r < kNumRows; ++r)
            for (int s = 0; s < kNumSteps; ++s)
                if (lcdCell (r, s).contains (p))
                {
                    const int slot = proc.getCurrentSlot();
                    paintRow = r;
                    paintValue = ! proc.patterns.cell (slot, r, s);
                    proc.patterns.setCell (slot, r, s, paintValue);
                    repaint (lcdRect());
                    return;
                }
        return;
    }

    for (int k = 0; k < kNumKeys; ++k)
        if (keyRect (k).contains (p))
        {
            if (e.mods.isPopupMenu())
            {
                if (const int v = keyVoice (k); v >= 0 && powered()) showDrumMenu (v);
                return;
            }
            heldKey = k;
            pressKey (k, e.mods.isShiftDown());
            repaint();
            return;
        }
}

void FrontPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (paintRow < 0) return;
    const auto p = e.getPosition();
    for (int s = 0; s < kNumSteps; ++s)
        if (lcdCell (paintRow, s).contains (p) && proc.patterns.cell (proc.getCurrentSlot(), paintRow, s) != paintValue)
        {
            proc.patterns.setCell (proc.getCurrentSlot(), paintRow, s, paintValue);
            repaint (lcdRect());
        }
}

void FrontPanel::mouseUp (const juce::MouseEvent&)
{
    paintRow = -1;
    if (heldKey >= 0) { heldKey = -1; repaint(); }
}

void FrontPanel::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    bool hand = kPowerHit.contains (p) || overScrew (p) || kLogo.contains (p);
    for (int k = 0; k < kNumKeys && ! hand; ++k) hand = keyRect (k).contains (p);
    setMouseCursor (hand ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void FrontPanel::showDrumMenu (int v)
{
    static const char* ids[kNumVoices] = { "bd", "sd", "cp", "ch", "oh", "cy" };
    static const char* names[kNumVoices] = { "BASS DRUM", "SNARE DRUM", "HAND CLAP", "C.HIHAT", "O.HIHAT", "CYMBAL" };
    const juce::String id = ids[v];
    auto val = [this] (const juce::String& pid) { return proc.apvts.getRawParameterValue (pid)->load() > 0.5f; };
    juce::PopupMenu m;
    m.addSectionHeader (names[v]);
    m.addItem (1, "Mute", true, val (id + "_mute"));
    m.addItem (2, "Solo", true, val (id + "_solo"));
    if (! proc.isStandalone()) m.addItem (3, "Own output", true, val (id + "_out"));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [this, id] (int r)
                     {
                         const char* sfx[] = { "", "_mute", "_solo", "_out" };
                         if (r < 1 || r > 3) return;
                         auto* p = proc.apvts.getParameter (id + sfx[r]);
                         p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
                     });
}

void FrontPanel::showLcdMenu()
{
    const int slot = proc.getCurrentSlot();
    std::vector<int> nonEmpty;
    for (int s = 0; s < kNumSlots; ++s) if (! MidiExport::isEmpty (proc.patterns, s)) nonEmpty.push_back (s);
    const auto chain = proc.getChain();

    juce::PopupMenu copy;
    for (int s = 0; s < kNumSlots; ++s)
        if (s != slot) copy.addItem (100 + s, slotName (s) + (MidiExport::isEmpty (proc.patterns, s) ? "" : "  (used)"));
    juce::PopupMenu m;
    m.addSectionHeader ("PATTERN " + slotName (slot));
    m.addItem (1, "Export this pattern as MIDI...", ! MidiExport::isEmpty (proc.patterns, slot));
    m.addItem (2, "Export all patterns in a row...", ! nonEmpty.empty());
    m.addItem (3, "Export every pattern as its own file...", ! nonEmpty.empty());
    m.addItem (4, "Export the song...", ! chain.empty());
    m.addSeparator();
    m.addSubMenu ("Copy pattern to", copy);
    m.addItem (5, "Clear pattern");
    m.addItem (6, "Clear song", ! chain.empty());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe = juce::Component::SafePointer<FrontPanel> (this), slot, nonEmpty, chain] (int r)
                     {
                         if (safe == nullptr) return;
                         auto& self = *safe;
                         if (r == 1) self.exportMidi ({ slot }, MidiExport::fileName (slot));
                         if (r == 2) self.exportMidi (nonEmpty, "UNIT110 Patterns.mid");
                         if (r == 3) self.exportSeparate();
                         if (r == 4) self.exportMidi (chain, "UNIT110 Song.mid");
                         if (r == 5) { self.proc.patterns.clear (slot); self.flash ("PATTERN CLEARED"); }
                         if (r == 6) { self.proc.chainClear(); self.flash ("SONG CLEARED"); }
                         if (r >= 100) { self.proc.patterns.copySlot (slot, r - 100); self.flash ("COPIED TO " + slotName (r - 100)); }
                     });
}

void FrontPanel::exportMidi (const std::vector<int>& slots, const juce::String& name)
{
    auto midi = MidiExport::make (proc.patterns, slots, len(), proc.currentBpm.load(), name.upToLastOccurrenceOf (".", false, false));
    chooser = std::make_unique<juce::FileChooser> ("Save as MIDI", exportStartFolder().getChildFile (name), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [midi] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f != juce::File()) MidiExport::write (midi, f.withFileExtension ("mid"));
                          });
}

void FrontPanel::exportSeparate()
{
    struct Item { juce::String name; juce::MidiFile file; };
    std::vector<Item> files;
    for (int s = 0; s < kNumSlots; ++s)
        if (! MidiExport::isEmpty (proc.patterns, s))
            files.push_back ({ MidiExport::fileName (s), MidiExport::make (proc.patterns, { s }, len(), proc.currentBpm.load(), "UNIT 110 Pattern " + slotName (s)) });
    chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the MIDI files", exportStartFolder());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [files] (const juce::FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (! dir.isDirectory()) return;
                              for (auto& f : files) MidiExport::write (f.file, dir.getChildFile (f.name));
                          });
}

// =================================================================================================
// editor
Tross110Editor::Tross110Editor (Tross110Processor& p)
    : AudioProcessorEditor (&p), proc (p), front (p, *this), pcb (p, *this)
{
    content.setBounds (0, 0, kDesignW, kDesignH);
    for (auto* c : std::initializer_list<juce::Component*> { &front, &pcb, &about })
    {
        c->setBounds (0, 0, kDesignW, kDesignH);
        content.addChildComponent (c);
    }
    front.setVisible (true);
    addAndMakeVisible (content);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    setResizeLimits (kDesignW / 2, kDesignH / 2, kDesignW * 6 / 5, kDesignH * 6 / 5);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kDesignW / kDesignH);
    setSize (kDesignW * 3 / 4, kDesignH * 3 / 4);
}

Tross110Editor::~Tross110Editor() = default;

void Tross110Editor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }

void Tross110Editor::resized()
{
    const float s = (float) getWidth() / (float) kDesignW;
    content.setTransform (juce::AffineTransform::scale (s));
}

void Tross110Editor::showAbout() { about.setVisible (true); about.toFront (false); }

void Tross110Editor::openCase (bool open, bool animate)
{
    if (! animate)
    {
        pcb.setAlpha (1.0f); front.setAlpha (1.0f);
        pcb.setVisible (open); front.setVisible (! open);
        return;
    }
    auto& anim = juce::Desktop::getInstance().getAnimator();
    if (open)
    {
        pcb.setAlpha (0.0f);
        pcb.setVisible (true);
        anim.fadeIn (&pcb, 260);
        anim.fadeOut (&front, 260);
    }
    else
    {
        front.setAlpha (0.0f);
        front.setVisible (true);
        anim.fadeIn (&front, 260);
        anim.fadeOut (&pcb, 260);
    }
}

bool Tross110Editor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        if (about.isVisible()) { about.setVisible (false); return true; }
        if (isCaseOpen()) { openCase (false); return true; }
    }
    return false;
}
