// The X11 host's compositor frame synchronisation
// (src/host/linux/frame_sync/x11_frame_sync.hpp): the values the sync
// counters take around each frame and for a resize's requests, and the
// display timing a _NET_WM_FRAME_TIMINGS message gives. Plain numbers in,
// no Xlib, so it is tested on every platform.
#include "src/host/linux/frame_sync/x11_frame_sync.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using gui_forms::DisplayTiming;
using gui_forms::DisplayTimingSource;
using gui_forms::FrameInterval;
using gui_forms::FrameTime;
using gui_forms::host::linux_detail::X11FrameSyncCounter;
using gui_forms::host::linux_detail::X11FrameTimings;
using gui_forms::host::linux_detail::x11_frame_sync_value;
using gui_forms::host::linux_detail::x11_frame_timings_display_timing;
using gui_forms::host::linux_detail::x11_monotonic_to_frame_time;
using namespace std::chrono_literals;

// Throws on failure, so the check runs in every build type.
void require(const bool condition, const char* const message) {
  if (!condition) throw std::runtime_error(message);
}

// Two 32-bit halves make one value; a negative long's upper bits are not
// part of its half.
void test_values_from_halves() {
  require(x11_frame_sync_value(5L, 0L) == 5U, "a low half alone");
  require(x11_frame_sync_value(1L, 2L) == ((std::uint64_t{2} << 32U) | 1U), "the high half above the low");
  require(x11_frame_sync_value(-1L, 0L) == 0xffffffffULL, "a sign-extended low half keeps 32 bits");
}

// Each frame takes the next odd value and then the next even one; an
// extended request moves the frames on to its value (an odd one rounded up
// to even first); a basic request is answered once.
void test_counter_frames_and_requests() {
  X11FrameSyncCounter counter;
  require(counter.end_frame().absent(), "no frame ends before one begins");
  const stxlib::Optional<std::uint64_t> first = counter.begin_frame();
  require(first.present() && *first.value() == 1U && counter.in_frame(), "the first frame begins odd at 1");
  require(counter.begin_frame().absent(), "a frame does not begin twice");
  const stxlib::Optional<std::uint64_t> done = counter.end_frame();
  require(done.present() && *done.value() == 2U && !counter.in_frame(), "and ends even at 2");
  counter.take_sync_request(41U, true);
  require(*counter.begin_frame().value() == 43U && *counter.end_frame().value() == 44U,
          "an odd extended request is rounded up to even, then the frame takes the next values");
  counter.take_sync_request(100U, true);
  require(*counter.begin_frame().value() == 101U && *counter.end_frame().value() == 102U,
          "an even extended request is where the frames continue");
  require(counter.take_basic_answer().absent(), "no basic answer without a basic request");
  counter.take_sync_request(7U, false);
  require(*counter.begin_frame().value() == 103U, "a basic request leaves the extended values");
  static_cast<void>(counter.end_frame());
  const stxlib::Optional<std::uint64_t> answer = counter.take_basic_answer();
  require(answer.present() && *answer.value() == 7U && counter.take_basic_answer().absent(),
          "a basic request is answered once with its value");
}

// Monotonic times carried by their age; a timings message gives display
// timing phased at its presentation, nominal timing at the drawn time when
// the presentation is unknown, and nothing without a usable interval.
void test_frame_timings() {
  const FrameTime steady_now{10s};
  const std::uint64_t now_microseconds = 5'000'000U;
  require(x11_monotonic_to_frame_time(4'990'000U, now_microseconds, steady_now) == steady_now - 10ms,
          "a stamp ten milliseconds old is ten milliseconds before now");
  X11FrameTimings timings;
  timings.drawn_microseconds = 4'980'000U;
  timings.presentation_offset_microseconds = 4'000;
  timings.refresh_interval_microseconds = 16'667U;
  const stxlib::Optional<DisplayTiming> shown = x11_frame_timings_display_timing(timings, now_microseconds, steady_now);
  require(shown.present() && (*shown.value()).source == DisplayTimingSource::display &&
              (*shown.value()).refresh_period == 16667us &&
              (*shown.value()).presentation_phase == steady_now - 20ms + 4ms,
          "the refresh interval phased at the drawn time plus the offset");
  timings.presentation_offset_microseconds = 0;
  const stxlib::Optional<DisplayTiming> drawn = x11_frame_timings_display_timing(timings, now_microseconds, steady_now);
  require(drawn.present() && (*drawn.value()).source == DisplayTimingSource::nominal &&
              (*drawn.value()).presentation_phase == steady_now - 20ms,
          "an unknown presentation gives nominal timing at the drawn time");
  timings.refresh_interval_microseconds = 0U;
  require(x11_frame_timings_display_timing(timings, now_microseconds, steady_now).absent(),
          "an unknown interval gives nothing");
  timings.refresh_interval_microseconds = 500U;
  require(x11_frame_timings_display_timing(timings, now_microseconds, steady_now).absent(),
          "an interval below the bound gives nothing");
}

} // namespace

int main() {
  try {
    test_values_from_halves();
    test_counter_frames_and_requests();
    test_frame_timings();
    std::cout << "gui_forms_x11_frame_sync_tests: all tests passed\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "gui_forms_x11_frame_sync_tests: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
