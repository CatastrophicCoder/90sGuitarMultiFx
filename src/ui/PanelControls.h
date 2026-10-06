#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace fivea::ui
{

// A rotary knob without a text box: values show on the panel's LED display instead.
class Knob final : public juce::Slider
{
public:
    explicit Knob(const juce::String& accessibleName);
};

// A round LED, lit or not.
class Led final : public juce::Component
{
public:
    explicit Led(bool isRectangular = false);
    void setLit(bool shouldBeLit);
    [[nodiscard]] bool isLit() const noexcept { return lit; }
    void paint(juce::Graphics& g) override;

private:
    bool lit = false;
    bool rectangular = false;
};

// One of the six footswitches on the deck: a tall pedal with a ridged step.
class FootSwitch final : public juce::Button
{
public:
    explicit FootSwitch(const juce::String& name);
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
};

// WRITE and BYPASS: small round black keys.
class RoundKey final : public juce::Button
{
public:
    explicit RoundKey(const juce::String& name);
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
};

// The six-position BANK/EFFECT slide switch (SRC-001 p. 2). Its lever lines up with the grid row
// it selects. Click or drag to move it.
class SlideSelector final : public juce::Component
{
public:
    SlideSelector();
    void setPosition(int newPosition, juce::NotificationType notification); // 1–6
    [[nodiscard]] int getPosition() const noexcept { return position; }

    // Where each position's lever sits, in this component's coordinates (set by the panel so the
    // lever lines up with the grid rows).
    void setPositionCentres(std::array<float, 6> centres);

    std::function<void(int)> onChange;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    [[nodiscard]] int positionNearest(float y) const noexcept;

    int position = 1;
    std::array<float, 6> positionCentres{};
};

} // namespace fivea::ui
