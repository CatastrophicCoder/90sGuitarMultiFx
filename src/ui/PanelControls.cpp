#include "ui/PanelControls.h"

#include "ui/PanelLookAndFeel.h"

#include <cmath>

namespace fivea::ui
{

Knob::Knob(const juce::String& accessibleName)
    : juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox)
{
    setTitle(accessibleName);
    setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    setMouseDragSensitivity(160);
    setVelocityBasedMode(false);
}

Led::Led(bool isRectangular) : rectangular(isRectangular)
{
    setInterceptsMouseClicks(false, false);
}

void Led::setLit(bool shouldBeLit)
{
    if (lit != shouldBeLit)
    {
        lit = shouldBeLit;
        repaint();
    }
}

void Led::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    if (lit)
    {
        g.setColour(colours::ledLit.withAlpha(0.35f));
        if (rectangular)
            g.fillRoundedRectangle(bounds, 2.0f);
        else
            g.fillEllipse(bounds);
    }
    const auto core = bounds.reduced(bounds.getHeight() * 0.18f);
    g.setColour(lit ? colours::ledLit : colours::ledUnlit);
    if (rectangular)
        g.fillRoundedRectangle(core, 1.5f);
    else
        g.fillEllipse(core);
}

FootSwitch::FootSwitch(const juce::String& name) : juce::Button(name)
{
    setTitle(name);
}

void FootSwitch::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(colours::knobRim);
    g.fillRoundedRectangle(bounds, 4.0f);

    // The pedal: a raised face with a step a fifth of the way down, pressed slightly when down.
    auto pedal = bounds.reduced(6.0f).withTrimmedTop(down ? 4.0f : 0.0f);
    g.setGradientFill(juce::ColourGradient{colours::footswitchFace.brighter(highlighted ? 0.18f : 0.1f), pedal.getX(),
                                           pedal.getY(), colours::footswitch, pedal.getX(), pedal.getBottom(), false});
    g.fillRoundedRectangle(pedal, 3.0f);

    const float step = pedal.getY() + pedal.getHeight() * 0.22f;
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRect(pedal.getX() + 4.0f, step, pedal.getWidth() - 8.0f, 3.0f);
    g.setColour(colours::faceEdge);
    g.fillRect(pedal.getX() + 4.0f, step + 3.0f, pedal.getWidth() - 8.0f, 1.5f);

    if (!isEnabled())
    {
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(bounds, 4.0f);
    }
}

RoundKey::RoundKey(const juce::String& name) : juce::Button(name)
{
    setTitle(name);
}

void RoundKey::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(colours::knobRim);
    g.fillEllipse(bounds);
    const auto face = bounds.reduced(bounds.getWidth() * (down ? 0.12f : 0.08f));
    g.setGradientFill(juce::ColourGradient{colours::knob.brighter(highlighted ? 0.3f : 0.18f), face.getX(), face.getY(),
                                           colours::knob.darker(0.4f), face.getRight(), face.getBottom(), false});
    g.fillEllipse(face);
    if (!isEnabled())
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillEllipse(bounds);
    }
}

SlideSelector::SlideSelector()
{
    setTitle("Bank/effect selector");
    setRepaintsOnMouseActivity(false);
}

void SlideSelector::setPositionCentres(std::array<float, 6> centres)
{
    positionCentres = centres;
    repaint();
}

void SlideSelector::setPosition(int newPosition, juce::NotificationType notification)
{
    newPosition = juce::jlimit(1, 6, newPosition);
    if (newPosition == position)
        return;
    position = newPosition;
    repaint();
    if (notification != juce::dontSendNotification && onChange)
        onChange(position);
}

int SlideSelector::positionNearest(float y) const noexcept
{
    int nearest = 1;
    float best = std::numeric_limits<float>::max();
    for (int index = 0; index < 6; ++index)
    {
        const float distance = std::abs(y - positionCentres[static_cast<std::size_t>(index)]);
        if (distance < best)
        {
            best = distance;
            nearest = index + 1;
        }
    }
    return nearest;
}

void SlideSelector::mouseDown(const juce::MouseEvent& event)
{
    setPosition(positionNearest(event.position.y), juce::sendNotificationSync);
}

void SlideSelector::mouseDrag(const juce::MouseEvent& event)
{
    setPosition(positionNearest(event.position.y), juce::sendNotificationSync);
}

void SlideSelector::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Housing: a black recess with ribbed sides, and a vertical slot.
    g.setColour(colours::knobRim);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(colours::face.darker(0.4f));
    g.fillRoundedRectangle(bounds.reduced(5.0f), 4.0f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    for (float ribY = bounds.getY() + 14.0f; ribY < bounds.getBottom() - 10.0f; ribY += 7.0f)
        g.fillRect(bounds.getRight() - 22.0f, ribY, 12.0f, 2.0f);

    const float slotX = bounds.getCentreX() - 4.0f;
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(slotX, bounds.getY() + 10.0f, 8.0f, bounds.getHeight() - 20.0f, 3.0f);

    // Lever at the selected position.
    const float leverY = positionCentres[static_cast<std::size_t>(position - 1)];
    const juce::Rectangle<float> lever{bounds.getCentreX() - 24.0f, leverY - 8.0f, 48.0f, 16.0f};
    g.setGradientFill(juce::ColourGradient{colours::knob.brighter(0.35f), lever.getX(), lever.getY(),
                                           colours::knob.darker(0.3f), lever.getX(), lever.getBottom(), false});
    g.fillRoundedRectangle(lever, 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawHorizontalLine(static_cast<int>(lever.getCentreY()), lever.getX() + 4.0f, lever.getRight() - 4.0f);
}

} // namespace fivea::ui
