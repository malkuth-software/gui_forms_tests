// Take-back over a Windows message box (owner, round 3; the macOS reference
// is macos_take_back_alert_tests.mm). While a model holds control, the
// user's input to a message box the program runs is held back and a press
// offers the take-back dialog; that dialog (a window outside modality, as
// the presenter's is: ApplicationWindow::outside_modality) is topmost over
// the box and takes the user's click; taking back leaves the box open, and
// the box then answers the user. With no gate the box answers at once.
// The model's control is a stand-in gate (session_directory.hpp,
// native_input_held) whose press shows the stand-in dialog. Each box is
// real: a thread timer, dispatched by the box's own modal loop, posts the
// user's clicks and keys to it, as the message queue would deliver them.
// A foreground test (gui_forms_foreground_test): it runs on a private
// desktop on a development machine.
#include "src/surface/windows/application/windows_host.cpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace gui_forms::host {
namespace {

constexpr wchar_t probe_title[] = L"GUI.Forms take-back probe";
constexpr wchar_t panel_class[] = L"GUIFormsTakeBackProbePanel";
constexpr UINT probe_period_milliseconds = 50U;
// How many timer ticks a run may take before the watchdog cancels the box.
constexpr unsigned maximum_probe_ticks = 200U;

struct TakeBackProbe final {
    bool model_holds{true};
    std::uint32_t presses{};
    HWND panel{};
    unsigned step{};
    unsigned ticks{};
    bool box_open_after_held_click{};
    bool box_open_after_held_key{};
    bool panel_shown_topmost{};
    bool panel_click_reached{};
    bool box_open_after_take_back{};
    bool closed_by_watchdog{};
};

// The probe the timer and the panel fill; neither callback has a context
// argument.
TakeBackProbe* active_probe = nullptr;

bool stand_in_gate(void* context, bool press) noexcept {
    TakeBackProbe& probe = *static_cast<TakeBackProbe*>(context);
    if (probe.model_holds && press) {
        ++probe.presses;
        static_cast<void>(ShowWindow(probe.panel, SW_SHOWNOACTIVATE));
    }
    return probe.model_holds;
}

// The stand-in take-back dialog: a press on it takes control back.
LRESULT CALLBACK panel_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_LBUTTONDOWN && active_probe != nullptr) {
        (*active_probe).panel_click_reached = true;
        (*active_probe).model_holds = false;
        static_cast<void>(ShowWindow(window, SW_HIDE));
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

BOOL CALLBACK find_probe_box(HWND window, LPARAM found) {
    std::array<wchar_t, 64> title{};
    const int length = GetWindowTextW(window, title.data(), static_cast<int>(title.size()));
    if (length <= 0 || std::wstring_view(title.data(), static_cast<std::size_t>(length)) != probe_title) {
        return TRUE;
    }
    *reinterpret_cast<HWND*>(found) = window;
    return FALSE;
}

HWND find_box() {
    HWND box = nullptr;
    EnumThreadWindows(GetCurrentThreadId(), find_probe_box, reinterpret_cast<LPARAM>(&box));
    return box;
}

// A click at the centre of `window`'s client area, posted as a down and an
// up.
void post_click(HWND window) {
    if (window == nullptr) return;
    RECT client{};
    static_cast<void>(GetClientRect(window, &client));
    const LPARAM centre = MAKELPARAM((client.right - client.left) / 2, (client.bottom - client.top) / 2);
    static_cast<void>(PostMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, centre));
    static_cast<void>(PostMessageW(window, WM_LBUTTONUP, 0, centre));
}

