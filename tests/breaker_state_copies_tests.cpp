// The blind breaker's state copies (state hunt 2026-11-23, run 6, O10 and
// the refusals beside it), driven the way the breaker drove them.
//
// O10: a control's own fact copied into a property of a child part. The
// parts are addressable by stable ID, so the property tools (the property
// table, MCP, the C ABI, a form file) can write the part directly; the
// breaker did exactly that (`a part <id>.property visible = true`) and then
// found the control's fact and the part's disagreeing after a resize and a
// layout. Each case here writes the part through the property table, lays
// out, and holds the control to its "agrees with itself" predicate.
//
// N4 and N5: a call that changes state and then throws. Each case calls with
// the breaker's refused input and checks that nothing changed.
//
// The aborts: MonthCalendar and Plot3DView disposed from inside a focus
// handler while they held the pointer capture. The disposal revokes the
// capture inside the noexcept SurfaceRoot::dispose_subtree, which told the
// disposed control; its override invalidated, DisposedError left a noexcept
// function and the process ended.

#include "gui_forms/binding/value/binding_value.hpp"
#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/help_book_viewer.hpp"
#include "gui_forms/controls/list_box.hpp"
#include "gui_forms/controls/masked_text_box.hpp"
#include "gui_forms/controls/master_detail_view.hpp"
#include "gui_forms/controls/month_calendar.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/plot3d_view.hpp"
#include "gui_forms/controls/split_container.hpp"
#include "gui_forms/controls/split_container_values.hpp"
#include "gui_forms/controls/tab_control.hpp"
#include "gui_forms/controls/text_box.hpp"
#include "gui_forms/controls/value_bar.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/help/helpbook_section/helpbook_section.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace gui_forms;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Writes `name` on the control `stable_id` names through the property table,
// as the breaker's part writes do.
void write_part(Window& window, const char* stable_id, const char* name, BindingValue value) {
    const Control::Ptr part = window.find(stable_id);
    require(part != nullptr, "the part is addressable by its stable ID");
    (*part).set_property_value(name, std::move(value));
}

void settle(Window& window) {
    window.perform_layout();
    window.perform_layout();
}

// --- The "agrees with itself" predicates -----------------------------------------

// SplitContainer: never both sides collapsed, no origin on a side that is not
// collapsed, and each panel shown exactly when its side is not collapsed.
bool split_container_whole(const SplitContainer& split) {
    if (split.first_collapsed() && split.second_collapsed()) return false;
    if (!split.first_collapsed() && split.first_collapse_origin() != SplitCollapseOrigin::none) return false;
    if (!split.second_collapsed() && split.second_collapse_origin() != SplitCollapseOrigin::none) return false;
    if ((*split.first_panel()).visible.read() == split.first_collapsed()) return false;
    if ((*split.second_panel()).visible.read() == split.second_collapsed()) return false;
    return true;
}

// MasterDetailView: its split agrees with itself, the presentation it reports
// is the one its panels show, and a requested mode other than automatic is
// the one presented.
bool master_detail_whole(const MasterDetailView& view) {
    const std::shared_ptr<SplitContainer> split = view.split_container();
    if (!split_container_whole(*split)) return false;
    const bool first_shown = (*(*split).first_panel()).visible.read();
    const bool second_shown = (*(*split).second_panel()).visible.read();
    const MasterDetailDisplayMode presented = view.effective_display_mode();
    if (presented == MasterDetailDisplayMode::master_only && (!first_shown || second_shown)) return false;
    if (presented == MasterDetailDisplayMode::detail_only && (first_shown || !second_shown)) return false;
    if (presented == MasterDetailDisplayMode::side_by_side && (!first_shown || !second_shown)) return false;
    const MasterDetailDisplayMode requested = view.display_mode.read();
    if (requested != MasterDetailDisplayMode::automatic && requested != presented) return false;
    return true;
}

// ValueBar: the editor is shown exactly while an edit is open.
bool value_bar_whole(const ValueBar& bar) {
    const bool whole = (*bar.editor()).visible.read() == bar.editing();
    return whole;
}

// MaskedTextBox: the editor's read_only is the box's, and the box publishes it.
bool masked_text_box_whole(const MaskedTextBox& box) {
    const bool read_only = box.read_only.read();
    return (*box.editor()).read_only.read() == read_only && box.states().read_only == read_only;
}

