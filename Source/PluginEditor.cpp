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
const juce::Point<float> kBoardScrews[] = { { 100, 118 }, { 308, 118 }, { 1190, 118 }, { 1398, 118 }, { 95, 245 }, { 1401, 245 },
                                            { 92, 366 }, { 1400, 465 }, { 95, 550 }, { 95, 678 }, { 1405, 678 }, { 375, 660 }, { 652, 656 } };
const juce::Rectangle<float> kBoardChips[] = { { 398, 162, 90, 100 }, { 540, 162, 126, 112 }, { 540, 330, 126, 114 } };
}

PcbView::PcbView (Tross110Processor& p, Tross110Editor& ed) : proc (p), editor (ed)
{
    board = juce::ImageCache::getFromMemory (BinaryData::pcb_png, BinaryData::pcb_pngSize);
    trimLook.kind = KnobLook::Trimmer;
    smallTrimLook.kind = KnobLook::SmallTrimmer;
    scale = (float) kDesignH / (float) board.getHeight(); xOff = 0.0f;   // stored at 2x the design size

    const float a0 = juce::MathConstants<float>::pi * 1.2f, a1 = juce::MathConstants<float>::pi * 2.8f;
    auto add = [&] (const juce::String& id, float bx, float by, bool small)
    {
        auto* k = trims.add (new Knob (proc.apvts, id, small ? smallTrimLook : trimLook, a0, a1));
        const float d = small ? 26.0f : 40.0f;
        const auto c = toView (bx, by);
        k->setBounds (juce::Rectangle<float> (d + 6, d + 6).withCentre (c).toNearestInt());
        addAndMakeVisible (k);
    };
    auto groupX = [] (int g) { return 114.0f + 195.7f * (float) g; };
    for (int g = 0; g < 7; ++g)
    {
        const float x0 = groupX (g), y = 802.0f;
        const juce::String id = kGroupIds[g];
        if (g == 0) { add ("acc_drive", x0, y, false); add ("acc_decay", x0 + 50, y, false); add ("accent", x0 + 100, y, false); }
        else        { add (id + "_tune", x0, y, false); add (id + "_decay", x0 + 50, y, false); add (id + "_lvl", x0 + 100, y, false); }
    }
    add ("bd_attack", groupX (1) + 112.0f, 754.0f, true);
    add ("sd_snappy", groupX (2) + 112.0f, 754.0f, true);
    add ("cy_tone",   groupX (5) + 112.0f, 754.0f, true);
    add ("cp_tail",   groupX (6) + 112.0f, 754.0f, true);
}

// board positions were measured on the 1.0.2 layout; this moves them onto the current artwork
juce::Point<float> PcbView::toView (float x, float y) const
{
    return { x, y };   // board positions are measured on the board image itself
}

void PcbView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (board, juce::Rectangle<float> (xOff, 0.0f, board.getWidth() * scale, (float) kDesignH));
}

bool PcbView::closesCase (juce::Point<float> p) const
{
    for (auto s : kBoardScrews) if (p.getDistanceFrom (toView (s.x, s.y)) < 16.0f) return true;
    for (auto c : kBoardChips)
        if (juce::Rectangle<float>::leftTopRightBottom (toView (c.getX(), c.getY()).x, toView (c.getX(), c.getY()).y,
                                                        toView (c.getRight(), c.getBottom()).x, toView (c.getRight(), c.getBottom()).y).contains (p)) return true;
    return false;
}

void PcbView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (closesCase (e.position) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void PcbView::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || closesCase (e.position)) editor.openCase (false);
}

// =================================================================================================
// front panel
juce::Rectangle<int> FrontPanel::keyRect (int key)
{
    static const int numX[8] = { 357, 450, 544, 637, 730, 824, 918, 1012 };
    static const int numY[8] = { 578, 578, 578, 578, 579, 579, 579, 579 };
    switch (key)
    {
        case Shift:   return { 92, 577, 104, 81 };
        case Bank:    return { 264, 578, 48, 81 };
        case Start:   return { 1123, 557, 106, 103 };
        case Stop:    return { 1300, 557, 106, 103 };
        case KAccent: return { 93, 728, 133, 88 };
        case KBD:     return { 287, 728, 133, 89 };
        case KSD:     return { 481, 728, 132, 89 };
        case KOH:     return { 675, 729, 132, 88 };
        case KCH:     return { 872, 729, 133, 88 };
        case KCY:     return { 1069, 730, 135, 88 };
        case KCP:     return { 1269, 730, 138, 88 };
        default:      break;
    }
    if (key >= N1 && key <= N8) return { numX[key - N1], numY[key - N1], 48, 80 };
    return {};
}

namespace
{
constexpr float kLabelW = 78.0f, kTopRow = 24.0f, kRowH = 22.5f;
const juce::Point<float> kFrontScrews[] = { { 42, 864 }, { 1460, 864 } };
const juce::Rectangle<int> kLogo { 98, 50, 560, 66 }, kPowerSwitch { 1236, 0, 76, 50 }, kPowerText { 1260, 60, 84, 42 },
                           kPowerTab { 1251, 17, 42, 11 };
const juce::Point<int> kKnobCentre[4] = { { 899, 303 }, { 1056, 301 }, { 1217, 301 }, { 1377, 301 } };
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
    const char* ids[4] = { "swing", "balance", "accent", "master" };
    for (int i = 0; i < 4; ++i)
    {
        auto* k = knobs.add (new Knob (proc.apvts, ids[i], knobLook, kKnobStart, kKnobEnd));
        k->setBounds (juce::Rectangle<int> (124, 124).withCentre (kKnobCentre[i]));
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
    if (blink % 15 == 0) repaint (kPowerSwitch);
}

void FrontPanel::drawScrew (juce::Graphics& g, juce::Point<float> c) const
{
    const float r = 14.0f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (r * 2 + 3, r * 2 + 3).withCentre (c.translated (1, 1.5f)));
    juce::ColourGradient gr (juce::Colour (0xffeef0ee), c.x - r * 0.5f, c.y - r * 0.6f, juce::Colour (0xff7d8285), c.x + r * 0.7f, c.y + r, true);
    g.setGradientFill (gr);
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
    g.setColour (juce::Colour (0xff5d6164));
    g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c), 0.8f);
    juce::Path x;
    x.addRectangle (-1.5f, -r * 0.62f, 3.0f, r * 1.24f);
    x.addRectangle (-r * 0.62f, -1.5f, r * 1.24f, 3.0f);
    x.applyTransform (juce::AffineTransform::rotation (0.35f).translated (c.x, c.y));
    g.setColour (juce::Colour (0xff3c4043));
    g.fillPath (x);
}