// The thread timer: each tick does the next step once the previous one's
// messages went through the box's loop.
void CALLBACK drive_box(HWND, UINT, UINT_PTR timer, DWORD) {
    TakeBackProbe* const probe = active_probe;
    if (probe == nullptr) return;
    ++(*probe).ticks;
    const HWND box = find_box();
    if ((*probe).ticks >= maximum_probe_ticks) {
        KillTimer(nullptr, timer);
        (*probe).closed_by_watchdog = true;
        if (box != nullptr) PostMessageW(box, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
        return;
    }
    if (box == nullptr) return;
    switch ((*probe).step) {
    case 0U:
        // 1: the user clicks Yes while the model holds control.
        post_click(GetDlgItem(box, IDYES));
        break;
    case 1U:
        // 2: the box is still open; the user presses Enter on it.
        (*probe).box_open_after_held_click = true;
        static_cast<void>(PostMessageW(GetDlgItem(box, IDYES), WM_KEYDOWN, VK_RETURN, 0x001C0001));
        static_cast<void>(PostMessageW(GetDlgItem(box, IDYES), WM_KEYUP, VK_RETURN, static_cast<LPARAM>(0xC01C0001U)));
        break;
    case 2U: {
        // 3: still open; the stand-in dialog is shown topmost and takes
        // the user's click.
        (*probe).box_open_after_held_key = true;
        const LONG_PTR extended = GetWindowLongPtrW((*probe).panel, GWL_EXSTYLE);
        (*probe).panel_shown_topmost = IsWindowVisible((*probe).panel) != FALSE && (extended & WS_EX_TOPMOST) != 0;
        post_click((*probe).panel);
        break;
    }
    case 3U:
        // 4: the box is the user's now: No.
        (*probe).box_open_after_take_back = true;
        post_click(GetDlgItem(box, IDNO));
        KillTimer(nullptr, timer);
        break;
    default: break;
    }
    ++(*probe).step;
}

// With no gate: the user clicks Yes once the box is up.
void CALLBACK answer_box(HWND, UINT, UINT_PTR timer, DWORD) {
    const HWND box = find_box();
    if (box == nullptr) return;
    KillTimer(nullptr, timer);
    post_click(GetDlgItem(box, IDYES));
}

struct TakeBackFixture final {
    static void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    static HostDialogChoice show(UINT_PTR (*arm)()) {
        HostMessageDialogRequest request{};
        request.title = "GUI.Forms take-back probe";
        request.message = "Keep the changes?";
        request.buttons = HostMessageButtons::yes_no;
        const UINT_PTR timer = arm();
        require(timer != 0U, "the probe timer starts");
        const HostDialogResult result = native_message_dialog(nullptr, 9U, request);
        KillTimer(nullptr, timer);
        require(result.status.accepted(), "the box answers");
        const HostMessageDialogResult* const value = std::get_if<HostMessageDialogResult>(&result.payload);
        require(value != nullptr, "the box answers with a choice");
        return (*value).choice;
    }

    static UINT_PTR arm_drive() { return SetTimer(nullptr, 0U, probe_period_milliseconds, drive_box); }
    static UINT_PTR arm_answer() { return SetTimer(nullptr, 0U, probe_period_milliseconds, answer_box); }

    static void run() {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW panel_type{};
        panel_type.cbSize = sizeof(panel_type);
        panel_type.lpfnWndProc = panel_procedure;
        panel_type.hInstance = instance;
        panel_type.lpszClassName = panel_class;
        require(RegisterClassExW(&panel_type) != 0U, "the stand-in dialog's class registers");
        // Made as the run makes a window outside modality: topmost, and
        // marked for the box's hook.
        const HWND panel = CreateWindowExW(WS_EX_TOPMOST, panel_class, L"Take back control", WS_POPUP | WS_BORDER,
                                           40, 40, 240, 120, nullptr, nullptr, instance, nullptr);
        require(panel != nullptr, "the stand-in dialog is created");
        mark_outside_modality(panel);

        TakeBackProbe probe{};
        probe.panel = panel;
        active_probe = &probe;
        gui_forms::detail::install_native_input_gate(&stand_in_gate, &probe);
        const HostDialogChoice held = show(&TakeBackFixture::arm_drive);
        gui_forms::detail::remove_user_input_gate(&probe);
        active_probe = nullptr;
        require(!probe.closed_by_watchdog, "the run finished before the watchdog");
        require(probe.box_open_after_held_click, "a click on the box while the model holds control is held");
        require(probe.box_open_after_held_key, "a key on the box while the model holds control is held");
        require(probe.presses == 2U, "each press (the click and the key) offered the take-back dialog once");
        require(probe.panel_shown_topmost, "the take-back dialog shows topmost over the box");
        require(probe.panel_click_reached, "the take-back dialog takes the user's click over the box");
        require(probe.box_open_after_take_back, "taking back leaves the box open");
        require(held == HostDialogChoice::no, "after taking back, the box answers the user");

        // No automation: the hook passes the first click.
        require(show(&TakeBackFixture::arm_answer) == HostDialogChoice::yes, "with no gate the box answers at once");

        unmark_outside_modality(panel);
        DestroyWindow(panel);
    }
};

} // namespace
} // namespace gui_forms::host

int main() {
    try {
        gui_forms::host::TakeBackFixture::run();
    } catch (const std::exception& error) {
        std::cerr << "windows take-back message box failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "windows take-back message box tests passed\n";
    return EXIT_SUCCESS;
}
