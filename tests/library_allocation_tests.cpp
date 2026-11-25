// GUI.Forms' own code allocates from STXlib's allocator, the program's own
// allocations stay the program's (owner, 2026-11-22;
// gui_forms/allocation/library_memory):
// - the program's `new` reaches the program's operator new (counted here)
//   and not the library seam;
// - a control the program creates, with `new`, std::make_unique, a subclass
//   of its own or make_control, takes its storage from the library seam, and
//   whichever owner lets it go releases it there;
// - LibraryAllocator and the seam's blocks are STXlib's: stxlib::memory
//   releases them;
// - a refusal is std::bad_alloc and leaves nothing allocated;
// - a sanitized build keeps the host allocator, as STXlib does.

#include "gui_forms/allocation/library_memory/library_memory.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/controls/label.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/allocation/library_memory/library_memory_diagnostics.hpp"

#include <stxlib/memory.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The program's own operator new: counts its calls on this thread and takes
// the C heap, so a library block handed to it would end the process (the
// C heap is not STXlib's allocator in a default build) and one of its
// blocks handed to STXlib would too.
namespace program_allocations {
thread_local std::uint64_t calls = 0U;
// Where a test's block escapes to (program_allocations_stay_the_programs).
int* volatile escaped = nullptr;

void* allocate(const std::size_t size) {
    calls = calls + 1U;
    void* const block = std::malloc(size == 0U ? 1U : size);
    if (block == nullptr) throw std::bad_alloc();
    return block;
}
} // namespace program_allocations

void* operator new(const std::size_t size) {
    void* const block = program_allocations::allocate(size);
    return block;
}
void* operator new[](const std::size_t size) {
    void* const block = program_allocations::allocate(size);
    return block;
}
void operator delete(void* block) noexcept { std::free(block); }
void operator delete[](void* block) noexcept { std::free(block); }
void operator delete(void* block, std::size_t) noexcept { std::free(block); }
void operator delete[](void* block, std::size_t) noexcept { std::free(block); }

namespace {

using gui_forms::Label;
using gui_forms::LibraryAllocator;
using gui_forms::Panel;
using gui_forms::StableId;
using gui_forms::detail::library_allocations_granted;
using gui_forms::detail::library_blocks_released;

int failures = 0;

void require(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        failures = failures + 1;
    }
}

// Whether this program is built with a sanitizer that intercepts the host
// allocator (STXlib then uses the host's C library).
constexpr bool sanitized() noexcept {
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    return true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(memory_sanitizer)
    return true;
#else
    return false;
#endif
#else
    return false;
#endif
}

// A program's own control type: deriving from a GUI.Forms control makes it
// GUI.Forms' to allocate.
class ProgramLabel final : public Label {
public:
    explicit ProgramLabel(StableId stable_id) : Label(std::move(stable_id), "program") {}
    std::uint64_t extra{};
};

// The seam's counts on this thread, before and after a step.
struct LibraryCounts final {
    std::uint64_t granted{};
    std::uint64_t released{};
};

LibraryCounts library_counts() noexcept {
    LibraryCounts counts{};
    counts.granted = library_allocations_granted();
    counts.released = library_blocks_released();
    return counts;
}

void backend_by_build() {
    const stxlib::memory::Backend backend = stxlib::memory::backend();
    if (sanitized()) {
        require(backend == stxlib::memory::Backend::host, "a sanitized build keeps the host allocator");
    } else {
        require(backend == stxlib::memory::Backend::stx_minmallocpp, "the library allocates from stx_minmallocpp");
    }
}

void program_allocations_stay_the_programs() {
    const LibraryCounts before = library_counts();
    const std::uint64_t program_before = program_allocations::calls;
    int* const value = new int(7);
    std::vector<int> numbers(16U, 1);
    // The blocks escape, so the optimiser may not elide the allocations
    // (new and delete are replaceable, and Clang removes an unobserved pair).
    program_allocations::escaped = value;
    program_allocations::escaped = numbers.data();
    require(program_allocations::calls >= program_before + 2U, "the program's new and vector reach its operator new");
    const LibraryCounts after = library_counts();
    require(after.granted == before.granted, "the program's own allocations do not reach the library seam");
    delete value;
}

