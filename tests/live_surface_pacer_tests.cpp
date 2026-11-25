// LiveSurfacePacer and presentation feedback (stage S2, threaded
// sub-surfaces design note sections 7 and 8).

#include "gui_forms/live_surface.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <stxlib/stop_token.hpp>
#include <thread>

namespace {

using namespace gui_forms;

// Throws on failure, so the check runs in every build type (Release defines
// NDEBUG); an uncaught throw fails the test.
void require(const bool condition, const char* const message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t publish_one(LiveSurface& surface) {
    LiveSurfaceWriteLease lease = surface.try_acquire_write();
    require(static_cast<bool>(lease), "a write lease");
    const std::uint64_t generation = lease.publish();
    return generation;
}

constexpr FrameInterval refresh = std::chrono::duration_cast<FrameInterval>(std::chrono::microseconds(16'667));

LiveDisplayTiming phased_at(FrameTime vblank) {
    LiveDisplayTiming timing;
    timing.refresh_period = refresh;
    timing.last_vblank = vblank;
    timing.source = LiveTimingSource::display_phased;
    return timing;
}

class CountWake final {
public:
    explicit CountWake(std::atomic<int>& count) noexcept : count_(&count) {}
    void operator()() const { ++(*count_); }

private:
    std::atomic<int>* count_;
};

// A presenter on its own thread: after `delay`, reports `generation` shown.
class LatePresenter final {
public:
    LatePresenter(std::shared_ptr<LiveSurface> surface, std::uint64_t generation, std::chrono::milliseconds delay)
        : surface_(std::move(surface)), generation_(generation), delay_(delay) {}
    void operator()() const {
        std::this_thread::sleep_for(delay_);
        (*surface_).notify_presented(generation_, phased_at(FrameClock::now()));
    }

private:
    std::shared_ptr<LiveSurface> surface_;
    std::uint64_t generation_{};
    std::chrono::milliseconds delay_{};
};

// Requests a stop after `delay`.
// A new stop source; failing to make one is a test failure.
stxlib::StopSource made_stop_source() {
    stxlib::OperationResult<stxlib::StopSource> made = stxlib::StopSource::create();
    require(made.succeeded(), "a stop source is made");
    return static_cast<stxlib::OperationResult<stxlib::StopSource>&&>(made).take_value();
}

class LateStop final {
public:
    LateStop(stxlib::StopSource& source, std::chrono::milliseconds delay) : source_(&source), delay_(delay) {}
    void operator()() const {
        std::this_thread::sleep_for(delay_);
        static_cast<void>((*source_).request_stop());
    }

private:
    stxlib::StopSource* source_;
    std::chrono::milliseconds delay_{};
};

void plan_follows_reports_and_deadlines() {
    std::shared_ptr<LiveSurface> surface = LiveSurface::create({8U, 4U});
    require(static_cast<bool>(surface), "surface");
    LiveSurfacePacer pacer(surface);
    const FrameTime start = FrameClock::now();
    LivePacingResult first = pacer.plan(start);
    require(first.outcome == LivePacingOutcome::frame, "the first frame is never held");
    require(first.frame_time == start, "without display timing the frame time is now");

    const std::uint64_t generation = publish_one(*surface);
    LivePacingResult held = pacer.plan(start);
    require(held.outcome == LivePacingOutcome::pending, "an unshown frame holds the next");
    // The deadline is one and a half of the pacer's 60 Hz fallback period;
    // refresh here is that period rounded to the microsecond, so the two
    // agree within one microsecond (B-gf-tests-07-F2).
    const FrameTime expected_deadline = start + refresh + refresh / 2;
    const FrameInterval tolerance = std::chrono::duration_cast<FrameInterval>(std::chrono::microseconds(1));
    require(held.wake_at >= expected_deadline - tolerance && held.wake_at <= expected_deadline + tolerance,
            "the hold names its deadline, one and a half refreshes on");
    LivePacingResult still = pacer.plan(start + refresh);
    require(still.outcome == LivePacingOutcome::pending, "still held within one and a half refreshes");
    LivePacingResult late = pacer.plan(start + refresh * 5);
    require(late.outcome == LivePacingOutcome::deadline, "a stalled presenter releases at the deadline");
    require(late.missed_frames == 4U, "the refreshes missed past the first");

    // Shown: the report releases the next frame, phased to the display.
    const std::uint64_t second = publish_one(*surface);
    require(pacer.plan(start + refresh * 5).outcome == LivePacingOutcome::pending, "held again");
    const FrameTime vblank = start + refresh * 5;
    (*surface).notify_presented(second, phased_at(vblank));
    const LivePresentationRecord record = (*surface).presentation_record();
    require(record.presented_generation == second && record.presents == 1U, "the record keeps the report");
    (*surface).notify_presented(generation, phased_at(vblank));
    require((*surface).presentation_record().presented_generation == second, "presented generations only grow");
    LivePacingResult shown = pacer.plan(vblank + refresh / 4);
    require(shown.outcome == LivePacingOutcome::frame, "a shown frame releases the next");
    require(shown.frame_time == vblank + refresh, "the frame time is the next vertical blank");
    require(shown.presented_generation == second, "the result names the shown generation");

    // Nothing published since: the next tick releases a frame.
    require(pacer.plan(vblank + refresh / 2).outcome == LivePacingOutcome::pending,
            "no publication waits for a tick");
    (*surface).notify_presented(second, phased_at(vblank + refresh));
    require(pacer.plan(vblank + refresh).outcome == LivePacingOutcome::frame, "a tick releases it");

    pacer.idle();
    const LivePacingResult idle = pacer.plan(vblank + refresh * 9);
    require(idle.outcome == LivePacingOutcome::pending && idle.wake_at == FrameTime::max(),
            "an idle pacer waits without a deadline");
    pacer.arm();
    require(!pacer.is_idle(), "arm ends idle");
}

void waits_wake_on_reports_and_stops() {
    std::shared_ptr<LiveSurface> surface = LiveSurface::create({8U, 4U});
    LiveSurfacePacer pacer(surface);
    stxlib::StopSource source = made_stop_source();
    require(pacer.wait_for_frame(source.token()).outcome == LivePacingOutcome::frame, "first frame at once");
    const std::uint64_t generation = publish_one(*surface);
    std::thread presenter(LatePresenter(surface, generation, std::chrono::milliseconds(5)));
    const LivePacingResult shown = pacer.wait_for_frame(source.token());
    presenter.join();
    require(shown.outcome == LivePacingOutcome::frame || shown.outcome == LivePacingOutcome::deadline,
            "a report or the deadline ends the wait");
    if (shown.outcome == LivePacingOutcome::frame) {
        require(shown.presented_generation == generation, "the report names the shown frame");
    }

    pacer.idle();
    std::thread stopper(LateStop(source, std::chrono::milliseconds(20)));
    const LivePacingResult stopped = pacer.wait_for_frame(source.token());
    stopper.join();
    require(stopped.outcome == LivePacingOutcome::stopped, "the stop token ends an idle wait");
    require(pacer.wait_for_frame(source.token()).outcome == LivePacingOutcome::stopped,
            "a stopped token answers at once");
}

void presented_wakes_connect_and_disconnect() {
    std::shared_ptr<LiveSurface> surface = LiveSurface::create({8U, 4U});
    std::atomic<int> wakes{};
    LiveSurfaceWakeConnection connection = (*surface).connect_presented_wake(CountWake(wakes));
    require(connection.connected(), "connected");
    const std::uint64_t generation = publish_one(*surface);
    require(wakes.load() == 0, "a publication is not a presentation");
    (*surface).notify_presented(generation, phased_at(FrameClock::now()));
    require(wakes.load() == 1, "a report wakes");
    connection.disconnect();
    (*surface).notify_presented(generation, phased_at(FrameClock::now()));
    require(wakes.load() == 1, "a disconnected wake is not called");
}

// The live timing a host reports for its window's display timing: the
// display's own as display_phased, any other paced rate as phased_timer,
// with the latest presentation at or before now as the vertical blank;
// nothing for assumed timing, so the pacer keeps its fallback.
void live_timing_follows_the_window() {
    const FrameTime now{std::chrono::milliseconds(1000)};
    const FrameInterval period = std::chrono::milliseconds(8);
    const LiveDisplayTiming display = live_display_timing(
        DisplayTiming::reported(DisplayTimingSource::display, period, FrameTime{std::chrono::milliseconds(3)}), now);
    require(display.source == LiveTimingSource::display_phased && display.refresh_period == period &&
                display.last_vblank == FrameTime{std::chrono::milliseconds(995)},
            "display timing is display_phased at the latest refresh at or before now");
    const LiveDisplayTiming on_grid =
        live_display_timing(DisplayTiming::fixed_rate(period, FrameTime{std::chrono::milliseconds(0)}), now);
    require(on_grid.source == LiveTimingSource::phased_timer && on_grid.last_vblank == now,
            "a fixed rate is a phased timer, and a refresh at now is the latest");
    const LiveDisplayTiming ahead = live_display_timing(
        DisplayTiming::reported(DisplayTimingSource::measured, period, FrameTime{std::chrono::milliseconds(1100)}), now);
    require(ahead.source == LiveTimingSource::phased_timer && ahead.last_vblank == FrameTime{std::chrono::milliseconds(996)},
            "a phase ahead of now is carried back onto the grid before now");
    const LiveDisplayTiming assumed = live_display_timing(DisplayTiming::assumed(), now);
    require(assumed.source == LiveTimingSource::none && assumed.refresh_period == FrameInterval::zero(),
            "assumed timing reports nothing");

    // The pacer reads the reported timing: its frame time is the next
    // refresh on the window's grid.
    std::shared_ptr<LiveSurface> surface = LiveSurface::create({8U, 4U});
    LiveSurfacePacer pacer(surface);
    const std::uint64_t generation = publish_one(*surface);
    (*surface).notify_presented(generation, display);
    require(pacer.display_time(now) == FrameTime{std::chrono::milliseconds(1003)},
            "the pacer's frame time is the window's next refresh");
}

} // namespace

int main() {
    plan_follows_reports_and_deadlines();
    waits_wake_on_reports_and_stops();
    presented_wakes_connect_and_disconnect();
    live_timing_follows_the_window();
    return 0;
}
