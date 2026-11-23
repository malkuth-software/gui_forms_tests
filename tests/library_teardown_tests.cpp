// GUI.Forms' teardown reserve (gui_forms include/gui_forms/allocation/
// library_memory/library_memory.hpp, "A module's end"): before the module's
// teardown a refused library allocation is null; after it the reserve
// serves it, aligned as asked, a release of the reserve's block does
// nothing, served blocks stay STXlib's, and an allocation the reserve
// cannot hold ends the process through stxlib::fail_fast with the record
// gf.allocation.teardown_reserve, never std::bad_alloc (checked in a child
// process where the host can fork one).
//
// The test observer stands in for STXlib's allocator refusing, as it does
// at Windows' process end.

#include "gui_forms/allocation/library_memory/library_memory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#define GF_TEARDOWN_CAN_FORK 1
#else
#define GF_TEARDOWN_CAN_FORK 0
#endif

namespace {

int failures = 0;

void require(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        failures = failures + 1;
    }
}

bool refuse_every_allocation(const std::size_t size) noexcept {
    static_cast<void>(size);
    return false;
}

[[nodiscard]] bool aligned_to(const void* const block, const std::size_t alignment) {
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(block);
    const bool aligned = address % static_cast<std::uintptr_t>(alignment) == 0U;
    return aligned;
}

// Before the teardown a refusal is the caller's exhaustion.
void refused_before_teardown() {
    require(!gui_forms::detail::library_teardown_begun(), "no teardown before it is begun");
    gui_forms::detail::set_library_allocation_observer(&refuse_every_allocation);
    void* const refused = gui_forms::detail::library_allocate(64U, 16U);
    gui_forms::detail::set_library_allocation_observer(nullptr);
    require(refused == nullptr, "a refused allocation is null before the teardown");
    require(gui_forms::detail::teardown_reserve_used() == 0U, "the reserve is untouched before the teardown");
}

#if GF_TEARDOWN_CAN_FORK
// A child begins the teardown and asks for more than the reserve holds: it
// must end abnormally with the record on standard error.
void exhausted_reserve_fails_fast() {
    std::array<int, 2> pipe_ends{-1, -1};
    if (::pipe(pipe_ends.data()) != 0) {
        require(false, "a pipe for the child's report");
        return;
    }
    const pid_t child = ::fork();
    if (child == 0) {
        static_cast<void>(::dup2(pipe_ends[1], 2));
        static_cast<void>(::close(pipe_ends[0]));
        gui_forms::detail::begin_library_teardown();
        gui_forms::detail::set_library_allocation_observer(&refuse_every_allocation);
        const std::size_t beyond = gui_forms::detail::teardown_reserve_capacity() + 1U;
        void* const block = gui_forms::detail::library_allocate(beyond, 16U);
        static_cast<void>(block);
        // Reached only if the reserve did not fail fast.
        ::_exit(0);
    }
    static_cast<void>(::close(pipe_ends[1]));
    std::string report;
    std::array<char, 256> buffer{};
    while (true) {
        const ssize_t got = ::read(pipe_ends[0], buffer.data(), buffer.size());
        if (got <= 0) break;
        report.append(buffer.data(), static_cast<std::size_t>(got));
    }
    static_cast<void>(::close(pipe_ends[0]));
    int status = 0;
    const pid_t waited = ::waitpid(child, &status, 0);
    require(waited == child, "the child is reaped");
    const bool clean_exit = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    require(!clean_exit, "an allocation the reserve cannot hold ends the process");
    require(report.find("gf.allocation.teardown_reserve") != std::string::npos,
            "the fail-fast report names gf.allocation.teardown_reserve");
}
#endif

// After the teardown began: the reserve serves refusals, aligned as asked;
// releasing its blocks does nothing; served blocks are STXlib's.
void reserve_after_teardown() {
    gui_forms::detail::begin_library_teardown();
    require(gui_forms::detail::library_teardown_begun(), "the teardown is marked");

    const std::uint64_t granted_before = gui_forms::detail::library_allocations_granted();
    const std::uint64_t released_before = gui_forms::detail::library_blocks_released();
    gui_forms::detail::set_library_allocation_observer(&refuse_every_allocation);
    void* const small = gui_forms::detail::library_allocate(40U, 16U);
    void* const wide = gui_forms::detail::library_allocate(100U, 256U);
    void* const empty = gui_forms::detail::library_allocate(0U, 16U);
    void* thrown_free = nullptr;
    bool threw = false;
    try {
        thrown_free = gui_forms::detail::library_allocate_or_throw(32U, 8U);
    } catch (...) {
        threw = true;
    }
    gui_forms::detail::set_library_allocation_observer(nullptr);

    require(small != nullptr && gui_forms::detail::teardown_reserve_owns(small), "the reserve serves a refusal");
    require(wide != nullptr && gui_forms::detail::teardown_reserve_owns(wide) && aligned_to(wide, 256U),
            "a reserve block has the alignment asked for");
    require(empty != nullptr && empty != small && empty != wide, "a zero-size reserve block is unique");
    require(!threw && gui_forms::detail::teardown_reserve_owns(thrown_free),
            "the throwing route never throws after the teardown began");
    require(gui_forms::detail::teardown_reserve_used() >= 40U + 100U + 1U + 32U, "the reserve counts its bytes");
    require(gui_forms::detail::teardown_reserve_used() <= gui_forms::detail::teardown_reserve_capacity(),
            "the reserve never hands out more than it holds");
    require(gui_forms::detail::library_allocations_granted() == granted_before + 4U,
            "reserve blocks count as granted");

    const std::size_t used = gui_forms::detail::teardown_reserve_used();
    gui_forms::detail::library_release(small);
    gui_forms::detail::library_release(wide);
    gui_forms::detail::library_release(empty);
    gui_forms::detail::library_release(thrown_free);
    require(gui_forms::detail::library_blocks_released() == released_before + 4U,
            "releasing reserve blocks counts and returns");
    require(gui_forms::detail::teardown_reserve_used() == used, "reserve blocks are never reused");

    int on_stack = 0;
    require(!gui_forms::detail::teardown_reserve_owns(nullptr), "null is not the reserve's");
    require(!gui_forms::detail::teardown_reserve_owns(&on_stack), "a stack address is not the reserve's");

    void* const served = gui_forms::detail::library_allocate(64U, 16U);
    require(served != nullptr && !gui_forms::detail::teardown_reserve_owns(served),
            "an allocation STXlib's allocator serves stays its own after the teardown began");
    gui_forms::detail::library_release(served);
}

} // namespace

int main() {
    refused_before_teardown();
#if GF_TEARDOWN_CAN_FORK
    exhausted_reserve_fails_fast();
#else
    std::cout << "library_teardown_tests: no fork on this host; the fail-fast child is left out\n";
#endif
    reserve_after_teardown();
    if (failures != 0) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "library_teardown_tests: all checks passed\n";
    return 0;
}