// HelpBookViewer: the navigation pane and its splitter are shown exactly when
// navigation_visible says, and the bookmarks list has one row per bookmark.
bool help_book_viewer_whole(Window& window, const HelpBookViewer& viewer, const char* stable_id) {
    const std::string prefix(stable_id);
    const Control::Ptr tabs = window.find(prefix + ".tabs");
    const Control::Ptr splitter = window.find(prefix + ".navigation-splitter");
    const std::shared_ptr<ListBox> bookmarks = std::dynamic_pointer_cast<ListBox>(window.find(prefix + ".bookmarks-list"));
    if (tabs == nullptr || splitter == nullptr || bookmarks == nullptr) return false;
    if ((*tabs).visible.read() != viewer.navigation_visible()) return false;
    if ((*splitter).visible.read() != viewer.navigation_visible()) return false;
    if ((*bookmarks).items.read().size() != viewer.bookmarks().size()) return false;
    return true;
}

// --- O10 ----------------------------------------------------------------------------

// Records each presentation change it is told of.
struct RecordChange final {
    std::vector<MasterDetailPresentationChange>* changes{};
    void operator()(const MasterDetailPresentationChange& change) const { (*changes).push_back(change); }
};

// The breaker's sequence (parts, seed 300011): display_mode written, then
// show_master, then `split.panel2.visible = true` by stable ID. The view
// reported master while the detail panel showed, after any layout:
// apply_display_mode returned early on its cached mode.
void master_detail_part_writes() {
    std::shared_ptr<MasterDetailView> view = make_control<MasterDetailView>(StableId("mdv"));
    std::shared_ptr<Panel> master = make_control<Panel>(StableId("mdv.master"));
    std::shared_ptr<Panel> detail = make_control<Panel>(StableId("mdv.detail"));
    static_cast<void>((*view).set_master(master));
    static_cast<void>((*view).set_detail(detail));
    Window window(view, {900.0, 420.0});
    settle(window);
    require(master_detail_whole(*view), "O10 master/detail: whole at rest");
    (*view).display_mode = MasterDetailDisplayMode::master_only;
    (*view).show_master();
    settle(window);
    require((*view).effective_display_mode() == MasterDetailDisplayMode::master_only && master_detail_whole(*view),
            "O10 master/detail: master only is presented");

    write_part(window, "mdv.split.panel2", "visible", BindingValue{true});
    settle(window);
    require((*view).effective_display_mode() == MasterDetailDisplayMode::master_only && master_detail_whole(*view),
            "O10 master/detail: a direct write to the detail panel is put back by layout");

    // Through the split the view hands out: the report follows the split at
    // once (it is read from it), and layout puts the wanted mode back.
    std::vector<MasterDetailPresentationChange> changes;
    SubscriptionToken changed = (*view).presentation_changed().subscribe(RecordChange{&changes});
    (*(*view).split_container()).set_second_collapsed(false);
    require((*view).effective_display_mode() == MasterDetailDisplayMode::side_by_side,
            "O10 master/detail: the report is the split's, never a stale copy");
    settle(window);
    require((*view).effective_display_mode() == MasterDetailDisplayMode::master_only && master_detail_whole(*view) &&
                changes.size() == 1U && changes.back().previous == MasterDetailDisplayMode::side_by_side &&
                changes.back().current == MasterDetailDisplayMode::master_only,
            "O10 master/detail: layout presents the wanted mode again and says so once");
    changed.disconnect();

    write_part(window, "mdv.split.panel1", "visible", BindingValue{false});
    settle(window);
    require(master_detail_whole(*view), "O10 master/detail: hiding the master panel directly is put back");
}

// The breaker's sequences (parts, seed 300019): `panel1.visible = true` while
// the first side is collapsed, and `panel2.visible = false` while the second
// is not, by stable ID; both lasted through a resize and a layout.
void split_container_part_writes() {
    std::shared_ptr<SplitContainer> split = make_control<SplitContainer>(StableId("split"));
    Window window(split, {400.0, 300.0});
    settle(window);
    require(split_container_whole(*split), "O10 split: whole at rest");
    (*split).set_first_collapsed(true);
    settle(window);
    require(split_container_whole(*split), "O10 split: whole with the first side collapsed");

    write_part(window, "split.panel1", "visible", BindingValue{true});
    window.resize({420.0, 300.0});
    settle(window);
    require((*split).first_collapsed() && split_container_whole(*split),
            "O10 split: a collapsed panel shown directly is hidden again by layout");

    (*split).set_first_collapsed(false);
    settle(window);
    write_part(window, "split.panel2", "visible", BindingValue{false});
    window.resize({400.0, 300.0});
    settle(window);
    require(!(*split).second_collapsed() && split_container_whole(*split),
            "O10 split: a panel hidden directly while its side is open is shown again by layout");
}

