#pragma once
// The block engine of the IC fuzz target (IC3; IC study section 7): what a
// generated block (gf_abi_ic_blocks.hpp, gui_forms tools/ic_abi_fuzz.py)
// needs to call its slot, written small so that phase IC2's engine can
// replace it without touching the table.
//
// A run reads one fuzz input as a program: each step picks a block and
// draws every argument of its slot from the bytes that follow, through
// Arguments. The arguments live in the engine's arena, one 4 KiB slot per
// parameter, so a buffer and the count beside it always agree (extent()
// draws a count below what the slot holds) and nothing the library writes
// can leave the arena. A misuse block breaks one of its clause's conjuncts
// on purpose: Arguments then returns null for the nulled parameter and the
// forced value for its count. Handles the library writes into a gf_object
// output are adopted: the runner keeps them for later draws and releases
// them when the input ends.
//
// Ownership: Arguments borrows the input for one step (begin() to the next
// begin()); the arena and the adopted handles are its own. Nothing here is
// shared between threads; the runner calls everything on the UI thread.

#include "gui_forms/c/gf_abi.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <string_view>

namespace gui_forms_tests::ic {

enum class Mode : std::uint8_t {
    legal,
    misuse,
};

// The state models of the declarations (gui_forms tools/ic_abi_fuzz.py's
// MODELS lists their states in the declarations' order): an object's
// lifecycle (objects.state), a window's (windows.lifecycle), a
// splashscreen's (runtime.splashscreen_state), or none for a static call.
enum class Model : std::uint8_t {
    none,
    object,
    window,
    splashscreen,
};

inline constexpr std::uint32_t no_state = std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t no_parameter = std::numeric_limits<std::uint32_t>::max();
inline constexpr std::size_t maximum_parameters = 10U;
inline constexpr std::size_t slot_bytes = 4096U;
inline constexpr std::size_t maximum_pool = 64U;

// One way to break a requires clause: the parameter passed null, and the
// count beside it forced to 1 (or no_parameter), so `not_null(buffer) or
// capacity == 0` fails as it must.
struct Violation final {
    std::uint32_t nulled{no_parameter};
    std::uint32_t forced{no_parameter};
};

// The fuzz input, read front to back; past its end every draw is zero, so
// a short input is still a whole program.
class FuzzInput final {
public:
    explicit FuzzInput(std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] std::uint8_t byte() noexcept {
        if (position_ >= bytes_.size()) return 0U;
        const std::uint8_t value = bytes_[position_];
        ++position_;
        return value;
    }

    [[nodiscard]] std::uint64_t word(std::size_t count) noexcept {
        std::uint64_t value = 0U;
        for (std::size_t index = 0U; index < count; ++index) {
            value = (value << 8U) | static_cast<std::uint64_t>(byte());
        }
        return value;
    }

    // Copies up to `capacity` bytes, the count first drawn from one byte
    // scaled to the capacity; returns how many were copied.
    std::size_t fill(std::span<std::uint8_t> out) noexcept {
        const std::size_t wanted = (static_cast<std::size_t>(byte()) * out.size()) / 255U;
        for (std::size_t index = 0U; index < wanted; ++index) out[index] = byte();
        return wanted;
    }

    [[nodiscard]] bool exhausted() const noexcept { return position_ >= bytes_.size(); }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_{};
};

// The handles a run holds: subjects it made and objects the library wrote
// into its outputs. Bounded: the runner releases the oldest when it is full.
class Pool final {
public:
    [[nodiscard]] bool full() const noexcept { return count_ == maximum_pool; }
    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] gf_object at(std::size_t index) const noexcept { return entries_[index]; }

    // Keeps `object`; false, keeping nothing, when the pool is full.
    [[nodiscard]] bool keep(gf_object object) noexcept {
        if (full() || object == 0U) return false;
        entries_[count_] = object;
        ++count_;
        return true;
    }

    // Forgets the entry at `index` (the last takes its place) and returns it.
    gf_object take(std::size_t index) noexcept {
        const gf_object taken = entries_[index];
        --count_;
        entries_[index] = entries_[count_];
        entries_[count_] = 0U;
        return taken;
    }

    // A handle that ended: drawn to exercise the stale-handle refusals.
    void remember_stale(gf_object object) noexcept { stale_ = object; }
    [[nodiscard]] gf_object stale() const noexcept { return stale_; }

private:
    std::array<gf_object, maximum_pool> entries_{};
    std::size_t count_{};
    // Never zero: before any handle ended, a generation no handle has had.
    gf_object stale_{0x00ff00ff00ff00ffULL};
};

