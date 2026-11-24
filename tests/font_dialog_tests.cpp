// FontDialog (stage W): the bundled faces it offers, styles per face, size
// range, preview, Apply, OK and Cancel, the owned overlay with its focus scope
// and revocation, Escape, closing and reopening from inside its own handlers
// and while it opens, its layout measured at text scale 2 (nothing clips),
// semantics (focus goes to the font list) and the published anatomy.

#include "gui_forms/anatomy/types/anatomy_types.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/button_base.hpp"
#include "gui_forms/controls/font_dialog.hpp"
#include "gui_forms/controls/list_box.hpp"
#include "gui_forms/controls/numeric_up_down.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/dirty.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/semantics/types/semantic_types.hpp"
#include "gui_forms/text/markdown/markdown.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/types/paint_types/paint_types.hpp"
#include "gui_forms/window/presentation/presentation_types.hpp"
#include "gui_forms/window/window.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace gui_forms;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

struct RecordClosed final {
    std::vector<FontDialogResult>* seen{};
    void operator()(const FontDialogResult& result) const { (*seen).push_back(result); }
};

struct RecordApply final {
    std::vector<FontSpec>* seen{};
    void operator()(const FontSpec& font) const { (*seen).push_back(font); }
};

// Counts the presses that reach a button.
struct CountClicks final {
    int* count{};
    void operator()(ButtonBase&) const { ++(*count); }
};

struct Fixture final {
    std::shared_ptr<Panel> root;
    std::shared_ptr<Button> owner;
    std::shared_ptr<FontDialog> dialog;
    std::unique_ptr<Window> window;
};

Fixture make_fixture() {
    Fixture fixture;
    fixture.root = make_control<Panel>(StableId("root"));
    fixture.owner = make_control<Button>(StableId("owner"), "Font...");
    (*fixture.owner).set_requested_bounds({10.0, 10.0, 100.0, 30.0});
    (*fixture.root).add_child(fixture.owner);
    fixture.dialog = make_control<FontDialog>(StableId("font"));
    fixture.window = std::make_unique<Window>(fixture.root, Size{800.0, 600.0});
    (*fixture.window).perform_layout();
    return fixture;
}