// The breaker's sequences (parts, seeds 300006 and 300010): `editor.visible
// = true` with no edit open, and `editor.visible = false` during an edit.
void value_bar_part_writes() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("root"));
    std::shared_ptr<ValueBar> bar = make_control<ValueBar>(StableId("bar"));
    (*bar).set_requested_bounds({10.0, 10.0, 200.0, 28.0});
    (*root).add_child(bar);
    Window window(root, {300.0, 120.0});
    settle(window);
    require(value_bar_whole(*bar), "O10 value bar: whole at rest");

    write_part(window, "bar.editor", "visible", BindingValue{true});
    window.resize({320.0, 120.0});
    settle(window);
    require(!(*bar).editing() && value_bar_whole(*bar),
            "O10 value bar: an editor shown with no edit open is hidden again by layout");

    require((*bar).begin_edit(), "O10 value bar: an edit opens");
    settle(window);
    require((*bar).editing() && value_bar_whole(*bar), "O10 value bar: whole while editing");
    write_part(window, "bar.editor", "visible", BindingValue{false});
    window.resize({300.0, 120.0});
    settle(window);
    require(value_bar_whole(*bar),
            "O10 value bar: hiding the open editor directly ends the edit or is put back, never both apart");
    (*bar).cancel_edit();
    settle(window);
    require(!(*bar).editing() && value_bar_whole(*bar), "O10 value bar: whole after the edit");
}

// The breaker's sequence (parts, seed 300002): `editor.read_only = true` on
// a box that is not read-only; and the other way, which would have made a
// read-only box take typing.
void masked_text_box_part_writes() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("root"));
    std::shared_ptr<MaskedTextBox> box = make_control<MaskedTextBox>(StableId("masked"), std::string("0000"));
    (*box).set_requested_bounds({10.0, 10.0, 160.0, 28.0});
    (*root).add_child(box);
    Window window(root, {300.0, 120.0});
    settle(window);
    require(masked_text_box_whole(*box), "O10 masked: whole at rest");

    write_part(window, "masked.editor", "read_only", BindingValue{true});
    window.resize({320.0, 120.0});
    settle(window);
    require(!(*box).read_only.read() && masked_text_box_whole(*box),
            "O10 masked: an editor made read-only directly follows the box again after layout");

    (*box).read_only = true;
    settle(window);
    require(masked_text_box_whole(*box), "O10 masked: whole when read-only");
    write_part(window, "masked.editor", "read_only", BindingValue{false});
    const std::string before((*box).text.read());
    require(window.request_focus((*box).editor()), "O10 masked: the editor takes focus");
    static_cast<void>(window.dispatch_text({"1"}));
    require(std::string((*box).text.read()) == before,
            "O10 masked: a read-only box refuses typing whatever its editor's own read_only says");
    settle(window);
    require(masked_text_box_whole(*box), "O10 masked: the editor follows the box again after layout");
}

// The breaker's sequence (parts, seed 300019): `tabs.visible = true` while
// navigation_visible is false.
void help_book_viewer_part_writes() {
    std::shared_ptr<HelpBookViewer> viewer = make_control<HelpBookViewer>(StableId("help"));
    Window window(viewer, {760.0, 540.0});
    settle(window);
    require(help_book_viewer_whole(window, *viewer, "help"), "O10 help: whole at rest");
    (*viewer).set_navigation_visible(false);
    settle(window);
    write_part(window, "help.tabs", "visible", BindingValue{true});
    window.resize({780.0, 540.0});
    settle(window);
    require(!(*viewer).navigation_visible() && help_book_viewer_whole(window, *viewer, "help"),
            "O10 help: tabs shown directly while the pane is hidden are hidden again by layout");
    (*viewer).set_navigation_visible(true);
    write_part(window, "help.navigation-splitter", "visible", BindingValue{false});
    settle(window);
    require(help_book_viewer_whole(window, *viewer, "help"),
            "O10 help: a splitter hidden directly while the pane shows is shown again");
}

// --- N4 and N5: refused calls change nothing -----------------------------------