// The words a text argument is drawn from, besides raw bytes: property,
// interaction and type names the schema knows, the empty string and bytes
// that are not UTF-8.
inline constexpr std::array<std::string_view, 24> dictionary{
    "",          "text",          "enabled",   "visible",   "items",       "checked",
    "value",     "title",         "font",      "bounds",    "name",        "text_changed",
    "click",     "selection_changed", "button", "label",    "panel",       "check_box",
    "text_box",  "list_view",     "main",      "ic.object", "\xff\xfe\x80", "settings.ok-button",
};

class Arguments final {
public:
    // Starts a step: the input to draw from, the subject a member call takes,
    // and the violation a misuse block applies (none for a legal block).
    void begin(FuzzInput& input, const Pool& pool, gf_object subject, Violation violation) noexcept {
        input_ = &input;
        pool_ = &pool;
        subject_ = subject;
        violation_ = violation;
        adopted_count_ = 0U;
    }

    [[nodiscard]] gf_object subject() const noexcept { return subject_; }

    // A handle: zero, a stale one, the subject or one the run holds.
    [[nodiscard]] gf_object object(std::size_t index) noexcept {
        static_cast<void>(index);
        const std::uint8_t choice = (*input_).byte();
        if (choice < 16U) return 0U;
        if (choice < 32U) return (*pool_).stale();
        if (choice < 96U || (*pool_).size() == 0U) return subject_;
        const gf_object held = (*pool_).at(static_cast<std::size_t>(choice) % (*pool_).size());
        return held;
    }

    [[nodiscard]] std::uint64_t u64(std::size_t index) noexcept {
        if (forced(index)) return 1U;
        const std::uint8_t choice = (*input_).byte();
        switch (choice % 8U) {
        case 0U: return 0U;
        case 1U: return 1U;
        case 2U: return std::numeric_limits<std::uint64_t>::max();
        case 3U: return static_cast<std::uint64_t>((*input_).byte());
        case 4U: return static_cast<std::uint64_t>((*input_).word(2U));
        default: break;
        }
        const std::uint64_t drawn = (*input_).word(8U);
        return drawn;
    }

    [[nodiscard]] std::uint32_t u32(std::size_t index) noexcept {
        const std::uint32_t drawn = static_cast<std::uint32_t>(u64(index));
        return drawn;
    }

    [[nodiscard]] std::int64_t i64(std::size_t index) noexcept {
        const std::int64_t drawn = static_cast<std::int64_t>(u64(index));
        return drawn;
    }

    [[nodiscard]] std::int32_t i32(std::size_t index) noexcept {
        const std::int32_t drawn = static_cast<std::int32_t>(u64(index));
        return drawn;
    }

    [[nodiscard]] double real(std::size_t index) noexcept {
        static_cast<void>(index);
        const std::uint8_t choice = (*input_).byte();
        switch (choice % 8U) {
        case 0U: return 0.0;
        case 1U: return 1.0;
        case 2U: return std::numeric_limits<double>::quiet_NaN();
        case 3U: return std::numeric_limits<double>::infinity();
        case 4U: return -1.0;
        case 5U: return static_cast<double>((*input_).byte());
        default: break;
        }
        const double drawn = std::bit_cast<double>((*input_).word(8U));
        return drawn;
    }

    // A count of elements of `element_size` bytes that fits the slot of the
    // buffer it bounds; the forced count when a misuse block breaks
    // `not_null(buffer) or count == 0`.
    [[nodiscard]] std::uint64_t extent(std::size_t index, std::size_t element_size) noexcept {
        if (forced(index)) return 1U;
        const std::size_t most = slot_bytes / element_size;
        const std::uint64_t drawn = (*input_).word(2U) % (static_cast<std::uint64_t>(most) + 1U);
        return drawn;
    }

    [[nodiscard]] gf_utf8_view text(std::size_t index) noexcept {
        std::uint8_t* const bytes = slots_[index].bytes.data();
        const std::uint8_t choice = (*input_).byte();
        std::size_t size = 0U;
        if (choice < 192U) {
            const std::string_view word = dictionary[static_cast<std::size_t>(choice) % dictionary.size()];
            std::memcpy(bytes, word.data(), word.size());
            size = word.size();
        } else {
            size = (*input_).fill(std::span<std::uint8_t>(bytes, 64U));
        }
        gf_utf8_view view{};
        view.data = reinterpret_cast<const char*>(bytes);
        view.size = static_cast<std::uint64_t>(size);
        return view;
    }

