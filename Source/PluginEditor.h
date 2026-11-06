#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// UNIT 110 editor: the photographed front panel (keys, knobs, LCD, power switch) and, behind a
// case screw, the open circuit board with the service trimmers. Everything is laid out in the
// panel image's own pixels (kDesignW x kDesignH) and scaled to the window.
namespace unit110ui
{
constexpr int kDesignW = 1500, kDesignH = 908;

// a knob or trimmer drawn from an image (or by hand) that turns with its value
struct KnobLook : juce::LookAndFeel_V4
{
    enum Kind { FrontKnob, Trimmer, SmallTrimmer } kind = FrontKnob;
    juce::Image knob;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
};

class Knob : public juce::Slider
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, KnobLook&, float startAngle, float endAngle);
    ~Knob() override { setLookAndFeel (nullptr); }
private:
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};
} // namespace unit110ui

class Tross110Editor;

// The custom About window (click the TRANSISTOR LABS logo)
class AboutBox : public juce::Component
{
public:
    AboutBox();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Rectangle<int> card, linkStore, linkMail;
};

// the open case: circuit board + service trimmers
class PcbView : public juce::Component
{
public:
    PcbView (Tross110Processor&, Tross110Editor&);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    juce::Point<float> toView (float x, float y) const;   // board-image pixels -> view
private:
    Tross110Processor& proc;
    Tross110Editor& editor;
    juce::Image board;
    unit110ui::KnobLook trimLook, smallTrimLook;
    juce::OwnedArray<unit110ui::Knob> trims;
    float scale = 1.0f, xOff = 0.0f;
    bool closesCase (juce::Point<float>) const;   // the screws and the three big chips
};

// the front panel
class FrontPanel : public juce::Component, private juce::Timer
{
public:
    FrontPanel (Tross110Processor&, Tross110Editor&);
    ~FrontPanel() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    enum Key { Shift, Bank, N1, N2, N3, N4, N5, N6, N7, N8, Start, Stop,
               KAccent, KBD, KSD, KOH, KCH, KCY, KCP, kNumKeys };
    static juce::Rectangle<int> keyRect (int key);
    static juce::Rectangle<int> lcdRect() { return { 128, 129, 498, 313 }; }
    static juce::Rectangle<int> lcdCell (int row, int step);   // row 0 = accent
    void pressKey (int key, bool shiftHeld);                   // what a click on a key does
    void flash (const juce::String& message);                  // a short message in the LCD's mode box
    bool isShiftLatched() const { return shiftLatched; }
    int  getStepRow() const { return stepRow; }
    int  getBank() const { return bank; }

    // the parameter-lock screen (double-click SHIFT): same LCD, one drum's locks for one setting
    void toggleLockView() { lockView = ! lockView; shiftLatched = false; repaint(); }
    bool isLockView() const { return lockView; }
    int  lockVoice = 0, lockParam = 0;   // tross110::Voice, tross110::LockParam
    static juce::Rectangle<float> lockLane();             // the bar area of the lock screen
    void setLockFromY (int step, float y);                // what a click / drag in the lock lane does
    juce::Rectangle<float> lockBox (int i) const;         // 0 = drum, 1 = setting, 2 = value
    void copyPattern();
    bool pastePattern();

private:
    Tross110Processor& proc;
    Tross110Editor& editor;
    juce::Image panel;
    unit110ui::KnobLook knobLook;
    juce::OwnedArray<unit110ui::Knob> knobs;

    int heldKey = -1;
    bool shiftLatched = false, accentArmed = false;
    int stepRow = 1;     // WRITE STEP: the row the number keys edit (0 = accent, 1..6 = drums)
    int stepHalf = 0;    // WRITE STEP: 0 = steps 1-8, 1 = steps 9-16
    int bank = 0;        // A..D
    int paintRow = -1;   // LCD drag-painting
    bool paintValue = false;
    bool lockView = false;
    int lockPaintStep = -1, lockShownStep = -1;
    struct Clip { bool valid = false; uint16_t rows[tross110::kNumRows] {}; float locks[tross110::kNumVoices][tross110::kNumLockParams][tross110::kNumSteps] {}; };
    Clip clip;
    void drawLockScreen (juce::Graphics&);
    juce::String lockValueText (float v) const;
    juce::String message;
    juce::uint32 messageUntil = 0;
    int blink = 0, lastVersion = -1, lastStep = -2, lastSlot = -1;
    std::unique_ptr<juce::FileChooser> chooser;

    void timerCallback() override;
    void drawLcd (juce::Graphics&);
    void drawKeyPressed (juce::Graphics&, juce::Rectangle<int>);
    void drawScrew (juce::Graphics&, juce::Point<float>) const;
    bool overScrew (juce::Point<int>) const;
    bool powered() const;
    int  len() const;
    void selectSlot (int slot);
    void showLcdMenu();
    void showDrumMenu (int voice);
    void exportMidi (const std::vector<int>& slots, const juce::String& name);
    void exportSeparate();
};

class Tross110Editor : public juce::AudioProcessorEditor
{
public:
    explicit Tross110Editor (Tross110Processor&);
    ~Tross110Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void showAbout();
    void openCase (bool open, bool animate = true);
    bool isCaseOpen() const { return pcb.isVisible(); }
    FrontPanel& getFront() { return front; }

private:
    Tross110Processor& proc;
    juce::Component content;   // design-size canvas, scaled to the window
    FrontPanel front;
    PcbView pcb;
    AboutBox about;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tross110Editor)
};