[[nodiscard]] bool throws_invalid_argument_or_logic_error(void (*call)(void*), void* context) {
    try {
        call(context);
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

struct ViewerCall final {
    HelpBookViewer* viewer{};
    std::string text{};
    std::vector<std::string> bookmarks{};
    std::shared_ptr<const HelpBookSection> section{};
};

void call_show_index(void* context) {
    ViewerCall& call = *static_cast<ViewerCall*>(context);
    (*call.viewer).show_index(call.text);
}

void call_show_search(void* context) {
    ViewerCall& call = *static_cast<ViewerCall*>(context);
    (*call.viewer).show_search(call.text);
}

void call_select_tab(void* context) {
    ViewerCall& call = *static_cast<ViewerCall*>(context);
    (*call.viewer).select_tab(static_cast<HelpNavigationTab>(7));
}

void call_set_bookmarks(void* context) {
    ViewerCall& call = *static_cast<ViewerCall*>(context);
    (*call.viewer).set_bookmarks(call.bookmarks);
}

void call_set_section(void* context) {
    ViewerCall& call = *static_cast<ViewerCall*>(context);
    (*call.viewer).set_section(call.section);
}

void call_set_navigation_width(void* context) {
    ViewerCall& call = *static_cast<ViewerCall*>(context);
    (*call.viewer).set_navigation_width(std::numeric_limits<double>::quiet_NaN());
}

// The breaker's N4 (fuzz seeds 3, 4, 6, 8, 23): show_index and show_search
// with text that is not UTF-8 moved the tab and showed the pane, then threw;
// set_bookmarks and set_section stored their input, then threw; and a NaN
// navigation width made every later layout throw. select_tab with a tab
// outside the four showed the pane before the tab control refused.
void help_book_viewer_refusals() {
    std::shared_ptr<HelpBookViewer> viewer = make_control<HelpBookViewer>(StableId("help"));
    Window window(viewer, {760.0, 540.0});
    settle(window);
    (*viewer).set_navigation_visible(false);
    settle(window);
    const std::string broken("\xC3\x28", 2U);
    ViewerCall call{};
    call.viewer = viewer.get();
    call.text = broken;
    const HelpNavigationTab tab = (*viewer).selected_tab();

    require(throws_invalid_argument_or_logic_error(&call_show_index, &call) && !(*viewer).navigation_visible() &&
                (*viewer).selected_tab() == tab,
            "N4 show_index: refused text leaves the pane and the tab as they were");
    require(throws_invalid_argument_or_logic_error(&call_show_search, &call) && !(*viewer).navigation_visible() &&
                (*viewer).selected_tab() == tab,
            "N4 show_search: refused text leaves the pane and the tab as they were");
    require(throws_invalid_argument_or_logic_error(&call_select_tab, &call) && !(*viewer).navigation_visible(),
            "N4 select_tab: a tab outside the four leaves the pane hidden");

    call.bookmarks = {"fine", broken};
    require(throws_invalid_argument_or_logic_error(&call_set_bookmarks, &call) && (*viewer).bookmarks().empty() &&
                help_book_viewer_whole(window, *viewer, "help"),
            "N4 set_bookmarks: refused bookmarks are not stored");

    std::shared_ptr<HelpBookSection> section = std::make_shared<HelpBookSection>();
    HelpBookRecord record{};
    record.stable_id = "topic";
    record.title = broken;
    (*section).records.push_back(record);
    HelpKeywordEntry keyword{};
    keyword.keyword = broken;
    keyword.folded = "x";
    keyword.record = 0U;
    (*section).keywords.push_back(keyword);
    call.section = section;
    require(throws_invalid_argument_or_logic_error(&call_set_section, &call) && (*viewer).section() == nullptr &&
                (*viewer).index_row_count() == 0U,
            "N4 set_section: a refused section is not stored and fills no list");

    require(throws_invalid_argument_or_logic_error(&call_set_navigation_width, &call),
            "O6 help: a NaN navigation width is refused");
    (*viewer).set_navigation_visible(true);
    settle(window);
    require(std::isfinite((*viewer).navigation_width()) && help_book_viewer_whole(window, *viewer, "help"),
            "O6 help: layout settles after the refused width");
}

struct RoleCall final {
    MasterDetailView* view{};
    Control::Ptr control{};
    bool detail{};
};

void call_set_role(void* context) {
    RoleCall& call = *static_cast<RoleCall*>(context);
    if (call.detail) {
        static_cast<void>((*call.view).set_detail(call.control));
    } else {
        static_cast<void>((*call.view).set_master(call.control));
    }
}

// The breaker's N5 (fuzz seed 20, free seed 1): set_master and set_detail
// with a control that contains the view refused the cycle after giving the
// control dock fill.
void master_detail_cycle_refusal() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("root"));
    std::shared_ptr<MasterDetailView> view = make_control<MasterDetailView>(StableId("mdv"));
    (*root).add_child(view);
    Window window(root, {900.0, 420.0});
    settle(window);
    require((*root).dock.read() == DockStyle::none, "N5: the root starts undocked");
    RoleCall call{};
    call.view = view.get();
    call.control = root;
    require(throws_invalid_argument_or_logic_error(&call_set_role, &call) && (*root).dock.read() == DockStyle::none &&
                (*view).master() == nullptr,
            "N5 set_master: a refused cycle leaves the control's dock as it was");
    call.detail = true;
    require(throws_invalid_argument_or_logic_error(&call_set_role, &call) && (*root).dock.read() == DockStyle::none &&
                (*view).detail() == nullptr,
            "N5 set_detail: a refused cycle leaves the control's dock as it was");
    settle(window);
    require(master_detail_whole(*view), "N5: the view agrees with itself after the refusals");
}

