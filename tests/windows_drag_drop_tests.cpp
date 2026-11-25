// Win32 drag and drop with other programs (services/windows_drag_drop.hpp):
// the effect and modifier conversions, the drag data a drag from the program
// offers read back as a destination reads it (file list, Unicode text, PNG),
// the formats it lists and refuses, the window's registration as a drop
// target, and a drag refused when no mouse button is held. A real drag
// needs a pressed button OLE follows, so the drop itself is a hand check
// (Explorer to the window, the window to Explorer); see the integration
// queue entry for host-drag-and-drop.

#include "services/windows_drag_drop.hpp"

#include <shellapi.h>
#include <shlobj.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace gui_forms;
using gui_forms::host::detail::WindowsDragDispatch;
using gui_forms::host::detail::WindowsDragOutcome;
using gui_forms::host::detail::WindowsDropRegistration;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "gui_forms_windows_drag_drop_tests: " << message << '\n';
    std::exit(1);
}

// Releases one COM reference on every exit.
class ReleaseScope final {
public:
    explicit ReleaseScope(IUnknown* object) noexcept : object_(object) {}
    ~ReleaseScope() {
        if (object_ != nullptr) (*object_).Release();
    }
    ReleaseScope(const ReleaseScope&) = delete;
    ReleaseScope& operator=(const ReleaseScope&) = delete;

private:
    IUnknown* object_{};
};

// Answers every drag event with copy and counts them.
class CountingDispatch final : public WindowsDragDispatch {
public:
    DragEffect dispatch_external_drag(DragEvent, POINT) override {
        ++events;
        return DragEffect::copy;
    }
    int events{};
};

FORMATETC global_format(CLIPFORMAT format) {
    FORMATETC result{};
    result.cfFormat = format;
    result.dwAspect = DVASPECT_CONTENT;
    result.lindex = -1;
    result.tymed = TYMED_HGLOBAL;
    return result;
}

void conversions() {
    require(host::detail::windows_drag_effects(DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_SCROLL) ==
                (DragEffect::copy | DragEffect::link),
            "OLE effects must map to the portable ones, the scroll bit dropped");
    require(host::detail::windows_drop_effects(DragEffect::move) == DROPEFFECT_MOVE,
            "a portable effect must map to its OLE bit");
    require(host::detail::windows_drag_modifiers(MK_SHIFT | MK_CONTROL | MK_LBUTTON) ==
                (Modifier::shift | Modifier::control),
            "the key state's modifiers must map and its buttons must not");
}

void data_round_trip() {
    const std::vector<DragDataItem> items{
        DragFileListData{{"C:\\captures\\a.iq", "C:\\captures\\\xCE\xA9.wav"}},
        DragTextData{"sent \xCE\xA9"},
        DragBinaryData{"image/png", {0x89, 0x50, 0x4e, 0x47}},
        // A second text item is not offered: CF_UNICODETEXT holds the first.
        DragTextData{"second"},
    };
    IDataObject* const data = host::detail::make_windows_drag_data(items);
    require(data != nullptr, "valid items must build drag data");
    const ReleaseScope reference(data);

    const std::vector<DragDataItem> read = host::detail::windows_drag_items(*data);
    require(read.size() == 3U, "a destination must read the file list, the text and the PNG");
    const DragFileListData* const files = std::get_if<DragFileListData>(&read[0]);
    require(files != nullptr && (*files).paths_utf8.size() == 2U &&
                (*files).paths_utf8[0] == "C:\\captures\\a.iq" &&
                (*files).paths_utf8[1] == "C:\\captures\\\xCE\xA9.wav",
            "CF_HDROP must carry every path in UTF-16 and read back as UTF-8");
    const DragTextData* const text = std::get_if<DragTextData>(&read[1]);
    require(text != nullptr && (*text).text_utf8 == "sent \xCE\xA9",
            "CF_UNICODETEXT must carry the first text item");
    const DragBinaryData* const png = std::get_if<DragBinaryData>(&read[2]);
    require(png != nullptr && (*png).media_type == "image/png" && (*png).bytes.size() == 4U &&
                (*png).bytes[0] == 0x89,
            "image/png must travel as the registered PNG format");

    FORMATETC absent = global_format(CF_BITMAP);
    require((*data).QueryGetData(&absent) == DV_E_FORMATETC, "a format not offered must be refused");
    FORMATETC text_format = global_format(CF_UNICODETEXT);
    require((*data).QueryGetData(&text_format) == S_OK, "an offered format must be answered");
    IEnumFORMATETC* listing = nullptr;
    require(SUCCEEDED((*data).EnumFormatEtc(DATADIR_GET, &listing)) && listing != nullptr,
            "the offered formats must be listed");
    const ReleaseScope listing_reference(listing);
    FORMATETC listed[8]{};
    ULONG fetched = 0;
    static_cast<void>((*listing).Next(8, listed, &fetched));
    require(fetched == 3U, "exactly the three offered formats must be listed");
    STGMEDIUM medium{};
    require((*data).SetData(&text_format, &medium, FALSE) == E_NOTIMPL, "the drag data must not change");
}