bool FrontPanel::overScrew (juce::Point<int> p) const
{
    for (auto s : kFrontScrews) if (p.toFloat().getDistanceFrom (s) < 18.0f) return true;
    return false;
}

void FrontPanel::drawKeyPressed (juce::Graphics& g, juce::Rectangle<int> r)
{
    // the key sinks: redraw it a little lower and darker, with a shadow along the top edge
    g.setColour (juce::Colour (0xff121416));
    g.fillRoundedRectangle (r.toFloat(), 4.0f);
    g.drawImage (panel, r.getX(), r.getY() + 2, r.getWidth(), r.getHeight() - 2, r.getX() * 2, r.getY() * 2, r.getWidth() * 2, (r.getHeight() - 2) * 2);
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
    g.drawImage (panel, juce::Rectangle<float> (0, 0, (float) kDesignW, (float) kDesignH));   // stored at 2x

    // POWER: the blue slider tab sits left (ON) or right (OFF)
    if (! powered())
    {
        const auto tab = panel.getClippedImage (kPowerTab * 2);
        g.setColour (juce::Colours::black);
        g.fillRect (kPowerTab.withTrimmedBottom (3));
        g.drawImage (tab, kPowerTab.translated (34, 0).toFloat());
    }

    drawPanelPrint (g);
    drawLcd (g);

    for (int k = 0; k < kNumKeys; ++k)
    {
        const bool down = k == heldKey || (k == Shift && (shiftLatched || lockView)) || (k == KAccent && accentArmed);
        if (down) drawKeyPressed (g, keyRect (k));
        drawKeyLegend (g, k, down ? 2.0f : 0.0f);
    }

    for (auto s : kFrontScrews) drawScrew (g, s);
}

// ---- the panel print, drawn as vector text over the text-free photo ---------------------------
namespace
{
const juce::Colour kPrintInk (0xff26282c), kKeyInk (0xfff1f5f6), kLogoInk (0xffc4c8c8);

// positions were measured on the original (printed) render; map them onto this artwork
// the artwork is straightened (tools/make_assets.py): each column is stretched so the top / bottom edges are level
float straightenY (float x, float y, float st, float ot, float sb, float ob)
{
    const float top = ot + st * x, bot = ob + sb * x;
    return 24.0f + (y - top) * (893.0f - 24.0f) / (bot - top);
}

juce::Rectangle<float> O (float x, float y, float w, float h)
{
    // the 1.0.5 render (scaled to 1920 wide) keeps the original layout: just the crop and the straightening
    const float nx = x - 195.0f, ny = y - 233.0f;
    const float y0 = straightenY (nx + w * 0.5f, ny, 0.000963038f, 24.21007f, 0.00878241f, 886.3991f);
    const float y1 = straightenY (nx + w * 0.5f, ny + h, 0.000963038f, 24.21007f, 0.00878241f, 886.3991f);
    return { nx, y0, w, y1 - y0 };
}

// text whose capitals fill the box height; squeezed or stretched (a little) to the box width when fit = true
void print (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> box, juce::Colour c, bool fit = true,
            juce::Justification j = juce::Justification::centred, bool bold = true, bool italic = false, const juce::String& face = {})
{
    auto f = juce::Font (juce::FontOptions (face.isEmpty() ? juce::Font::getDefaultSansSerifFontName() : face, box.getHeight() / 0.72f,
                                            (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0)));
    if (fit)
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, t);
        if (w > 0) f.setHorizontalScale (juce::jlimit (0.7f, 1.25f, box.getWidth() / w));
    }
    else
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, t);
        if (w > box.getWidth() * 1.15f) f.setHorizontalScale (box.getWidth() * 1.15f / w);
    }
    g.setColour (c);
    g.setFont (f);
    // centre the capitals (not the whole line) on the box
    const float asc = f.getAscent(), capH = box.getHeight();
    auto line = juce::Rectangle<float> (box.getX() - 40.0f, box.getBottom() - asc, box.getWidth() + 80.0f, f.getHeight());
    juce::ignoreUnused (capH);
    g.drawText (t, j == juce::Justification::centred ? line : line.withX (j.testFlags (juce::Justification::left) ? box.getX() : box.getRight() - line.getWidth()).withWidth (line.getWidth()),
                j.getOnlyHorizontalFlags() | juce::Justification::top, false);
}

void drawArrow (juce::Graphics& g, float x0, float x1, float y, bool left)
{
    juce::Path a;
    const float h = 7.0f, head = 9.0f;
    if (left)
    {
        a.addTriangle (x0, y, x0 + head, y - h, x0 + head, y + h);
        a.addRectangle (x0 + head - 1.0f, y - 3.2f, x1 - x0 - head + 1.0f, 6.4f);
    }
    else
    {
        a.addTriangle (x1, y, x1 - head, y - h, x1 - head, y + h);
        a.addRectangle (x0, y - 3.2f, x1 - x0 - head + 1.0f, 6.4f);
    }
    g.fillPath (a);
}
}

