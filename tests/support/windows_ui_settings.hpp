// WinRT's UISettings and its IUISettings4 interface (Windows 10 1803), for the
// compositor test and probe. They are Windows Runtime types, which STXlib's
// Windows API headers (generated from the Win32 metadata) do not declare, so
// the two names used are declared here from Microsoft Learn: the runtime
// class name, and the interface's one method after IInspectable's, in vtable
// order (UISettings.AdvancedEffectsEnabled). The interface identifier is
// IUISettings4's.
#pragma once

#include <windows.h>
#include <inspectable.h>

namespace gui_forms_tests {

inline constexpr wchar_t ui_settings_class_name[] = L"Windows.UI.ViewManagement.UISettings";

inline constexpr GUID ui_settings4_interface{
    0x52bb3002, 0x919b, 0x4d6b, {0x9b, 0x78, 0x8d, 0xd6, 0x6f, 0xf4, 0xb9, 0x3b}};

struct UISettings4 : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_AdvancedEffectsEnabled(unsigned char* value) = 0;
};

} // namespace gui_forms_tests
