#include "core/PresetState.h"

#include "core/ParameterIds.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <optional>

namespace fivea::state
{

namespace
{

constexpr const char* parametersTag = "Parameters";
constexpr const char* parameterTag = "Parameter";
constexpr const char* schemaVersionAttribute = "schemaVersion";
constexpr const char* modelAttribute = "model";
constexpr const char* idAttribute = "id";
constexpr const char* valueAttribute = "value";
constexpr const char* programsTag = "Programs";
constexpr const char* programTag = "Program";
constexpr const char* programValueTag = "Value";
constexpr const char* bankAttribute = "bank";
constexpr const char* programAttribute = "program";
constexpr const char* modeAttribute = "mode";
constexpr const char* slotAttribute = "slot";
constexpr const char* nameAttribute = "name";
constexpr const char* programModeName = "program";
constexpr const char* manualEditModeName = "manualEdit";

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

// A whole number, optionally negative, written as plain digits.
std::optional<int> parseInteger(const juce::String& text)
{
    const auto digits = text.trim().toStdString();
    int value = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);

    if (digits.empty() || error != std::errc{} || end != digits.data() + digits.size())
        return std::nullopt;

    return value;
}

// "1-3" → bank 1, program 3.
std::optional<ProgramLocation> parseSlot(const juce::String& text)
{
    const auto bank = parseInteger(text.upToFirstOccurrenceOf("-", false, false));
    const auto program = parseInteger(text.fromFirstOccurrenceOf("-", false, false));
    if (!bank.has_value() || !program.has_value()) // without a dash, the program part is empty
        return std::nullopt;

    const ProgramLocation location{*bank, *program};
    return location.isValid() ? std::optional{location} : std::nullopt;
}

juce::String slotText(ProgramLocation location)
{
    return juce::String(location.bank) + "-" + juce::String(location.program);
}

const juce::XmlElement* findChild(const juce::XmlElement* parent, const char* tag, const juce::String& id)
{
    if (parent == nullptr)
        return nullptr;

    for (const auto* child : parent->getChildWithTagNameIterator(tag))
        if (child->getStringAttribute(idAttribute) == id)
            return child;

    return nullptr;
}

const juce::XmlElement* findParameter(const juce::XmlElement* parameters, const juce::String& id)
{
    return findChild(parameters, parameterTag, id);
}

// Each schema change adds a step here that rewrites an older document into the next version's
// shape.
//   1 → 2: adds <Programs>. A document without it already reads as the factory programs with 1-1
//          selected, so nothing needs rewriting.
void migrateToCurrentSchema(juce::XmlElement& /*xml*/, int /*fromVersion*/) {}

void writeProgram(juce::XmlElement& parent, ProgramLocation location, const Program& program)
{
    auto* element = parent.createNewChildElement(programTag);
    element->setAttribute(slotAttribute, slotText(location));
    element->setAttribute(nameAttribute, juce::String{std::string{program.name.view()}});

    for (std::size_t block = 0; block < numEffectBlocks; ++block)
    {
        auto* value = element->createNewChildElement(programValueTag);
        value->setAttribute(idAttribute, ParameterIds::effectEnabled[block]);
        value->setAttribute(valueAttribute, program.effectEnabled[block] ? 1 : 0);
    }
    for (int index = 0; index < numProgramControls; ++index)
    {
        const auto control = static_cast<ProgramControl>(index);
        auto* value = element->createNewChildElement(programValueTag);
        value->setAttribute(idAttribute, parameterIdFor(control));
        value->setAttribute(valueAttribute, controlValue(program, control));
    }
}

// Starts from a default program, so a value that is missing or unreadable takes the control's
// default, as a missing parameter does.
Program readProgram(const juce::XmlElement& element)
{
    Program program;
    program.name = ProgramName::from(element.getStringAttribute(nameAttribute).toStdString());

    for (std::size_t block = 0; block < numEffectBlocks; ++block)
        if (const auto* value = findChild(&element, programValueTag, ParameterIds::effectEnabled[block]))
            if (const auto number = parseInteger(value->getStringAttribute(valueAttribute)))
                program.effectEnabled[block] = *number != 0;

    for (int index = 0; index < numProgramControls; ++index)
    {
        const auto control = static_cast<ProgramControl>(index);
        if (const auto* value = findChild(&element, programValueTag, parameterIdFor(control)))
            if (const auto number = parseInteger(value->getStringAttribute(valueAttribute)))
            {
                const auto range = controlRange(control);
                controlValue(program, control) = std::clamp(*number, range.minimum, range.maximum);
            }
    }
    return program;
}

ProgramState readProgramState(const juce::XmlElement* element)
{
    ProgramState programs;
    if (element == nullptr)
        return programs;

    for (const auto* child : element->getChildWithTagNameIterator(programTag))
    {
        const auto location = parseSlot(child->getStringAttribute(slotAttribute));
        if (location.has_value() && location->bank == userBank)
            programs.bank.write(*location, readProgram(*child));
    }

    const auto bank = parseInteger(element->getStringAttribute(bankAttribute));
    const auto program = parseInteger(element->getStringAttribute(programAttribute));
    programs.selection.select(ProgramLocation{bank.value_or(userBank), program.value_or(1)}.clamped());

    programs.mode = element->getStringAttribute(modeAttribute) == manualEditModeName ? ProgramMode::ManualEdit
                                                                                     : ProgramMode::Program;
    return programs;
}

} // namespace

std::unique_ptr<juce::XmlElement> toXml(const juce::AudioProcessor& processor, const ProgramState& programs)
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

    auto* programList = xml->createNewChildElement(programsTag);
    const auto selected = programs.selection.selected();
    programList->setAttribute(bankAttribute, selected.bank);
    programList->setAttribute(programAttribute, selected.program);
    programList->setAttribute(modeAttribute,
                              programs.mode == ProgramMode::ManualEdit ? manualEditModeName : programModeName);
    for (int program = 1; program <= programsPerBank; ++program)
        writeProgram(*programList, {userBank, program}, programs.bank.at({userBank, program}));

    return xml;
}

LoadResult fromXml(const juce::XmlElement& source, juce::AudioProcessor& processor, ProgramState& programs)
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

    programs = readProgramState(xml.getChildByName(programsTag));

    return {true, schemaVersion};
}

} // namespace fivea::state
