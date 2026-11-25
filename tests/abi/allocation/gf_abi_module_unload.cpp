// The C ABI library's teardown (gui_forms include/gui_forms/allocation/
// library_memory/library_memory.hpp, "A module's end"): the program loads
// the library at run time (dlopen, LoadLibrary), negotiates the objects
// table through gf_abi_get_table, makes and releases objects, and then
// either
// - unload: makes and releases them on a thread that ends first, and then
//   unloads the library (dlclose,
//   FreeLibrary), whose static destructors run at its detach; the program
//   then allocates on its own and exits cleanly; or
// - exit: keeps some objects alive and returns from main with the library
//   still loaded, so its static destructors run at the process's end (on
//   Windows at DLL_PROCESS_DETACH after ExitProcess has stopped the other
//   threads, where STXlib's allocator refuses what the thread's cache
//   cannot serve and the teardown reserve serves instead).
// A std::bad_alloc thrown from a destructor there, a fault, or the
// teardown reserve's fail-fast report ends the process abnormally, which
// fails the test.
//
// Usage: gf_abi_module_unload <library path> unload|exit

#include "gui_forms/c/gf_abi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

int failures = 0;

void require(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        failures = failures + 1;
    }
}

using GetTable = decltype(&gf_abi_get_table);

// The loaded library, and its entry point.
struct LoadedLibrary final {
#if defined(_WIN32)
    HMODULE module{nullptr};
#else
    void* module{nullptr};
#endif
    GetTable get_table{nullptr};
};

[[nodiscard]] LoadedLibrary load_library(const char* const path) {
    LoadedLibrary loaded{};
#if defined(_WIN32)
    loaded.module = ::LoadLibraryA(path);
    if (loaded.module == nullptr) return loaded;
    const FARPROC found = ::GetProcAddress(loaded.module, "gf_abi_get_table");
    // Through void*, which Windows' function and data pointers share.
    loaded.get_table = reinterpret_cast<GetTable>(reinterpret_cast<void*>(found));
#else
    loaded.module = ::dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (loaded.module == nullptr) return loaded;
    void* const found = ::dlsym(loaded.module, "gf_abi_get_table");
    loaded.get_table = reinterpret_cast<GetTable>(found);
#endif
    return loaded;
}

[[nodiscard]] bool unload_library(LoadedLibrary& loaded) {
#if defined(_WIN32)
    const bool unloaded = ::FreeLibrary(loaded.module) != 0;
#else
    const bool unloaded = ::dlclose(loaded.module) == 0;
#endif
    loaded.module = nullptr;
    loaded.get_table = nullptr;
    return unloaded;
}

gf_utf8_view text_view(const std::string_view text) {
    gf_utf8_view view{};
    view.data = text.data();
    view.size = text.size();
    return view;
}

// `count` objects the library makes.
[[nodiscard]] std::vector<gf_object> make_objects(const gf_objects_table& objects, const std::uint32_t count) {
    std::vector<gf_object> made;
    for (std::uint32_t index = 0U; index < count; ++index) {
        const std::string stable_id = "unload.label" + std::to_string(index);
        gf_object object = 0U;
        const gf_status created = objects.create(text_view("label"), text_view(stable_id), &object);
        require(created == GF_STATUS_OK && object != 0U, "the loaded library creates a label");
        if (created == GF_STATUS_OK) made.push_back(object);
    }
    return made;
}

void release_objects(const gf_objects_table& objects, const std::vector<gf_object>& made) {
    for (const gf_object object : made) {
        require(objects.release(object) == GF_STATUS_OK, "the loaded library releases its object");
    }
}

// Rounds of make and release, so the library's allocator has served, cached
// and released blocks before its teardown.
void make_and_release_rounds(const gf_objects_table& objects) {
    for (std::uint32_t round = 0U; round < 8U; ++round) {
        const std::vector<gf_object> made = make_objects(objects, 32U);
        release_objects(objects, made);
    }
}

// The unload mode's work, on a thread that ends before the unload.
void unload_mode_work(const gf_objects_table& objects) {
    make_and_release_rounds(objects);
    const std::vector<gf_object> made = make_objects(objects, 16U);
    release_objects(objects, made);
}

} // namespace

int main(const int argument_count, char** const arguments) {
    if (argument_count != 3) {
        std::cerr << "usage: gf_abi_module_unload <library path> unload|exit\n";
        return 2;
    }
    const std::string_view mode(arguments[2]);
    const bool unload = mode == "unload";
    if (!unload && mode != "exit") {
        std::cerr << "FAIL: unknown mode " << mode << '\n';
        return 2;
    }
    LoadedLibrary loaded = load_library(arguments[1]);
    if (loaded.module == nullptr || loaded.get_table == nullptr) {
        std::cerr << "FAIL: the library did not load, or has no gf_abi_get_table\n";
        return 1;
    }
    gf_objects_table objects{};
    objects.struct_size = static_cast<std::uint32_t>(sizeof(objects));
    const gf_status negotiated =
        (*loaded.get_table)(GF_TABLE_OBJECTS, GF_ABI_MAJOR, GF_ABI_MINOR, &objects, sizeof(objects));
    if (negotiated != GF_STATUS_OK) {
        std::cerr << "FAIL: the objects table did not negotiate\n";
        return 1;
    }

    if (unload) {
        // On a thread that ends before the unload: its end runs the library's
        // thread_local destructors and frees what STXlib keeps per thread,
        // which a thread still running at the unload keeps past it (the
        // library cannot reach another thread's storage). LeakSanitizer then
        // reports only what the library itself leaves behind.
        std::thread worker(unload_mode_work, std::cref(objects));
        worker.join();
        require(unload_library(loaded), "the library unloads");
        // The program's own allocator is untouched by the library's end.
        std::unique_ptr<std::array<std::uint8_t, 4096>> after = std::make_unique<std::array<std::uint8_t, 4096>>();
        (*after)[0] = 1U;
        require((*after)[0] == 1U, "the program allocates after the unload");
    } else {
        make_and_release_rounds(objects);
        // Left alive: the library's teardown at the process's end owns them.
        const std::vector<gf_object> kept = make_objects(objects, 16U);
        require(kept.size() == 16U, "objects stay alive into the process's end");
    }

    if (failures != 0) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "gf_abi_module_unload " << mode << ": all checks passed\n";
    return 0;
}