void faces_and_styles() {
    const std::span<const BundledFontFace> faces = bundled_font_faces();
    require(faces.size() == 3U && faces[0].role == FontRole::control && !faces[0].italic &&
                faces[1].family == "Carlito" && faces[1].italic && faces[2].role == FontRole::monospace,
            "the dialog offers the three bundled role faces");
    Fixture fixture = make_fixture();
    FontDialog& dialog = *fixture.dialog;
    require(dialog.font.read() == FontSpec{FontRole::content, 9.0, 400, false} &&
                (*dialog.face_list()).items.read().size() == 3U && (*dialog.style_list()).items.read().size() == 4U,
            "the default is 9-unit Carlito, with four styles");
    dialog.font = FontSpec{FontRole::monospace, 14.0, 600, true};
    require(dialog.selection() == FontSpec{FontRole::monospace, 14.0, 700, true} &&
                (*dialog.face_list()).selected_index() == stxlib::Optional<std::size_t>::of(2U) && (*dialog.style_list()).selected_index() == stxlib::Optional<std::size_t>::of(3U) &&
                (*dialog.size_box()).value.read() == 14.0,
            "a font is shown at the nearest weight the faces have");
    (*dialog.face_list()).select_index(0U);
    require(dialog.selection().role == FontRole::control && !dialog.selection().italic &&
                dialog.selection().weight == 700U && (*dialog.style_list()).items.read().size() == 2U &&
                (*dialog.style_list()).selected_index() == stxlib::Optional<std::size_t>::of(1U),
            "Portsmouth Rapids has no italics: the style list shrinks and italic is dropped");
    (*dialog.style_list()).select_index(0U);
    (*dialog.size_box()).value = 20.0;
    require(dialog.selection() == FontSpec{FontRole::control, 20.0, 400, false},
            "style and size controls change the selection");
    dialog.set_size_range(10.0, 16.0);
    require(dialog.selection().size == 16.0 && (*dialog.size_box()).maximum.read() == 16.0,
            "the size range clamps the selection");
    bool threw = false;
    try {
        dialog.set_size_range(12.0, 8.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "a reversed size range is refused");
    require(dialog.font.read() == FontSpec{FontRole::monospace, 14.0, 600, true},
            "changing the selection leaves font() until OK or Apply");
}

void overlay_ok_cancel_apply() {
    Fixture fixture = make_fixture();
    Window& window = *fixture.window;
    FontDialog& dialog = *fixture.dialog;
    std::vector<FontDialogResult> closed;
    std::vector<FontSpec> applied;
    SubscriptionToken closing = dialog.closed().subscribe(RecordClosed{&closed});
    SubscriptionToken applying = dialog.apply().subscribe(RecordApply{&applied});
    require(!dialog.show(nullptr), "a dialog needs an owner");
    require(dialog.show(fixture.owner) && dialog.is_open() && window.focus_scope_depth() == 1U &&
                window.focused_control() == dialog.face_list() && window.find("font") != nullptr,
            "show opens the dialog over the window and holds the focus in it");
    // 460 x 332 at text scale 1 unless a measured text needs more.
    const Rect bounds = dialog.absolute_bounds();
    require(bounds.width >= 460.0 && bounds.height >= 332.0 && bounds.x == std::floor((800.0 - bounds.width) * 0.5) &&
                bounds.y == std::floor((600.0 - bounds.height) * 0.5),
            "the dialog is centred in the window at its measured size");
    // The point is inside the owner button beneath the dialog: the press
    // and release must not click it or close the dialog.
    int owner_clicks = 0;
    SubscriptionToken owner_clicked = (*fixture.owner).clicked().subscribe(CountClicks{&owner_clicks});
    static_cast<void>(window.dispatch_pointer({PointerAction::down, PointerButton::primary, {50.0, 25.0}}));
    static_cast<void>(window.dispatch_pointer({PointerAction::up, PointerButton::primary, {50.0, 25.0}}));
    require(owner_clicks == 0 && dialog.is_open(),
            "a press outside the dialog does not reach the page or close it");
    owner_clicked.disconnect();

    (*dialog.size_box()).value = 12.0;
    require((*dialog.apply_button()).perform_click() && applied.size() == 1U && applied[0].size == 12.0 &&
                dialog.font.read().size == 12.0 && dialog.is_open(),
            "Apply takes the selection and keeps the dialog open");
    (*dialog.size_box()).value = 18.0;
    require(window.dispatch_key({KeyAction::down, PhysicalKey::escape}) && !dialog.is_open() &&
                closed.size() == 1U && closed[0].result == DialogResult::cancel &&
                dialog.font.read().size == 12.0 && window.focus_scope_depth() == 0U &&
                window.find("font") == nullptr,
            "Escape cancels: the applied font stays and the overlay and its focus scope go");

    require(dialog.show(fixture.owner), "the dialog opens again");
    require(dialog.selection().size == 12.0, "a reopened dialog starts from font()");
    (*dialog.face_list()).select_index(0U);
    require((*dialog.ok_button()).perform_click() && !dialog.is_open() && closed.size() == 2U &&
                closed[1].result == DialogResult::ok && closed[1].font.role == FontRole::control &&
                dialog.font.read().role == FontRole::control,
            "OK takes the selection and closes");

    require(dialog.show(fixture.owner), "the dialog opens a third time");
    (*fixture.owner).visible = false;
    require(!dialog.is_open() && closed.size() == 3U && closed[2].result == DialogResult::cancel &&
                window.focus_scope_depth() == 0U,
            "hiding the owner revokes the dialog as a cancel");
    (*fixture.owner).visible = true;
    require(dialog.show(fixture.owner) && dialog.is_open(), "the dialog survives its revocation");
    dialog.close(DialogResult::cancel);
}

// Closes the dialog from inside its Apply handler.
struct CloseOnApply final {
    FontDialog* dialog{};
    void operator()(const FontSpec&) const { (*dialog).close(DialogResult::cancel); }
};

// Opens the dialog again from inside its closed handler, once.
struct ReopenOnClosed final {
    FontDialog* dialog{};
    Control::Ptr owner{};
    int* reopened{};
    void operator()(const FontDialogResult&) const {
        if (*reopened > 0) return;
        ++(*reopened);
        static_cast<void>((*dialog).show(owner));
    }
};

// A validating handler that closes the dialog as it opens: the focus move
// begin_focus_scope makes runs it.
struct CloseOnValidating final {
    FontDialog* dialog{};
    bool* ran{};
    void operator()(ControlValidationEvent&) const {
        if (!(*dialog).is_open()) return;
        *ran = true;
        (*dialog).close(DialogResult::cancel);
    }
};

// The presentation state through its own handlers: closed from Apply's
// handler, opened again from closed's handler, closed while it opens. Each
// leaves the dialog either open with its layer and one focus scope, or
// closed with neither.
void closes_and_reopens_from_handlers() {
    Fixture fixture = make_fixture();
    Window& window = *fixture.window;
    FontDialog& dialog = *fixture.dialog;
    std::vector<FontDialogResult> closed;
    SubscriptionToken closing = dialog.closed().subscribe(RecordClosed{&closed});

    SubscriptionToken applying = dialog.apply().subscribe(CloseOnApply{&dialog});
    require(dialog.show(fixture.owner) && dialog.presented_surface() == NativePopupSurface::overlay,
            "the dialog opens as an overlay");
    require((*dialog.apply_button()).perform_click() && !dialog.is_open() && closed.size() == 1U &&
                closed[0].result == DialogResult::cancel && window.focus_scope_depth() == 0U &&
                window.find("font") == nullptr && dialog.presented_surface() == NativePopupSurface::none &&
                !dialog.parent(),
            "closed from its Apply handler, the dialog leaves no layer, scope or parent");
    applying.disconnect();

    int reopened = 0;
    SubscriptionToken reopening = dialog.closed().subscribe(ReopenOnClosed{&dialog, fixture.owner, &reopened});
    require(dialog.show(fixture.owner), "the dialog opens again");
    dialog.close(DialogResult::ok);
    require(reopened == 1 && dialog.is_open() && closed.size() == 2U && window.focus_scope_depth() == 1U &&
                window.find("font") != nullptr && dialog.presented_surface() == NativePopupSurface::overlay,
            "opened again from its closed handler, it has one layer and one scope");
    reopening.disconnect();
    dialog.close(DialogResult::cancel);
    require(!dialog.is_open() && window.focus_scope_depth() == 0U && window.find("font") == nullptr,
            "and closes once more");

    require(window.request_focus(fixture.owner), "the owner takes the focus");
    bool ran = false;
    SubscriptionToken validating = (*fixture.owner).validating().subscribe(CloseOnValidating{&dialog, &ran});
    require(dialog.show(fixture.owner), "show reports the dialog it opened");
    require(!ran || (!dialog.is_open() && window.focus_scope_depth() == 0U && window.find("font") == nullptr),
            "a dialog closed while it opens ends its focus scope and leaves its layer");
    validating.disconnect();
    if (dialog.is_open()) dialog.close(DialogResult::cancel);
    require(dialog.show(fixture.owner) && dialog.is_open() && window.focus_scope_depth() == 1U,
            "and opens normally afterwards");
    dialog.close(DialogResult::cancel);
}

// What `control`'s one-line text wants, measured with nothing in the way.
Size wanted_size(Control& control) {
    const Size result = control.measure(Size{1.0e6, 1.0e6});
    return result;
}

// At text scale 2 every caption, list name, button and the dialog itself
// get the room their text needs: nothing is clipped.
void nothing_clips_at_text_scale_two() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("root"));
    std::shared_ptr<Button> owner = make_control<Button>(StableId("owner"), "Font...");
    (*owner).set_requested_bounds({10.0, 10.0, 100.0, 30.0});
    (*root).add_child(owner);
    std::shared_ptr<FontDialog> dialog = make_control<FontDialog>(StableId("font"));
    Window window(root, Size{1800.0, 1400.0});
    PresentationSettings settings = window.presentation_settings();
    settings.text_scale = 2.0;
    window.set_presentation_settings(settings);
    window.perform_layout();
    // A long, translated label: the button grows rather than clipping it.
    (*(*dialog).cancel_button()).text = std::string("Abbrechen und alle Änderungen verwerfen");
    require((*dialog).show(owner), "the dialog opens at text scale 2");
    window.perform_layout();
    const Rect bounds = (*dialog).absolute_bounds();
    require(bounds.width >= 920.0 && bounds.height >= 664.0, "every length doubles at text scale 2");
    require(bounds.x >= 0.0 && bounds.y >= 0.0 && bounds.x + bounds.width <= 1800.0 &&
                bounds.y + bounds.height <= 1400.0,
            "the dialog fits in its window");

    ListBox& faces = *(*dialog).face_list();
    const FontSpec face_font = faces.text_font(faces.font.read());
    const double face_width = faces.absolute_bounds().width;
    for (const std::string& face : faces.items.read()) {
        const double text = faces.measure_text_extent_utf8(face, face_font).logical_size.width;
        require(text + 16.0 <= face_width, "every face name fits in the font list with its row insets");
    }
    ListBox& styles = *(*dialog).style_list();
    const FontSpec style_font = styles.text_font(styles.font.read());
    const double bold_italic = styles.measure_text_extent_utf8("Bold Italic", style_font).logical_size.width;
    require(bold_italic + 16.0 <= styles.absolute_bounds().width, "the longest style name fits in the style list");

    const char* const captions[] = {"font.face-caption", "font.style-caption", "font.size-caption"};
    for (const char* identity : captions) {
        const Control::Ptr caption = window.find(identity);
        require(caption != nullptr, "the captions are found");
        const Size wanted = wanted_size(*caption);
        const Rect placed = (*caption).absolute_bounds();
        require(wanted.width <= placed.width && wanted.height <= placed.height, "every caption fits");
    }
    const std::shared_ptr<Button> buttons[] = {(*dialog).ok_button(), (*dialog).cancel_button(),
                                               (*dialog).apply_button()};
    for (const std::shared_ptr<Button>& button : buttons) {
        const Size wanted = wanted_size(*button);
        const Rect placed = (*button).absolute_bounds();
        require(wanted.width <= placed.width && wanted.height <= placed.height,
                "every button fits its label, the long one included");
        require(placed.x >= bounds.x && placed.x + placed.width <= bounds.x + bounds.width,
                "every button lies inside the dialog");
    }
    const FontDialog::Parts parts = (*dialog).parts();
    require(parts.preview_well.present && parts.preview_well.bounds.height >= 188.0,
            "the preview well scales with the text");
    (*dialog).close(DialogResult::cancel);

    settings.text_scale = 1.0;
    window.set_presentation_settings(settings);
    (*(*dialog).cancel_button()).text = std::string("Cancel");
    require((*dialog).show(owner), "the dialog opens again at text scale 1");
    window.perform_layout();
    require((*dialog).absolute_bounds().width < 920.0, "and is measured again, at its smaller size");
    (*dialog).close(DialogResult::cancel);
}