void controls_are_the_librarys() {
    LibraryCounts before = library_counts();
    Label* const raw = new Label(StableId("allocation.raw"), "raw");
    LibraryCounts after = library_counts();
    require(after.granted >= before.granted + 1U, "new Label takes its storage from the library seam");
    delete raw;
    require(library_blocks_released() >= after.released + 1U, "delete releases it there");

    before = library_counts();
    {
        const std::unique_ptr<Panel> owned = std::make_unique<Panel>(StableId("allocation.unique"));
        require(library_allocations_granted() >= before.granted + 1U, "std::make_unique of a control is the library's");
    }
    require(library_blocks_released() >= before.released + 1U, "std::default_delete releases it to the library");

    before = library_counts();
    {
        const std::unique_ptr<ProgramLabel> derived = std::make_unique<ProgramLabel>(StableId("allocation.derived"));
        (*derived).extra = 3U;
        require(library_allocations_granted() >= before.granted + 1U, "a program's subclass of a control is the library's");
    }
    require(library_blocks_released() >= before.released + 1U, "and is released there through the virtual destructor");

    before = library_counts();
    std::shared_ptr<Label> shared = gui_forms::make_control<Label>(StableId("allocation.shared"), "shared");
    after = library_counts();
    require(after.granted >= before.granted + 1U, "make_control allocates the control and its block from the library");
    // The program's own owner, last to let go, releases to the library.
    std::shared_ptr<Label> program_owner = shared;
    shared.reset();
    program_owner.reset();
    require(library_blocks_released() >= after.released + 1U, "the last shared owner releases to the library");
}

void blocks_are_stxlibs() {
    void* const raw = gui_forms::detail::library_allocate(48U, 16U);
    require(raw != nullptr, "the seam serves a block");
    // A block from anywhere but STXlib's allocator would end the process here.
    stxlib::memory::release(raw);

    LibraryAllocator<std::uint64_t> allocator{};
    std::uint64_t* const values = allocator.allocate(8U);
    require(values != nullptr, "LibraryAllocator serves storage");
    stxlib::memory::release(static_cast<void*>(values));

    const LibraryCounts before = library_counts();
    const std::uint64_t program_before = program_allocations::calls;
    {
        std::vector<std::uint64_t, LibraryAllocator<std::uint64_t>> library_vector{};
        library_vector.reserve(32U);
        library_vector.push_back(1U);
    }
    require(library_allocations_granted() >= before.granted + 1U, "a LibraryAllocator container is the library's");
    require(library_blocks_released() >= before.released + 1U, "and releases there");
    require(program_allocations::calls == program_before, "and never reaches the program's operator new");
}

// Refuses every library allocation while armed.
bool refuse_armed = false;
bool refusing_observer(const std::size_t) noexcept {
    return !refuse_armed;
}

void refusal_is_bad_alloc() {
    gui_forms::detail::set_library_allocation_observer(&refusing_observer);
    refuse_armed = true;
    const LibraryCounts before = library_counts();
    bool refused_new = false;
    try {
        Label* const label = new Label(StableId("allocation.refused"));
        delete label;
    } catch (const std::bad_alloc&) {
        refused_new = true;
    }
    bool refused_shared = false;
    try {
        const std::shared_ptr<Label> label = gui_forms::make_control<Label>(StableId("allocation.refused.shared"));
        require(label == nullptr, "unreachable");
    } catch (const std::bad_alloc&) {
        refused_shared = true;
    }
    refuse_armed = false;
    gui_forms::detail::set_library_allocation_observer(nullptr);
    const LibraryCounts after = library_counts();
    require(refused_new && refused_shared, "a refused library allocation is std::bad_alloc");
    require(after.granted - before.granted == after.released - before.released,
            "a refusal leaves nothing allocated");
}

} // namespace

int main() {
    backend_by_build();
    program_allocations_stay_the_programs();
    controls_are_the_librarys();
    blocks_are_stxlibs();
    refusal_is_bad_alloc();
    if (failures != 0) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "library_allocation_tests: all checks passed\n";
    return 0;
}
