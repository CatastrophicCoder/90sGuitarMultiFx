#include "ui/SevenSegmentDisplay.h"

#include "ui/PanelLookAndFeel.h"

#include <cstdlib>

namespace fivea::ui
{

std::uint8_t SevenSegmentDisplay::segmentsFor(char character) noexcept
{
    switch (character)
    {
    case '0':
        return 0x3F;
    case '1':
        return 0x06;
    case '2':
        return 0x5B;
    case '3':
        return 0x4F;
    case '4':
        return 0x66;
    case '5':
        return 0x6D;
    case '6':
        return 0x7D;
    case '7':
        return 0x07;
    case '8':
        return 0x7F;
    case '9':
        return 0x6F;
    case '-':
        return 0x40;
    default:
        return 0x00;
    }
}

juce::String SevenSegmentDisplay::textFor(int value)
{
    if (value < 0)
        return "-" + juce::String(std::min(std::abs(value), 9));
    if (value < 10)
        return " " + juce::String(value);
    return juce::String(std::min(value, 99));
}

void SevenSegmentDisplay::showValue(int value)
{
    text = textFor(value);
    repaint();
}

void SevenSegmentDisplay::showStandBy()
{
    text = "--";
    repaint();
}

void SevenSegmentDisplay::setDot(bool lit)
{
    dot = lit;
    repaint();
}

void SevenSegmentDisplay::paintDigit(juce::Graphics& g, juce::Rectangle<float> area, std::uint8_t segments) const
{
    // Segment geometry in a unit cell (0–1 across, 0–1 down), slanted like a real LED digit.
    const float w = area.getWidth();
    const float h = area.getHeight();
    const float t = 0.14f * w; // segment thickness
    const float slant = 0.12f * w;

    auto point = [&](float x, float y)
    {
        return juce::Point<float>{area.getX() + x * w + slant * (1.0f - y), area.getY() + y * h};
    };
    auto horizontal = [&](float y)
    {
        juce::Path p;
        const float half = t / (2.0f * h);
        p.startNewSubPath(point(0.12f, y));
        p.lineTo(point(0.18f, y - half));
        p.lineTo(point(0.82f, y - half));
        p.lineTo(point(0.88f, y));
        p.lineTo(point(0.82f, y + half));
        p.lineTo(point(0.18f, y + half));
        p.closeSubPath();
        return p;
    };
    auto vertical = [&](float x, float top, float bottom)
    {
        juce::Path p;
        const float half = t / (2.0f * w);
        p.startNewSubPath(point(x, top));
        p.lineTo(point(x + half, top + 0.04f));
        p.lineTo(point(x + half, bottom - 0.04f));
        p.lineTo(point(x, bottom));
        p.lineTo(point(x - half, bottom - 0.04f));
        p.lineTo(point(x - half, top + 0.04f));
        p.closeSubPath();
        return p;
    };

    const std::array<juce::Path, 7> shapes{
        horizontal(0.06f), vertical(0.88f, 0.08f, 0.48f), vertical(0.88f, 0.52f, 0.92f),
        horizontal(0.94f), vertical(0.12f, 0.52f, 0.92f), vertical(0.12f, 0.08f, 0.48f),
        horizontal(0.50f)};

    for (std::size_t segment = 0; segment < shapes.size(); ++segment)
    {
        const bool lit = (segments >> segment) & 1u;
        g.setColour(lit ? colours::ledLit : colours::ledUnlit);
        g.fillPath(shapes[segment]);
    }
}

void SevenSegmentDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float dotSpace = bounds.getWidth() * 0.08f;
    const float digitWidth = (bounds.getWidth() - dotSpace) / 2.0f;

    for (int index = 0; index < 2; ++index)
    {
        const char character = index < text.length() ? static_cast<char>(text[index]) : ' ';
        paintDigit(g,
                   {bounds.getX() + static_cast<float>(index) * digitWidth, bounds.getY(), digitWidth * 0.92f,
                    bounds.getHeight()},
                   segmentsFor(character));
    }

    const float dotSize = digitWidth * 0.14f;
    g.setColour(dot ? colours::ledLit : colours::ledUnlit);
    g.fillEllipse(bounds.getRight() - dotSize * 1.2f, bounds.getBottom() - dotSize * 1.1f, dotSize, dotSize);
}

} // namespace fivea::ui