// --- Disposed from a focus handler while pressed ----------------------------------

// Disposes its subject on the first focus notification it sees.
struct DisposeOnFocus final {
    Control* subject{};
    int* fired{};
    void operator()(bool) const {
        if (*fired != 0 || subject == nullptr || !(*subject).is_alive()) return;
        ++*fired;
        (*subject).dispose();
    }
};

// The breaker's aborts (MonthCalendar fuzz seed 23, Plot3DView fuzz seed 6):
// the subject holds the capture from a press; focus moves to a bystander,
// whose focus handler disposes the subject.
void disposed_from_focus_handler_while_pressed(const Control::Ptr& subject, const char* name) {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("root"));
    (*subject).set_requested_bounds({0.0, 0.0, 320.0, 240.0});
    std::shared_ptr<Button> bystander = make_control<Button>(StableId("bystander"));
    (*bystander).set_requested_bounds({0.0, 250.0, 80.0, 24.0});
    (*root).add_child(subject);
    (*root).add_child(bystander);
    Window window(root, {340.0, 290.0});
    settle(window);
    PointerEvent press{PointerAction::down, PointerButton::primary, Point{160.0, 150.0}};
    press.pointer_id = 1U;
    static_cast<void>(window.dispatch_pointer(press));
    require(window.captured_control() == subject, name);
    int fired = 0;
    SubscriptionToken token = (*bystander).focus_observed().subscribe(DisposeOnFocus{subject.get(), &fired});
    static_cast<void>(window.request_focus(bystander));
    require(fired == 1 && !(*subject).is_alive() && window.captured_control() == nullptr,
            "disposed while pressed: the subject is gone and holds no capture");
    PointerEvent release{PointerAction::up, PointerButton::primary, Point{160.0, 150.0}};
    release.pointer_id = 1U;
    static_cast<void>(window.dispatch_pointer(release));
    settle(window);
    static_cast<void>(window.semantic_snapshot());
    token.disconnect();
}

// The plain form of the same abort: disposed while it holds the capture.
void disposed_while_pressed(const Control::Ptr& subject, const char* name) {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("root"));
    (*subject).set_requested_bounds({0.0, 0.0, 320.0, 240.0});
    (*root).add_child(subject);
    Window window(root, {340.0, 260.0});
    settle(window);
    PointerEvent press{PointerAction::down, PointerButton::primary, Point{160.0, 150.0}};
    press.pointer_id = 1U;
    static_cast<void>(window.dispatch_pointer(press));
    require(window.captured_control() == subject, name);
    (*subject).dispose();
    require(!(*subject).is_alive() && window.captured_control() == nullptr,
            "disposed while pressed: no capture is left on the disposed control");
    settle(window);
    static_cast<void>(window.semantic_snapshot());
}

} // namespace

int main() {
    try {
        master_detail_part_writes();
        split_container_part_writes();
        value_bar_part_writes();
        masked_text_box_part_writes();
        help_book_viewer_part_writes();
        help_book_viewer_refusals();
        master_detail_cycle_refusal();
        disposed_from_focus_handler_while_pressed(make_control<MonthCalendar>(StableId("calendar")),
                                                  "the calendar's press takes the capture");
        disposed_from_focus_handler_while_pressed(make_control<Plot3DView>(StableId("plot")),
                                                  "the plot's press takes the capture");
        disposed_while_pressed(make_control<MonthCalendar>(StableId("calendar")), "the calendar's press takes the capture");
        disposed_while_pressed(make_control<Plot3DView>(StableId("plot")), "the plot's press takes the capture");
        std::cout << "breaker-state-copies-tests: pass\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "breaker-state-copies-tests: fail: " << error.what() << '\n';
        return 1;
    }
}
