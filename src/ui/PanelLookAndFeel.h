#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fivea::ui
{

// The replica panel's colours: black housing and face, red graphics, light printing, red LEDs
// (docs/panel-specification.md). Chosen for the replica, not sampled from the reference photo.
namespace colours
{
inline const juce::Colour housing{0xff141518};
inline const juce::Colour face{0xff1e2023};
inline const juce::Colour faceEdge{0xff2c2f33};
inline const juce::Colour deck{0xff17191c};
inline const juce::Colour printing{0xffe6e6e3};
inline const juce::Colour printingDim{0xffa9aaa7};
inline const juce::Colour gridLine{0xffbdbdba};
inline const juce::Colour red{0xffd52a3c};
inline const juce::Colour redLabel{0xffe0405a};
inline const juce::Colour displayWindow{0xff0b0b0d};
inline const juce::Colour ledLit{0xffff3b30};
inline const juce::Colour ledUnlit{0xff24100f};
inline const juce::Colour knob{0xff26282b};
inline const juce::Colour knobRim{0xff0c0d0f};
inline const juce::Colour footswitch{0xff1b1d20};
inline const juce::Colour footswitchFace{0xff2a2d31};
} // namespace colours

// Panel typography and controls. Lettering is Barlow Condensed (SIL Open Font License 1.1),
// embedded in the plugin (resources/fonts).
class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    [[nodiscard]] juce::Font font(float height, bool semiBold = false) const;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float startAngle,
                          float endAngle, juce::Slider& slider) override;

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& requested) override;

private:
    juce::Typeface::Ptr regular;
    juce::Typeface::Ptr semiBold;
};

} // namespace fivea::ui
