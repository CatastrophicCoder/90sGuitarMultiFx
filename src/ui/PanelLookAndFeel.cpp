#include "ui/PanelLookAndFeel.h"

#include "PanelFonts.h"

#include <cmath>

namespace fivea::ui
{

PanelLookAndFeel::PanelLookAndFeel()
    : regular(juce::Typeface::createSystemTypefaceFor(PanelFonts::BarlowCondensedRegular_ttf,
                                                      PanelFonts::BarlowCondensedRegular_ttfSize))
    , semiBold(juce::Typeface::createSystemTypefaceFor(PanelFonts::BarlowCondensedSemiBold_ttf,
                                                       PanelFonts::BarlowCondensedSemiBold_ttfSize))
{
    setColour(juce::ResizableWindow::backgroundColourId, colours::housing);
    setColour(juce::PopupMenu::backgroundColourId, colours::face);
    setColour(juce::PopupMenu::textColourId, colours::printing);
}

juce::Font PanelLookAndFeel::font(float height, bool useSemiBold) const
{
    return juce::Font{juce::FontOptions{useSemiBold ? semiBold : regular}.withHeight(height)};
}

juce::Typeface::Ptr PanelLookAndFeel::getTypefaceForFont(const juce::Font& requested)
{
    // JUCE's own widgets (popups, tooltips) use the panel's lettering too.
    if (requested.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return requested.isBold() ? semiBold : regular;
    return LookAndFeel_V4::getTypefaceForFont(requested);
}

void PanelLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    // A small black knurled knob with a white pointer, as on the original panel.
    const auto bounds = juce::Rectangle<int>{x, y, width, height}.toFloat().reduced(1.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float alpha = slider.isEnabled() ? 1.0f : 0.55f;

    g.setColour(colours::knobRim.withMultipliedAlpha(alpha));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    const float faceRadius = radius * 0.86f;
    g.setGradientFill(juce::ColourGradient{colours::knob.brighter(0.25f), centre.x - faceRadius * 0.4f,
                                           centre.y - faceRadius * 0.6f, colours::knob.darker(0.3f),
                                           centre.x + faceRadius, centre.y + faceRadius, true});
    g.fillEllipse(centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);

    // Knurling: fine radial ticks round the edge.
    g.setColour(juce::Colours::black.withAlpha(0.35f * alpha));
    for (int tick = 0; tick < 24; ++tick)
    {
        const float angle = juce::MathConstants<float>::twoPi * static_cast<float>(tick) / 24.0f;
        g.drawLine(centre.x + std::sin(angle) * faceRadius * 0.82f, centre.y - std::cos(angle) * faceRadius * 0.82f,
                   centre.x + std::sin(angle) * faceRadius, centre.y - std::cos(angle) * faceRadius, 1.0f);
    }

    const float angle = startAngle + position * (endAngle - startAngle);
    g.setColour(colours::printing.withMultipliedAlpha(alpha));
    juce::Path pointer;
    pointer.addRoundedRectangle(-faceRadius * 0.07f, -faceRadius * 0.95f, faceRadius * 0.14f, faceRadius * 0.5f,
                                faceRadius * 0.05f);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre));
}

} // namespace fivea::ui