void refused_items() {
    const std::vector<DragDataItem> malformed{DragFileListData{{std::string{"\xF0\x28\x8C\x28", 4}}}};
    require(host::detail::make_windows_drag_data(malformed) == nullptr,
            "a path that is not UTF-8 must build no drag data");
}

void registration() {
    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const HWND window = CreateWindowExW(0, L"STATIC", L"drag target", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100,
                                        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    require(window != nullptr, "the test window must be created");
    CountingDispatch dispatch;
    {
        WindowsDropRegistration registration;
        require(registration.register_window(window, dispatch) && registration.registered(),
                "a window must register as a drop target");
        require(!registration.register_window(window, dispatch), "a window registers once");
        registration.revoke();
        require(!registration.registered(), "revoke must end the registration");
        require(registration.register_window(window, dispatch), "a revoked window may register again");
    }
    require(dispatch.events == 0, "registration alone must dispatch nothing");
    DestroyWindow(window);
    if (SUCCEEDED(apartment)) CoUninitialize();
}

void drag_without_button() {
    if ((GetKeyState(VK_LBUTTON) & 0x8000) != 0 || (GetKeyState(VK_RBUTTON) & 0x8000) != 0 ||
        (GetKeyState(VK_MBUTTON) & 0x8000) != 0) {
        std::cout << "gui_forms_windows_drag_drop_tests: a button is held; the refusal check is skipped\n";
        return;
    }
    HostDragSourceRequest request;
    request.items = {DragTextData{"no button"}};
    request.allowed_effects = DragEffect::copy;
    const bool source_closed = false;
    const WindowsDragOutcome outcome = host::detail::run_windows_drag(request, source_closed);
    require(outcome.status.error == HostServiceError::backend_failure &&
                outcome.performed_effect == DragEffect::none,
            "a drag with no button held must be refused before DoDragDrop");
}

// The hostile source window: its timer's handler destroys it, and its
// WM_DESTROY sets the flag the host's WM_DESTROY sets
// (WindowsHostServices::source_window_destroyed).
LRESULT CALLBACK hostile_source_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    if (message == WM_TIMER) {
        KillTimer(window, wparam);
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        bool* const closed = reinterpret_cast<bool*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (closed != nullptr) *closed = true;
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

// Hostile handlers under a drag (house style section 5): DoDragDrop's loop
// dispatches messages, so a timer can destroy the source window mid-drag.
// The drop source must then cancel at its next QueryContinueDrag, with the
// button still held, as the loop here simulates (a real drag needs a held
// button OLE follows).
void drag_cancelled_by_source_destruction() {
    WNDCLASSEXW type{};
    type.cbSize = sizeof(type);
    type.lpfnWndProc = &hostile_source_procedure;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"GuiFormsHostileDragSource";
    require(RegisterClassExW(&type) != 0, "the hostile window class must register");
    bool source_closed = false;
    const HWND window = CreateWindowExW(0, type.lpszClassName, L"drag source", WS_OVERLAPPEDWINDOW, 0, 0, 200,
                                        100, nullptr, nullptr, type.hInstance, nullptr);
    require(window != nullptr, "the hostile source window must be created");
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&source_closed));
    IDropSource* const source = host::detail::make_windows_drop_source(MK_LBUTTON, source_closed);
    const ReleaseScope source_reference(source);
    require(source != nullptr, "the drop source must be created");
    require((*source).QueryContinueDrag(FALSE, MK_LBUTTON) == S_OK, "a held button continues the drag");
    require(SetTimer(window, 1U, 20U, nullptr) != 0U, "the hostile timer must start");
    const ULONGLONG give_up = GetTickCount64() + 2000U;
    HRESULT answer = S_OK;
    while (answer == S_OK && GetTickCount64() < give_up) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) DispatchMessageW(&message);
        answer = (*source).QueryContinueDrag(FALSE, MK_LBUTTON);
        if (answer == S_OK) MsgWaitForMultipleObjects(0, nullptr, FALSE, 10U, QS_ALLINPUT);
    }
    require(source_closed && IsWindow(window) == FALSE, "the timer's handler must destroy the source window");
    require(answer == DRAGDROP_S_CANCEL, "a drag whose source window is destroyed must be cancelled");
    UnregisterClassW(type.lpszClassName, type.hInstance);
}

} // namespace

int main() {
    conversions();
    data_round_trip();
    refused_items();
    registration();
    drag_without_button();
    drag_cancelled_by_source_destruction();
    std::cout << "gui_forms_windows_drag_drop_tests: all tests passed\n";
    return EXIT_SUCCESS;
}
