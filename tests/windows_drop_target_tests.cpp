// Drops from other programs on the Windows host, in process: the Gallery
// window on the real Windows host, its retained-rows CollectionList under
// the cursor, and the window's own registered IDropTarget driven through
// DragEnter, DragOver and Drop with a CF_HDROP and then a CF_UNICODETEXT
// data object, as OLE drives it for Explorer or a browser: first called
// directly, then from another apartment through a COM proxy while this
// thread runs the host's message pump, as a drag from another program
// arrives. Each drop must reach the collection and show
// "Drop received · ..." in gallery.command.status. A trace line per step
// names where a drop stops. OLE's own DoDragDrop is not driven: it needs the
// input desktop (GetCursorPos fails on a private desktop, error 5).
#include "demo/gallery.hpp"
#include "gui_forms/controls/label.hpp"
#include "gui_forms/controls/list_box.hpp"
#include "gui_forms/window.hpp"
#include "src/surface/windows/application/windows_host.cpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace gui_forms::host {
namespace {

// A drag from another apartment, as OLE delivers one from another program:
// the window's target is reached through a COM proxy, so each call arrives
// through the window thread's message queue while that thread pumps, and the
// data object the target reads is itself a proxy.
struct ForeignDrag final {
    IStream* stream{};
    POINTL screen{};
    std::vector<DragDataItem> items;
    DWORD ui_thread{};
    DWORD entered{DROPEFFECT_NONE};
    DWORD over{DROPEFFECT_NONE};
    DWORD dropped{DROPEFFECT_NONE};
    HRESULT result{E_FAIL};
    std::atomic<bool> done{false};

    void run() {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IDropTarget* proxy = nullptr;
        HRESULT status = CoGetInterfaceAndReleaseStream(stream, IID_IDropTarget, reinterpret_cast<void**>(&proxy));
        IDataObject* const data = detail::make_windows_drag_data(items);
        if (SUCCEEDED(status) && data != nullptr) {
            const DWORD offered = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
            DWORD effect = offered;
            status = (*proxy).DragEnter(data, MK_LBUTTON, screen, &effect);
            entered = effect;
            if (SUCCEEDED(status)) {
                effect = offered;
                status = (*proxy).DragOver(MK_LBUTTON, screen, &effect);
                over = effect;
            }
            if (SUCCEEDED(status)) {
                effect = offered;
                status = (*proxy).Drop(data, 0U, screen, &effect);
                dropped = effect;
            }
        }
        result = status;
        if (data != nullptr) (*data).Release();
        if (proxy != nullptr) (*proxy).Release();
        if (SUCCEEDED(com)) CoUninitialize();
        done.store(true, std::memory_order_release);
        static_cast<void>(PostThreadMessageW(ui_thread, WM_NULL, 0U, 0));
    }
};

struct DropHostFixture final {
    static void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    static std::string effect_name(DWORD effect) {
        std::string name;
        if ((effect & DROPEFFECT_COPY) != 0U) name += "copy ";
        if ((effect & DROPEFFECT_MOVE) != 0U) name += "move ";
        if ((effect & DROPEFFECT_LINK) != 0U) name += "link ";
        if (name.empty()) name = "none ";
        return name;
    }

    static void trace(const char* step, DWORD effect, WindowsHostState& state, const Label& status) {
        const HostSessionSnapshot snapshot = state.session_.snapshot();
        std::cout << "drop trace: " << step << " effect=" << effect_name(effect)
                  << "drag_events=" << snapshot.drag_events << " drag_drops=" << snapshot.drag_drops
                  << " rejected=" << snapshot.events_rejected
                  << " modal_suppressed=" << snapshot.modal_input_suppressions << " status=\""
                  << status.text.read() << "\"\n";
    }

    // One drag of `items` from another program onto the collection's centre.
    static void drop_once(WindowsHostState& state, IDropTarget& target, POINTL screen, const Label& status,
                          const std::vector<DragDataItem>& items, const std::string& expected) {
        IDataObject* const data = detail::make_windows_drag_data(items);
        require(data != nullptr, "the drag data object is made");
        const DWORD offered = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
        DWORD effect = offered;
        require(SUCCEEDED(target.DragEnter(data, MK_LBUTTON, screen, &effect)), "DragEnter succeeds");
        trace("enter", effect, state, status);
        require(effect == DROPEFFECT_COPY, "the collection accepts a copy at DragEnter");
        effect = offered;
        require(SUCCEEDED(target.DragOver(MK_LBUTTON, screen, &effect)), "DragOver succeeds");
        trace("over", effect, state, status);
        require(effect == DROPEFFECT_COPY, "the collection accepts a copy at DragOver");
        effect = offered;
        // OLE calls Drop after the button is released: no button is held.
        require(SUCCEEDED(target.Drop(data, 0U, screen, &effect)), "Drop succeeds");
        trace("drop", effect, state, status);
        (*data).Release();
        require(effect == DROPEFFECT_COPY, "the drop performs a copy");
        require(status.text.read() == expected, "the status line shows the drop: " + expected);
    }

