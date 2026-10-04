#include "core/PresetState.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <optional>

namespace a5::state
{

namespace
{

constexpr const char* parametersTag = "Parameters";
constexpr const char* parameterTag = "Parameter";
constexpr const char* schemaVersionAttribute = "schemaVersion";
constexpr const char* modelAttribute = "model";
constexpr const char* idAttribute = "id";
constexpr const char* valueAttribute = "value";

// juce::String::getDoubleValue() returns 0 for garbage, which would silently become a valid
// setting; a state value must parse completely or be treated as missing.
std::optional<double> parseFiniteNumber(const juce::String& text)
{
    const auto trimmed = text.trim().toStdString();
    if (trimmed.empty())
        return std::nullopt;

    char* end = nullptr;
    const double value = std::strtod(trimmed.c_str(), &end);

    if (end != trimmed.c_str() + trimmed.size() || !std::isfinite(value))
        return std::nullopt;

    return value;
}

// A schema version is a whole number ≥ 1 written as plain digits; "1.5", "1e0" or "abc" is not one.
std::optional<int> parseSchemaVersion(const juce::String& text)
{
    const auto digits = text.trim().toStdString();
    int version = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), version);

    if (digits.empty() || error != std::errc{} || end != digits.data() + digits.size() || version < 1)
        return std::nullopt;

    return version;
}

const juce::XmlElement* findParameter(const juce::XmlElement* parameters, const juce::String& id)
{
    if (parameters == nullptr)
        return nullptr;

    for (const auto* child : parameters->getChildWithTagNameIterator(parameterTag))
        if (child->getStringAttribute(idAttribute) == id)
            return child;

    return nullptr;
}

// Each schema change adds a step here that rewrites an older document into the next version's
// shape. Version 1 is the first, so there is nothing to migrate yet.
void migrateToCurrentSchema(juce::XmlElement& /*xml*/, int /*fromVersion*/) {}

} // namespace

std::unique_ptr<juce::XmlElement> toXml(const juce::AudioProcessor& processor)
{
    auto xml = std::make_unique<juce::XmlElement>(rootTag);
    xml->setAttribute(schemaVersionAttribute, currentSchemaVersion);
    xml->setAttribute(modelAttribute, modelIdentifier);

    auto* parameters = xml->createNewChildElement(parametersTag);

    for (const auto* parameter : processor.getParameters())
    {
        const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr)
            continue;

        auto* element = parameters->createNewChildElement(parameterTag);
        element->setAttribute(idAttribute, ranged->getParameterID());
        element->setAttribute(valueAttribute, static_cast<double>(ranged->convertFrom0to1(ranged->getValue())));
    }

    return xml;
}

LoadResult fromXml(const juce::XmlElement& source, juce::AudioProcessor& processor)
{
    if (!source.hasTagName(rootTag))
        return {};

    const auto version = parseSchemaVersion(source.getStringAttribute(schemaVersionAttribute));
    if (!version.has_value())
        return {};

    const int schemaVersion = *version;

    // A newer schema is still read: its known parameters load and anything it added is ignored.
    juce::XmlElement xml{source};
    if (schemaVersion < currentSchemaVersion)
        migrateToCurrentSchema(xml, schemaVersion);

    const auto* parameters = xml.getChildByName(parametersTag);

    for (auto* parameter : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr)
            continue;

        std::optional<double> plainValue;
        if (const auto* element = findParameter(parameters, ranged->getParameterID()))
            plainValue = parseFiniteNumber(element->getStringAttribute(valueAttribute));

        // convertTo0to1 clamps to the parameter's range.
        const float normalised =
            plainValue.has_value() ? ranged->convertTo0to1(static_cast<float>(*plainValue)) : ranged->getDefaultValue();

        ranged->setValueNotifyingHost(normalised);
    }

    return {true, schemaVersion};
}

} // namespace a5::state