    // A record or number of type T passed by value whose every byte pattern
    // is a value (integers and doubles only).
    template <typename T>
    [[nodiscard]] T plain(std::size_t index) noexcept {
        static_cast<void>(index);
        T value{};
        std::array<std::uint8_t, sizeof(T)> drawn{};
        for (std::size_t position = 0U; position < drawn.size(); ++position) drawn[position] = (*input_).byte();
        std::memcpy(&value, drawn.data(), sizeof(T));
        return value;
    }

    template <typename T>
    [[nodiscard]] T value(std::size_t index) noexcept {
        static_cast<void>(index);
        T zero{};
        return zero;
    }

    // The slot of parameter `index` as an array of value-initialized T, each
    // with struct_size set when `sized`; null when a misuse block nulls it.
    template <typename T>
    [[nodiscard]] T* output(std::size_t index, bool sized) noexcept {
        static_assert(sizeof(T) <= slot_bytes, "an argument record is larger than its arena slot");
        if (nulled(index)) return nullptr;
        T* const first = construct<T>(index, sized);
        return first;
    }

    template <typename T>
    [[nodiscard]] const T* input(std::size_t index, bool sized) noexcept {
        static_assert(sizeof(T) <= slot_bytes, "an argument record is larger than its arena slot");
        if (nulled(index)) return nullptr;
        const T* const first = construct<T>(index, sized);
        return first;
    }

    // Bytes drawn from the input (an image, a sheet, a theme package).
    template <typename T>
    [[nodiscard]] const T* bytes_input(std::size_t index) noexcept {
        static_assert(sizeof(T) == 1U, "bytes_input reads bytes");
        if (nulled(index)) return nullptr;
        T* const first = construct<T>(index, false);
        std::span<std::uint8_t> bytes(reinterpret_cast<std::uint8_t*>(first), slot_bytes);
        static_cast<void>((*input_).fill(bytes));
        return first;
    }

    // An array of handles drawn as object() draws one.
    [[nodiscard]] const gf_object* objects_input(std::size_t index) noexcept {
        if (nulled(index)) return nullptr;
        gf_object* const first = construct<gf_object>(index, false);
        const std::size_t count = slot_bytes / sizeof(gf_object);
        for (std::size_t position = 0U; position < count; ++position) first[position] = object(index);
        return first;
    }

    // Keeps a handle the call wrote into a gf_object output on success.
    void adopt(gf_status status, const gf_object* written) noexcept {
        if (status != GF_STATUS_OK || written == nullptr || *written == 0U) return;
        if (adopted_count_ == adopted_.size()) return;
        adopted_[adopted_count_] = *written;
        ++adopted_count_;
    }

    [[nodiscard]] std::span<const gf_object> adopted() const noexcept {
        const std::span<const gf_object> written(adopted_.data(), adopted_count_);
        return written;
    }

private:
    struct alignas(16) Slot final {
        std::array<std::uint8_t, slot_bytes> bytes;
    };

    [[nodiscard]] bool nulled(std::size_t index) const noexcept {
        const bool chosen = violation_.nulled != no_parameter && static_cast<std::size_t>(violation_.nulled) == index;
        return chosen;
    }

    [[nodiscard]] bool forced(std::size_t index) const noexcept {
        const bool chosen = violation_.forced != no_parameter && static_cast<std::size_t>(violation_.forced) == index;
        return chosen;
    }

    // Begins the lifetime of every T the slot holds, value-initialized
    // (zero), with struct_size set to sizeof(T) when `sized`.
    template <typename T>
    [[nodiscard]] T* construct(std::size_t index, bool sized) noexcept {
        std::uint8_t* const bytes = slots_[index].bytes.data();
        const std::size_t count = slot_bytes / sizeof(T);
        T* first = nullptr;
        for (std::size_t position = 0U; position < count; ++position) {
            T* const element = ::new (static_cast<void*>(bytes + position * sizeof(T))) T{};
            if (position == 0U) first = element;
            // An extensible record starts with its uint32_t struct_size (the
            // generator passes `sized` only for those, from the manifest).
            if (sized) {
                const std::uint32_t size = static_cast<std::uint32_t>(sizeof(T));
                std::memcpy(static_cast<void*>(element), &size, sizeof(size));
            }
        }
        return first;
    }

    FuzzInput* input_{};
    const Pool* pool_{};
    gf_object subject_{};
    Violation violation_{};
    std::array<Slot, maximum_parameters> slots_{};
    std::array<gf_object, maximum_parameters> adopted_{};
    std::size_t adopted_count_{};
};

} // namespace gui_forms_tests::ic