    // The same drag as drop_once, made from another apartment while this
    // thread runs the host's own message pump (run_windows' loop body).
    static void drop_from_another_apartment(WindowsHostState& state, IDropTarget& target, POINTL screen,
                                            const Label& status, const std::vector<DragDataItem>& items,
                                            const std::string& expected) {
        ForeignDrag drag;
        drag.screen = screen;
        drag.items = items;
        drag.ui_thread = GetCurrentThreadId();
        require(SUCCEEDED(CoMarshalInterThreadInterfaceInStream(IID_IDropTarget, &target, &drag.stream)),
                "the drop target marshals to another apartment");
        std::thread worker(&ForeignDrag::run, &drag);
        const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        bool input_first = true;
        while (!drag.done.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() - started < std::chrono::seconds(10)) {
            static_cast<void>(MsgWaitForMultipleObjectsEx(0U, nullptr, 100U, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
            MSG pending{};
            while (detail::take_window_message(pending, input_first)) {
                TranslateMessage(&pending);
                DispatchMessageW(&pending);
                input_first = !input_first;
            }
            state.present_pending_frame();
        }
        worker.join();
        std::cout << "drop trace: foreign apartment result=0x" << std::hex << static_cast<unsigned long>(drag.result)
                  << std::dec << " enter=" << effect_name(drag.entered) << "over=" << effect_name(drag.over)
                  << "drop=" << effect_name(drag.dropped) << '\n';
        trace("foreign drop", drag.dropped, state, status);
        require(SUCCEEDED(drag.result), "the foreign drag's calls succeed");
        require(drag.dropped == DROPEFFECT_COPY, "the foreign drop performs a copy");
        require(status.text.read() == expected, "the status line shows the foreign drop: " + expected);
    }

    static void run() {
        WNDCLASSEXW native_class{};
        native_class.cbSize = sizeof(native_class);
        native_class.lpfnWndProc = window_procedure;
        native_class.hInstance = GetModuleHandleW(nullptr);
        native_class.lpszClassName = L"GUIForms.DropTargetFixture.v1";
        const ATOM registered_class = RegisterClassExW(&native_class);
        require(registered_class != 0, "fixture class registration");

        std::unique_ptr<Window> owned = gallery::make_gallery();
        Window& model = *owned;
        require(gallery::select_gallery_page(model, "retained-rows"), "the retained-rows page selects");
        const Size size = model.client_size();
        WindowsHostOptions options{};
        options.quit_thread_on_close = false;
        options.print_metrics_on_close = false;
        options.scale_override = 1.0;
        WindowsHostState state(std::move(owned), options);
        HWND window = CreateWindowExW(WS_EX_NOACTIVATE,
                                      reinterpret_cast<LPCWSTR>(static_cast<ULONG_PTR>(registered_class)),
                                      L"Drop target fixture", WS_POPUP | WS_VISIBLE, 0, 0,
                                      static_cast<int>(size.width), static_cast<int>(size.height), nullptr,
                                      nullptr, GetModuleHandleW(nullptr), &state);
        require(window != nullptr, "fixture native creation");
        require(state.initialize(window), "fixture host initialization");
        InvalidateRect(state.hwnd_, nullptr, FALSE);
        SendMessageW(state.hwnd_, WM_PAINT, 0, 0);

        IDropTarget* const target = state.drop_registration_.registered_target();
        require(target != nullptr, "the window registered its drop target");
        const std::shared_ptr<ListBox> collection = std::dynamic_pointer_cast<ListBox>(model.find("gallery.collection"));
        const std::shared_ptr<Label> status = std::dynamic_pointer_cast<Label>(model.find("gallery.command.status"));
        require(collection != nullptr && status != nullptr, "the collection and the status line exist");
        const Rect bounds = (*collection).absolute_bounds();
        POINT client{static_cast<LONG>(bounds.x + bounds.width * 0.5), static_cast<LONG>(bounds.y + bounds.height * 0.5)};
        require(ClientToScreen(window, &client) != FALSE, "the drop point converts to the screen");
        const POINTL screen{client.x, client.y};
        std::cout << "drop trace: collection at " << bounds.x << ',' << bounds.y << ' ' << bounds.width << 'x'
                  << bounds.height << ", screen point " << screen.x << ',' << screen.y << '\n';

        drop_once(state, *target, screen, *status, {DragFileListData{{"C:\\captures\\live.iq"}}},
                  "Drop received \xC2\xB7 1 file \xC2\xB7 0 text \xC2\xB7 0 data");
        drop_once(state, *target, screen, *status, {DragTextData{"dragged text"}},
                  "Drop received \xC2\xB7 0 files \xC2\xB7 1 text \xC2\xB7 0 data");

        drop_from_another_apartment(state, *target, screen, *status,
                                    {DragFileListData{{"C:\\captures\\two.iq", "C:\\captures\\three.iq"}}},
                                    "Drop received \xC2\xB7 2 files \xC2\xB7 0 text \xC2\xB7 0 data");
        drop_from_another_apartment(state, *target, screen, *status, {DragTextData{"foreign text"}},
                                    "Drop received \xC2\xB7 0 files \xC2\xB7 1 text \xC2\xB7 0 data");

        DestroyWindow(window);
        require(UnregisterClassW(native_class.lpszClassName, native_class.hInstance) != FALSE,
                "fixture class release");
    }
};

} // namespace
} // namespace gui_forms::host

int main() {
    try {
        gui_forms::host::DropHostFixture::run();
    } catch (const std::exception& error) {
        std::cerr << "windows drop target failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "windows drop target tests passed\n";
    return EXIT_SUCCESS;
}
