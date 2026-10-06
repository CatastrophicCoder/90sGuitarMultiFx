#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>

namespace fivea::ui
{

// The panel's 2-digit LED display (SRC-001 p. 2: "8 segment LED x 2"), drawn in code: seven
// segments per digit plus a decimal point, lit and unlit. Shows a value as the original does in
// Edit mode (p. 7): 0–15 as one or two digits, −7…−1 as a minus and a digit; "--" on stand-by.
class SevenSegmentDisplay final : public juce::Component
{
public:
    // Bit 0 = segment a (top), then b (top right), c (bottom right), d (bottom), e (bottom left),
    // f (top left), g (middle).
    [[nodiscard]] static std::uint8_t segmentsFor(char character) noexcept;

    // The two characters the display shows for a value, e.g. 7 → " 7", 12 → "12", −3 → "-3".
    [[nodiscard]] static juce::String textFor(int value);

    void showValue(int value);
    void showStandBy();
    void setDot(bool lit);

    [[nodiscard]] const juce::String& getText() const noexcept { return text; }
    [[nodiscard]] bool isDotLit() const noexcept { return dot; }

    void paint(juce::Graphics& g) override;

private:
    void paintDigit(juce::Graphics& g, juce::Rectangle<float> area, std::uint8_t segments) const;

    juce::String text{"--"};
    bool dot = false;
};

} // namespace fivea::ui