void FrontPanel::drawPanelPrint (juce::Graphics& g)
{
    // logo + model on the black surround
    {
        const auto box = O (293, 284, 62, 64);
        juce::Path hp;
        hp.addPolygon (box.getCentre(), 6, box.getHeight() * 0.48f, 0.0f);
        g.setColour (kLogoInk);
        g.strokePath (hp, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved));
        print (g, "TR", box.withSizeKeepingCentre (34, 24), kLogoInk, true, juce::Justification::centred, false);
        print (g, "TRANSISTOR LABS", O (366, 301, 275, 29), kLogoInk, true, juce::Justification::centred, false);
        print (g, "UNIT110", O (694, 309, 157, 32), kLogoInk);
    }
    // top edge
    print (g, "ACC TRIG OUT", O (1105, 302, 140, 17), kPrintInk);
    print (g, "DC 9V", O (1303, 302, 63, 17), kPrintInk);
    print (g, "ON-OFF", O (1466, 299, 56, 11), kPrintInk);
    print (g, "POWER", O (1461, 314, 70, 15), kPrintInk);

    // knobs: names, ticks, scales
    print (g, "SWING", O (1052, 430, 87, 24), kPrintInk, false);
    print (g, "BALANCE", O (1187, 431, 133, 24), kPrintInk);
    print (g, "ACCENT", O (1355, 431, 118, 25), kPrintInk);
    print (g, "VOLUME", O (1514, 433, 122, 25), kPrintInk);
    print (g, "0", O (1245, 461, 16, 19), kPrintInk, false);
    print (g, "5", O (1406, 462, 15, 19), kPrintInk, false);
    print (g, "5", O (1567, 463, 15, 19), kPrintInk, false);
    print (g, "STRAIGHT", O (998, 589, 84, 16), kPrintInk, false);
    print (g, "SHUFFLE", O (1112, 589, 72, 16), kPrintInk, false);
    print (g, "5", O (1213, 587, 14, 19), kPrintInk, false);
    print (g, "5", O (1280, 587, 14, 19), kPrintInk, false);
    print (g, "0", O (1372, 588, 16, 19), kPrintInk, false);
    print (g, "10", O (1434, 589, 32, 19), kPrintInk, false);
    print (g, "0", O (1533, 589, 16, 19), kPrintInk, false);
    print (g, "10", O (1596, 590, 33, 19), kPrintInk, false);
    g.setColour (kPrintInk);
    for (int k = 0; k < 4; ++k)
    {
        const auto c = kKnobCentre[k].toFloat();
        for (int t = 0; t <= 10; ++t)
        {
            if (t == 5 && k > 0) continue;   // BALANCE / ACCENT / VOLUME have a number at the top instead
            const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) t);
            const float r0 = (t == 5 ? 61.0f : 63.0f), r1 = (t == 5 ? 76.0f : 73.0f);
            g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, 2.4f);
        }
    }

    // function labels above the number keys
    print (g, juce::String::fromUTF8 ("I\xc2\xb7II"), O (468, 767, 27, 14), kPrintInk, false);
    print (g, "PLAY", O (601, 747, 40, 15), kPrintInk, false);
    print (g, "SONG", O (545, 767, 58, 15), kPrintInk);
    print (g, "PATTERN", O (623, 768, 88, 15), kPrintInk);
    print (g, "WRITE", O (826, 750, 57, 15), kPrintInk, false);
    print (g, "SONG", O (732, 769, 59, 15), kPrintInk);
    print (g, "STEP", O (828, 770, 53, 15), kPrintInk);
    print (g, "TAP", O (928, 771, 40, 15), kPrintInk);
    print (g, "PATTERN", O (1093, 755, 88, 15), kPrintInk);
    print (g, "MEASURE", O (1184, 756, 97, 15), kPrintInk);
    print (g, "CLEAR", O (1104, 773, 69, 15), kPrintInk);
    print (g, "RESET", O (1198, 774, 70, 15), kPrintInk);
    {
        // 12/16 as a stacked fraction
        print (g, "12", O (1021, 753, 22, 14), kPrintInk, false);
        print (g, "16", O (1041, 773, 25, 14), kPrintInk, false);
        const auto a = O (1030, 789, 0, 0).getPosition(), b = O (1058, 754, 0, 0).getPosition();
        g.setColour (kPrintInk);
        g.drawLine (a.x, a.y, b.x, b.y, 2.0f);
    }
    {
        // the PLAY and WRITE brackets
        g.setColour (kPrintInk);
        auto bracket = [&g] (juce::Rectangle<float> l, juce::Rectangle<float> r)
        {
            g.drawLine (l.getX(), l.getY(), l.getRight(), l.getY(), 1.8f);
            g.drawLine (l.getX(), l.getY() - 0.9f, l.getX(), l.getY() + 7.0f, 1.8f);
            g.drawLine (r.getX(), r.getY(), r.getRight(), r.getY(), 1.8f);
            g.drawLine (r.getRight(), r.getY() - 0.9f, r.getRight(), r.getY() + 7.0f, 1.8f);
        };
        bracket (O (572, 754, 25, 0), O (645, 754, 25, 0));
        bracket (O (757, 757, 63, 0), O (891, 757, 62, 0));
    }

    // above the drum keys
    print (g, "DOWN", O (498, 921, 56, 15), kPrintInk);
    print (g, "MEASURE", O (598, 921, 95, 15), kPrintInk);
    print (g, "UP", O (734, 921, 25, 15), kPrintInk);
    g.setColour (kPrintInk);
    {
        const auto l = O (559, 928, 34, 0), r = O (697, 928, 32, 0);
        drawArrow (g, l.getX(), l.getRight(), l.getY(), true);
        drawArrow (g, r.getX(), r.getRight(), r.getY(), false);
    }
    print (g, "ENTER", O (904, 925, 66, 15), kPrintInk);
    print (g, "DC", O (1114, 928, 38, 15), kPrintInk, false, juce::Justification::centred, true, true, juce::Font::getDefaultSerifFontName());

    // drum names
    print (g, "ACCENT", O (298, 1060, 113, 22), kPrintInk);
    print (g, "BASS DRUM", O (466, 1062, 164, 22), kPrintInk);
    print (g, "SNARE DRUM", O (652, 1065, 179, 22), kPrintInk);
    print (g, "O.HIHAT", O (881, 1067, 111, 22), kPrintInk);
    print (g, "C.HIHAT", O (1077, 1071, 111, 22), kPrintInk);
    print (g, "CYMBAL", O (1274, 1074, 115, 22), kPrintInk);
    print (g, "HAND CLAP", O (1451, 1076, 162, 22), kPrintInk);
}

