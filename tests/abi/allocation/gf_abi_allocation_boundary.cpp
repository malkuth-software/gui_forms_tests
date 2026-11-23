// Memory across the C ABI is freed by the side that allocated it (owner,
// 2026-11-22; gui_forms include/gui_forms/allocation/library_memory): the
// library's objects are handles its own create and release calls make and
// free, text comes back in the caller's buffer, and a callback's context,
// allocated by the program, goes back to the program's release exactly once.
// The program links only the ABI library, as a C++ consumer of the SDK
// would, with an operator new of its own that counts and tracks its blocks:
// - the library never calls it (the library's objects come from its own
//   allocator, STXlib's, and its standard library is its own: libc++ is
//   linked statically into the library and kept local), unless the build
//   shares the system's C++ runtime (GF_ALLOCATION_SHARED_CXX_RUNTIME);
// - the program's delete only ever sees blocks its new made.

#include "gui_forms/c/gf_abi.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>

#if !defined(GF_ALLOCATION_SHARED_CXX_RUNTIME)
#define GF_ALLOCATION_SHARED_CXX_RUNTIME 0
#endif

// The program's allocator: counts calls and keeps a small table of its live
// blocks, so a block it did not make reaching its delete is caught.
namespace program_allocations {
std::uint64_t calls = 0U;
std::uint64_t foreign_deletes = 0U;
std::array<void*, 512> live{};

void remember(void* const block) noexcept {
    for (std::size_t index = 0U; index < live.size(); ++index) {
        if (live[index] == nullptr) {
            live[index] = block;
            return;
        }
    }
}

// Whether `block` was one of the program's, forgetting it. A block made
// while the table was full is taken as the program's.
bool forget(void* const block) noexcept {
    bool full = true;
    for (std::size_t index = 0U; index < live.size(); ++index) {
        if (live[index] == block) {
            live[index] = nullptr;
            return true;
        }
        if (live[index] == nullptr) full = false;
    }
    return full;
}

void* allocate(const std::size_t size) {
    calls = calls + 1U;
    void* const block = std::malloc(size == 0U ? 1U : size);
    if (block == nullptr) throw std::bad_alloc();
    remember(block);
    return block;
}

void release(void* const block) noexcept {
    if (block == nullptr) return;
    if (!forget(block)) {
        foreign_deletes = foreign_deletes + 1U;
        return;
    }
    std::free(block);
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
void operator delete(void* block) noexcept { program_allocations::release(block); }
void operator delete[](void* block) noexcept { program_allocations::release(block); }
void operator delete(void* block, std::size_t) noexcept { program_allocations::release(block); }
void operator delete[](void* block, std::size_t) noexcept { program_allocations::release(block); }

namespace {

using gui_forms::abi::EventsTable;
using gui_forms::abi::ObjectReference;
using gui_forms::abi::ObjectsTable;
using gui_forms::abi::PropertiesTable;
using gui_forms::abi::SubscriptionReference;

int failures = 0;

void require(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        failures = failures + 1;
    }
}

gf_utf8_view text_view(const std::string_view text) {
    gf_utf8_view view{};
    view.data = text.data();
    view.size = text.size();
    return view;
}

// A callback context the program allocates and its release frees.
struct ProgramContext final {
    std::uint32_t invocations{};
    std::uint32_t* releases{};
};

void context_invoke(void* context, gf_event* event) {
    static_cast<void>(event);
    ProgramContext* const owned = static_cast<ProgramContext*>(context);
    (*owned).invocations = (*owned).invocations + 1U;
}

void context_release(void* context) {
    ProgramContext* const owned = static_cast<ProgramContext*>(context);
    std::uint32_t* const releases = (*owned).releases;
    delete owned;
    *releases = *releases + 1U;
}

gf_status set_checked(const PropertiesTable& properties, const gf_object check_box, const bool checked) {
    gf_value value{};
    value.struct_size = static_cast<std::uint32_t>(sizeof(value));
    value.kind = GF_VALUE_KIND_BOOLEAN;
    value.boolean = checked ? 1U : 0U;
    const gf_status set = properties.set(check_box, text_view("checked"), &value);
    return set;
}

// Objects made and released by the library, their text read into the
// program's buffer: the program's allocator is never called.
void handles_round_trip(const ObjectsTable& objects) {
    const std::uint64_t calls_before = program_allocations::calls;
    for (std::uint32_t round = 0U; round < 64U; ++round) {
        gf_object made = 0U;
        const gf_status created = objects.create(text_view("label"), text_view("allocation.label"), &made);
        require(created == GF_STATUS_OK && made != 0U, "the library creates a label");
        std::array<char, 64> buffer{};
        std::uint64_t required = 0U;
        const gf_status read = objects.type_id(made, buffer.data(), buffer.size(), &required);
        const std::string_view type(buffer.data(), static_cast<std::size_t>(required));
        require(read == GF_STATUS_OK && type == "label", "the type id comes back in the program's buffer");
        require(objects.release(made) == GF_STATUS_OK, "the library releases its object");
        gf_object_state state = 0U;
        require(objects.state(made, &state) == GF_STATUS_STALE_HANDLE, "the released handle is stale");
    }
    if (!GF_ALLOCATION_SHARED_CXX_RUNTIME) {
        require(program_allocations::calls == calls_before,
                "the library's objects never reach the program's operator new");
    }
}

// A context the program allocates is released by the program, once.
void contexts_go_back(const ObjectsTable& objects, const EventsTable& events, const PropertiesTable& properties) {
    gf_object made = 0U;
    require(objects.create(text_view("check_box"), text_view("allocation.check"), &made) == GF_STATUS_OK,
            "the library creates a check box");
    const ObjectReference check_box(objects, made);
    std::uint32_t releases = 0U;
    ProgramContext* const context = new ProgramContext();
    (*context).releases = &releases;
    gf_callback callback{};
    callback.struct_size = static_cast<std::uint32_t>(sizeof(callback));
    callback.invoke = &context_invoke;
    callback.release = &context_release;
    callback.context = context;
    gf_subscription subscribed = 0U;
    require(events.subscribe(check_box.get(), text_view("check_state_changed"), &callback, &subscribed) ==
                GF_STATUS_OK,
            "subscribe with a program-allocated context");
    {
        const SubscriptionReference subscription(events, subscribed);
        require(set_checked(properties, check_box.get(), true) == GF_STATUS_OK, "check the box");
    }
    require(releases == 1U, "the library hands the context back to the program's release once");
}

} // namespace

int main() {
    ObjectsTable objects;
    EventsTable events;
    PropertiesTable properties;
    if (ObjectsTable::negotiate(objects) != GF_STATUS_OK || EventsTable::negotiate(events) != GF_STATUS_OK ||
        PropertiesTable::negotiate(properties) != GF_STATUS_OK) {
        std::cerr << "FAIL: the tables did not negotiate\n";
        return 1;
    }
    handles_round_trip(objects);
    contexts_go_back(objects, events, properties);
    require(program_allocations::foreign_deletes == 0U, "the program's delete never saw a block it did not make");
    if (failures != 0) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "gui_forms_abi_allocation_boundary: all checks passed\n";
    return 0;
}