void embedded_semantics_and_anatomy() {
    Fixture fixture = make_fixture();
    FontDialog& dialog = *fixture.dialog;
    std::vector<FontDialogResult> closed;
    SubscriptionToken closing = dialog.closed().subscribe(RecordClosed{&closed});
    dialog.set_requested_bounds({0.0, 60.0, 460.0, 332.0});
    (*fixture.root).add_child(fixture.dialog);
    (*fixture.window).perform_layout();
    require(!dialog.show(fixture.owner), "a dialog placed in a layout does not open as an overlay");
    (*dialog.style_list()).select_index(1U);
    require((*dialog.ok_button()).perform_click() && closed.size() == 1U && closed[0].result == DialogResult::ok &&
                dialog.font.read().weight == 700U,
            "placed in a layout, OK reports and takes the selection");
    require((*fixture.window).request_focus(fixture.owner), "focus starts on the owner button");
    require((*fixture.window).perform_semantic_action("font", SemanticAction::focus) &&
                (*fixture.window).focused_control() == dialog.face_list(),
            "semantic focus on the dialog moves focus to its font list");

    const ControlAnatomy& anatomy = dialog.anatomy();
    require(validate_control_anatomy(anatomy).defect == AnatomyDefect::none && anatomy.control_id == "font_dialog",
            "FontDialog publishes a valid anatomy");
    const FontDialog::Parts parts = dialog.parts();
    require(parts.face.present && parts.title.present && parts.preview_well.present &&
                parts.preview_text.present && parts.preview_well.bounds.contains(
                    Point{parts.preview_text.bounds.x, parts.preview_text.bounds.y}),
            "the face, title and preview parts are published, the text inside its well");
    const SemanticDescriptor descriptor = dialog.semantic_descriptor();
    require(descriptor.role == SemanticRole::dialog && descriptor.name == "Font" &&
                descriptor.value == "Carlito Bold 9" && descriptor.modal == dialog.is_open(),
            "the dialog is a named dialog valued with its selection, modal only while open");
    bool threw = false;
    try {
        dialog.preview_text = std::string("\xFF");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "invalid preview text is refused");
    dialog.show_apply = false;
    require(!(*dialog.apply_button()).visible.read(), "Apply can be hidden");
}

} // namespace

int main() {
    faces_and_styles();
    overlay_ok_cancel_apply();
    closes_and_reopens_from_handlers();
    nothing_clips_at_text_scale_two();
    embedded_semantics_and_anatomy();
    std::cout << "FontDialog: faces, styles, sizes, overlay, OK, Cancel, Apply, revocation, handlers, text scale 2, "
                 "semantics and anatomy passed\n";
    return 0;
}
