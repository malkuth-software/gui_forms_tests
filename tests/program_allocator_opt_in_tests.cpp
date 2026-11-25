// The opt-in whole-program allocator (gui_forms
// src/runtime/program_allocator/program_allocator_replacement.cpp, which
// this program compiles in as gui_forms_use_library_allocator does): every
// form of the program's operator new takes its block from STXlib's
// allocator and every delete releases it there; a block STXlib's allocator
// serves crosses to delete and back; exhaustion is std::bad_alloc or null;
// a sanitized build leaves the replacement out and keeps the host's. After
// the module's teardown began, delete leaves a teardown reserve block where
// it is and new still takes STXlib's blocks while they are served.

#include "gui_forms/allocation/library_memory/library_memory.hpp"
#include "gui_forms/allocation/library_memory/library_teardown.hpp"
#include "gui_forms/allocation/library_memory/program_allocator.hpp"

#include <stxlib/memory.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void require(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        failures = failures + 1;
    }
}

// As the replacement decides.
constexpr bool sanitized() noexcept {
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__) || defined(__SANITIZE_HWADDRESS__)
    return true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(memory_sanitizer) || \
    __has_feature(hwaddress_sanitizer)
    return true;
#else
    return false;
#endif
#else
    return false;
#endif
}

// Counts new-handler calls; uninstalls itself so the retry gives up.
int handler_calls = 0;
void counting_handler() {
    handler_calls = handler_calls + 1;
    static_cast<void>(std::set_new_handler(nullptr));
}

void every_form_is_stxlibs() {
    // A block from anywhere but STXlib's allocator would end the process at
    // each release below.
    void* const plain = ::operator new(40U);
    stxlib::memory::release(plain);
    void* const array = ::operator new[](40U);
    stxlib::memory::release(array);
    void* const quiet = ::operator new(40U, std::nothrow);
    require(quiet != nullptr, "the nothrow form serves a block");
    stxlib::memory::release(quiet);
    void* const aligned = ::operator new(40U, std::align_val_t(256U));
    require(reinterpret_cast<std::uintptr_t>(aligned) % 256U == 0U, "the aligned form honours its alignment");
    stxlib::memory::release(aligned);

    // And the other way: STXlib's blocks go back through delete.
    void* const from_stxlib = stxlib::memory::allocate(24U, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    ::operator delete(from_stxlib);
    void* const sized = stxlib::memory::allocate(24U, 64U);
#if defined(__cpp_sized_deallocation)
    ::operator delete(sized, std::size_t{24U}, std::align_val_t(64U));
#else
    // No sized deallocation in this compiler's mode (clang for windows-gnu
    // leaves it off): the unsized aligned form.
    ::operator delete(sized, std::align_val_t(64U));
#endif

    // The program's containers and strings ride on it.
    std::vector<std::uint64_t> numbers(64U, 5U);
    std::string text(200U, 'x');
    const std::shared_ptr<int> shared = std::make_shared<int>(9);
    require(numbers[63] == 5U && text.size() == 200U && *shared == 9, "containers work on STXlib's allocator");
}

void exhaustion() {
    const std::size_t huge = static_cast<std::size_t>(-1) / 2U + 1U;
    bool thrown = false;
    try {
        void* const block = ::operator new(huge);
        ::operator delete(block);
    } catch (const std::bad_alloc&) {
        thrown = true;
    }
    require(thrown, "a refused block is std::bad_alloc");
    require(::operator new(huge, std::nothrow) == nullptr, "the nothrow form answers null");
    handler_calls = 0;
    static_cast<void>(std::set_new_handler(&counting_handler));
    thrown = false;
    try {
        void* const block = ::operator new(huge);
        ::operator delete(block);
    } catch (const std::bad_alloc&) {
        thrown = true;
    }
    require(thrown && handler_calls == 1, "the new-handler runs once before std::bad_alloc");
}

// Last, since a teardown is never undone: a delete of a reserve block does
// nothing (STXlib's release would end the process on it), and an
// allocation STXlib's allocator serves is still its own.
void teardown_reserve_blocks() {
    gui_forms::detail::begin_library_teardown();
    void* const reserved = gui_forms::detail::teardown_allocate(24U, 16U);
    require(gui_forms::detail::teardown_reserve_owns(reserved), "the teardown reserve serves the block");
    ::operator delete(reserved);
    ::operator delete[](reserved);
    ::operator delete(reserved, std::align_val_t{16U});
    void* const served = ::operator new(48U);
    require(!gui_forms::detail::teardown_reserve_owns(served), "a served block is STXlib's after teardown began");
    stxlib::memory::release(served);
}

} // namespace

int main() {
    if (sanitized()) {
        require(stxlib::memory::backend() == stxlib::memory::Backend::host,
                "a sanitized build keeps the host allocator, and the replacement is left out");
        std::cout << "program_allocator_opt_in_tests: sanitized, replacement left out\n";
        return failures == 0 ? 0 : 1;
    }
    every_form_is_stxlibs();
    exhaustion();
    teardown_reserve_blocks();
    if (failures != 0) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "program_allocator_opt_in_tests: all checks passed\n";
    return 0;
}
