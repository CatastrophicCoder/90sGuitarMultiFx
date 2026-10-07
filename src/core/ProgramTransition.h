#pragma once

#include "core/AudioBufferView.h"

#include <atomic>
#include <cstdint>

namespace fivea
{

// A program change dips the output (owner's decision 2026-10-07; PLACEHOLDER, evidence register
// EV-122): fade out on the old program's settings, switch to the new ones and reset the effects
// while silent (so tails are cut), fade in.
//
// The caller applies the fade-out to the effects' output, so the old program is heard fading, and
// the fade-in to their input: blocks restarted from silence then see a signal that starts smoothly,
// where a cut-in signal would leave a step in every filter and delay line (heard after the fade,
// once a delay plays it back).
//
// The new settings reach the audio thread as host parameters, set one at a time on the message
// thread, so the audio thread must neither hear them during the fade-out nor switch before all of
// them are set. The handshake:
//
//   message thread   ticket = post();  set every parameter;  complete(ticket);
//   audio thread     poll() sees the request: hold the old settings and fade out;
//                    once silent, readyToSwitch() waits for complete(); switch; switched(); fade in
//
// With fadeAudio false (global bypass on: the output is the dry signal and nothing is heard of the
// program), the old settings are held at full gain until the switch, and nothing is reset.
class ProgramTransition
{
public:
    // Message thread.
    [[nodiscard]] std::uint32_t post() noexcept { return requested.fetch_add(1, std::memory_order_acq_rel) + 1; }
    void complete(std::uint32_t ticket) noexcept { completed.store(ticket, std::memory_order_release); }

    // Audio thread (or before processing starts).
    void prepare(double sampleRate, double fadeOutSeconds, double fadeInSeconds) noexcept;
    void reset() noexcept; // no transition; requests posted so far count as handled

    // Notices a new request; returns true if there was one. Starts or continues the fade-out (from
    // the present gain, if it was fading in).
    bool poll(bool fadeAudio) noexcept;

    // While true, the caller keeps processing with the settings it had when the request arrived.
    [[nodiscard]] bool holdsSettings() const noexcept { return phase == Phase::FadingOut || isWaiting(); }
    [[nodiscard]] bool isFadingOut() const noexcept { return phase == Phase::FadingOut; }
    [[nodiscard]] bool isSilent() const noexcept { return phase == Phase::Silent; }
    [[nodiscard]] bool isFadingIn() const noexcept { return phase == Phase::FadingIn; }
    [[nodiscard]] bool isIdle() const noexcept { return phase == Phase::Idle; }

    // Samples left in the fade-out, the last of them at gain 0. A caller splits its block here, so
    // the switch happens at the first silent sample instead of a block later.
    [[nodiscard]] int samplesUntilSilent() const noexcept { return phase == Phase::FadingOut ? position : 0; }

    // Silent (or holding, without a fade), and the newest request's settings are all in place.
    [[nodiscard]] bool readyToSwitch() const noexcept
    {
        return isWaiting() && completed.load(std::memory_order_acquire) == handled;
    }

    // The caller has switched to the new settings: fade in (or, without a fade, carry on).
    void switched() noexcept;

    // Multiplies the buffer by the transition's gain, advancing it. Bit-transparent when idle.
    void applyGain(AudioBufferView buffer) noexcept;

private:
    enum class Phase
    {
        Idle,
        FadingOut,
        Silent,
        Holding, // no fade: old settings at full gain until the switch
        FadingIn
    };

    [[nodiscard]] bool isWaiting() const noexcept { return phase == Phase::Silent || phase == Phase::Holding; }

    std::atomic<std::uint32_t> requested{0};
    std::atomic<std::uint32_t> completed{0};
    std::uint32_t handled = 0;

    Phase phase = Phase::Idle;
    int fadeOutLength = 0;
    int fadeInLength = 0;
    int position = 0; // fading out: samples left; fading in: samples done
};

} // namespace fivea