// a 7-segment digit, slanted like the keys' print
static void sevenSeg (juce::Graphics& g, int digit, juce::Rectangle<float> r)
{
    static const int segs[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };   // a b c d e f g
    const float w = r.getWidth(), h = r.getHeight(), t = juce::jmax (2.6f, w * 0.17f), slant = 0.12f;
    auto P = [&] (float x, float y) { return juce::Point<float> (r.getX() + x + (h - y) * slant, r.getY() + y); };
    auto seg = [&] (juce::Point<float> a, juce::Point<float> b)
    {
        juce::Path p;
        p.startNewSubPath (a); p.lineTo (b);
        g.strokePath (p, juce::PathStrokeType (t, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    };
    const float x0 = t * 0.5f, x1 = w - h * slant - t * 0.5f, y0 = t * 0.5f, ym = h * 0.5f, y1 = h - t * 0.5f, gap = t * 0.55f;
    const int m = segs[juce::jlimit (0, 9, digit)];
    if (m & 0x01) seg (P (x0 + gap, y0), P (x1 - gap, y0));
    if (m & 0x02) seg (P (x1, y0 + gap), P (x1, ym - gap));
    if (m & 0x04) seg (P (x1, ym + gap), P (x1, y1 - gap));
    if (m & 0x08) seg (P (x0 + gap, y1), P (x1 - gap, y1));
    if (m & 0x10) seg (P (x0, ym + gap), P (x0, y1 - gap));
    if (m & 0x20) seg (P (x0, y0 + gap), P (x0, ym - gap));
    if (m & 0x40) seg (P (x0 + gap, ym), P (x1 - gap, ym));
}

void FrontPanel::drawKeyLegend (juce::Graphics& g, int key, float dy)
{
    const auto r = keyRect (key).toFloat().translated (0, dy);
    const auto c = r.getCentre();
    g.setColour (kKeyInk);
    const juce::PathStrokeType line (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    switch (key)
    {
        case Shift: print (g, "SHIFT", r.withSizeKeepingCentre (60, 15), kKeyInk, false); break;
        case Bank:
        {
            const auto b = r.withSizeKeepingCentre (26, 30);
            g.drawLine (b.getCentreX(), b.getY(), b.getCentreX(), b.getBottom(), 1.6f);
            g.drawLine (b.getX(), b.getCentreY(), b.getRight(), b.getCentreY(), 1.6f);
            const char* l[4] = { "A", "B", "C", "D" };
            for (int i = 0; i < 4; ++i)
                print (g, l[i], juce::Rectangle<float> (b.getX() + (i % 2) * 13.0f + 2.5f, b.getY() + (i / 2) * 15.0f + 3.5f, 8.0f, 8.5f), kKeyInk, false);
            break;
        }
        case Start:
            g.fillEllipse (juce::Rectangle<float> (21, 21).withCentre (c.translated (0, -17)));
            print (g, "START", juce::Rectangle<float> (80, 17).withCentre (c.translated (0, 14)), kKeyInk, false);
            break;
        case Stop:
            g.drawEllipse (juce::Rectangle<float> (20, 20).withCentre (c.translated (0, -17)), 2.6f);
            print (g, "STOP", juce::Rectangle<float> (70, 17).withCentre (c.translated (0, 14)), kKeyInk, false);
            break;
        case KAccent:
        {
            // signal waves rising from a dot
            const auto o = c.translated (0, 16);
            g.fillEllipse (juce::Rectangle<float> (11, 11).withCentre (o));
            for (int i = 1; i <= 3; ++i)
            {
                juce::Path a;
                const float rr = 9.0f + 10.0f * (float) i;
                a.addCentredArc (o.x, o.y, rr, rr, 0.0f, -1.0f, 1.0f, true);
                g.strokePath (a, line);
            }
            break;
        }
        case KBD:
        {
            // bass drum seen from the side: shell + front head + beater
            const auto b = r.withSizeKeepingCentre (64, 56);
            juce::Path p;
            p.addEllipse (b.getX() + 18, b.getY(), 36, 56);
            g.strokePath (p, line);
            juce::Path shell;
            shell.startNewSubPath (b.getX() + 36, b.getY());
            shell.quadraticTo (b.getX(), b.getY() + 4, b.getX() + 2, b.getCentreY());
            shell.quadraticTo (b.getX(), b.getBottom() - 4, b.getX() + 36, b.getBottom());
            g.strokePath (shell, line);
            g.drawLine (b.getX() + 36, b.getCentreY() + 4, b.getX() + 36, b.getBottom() - 10, 3.0f);
            g.fillEllipse (juce::Rectangle<float> (11, 11).withCentre ({ b.getX() + 36, b.getCentreY() + 2 }));
            break;
        }
        case KSD:
        {
            const auto b = r.withSizeKeepingCentre (76, 44);
            juce::Path p;
            p.addEllipse (b.getX(), b.getY(), b.getWidth(), 16);
            p.startNewSubPath (b.getX(), b.getY() + 8); p.lineTo (b.getX(), b.getBottom() - 8);
            p.addCentredArc (b.getCentreX(), b.getBottom() - 8, b.getWidth() * 0.5f, 8, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
            p.lineTo (b.getRight(), b.getY() + 8);
            for (int i = 1; i <= 2; ++i)
            {
                const float y = b.getY() + 8 + i * 9.0f;
                p.addCentredArc (b.getCentreX(), y, b.getWidth() * 0.5f, 8, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            }
            g.strokePath (p, juce::PathStrokeType (2.4f));
            break;
        }
        case KOH: case KCH: case KCY:
        {
            auto cym = [&] (float y, bool flipUp)
            {
                juce::Path p;
                const float w = 36.0f;
                p.startNewSubPath (c.x - w, y);
                p.quadraticTo (c.x - 10, y + (flipUp ? -3.0f : 3.0f), c.x - 5, y + (flipUp ? -9.0f : 9.0f));
                p.lineTo (c.x + 5, y + (flipUp ? -9.0f : 9.0f));
                p.quadraticTo (c.x + 10, y + (flipUp ? -3.0f : 3.0f), c.x + w, y);
                p.closeSubPath();
                g.strokePath (p, juce::PathStrokeType (2.6f));
            };
            if (key == KOH)
            {
                g.drawLine (c.x, c.y - 26, c.x, c.y + 26, 2.6f);
                cym (c.y - 8, true);
                cym (c.y + 10, false);
            }
            else if (key == KCH)
            {
                g.drawLine (c.x, c.y - 20, c.x, c.y + 20, 2.6f);
                cym (c.y, true);
                cym (c.y, false);
            }
            else
            {
                juce::Path p;
                p.startNewSubPath (c.x - 46, c.y - 6);
                p.quadraticTo (c.x - 10, c.y - 14, c.x, c.y - 20);
                p.quadraticTo (c.x + 10, c.y - 14, c.x + 46, c.y - 6);
                g.strokePath (p, juce::PathStrokeType (3.0f));
                g.drawLine (c.x, c.y - 20, c.x, c.y + 28, 3.0f);
            }
            break;
        }
        case KCP:
        {
            // two raised hands and clap lines
            auto hand = [&] (float x0, float lean)
            {
                juce::Path p;
                const float base = c.y + 26;
                p.startNewSubPath (x0, base);
                p.lineTo (x0 - 2 + lean, c.y + 2);
                for (int f = 0; f < 4; ++f)
                {
                    const float fx = x0 - 2 + lean + f * 6.5f, top = c.y - 22 + std::abs (1.5f - (float) f) * 4.0f;
                    p.lineTo (fx, top + 3);
                    p.quadraticTo (fx + 3.2f, top - 3, fx + 6.4f, top + 3);
                    p.lineTo (fx + 6.4f, c.y - 2);
                }
                p.lineTo (x0 + 28 + lean, c.y + 4);
                p.lineTo (x0 + 26, base);
                g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            };
            hand (c.x - 18, -2.0f);
            hand (c.x - 4, 2.0f);
            for (int i = 0; i < 3; ++i)
            {
                const float y = c.y - 14 + i * 12.0f;
                g.drawLine (c.x - 44, y + (i - 1) * 3.0f, c.x - 32, y, 2.2f);
            }
            break;
        }
        default:
            if (key >= N1 && key <= N8) sevenSeg (g, key - N1 + 1, r.withSizeKeepingCentre (20, 40).translated (-1, 0));
            break;
    }
}

juce::Rectangle<float> FrontPanel::lcdBox (int i) const
{
    const auto glass = lcdRect().toFloat();
    const float by = glass.getY() + kTopRow + kRowH * kNumRows + 7.0f, bh = glass.getBottom() - 6.0f - by;
    if (i == 0) return { glass.getX() + 6.0f, by, 244.0f, bh };
    if (i == 1) return { glass.getX() + 256.0f, by, 116.0f, bh };
    return { glass.getX() + 378.0f, by, glass.getWidth() - 386.0f, bh };
}

juce::Font FrontPanel::MenuLook::getPopupMenuFont() { return font (17.0f, false); }

bool FrontPanel::drumFlag (int voice, const char* suffix) const
{
    static const char* ids[kNumVoices] = { "bd", "sd", "cp", "ch", "oh", "cy" };
    return proc.apvts.getRawParameterValue (juce::String (ids[voice]) + suffix)->load() > 0.5f;
}

void FrontPanel::toggleDrumFlag (int voice, const char* suffix)
{
    static const char* ids[kNumVoices] = { "bd", "sd", "cp", "ch", "oh", "cy" };
    auto* p = proc.apvts.getParameter (juce::String (ids[voice]) + suffix);
    p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
}

void FrontPanel::drawLcd (juce::Graphics& g)
{
    const auto glass = lcdRect().toFloat();
    if (! powered()) return;   // a dark (blank) display
    if (lockView) { drawLockScreen (g); return; }

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
        // mute / solo: M or S after the name; silenced rows go faint, a muted name blinks
        bool anySolo = false;
        for (int v = 0; v < kNumVoices; ++v) anySolo = anySolo || drumFlag (v, "_solo");
        const bool muted = r > 0 && drumFlag (r - 1, "_mute"), soloed = r > 0 && drumFlag (r - 1, "_solo");
        const bool silent = r > 0 && (soloed ? false : (muted || anySolo));
        const auto nameBox = juce::Rectangle<float> (gridArea.getX() + 10.0f, y, kLabelW - 14.0f, kRowH);
        if (! (muted && (blink / 8) % 2))
            g.drawText (kRowNames[r], nameBox, juce::Justification::centredLeft);
        if (soloed || muted)
        {
            const auto tag = juce::Rectangle<float> (nameBox.getRight() - 12.0f, y + 4.0f, 12.0f, kRowH - 8.0f);
            g.fillRect (tag);
            g.setColour (lcdGlass);
            g.drawText (soloed ? "S" : "M", tag, juce::Justification::centred);
            g.setColour (ink());
        }
        for (int s = 0; s < kNumSteps; ++s)
        {
            const auto cell = juce::Rectangle<float> (x0 + s * cw, y, cw, kRowH);
            const bool on = proc.patterns.cell (slot, r, s);
            g.setColour (on ? ink (silent ? 0.22f : (s < length ? 0.88f : 0.3f)) : ghost);
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
    juce::ignoreUnused (by, bh);
    const juce::Rectangle<float> boxes[3] = { lcdBox (0), lcdBox (1), lcdBox (2) };
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
    if (const float sw = proc.apvts.getRawParameterValue ("swing")->load(); sw > 50.05f) t << "  SW " << juce::roundToInt (sw);
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

void FrontPanel::pressKey (int key, bool shiftHeld, bool altHeld)
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
    else if (const int v = keyVoice (key); v >= 0 && shift)   // SHIFT + drum = mute
    {
        toggleDrumFlag (v, "_mute");
        flash (juce::String (kRowNames[v + 1]) + (drumFlag (v, "_mute") ? " MUTED" : " ON"));
    }
    else if (const int v2 = keyVoice (key); v2 >= 0 && altHeld)   // OPTION / ALT + drum = solo
    {
        toggleDrumFlag (v2, "_solo");
        flash (juce::String (kRowNames[v2 + 1]) + (drumFlag (v2, "_solo") ? " SOLO" : " SOLO OFF"));
    }
    else if (const int v = keyVoice (key); v >= 0)
    {
        if (lockView) { lockVoice = v; if (lockParam == LExtra && ! hasExtra (v)) lockParam = LTune; proc.panelHit (v, false); }
        else if (mode == Tross110Processor::WriteStep) stepRow = v + 1;
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

// ---- the parameter-lock screen ---------------------------------------------------------------
namespace
{
const char* kVoiceNames[kNumVoices] = { "BASS DRUM", "SNARE", "HAND CLAP", "C.HIHAT", "O.HIHAT", "CYMBAL" };
juce::String lockParamName (int voice, int param)
{
    switch (param)
    {
        case LTune:  return "TUNE";
        case LDecay: return "DECAY";
        case LLevel: return "LEVEL";
        default:     return voice == BD ? "ATTACK" : voice == SD ? "SNAPPY" : voice == CP ? "TAIL" : "TONE";
    }
}
}

juce::Rectangle<float> FrontPanel::lockLane()
{
    const auto glass = lcdRect().toFloat();
    const float x0 = glass.getX() + kLabelW;
    return { x0, glass.getY() + kTopRow, glass.getRight() - 8.0f - x0, kRowH * kNumRows };
}

juce::Rectangle<float> FrontPanel::lockBox (int i) const
{
    const auto glass = lcdRect().toFloat();
    const float by = glass.getY() + kTopRow + kRowH * kNumRows + 7.0f, bh = glass.getBottom() - 6.0f - by;
    if (i == 0) return { glass.getX() + 6.0f, by, 160.0f, bh };
    if (i == 1) return { glass.getX() + 172.0f, by, 150.0f, bh };
    return { glass.getX() + 328.0f, by, glass.getWidth() - 336.0f, bh };
}

juce::String FrontPanel::lockValueText (float v) const
{
    if (lockParam == LTune)  return (v >= 0 ? "+" : "") + juce::String (v, 1) + " ST";
    if (lockParam == LLevel) return v <= -69.9f ? juce::String ("-INF DB") : juce::String (v, 1) + " DB";
    return juce::String (juce::roundToInt (v)) + " %";
}

void FrontPanel::setLockFromY (int step, float y)
{
    const auto lane = lockLane();
    const float t = juce::jlimit (0.0f, 1.0f, 1.0f - (y - lane.getY()) / lane.getHeight());
    float v = lockMin (lockParam) + t * (lockMax (lockParam) - lockMin (lockParam));
    if (lockParam == LTune) v = std::round (v * 2.0f) * 0.5f;   // half-semitone steps
    else if (lockParam == LLevel) v = std::round (v * 2.0f) * 0.5f;
    else v = std::round (v);
    proc.patterns.setLock (proc.getCurrentSlot(), lockVoice, lockParam, step, v);
    lockShownStep = step;
}

void FrontPanel::drawLockScreen (juce::Graphics& g)
{
    const auto glass = lcdRect().toFloat();
    const int slot = proc.getCurrentSlot(), playStep = proc.getPlayStep(), length = len();
    auto ink = [] (float a = 0.86f) { return lcdInk.withAlpha (a); };
    const auto lane = lockLane();
    const float cw = lane.getWidth() / 16.0f;

    for (int s = 0; s < kNumSteps; ++s)
    {
        auto r = juce::Rectangle<float> (lane.getX() + s * cw, glass.getY() + 6.0f, cw, 15.0f);
        const bool on = s == playStep;
        if (on) { g.setColour (ink()); g.fillRect (r.reduced (1.0f, 0)); }
        g.setColour (on ? lcdGlass : ink (s < length ? 0.86f : 0.25f));
        g.setFont (font (10.5f));
        g.drawText (juce::String (s + 1), r, juce::Justification::centred);
    }

    // left column: what is being locked, and the scale
    auto col = juce::Rectangle<float> (glass.getX() + 6.0f, lane.getY(), kLabelW - 10.0f, lane.getHeight());
    g.setColour (ink (0.75f));
    g.drawRect (col.getUnion (lane), 1.4f);
    g.drawVerticalLine ((int) lane.getX(), lane.getY(), lane.getBottom());
    g.setColour (ink());
    g.fillRect (col.withHeight (18.0f).reduced (2.0f));
    g.setColour (lcdGlass);
    g.setFont (font (10.5f));
    g.drawText ("P-LOCK", col.withHeight (18.0f), juce::Justification::centred);
    g.setColour (ink());
    g.setFont (font (9.5f));
    g.drawText (lockValueText (lockMax (lockParam)), col.withTrimmedTop (20.0f).withHeight (14.0f).withTrimmedLeft (4.0f), juce::Justification::centredLeft);
    g.drawText (lockValueText (lockMin (lockParam)), col.withTrimmedTop (col.getHeight() - 15.0f).withTrimmedLeft (4.0f), juce::Justification::centredLeft);
    g.setFont (font (11.0f));
    g.drawFittedText (juce::String (kVoiceNames[lockVoice]) + "\n" + lockParamName (lockVoice, lockParam),
                      col.withTrimmedTop (40.0f).withTrimmedBottom (20.0f).withTrimmedLeft (4.0f).toNearestInt(), juce::Justification::centredLeft, 3);

    // bars
    const bool bipolar = lockParam == LTune;
    const float base = bipolar ? lane.getCentreY() : lane.getBottom() - 1.0f;
    if (bipolar) { g.setColour (ink (0.35f)); g.drawHorizontalLine ((int) base, lane.getX(), lane.getRight()); }
    for (int s = 0; s < kNumSteps; ++s)
    {
        const auto c = juce::Rectangle<float> (lane.getX() + s * cw, lane.getY(), cw, lane.getHeight()).reduced (3.5f, 2.0f);
        if (s > 0) { g.setColour (ink (0.18f)); g.drawVerticalLine ((int) (lane.getX() + s * cw), lane.getY(), lane.getBottom()); }
        const float v = proc.patterns.getLock (slot, lockVoice, lockParam, s);
        const bool trig = proc.patterns.cell (slot, lockVoice + 1, s);
        if (isLock (v))
        {
            const float t = (v - lockMin (lockParam)) / (lockMax (lockParam) - lockMin (lockParam));
            const float y = lane.getBottom() - 2.0f - t * (lane.getHeight() - 4.0f);
            g.setColour (ink (s < length ? 0.88f : 0.3f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (c.getX(), juce::jmin (y, base), c.getRight(), juce::jmax (y, base)).withHeight (juce::jmax (2.0f, std::abs (y - base))));
        }
        else
        {
            g.setColour (lcdInk.withAlpha (0.055f));
            g.fillRect (c);
        }
        // the drum's own step: a mark under the bar
        if (trig) { g.setColour (ink()); g.fillRect (c.getX() + 2.0f, lane.getBottom() + 1.5f, c.getWidth() - 4.0f, 2.5f); }
    }
    if (playStep >= 0) { g.setColour (ink (0.12f)); g.fillRect (juce::Rectangle<float> (lane.getX() + playStep * cw, lane.getY(), cw, lane.getHeight())); }

    // lower boxes: DRUM | SETTING | VALUE (click the first two to change them)
    const char* titles[3] = { "DRUM  (click)", "SETTING  (click)", "VALUE" };
    for (int i = 0; i < 3; ++i)
    {
        const auto b = lockBox (i);
        g.setColour (ink (0.75f));
        g.drawRoundedRectangle (b, 3.0f, 1.4f);
        g.setFont (font (8.5f));
        g.drawText (titles[i], b.withHeight (13.0f).withTrimmedLeft (5.0f), juce::Justification::centredLeft);
        g.drawHorizontalLine ((int) (b.getY() + 13.0f), b.getX(), b.getRight());
    }
    g.setColour (ink());
    g.setFont (font (18.0f));
    g.drawFittedText (kVoiceNames[lockVoice], lockBox (0).withTrimmedTop (14.0f).reduced (6.0f, 2.0f).withTrimmedBottom (16.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    g.drawFittedText (lockParamName (lockVoice, lockParam), lockBox (1).withTrimmedTop (14.0f).reduced (6.0f, 2.0f).withTrimmedBottom (16.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    const bool msgOn = message.isNotEmpty();
    juce::String val = "-";
    if (msgOn) val = message;
    else if (lockShownStep >= 0)
    {
        const float v = proc.patterns.getLock (slot, lockVoice, lockParam, lockShownStep);
        val = "STEP " + juce::String (lockShownStep + 1) + "  " + (isLock (v) ? lockValueText (v) : juce::String ("KNOB"));
    }
    g.setFont (font (msgOn ? 14.0f : 16.0f));
    g.drawFittedText (val, lockBox (2).withTrimmedTop (14.0f).reduced (6.0f, 2.0f).withTrimmedBottom (16.0f).toNearestInt(), juce::Justification::centredLeft, 2);
    g.setFont (font (9.5f));
    g.drawText ("DRAG = SET  DBL-CLICK = CLEAR", lockBox (2).withTrimmedTop (lockBox (2).getHeight() - 15.0f).withTrimmedLeft (6.0f), juce::Justification::centredLeft);
    g.drawText ("PATTERN " + slotName (slot) + (shiftLatched ? "  SHIFT" : ""), lockBox (0).withTrimmedTop (lockBox (0).getHeight() - 15.0f).withTrimmedLeft (6.0f), juce::Justification::centredLeft);
}

// ---- copy / paste --------------------------------------------------------------------------
void FrontPanel::copyPattern()
{
    const int slot = proc.getCurrentSlot();
    clip.valid = true;
    for (int r = 0; r < kNumRows; ++r) clip.rows[r] = proc.patterns.get (slot, r);
    for (int v = 0; v < kNumVoices; ++v) for (int p = 0; p < kNumLockParams; ++p) for (int s = 0; s < kNumSteps; ++s)
        clip.locks[v][p][s] = proc.patterns.getLock (slot, v, p, s);
    flash ("COPIED " + slotName (slot));
}

bool FrontPanel::pastePattern()
{
    if (! clip.valid) return false;
    const int slot = proc.getCurrentSlot();
    for (int r = 0; r < kNumRows; ++r) proc.patterns.set (slot, r, clip.rows[r]);
    for (int v = 0; v < kNumVoices; ++v) for (int p = 0; p < kNumLockParams; ++p) for (int s = 0; s < kNumSteps; ++s)
        proc.patterns.setLock (slot, v, p, s, clip.locks[v][p][s]);
    flash ("PASTED TO " + slotName (slot));
    return true;
}

void FrontPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (kPowerSwitch.contains (p))
    {
        auto* pw = proc.apvts.getParameter ("power");
        pw->setValueNotifyingHost (powered() ? 0.0f : 1.0f);
        repaint();
        return;
    }
    if (overScrew (p) || kPowerText.contains (p)) { editor.openCase (true); return; }
    if (kLogo.contains (p)) { editor.showAbout(); return; }

    if (lcdRect().contains (p) && powered())
    {
        if (e.mods.isPopupMenu()) { showLcdMenu(); return; }
        if (lockView)
        {
            const auto lane = lockLane();
            if (lane.contains (e.position))
            {
                const int st = juce::jlimit (0, kNumSteps - 1, (int) ((e.position.x - lane.getX()) / (lane.getWidth() / 16.0f)));
                if (e.getNumberOfClicks() >= 2 || e.mods.isAltDown())
                {
                    proc.patterns.clearLock (proc.getCurrentSlot(), lockVoice, lockParam, st);
                    lockShownStep = st;
                }
                else { lockPaintStep = st; setLockFromY (st, e.position.y); }
            }
            else if (lockBox (0).contains (e.position)) { lockVoice = (lockVoice + 1) % kNumVoices; if (lockParam == LExtra && ! hasExtra (lockVoice)) lockParam = LTune; }
            else if (lockBox (1).contains (e.position)) { lockParam = (lockParam + 1) % (hasExtra (lockVoice) ? kNumLockParams : kNumLockParams - 1); }
            repaint (lcdRect());
            return;
        }
        if (proc.isStandalone() && lcdBox (2).contains (e.position))
        {
            auto* t = proc.apvts.getParameter ("tempo");
            if (e.getNumberOfClicks() >= 2) t->setValueNotifyingHost (t->convertTo0to1 (120.0f));
            tempoDragging = true;
            tempoDragStart = proc.apvts.getRawParameterValue ("tempo")->load();
            return;
        }
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
            if (k == Shift && e.getNumberOfClicks() >= 2) { toggleLockView(); flash (lockView ? "P-LOCK" : modeName (proc.getMode())); return; }
            pressKey (k, e.mods.isShiftDown(), e.mods.isAltDown());
            repaint();
            return;
        }
}

void FrontPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (tempoDragging)
    {
        auto* t = proc.apvts.getParameter ("tempo");
        const float bpm = juce::jlimit (40.0f, 250.0f, std::round (tempoDragStart - (float) e.getDistanceFromDragStartY() * 0.5f));
        t->setValueNotifyingHost (t->convertTo0to1 (bpm));
        repaint (lcdRect());
        return;
    }
    if (lockPaintStep >= 0)
    {
        const auto lane = lockLane();
        const int st = juce::jlimit (0, kNumSteps - 1, (int) ((e.position.x - lane.getX()) / (lane.getWidth() / 16.0f)));
        setLockFromY (st, e.position.y);
        repaint (lcdRect());
        return;
    }
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
    lockPaintStep = -1;
    tempoDragging = false;
    if (heldKey >= 0) { heldKey = -1; repaint(); }
}

void FrontPanel::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    bool hand = kPowerSwitch.contains (p) || kPowerText.contains (p) || overScrew (p) || kLogo.contains (p) || lcdRect().contains (p);
    for (int k = 0; k < kNumKeys && ! hand; ++k) hand = keyRect (k).contains (p);
    setMouseCursor (hand ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void FrontPanel::showDrumMenu (int v)
{
    if (proc.isStandalone()) return;   // the app has only the main output
    static const char* names[kNumVoices] = { "BASS DRUM", "SNARE DRUM", "HAND CLAP", "C.HIHAT", "O.HIHAT", "CYMBAL" };
    juce::PopupMenu m;
    m.setLookAndFeel (&menuLook);
    m.addSectionHeader (names[v]);
    m.addItem (1, "Own output", true, drumFlag (v, "_out"));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition().withStandardItemHeight (30),
                     [safe = juce::Component::SafePointer<FrontPanel> (this), v] (int r)
                     {
                         if (safe != nullptr && r == 1) safe->toggleDrumFlag (v, "_out");
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
    m.setLookAndFeel (&menuLook);
    copy.setLookAndFeel (&menuLook);
    m.addSectionHeader ("PATTERN " + slotName (slot));
    m.addItem (1, "Export this pattern as MIDI...", ! MidiExport::isEmpty (proc.patterns, slot));
    m.addItem (2, "Export all patterns in a row...", ! nonEmpty.empty());
    m.addItem (3, "Export every pattern as its own file...", ! nonEmpty.empty());
    m.addItem (4, "Export the song...", ! chain.empty());
    m.addSeparator();
    m.addItem (7, "Copy pattern");
    m.addItem (8, clip.valid ? "Paste pattern into " + slotName (slot) : juce::String ("Paste pattern"), clip.valid);
    m.addSubMenu ("Copy pattern to", copy);
    m.addSeparator();
    if (lockView)
    {
        m.addItem (9, "Clear these locks (" + lockParamName (lockVoice, lockParam) + ")");
        m.addItem (10, "Clear all locks of this drum");
        m.addItem (11, "Clear all locks in this pattern", proc.patterns.hasAnyLock (slot));
    }
    m.addItem (5, "Clear pattern");
    m.addItem (6, "Clear song", ! chain.empty());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition().withStandardItemHeight (30),
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
                         if (r == 7) self.copyPattern();
                         if (r == 8) self.pastePattern();
                         if (r == 9)  { self.proc.patterns.clearLocks (slot, self.lockVoice, self.lockParam); self.flash ("LOCKS CLEARED"); }
                         if (r == 10) { self.proc.patterns.clearLocks (slot, self.lockVoice, -1); self.flash ("LOCKS CLEARED"); }
                         if (r == 11) { self.proc.patterns.clearLocks (slot, -1, -1); self.flash ("ALL LOCKS CLEARED"); }
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
