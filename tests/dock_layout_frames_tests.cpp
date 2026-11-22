// The frames presentation of the docking layout (DK decisions 60 to 63): the
// document area's presentation, the frame edits and their refusals, the
// layout's own frame bookkeeping (defaults, stacking, icon slots), saving
// and restoring frames, and the arrangement planners (cascade, tiles,
// icons).

#include "gui_forms/component.hpp"
#include "gui_forms/docking.hpp"
#include "gui_forms/event.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using gui_forms::DockDocumentPresentation;
using gui_forms::DockEdit;
using gui_forms::DockEditOrigin;
using gui_forms::DockFrameArrangement;
using gui_forms::DockFrameMemory;
using gui_forms::DockFrameMetrics;
using gui_forms::DockFrameState;
using gui_forms::DockLayout;
using gui_forms::DockLayoutWriteResult;
using gui_forms::DockPanelDescriptor;
using gui_forms::DockPanelId;
using gui_forms::DockPanelKind;
using gui_forms::DockPanelRecord;
using gui_forms::DockRefusalReason;
using gui_forms::DockRestoreOptions;
using gui_forms::DockRestoreResult;
using gui_forms::DockRestoreStatus;
using gui_forms::DockSide;
using gui_forms::DockTransaction;
using gui_forms::DockTransactionResult;
using gui_forms::Rect;
using gui_forms::Size;

int failures = 0;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(double left, double right) {
    const bool close = std::fabs(left - right) < 1e-9;
    return close;
}

bool same_rect(Rect left, Rect right) {
    const bool same = near(left.x, right.x) && near(left.y, right.y) && near(left.width, right.width) &&
                      near(left.height, right.height);
    return same;
}

DockPanelDescriptor document(std::string id) {
    DockPanelDescriptor descriptor{};
    descriptor.id = DockPanelId(std::move(id));
    descriptor.kind = DockPanelKind::document;
    return descriptor;
}

DockPanelDescriptor tool(std::string id) {
    DockPanelDescriptor descriptor{};
    descriptor.id = DockPanelId(std::move(id));
    descriptor.kind = DockPanelKind::tool;
    descriptor.default_placement.side = DockSide::left;
    return descriptor;
}

DockTransactionResult apply_one(DockLayout& layout, DockEdit edit, DockEditOrigin origin = DockEditOrigin::program) {
    DockTransaction transaction{};
    transaction.origin = origin;
    transaction.edits.push_back(std::move(edit));
    const DockTransactionResult result = layout.apply(transaction);
    return result;
}

// Three documents and a tool panel, all shown.
void register_standard(DockLayout& layout) {
    require(layout.register_panel(document("doc:a")), "doc:a registers");
    require(layout.register_panel(document("doc:b")), "doc:b registers");
    require(layout.register_panel(document("doc:c")), "doc:c registers");
    require(layout.register_panel(tool("tools.output")), "the tool registers");
}

void show_standard(DockLayout& layout) {
    require(apply_one(layout, DockEdit::show(DockPanelId("doc:a"))).applied, "doc:a shows");
    require(apply_one(layout, DockEdit::show(DockPanelId("doc:b"))).applied, "doc:b shows");
    require(apply_one(layout, DockEdit::show(DockPanelId("doc:c"))).applied, "doc:c shows");
    require(apply_one(layout, DockEdit::show(DockPanelId("tools.output"))).applied, "the tool shows");
}

const DockFrameMemory* frame_of(const DockLayout& layout, std::string_view id) {
    for (const DockPanelRecord& record : layout.state().panels) {
        if (record.id.value == id) return record.frame.value();
    }
    return nullptr;
}

void test_presentation_and_defaults() {
    DockLayout layout;
    register_standard(layout);
    show_standard(layout);
    require(layout.state().document_presentation == DockDocumentPresentation::tabs, "tabs is the default");
    require(frame_of(layout, "doc:a") == nullptr, "tabs give no frames");
    const DockTransactionResult result =
        apply_one(layout, DockEdit::set_document_presentation(DockDocumentPresentation::frames));
    require(result.applied, "the presentation changes");
    require(layout.state().document_presentation == DockDocumentPresentation::frames, "frames now");
    const DockFrameMemory* const a = frame_of(layout, "doc:a");
    const DockFrameMemory* const b = frame_of(layout, "doc:b");
    const DockFrameMemory* const c = frame_of(layout, "doc:c");
    require(a != nullptr && b != nullptr && c != nullptr, "every area document has a frame");
    require(frame_of(layout, "tools.output") == nullptr, "a tool panel has none");
    if (a == nullptr || b == nullptr || c == nullptr) return;
    require(same_rect((*a).bounds, Rect{0.0, 0.0, 480.0, 320.0}), "the first frame takes the corner");
    require(same_rect((*b).bounds, Rect{24.0, 24.0, 480.0, 320.0}), "the second cascades one step");
    require(same_rect((*c).bounds, Rect{48.0, 48.0, 480.0, 320.0}), "the third cascades two steps");
    require((*a).order == 1U && (*b).order == 2U && (*c).order == 3U, "later frames stack in front");
    require((*a).state == DockFrameState::normal, "a default frame is normal");
    require(layout.check_invariants().valid, "the layout stays valid");
}

void test_select_brings_to_front() {
    DockLayout layout;
    register_standard(layout);
    show_standard(layout);
    require(apply_one(layout, DockEdit::set_document_presentation(DockDocumentPresentation::frames)).applied,
            "frames");
    require(apply_one(layout, DockEdit::select(DockPanelId("doc:a"))).applied, "doc:a selects");
    const DockFrameMemory* const a = frame_of(layout, "doc:a");
    const DockFrameMemory* const b = frame_of(layout, "doc:b");
    const DockFrameMemory* const c = frame_of(layout, "doc:c");
    if (a == nullptr || b == nullptr || c == nullptr) {
        require(false, "frames exist");
        return;
    }
    require((*a).order == 3U, "the selected frame is in front, numbered again");
    require((*b).order == 1U && (*c).order == 2U, "the others keep their stacking");
}

void test_frame_edits_and_refusals() {
    DockLayout layout;
    register_standard(layout);
    show_standard(layout);
    const DockPanelId a("doc:a");
    // Accepted in the tabs presentation too: the program may place frames
    // before they show.
    DockTransactionResult result = apply_one(layout, DockEdit::set_frame_bounds(a, Rect{10.0, 20.0, 300.0, 200.0}));
    require(result.applied, "a frame is placed in tabs");
    const DockFrameMemory* frame = frame_of(layout, "doc:a");
    require(frame != nullptr && same_rect((*frame).bounds, Rect{10.0, 20.0, 300.0, 200.0}), "the bounds are kept");
    require(frame_of(layout, "doc:b") == nullptr, "tabs make no other frames");
    result = apply_one(layout, DockEdit::set_frame_bounds(a, Rect{0.0, 0.0, 0.0, 100.0}));
    require(!result.applied && result.refusal.reason == DockRefusalReason::invalid_frame, "an empty frame is refused");
    result = apply_one(layout, DockEdit::set_frame_bounds(a, Rect{std::nan(""), 0.0, 10.0, 10.0}));
    require(!result.applied && result.refusal.reason == DockRefusalReason::invalid_frame,
            "a frame that is not finite is refused");
    result = apply_one(layout, DockEdit::set_frame_state(DockPanelId("tools.output"), DockFrameState::minimized));
    require(!result.applied && result.refusal.reason == DockRefusalReason::not_in_main_site,
            "a panel outside the document area has no frame");
    require(apply_one(layout, DockEdit::hide(DockPanelId("doc:c"))).applied, "doc:c hides");
    result = apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:c"), DockFrameState::maximized));
    require(!result.applied && result.refusal.reason == DockRefusalReason::not_placed,
            "a hidden document has no frame shown");
    result = apply_one(layout, DockEdit::set_frame_bounds(DockPanelId("doc:none"), Rect{0.0, 0.0, 10.0, 10.0}));
    require(!result.applied && result.refusal.reason == DockRefusalReason::not_placed, "an unknown panel is refused");
}

void test_states_and_icon_slots() {
    DockLayout layout;
    register_standard(layout);
    show_standard(layout);
    require(apply_one(layout, DockEdit::set_document_presentation(DockDocumentPresentation::frames)).applied,
            "frames");
    const Rect before = (*frame_of(layout, "doc:a")).bounds;
    require(apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:a"), DockFrameState::minimized)).applied,
            "doc:a minimizes");
    require(apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:b"), DockFrameState::minimized)).applied,
            "doc:b minimizes");
    require((*frame_of(layout, "doc:a")).icon_slot == 0U, "the first icon takes slot 0");
    require((*frame_of(layout, "doc:b")).icon_slot == 1U, "the second takes slot 1");
    require(apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:a"), DockFrameState::normal)).applied,
            "doc:a restores");
    require(same_rect((*frame_of(layout, "doc:a")).bounds, before), "restoring keeps the normal bounds");
    require(apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:c"), DockFrameState::minimized)).applied,
            "doc:c minimizes");
    require((*frame_of(layout, "doc:c")).icon_slot == 0U, "a freed slot is taken again");
    require(apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:a"), DockFrameState::maximized)).applied,
            "doc:a maximizes");
    require((*frame_of(layout, "doc:a")).state == DockFrameState::maximized, "maximized");
    require(same_rect((*frame_of(layout, "doc:a")).bounds, before), "maximizing keeps the normal bounds");
    // Two minimized frames given one slot: the layout moves the later one.
    DockTransaction clash{};
    clash.edits.push_back(DockEdit::set_frame_icon_slot(DockPanelId("doc:b"), 0U));
    require(layout.apply(clash).applied, "the slot edit applies");
    const std::uint32_t b_slot = (*frame_of(layout, "doc:b")).icon_slot;
    const std::uint32_t c_slot = (*frame_of(layout, "doc:c")).icon_slot;
    require(b_slot != c_slot, "no two icons share a slot");
}

void test_lock_allows_frame_edits() {
    DockLayout layout;
    register_standard(layout);
    show_standard(layout);
    layout.set_user_docking_locked(true);
    DockTransactionResult result =
        apply_one(layout, DockEdit::set_document_presentation(DockDocumentPresentation::frames), DockEditOrigin::user);
    require(result.applied, "the presentation changes under the lock");
    result = apply_one(layout, DockEdit::set_frame_bounds(DockPanelId("doc:b"), Rect{5.0, 5.0, 200.0, 100.0}),
                       DockEditOrigin::user);
    require(result.applied, "a user moves a frame under the lock");
    result = apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:b"), DockFrameState::minimized),
                       DockEditOrigin::user);
    require(result.applied, "a user minimizes a frame under the lock");
    result = apply_one(layout, DockEdit::hide(DockPanelId("doc:b")), DockEditOrigin::user);
    require(!result.applied && result.refusal.reason == DockRefusalReason::user_docking_locked,
            "moving a panel is still locked");
}

class ChangeRecorder final : public gui_forms::Component {
public:
    void record(const gui_forms::DockLayoutChange& change) {
        ++count;
        last = change;
    }
    std::size_t count{0};
    gui_forms::DockLayoutChange last{};
};

// A document frame lives in the main site, so moving one is not a floating
// window's frames-only change: the site lays its area out again.
void test_change_is_not_frames_only() {
    DockLayout layout;
    register_standard(layout);
    show_standard(layout);
    ChangeRecorder recorder;
    gui_forms::on(layout.changed(), recorder, &ChangeRecorder::record);
    const DockTransactionResult result =
        apply_one(layout, DockEdit::set_frame_bounds(DockPanelId("doc:a"), Rect{1.0, 1.0, 100.0, 100.0}));
    require(result.applied, "the frame moves");
    require(recorder.count == 1U, "one change is published");
    require(!recorder.last.frames_only, "a document frame's move is not frames-only");
    require(recorder.last.edit_count == 1U, "the change counts its edit");
}

void test_save_and_restore() {
    DockLayout layout;
    register_standard(layout);
    show_standard(layout);
    const DockLayoutWriteResult tabs = layout.write_xml();
    require(static_cast<bool>(tabs), "the tabs layout writes");
    require(tabs.text.find("presentation=") == std::string::npos, "tabs write no presentation");
    require(apply_one(layout, DockEdit::set_document_presentation(DockDocumentPresentation::frames)).applied,
            "frames");
    require(apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:b"), DockFrameState::minimized)).applied,
            "doc:b minimizes");
    require(apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:c"), DockFrameState::maximized)).applied,
            "doc:c maximizes");
    const DockLayoutWriteResult written = layout.write_xml();
    require(static_cast<bool>(written), "the frames layout writes");
    require(written.text.find("presentation=\"frames\"") != std::string::npos, "the presentation is written");
    require(written.text.find("<frame ") != std::string::npos, "frames are written");
    require(written.text.find("state=\"minimized\"") != std::string::npos, "a minimized state is written");
    DockLayout restored;
    register_standard(restored);
    const DockRestoreResult result = restored.restore_xml(written.text, DockRestoreOptions{});
    require(result.status == DockRestoreStatus::restored, "the frames layout restores");
    require(restored.state() == layout.state(), "the restored state is the saved one");
    const DockLayoutWriteResult again = restored.write_xml();
    require(again.text == written.text, "the same layout writes the same bytes");
}

void test_restore_refuses_bad_frames() {
    DockLayout layout;
    register_standard(layout);
    show_standard(layout);
    require(apply_one(layout, DockEdit::set_document_presentation(DockDocumentPresentation::frames)).applied,
            "frames");
    const std::string written = layout.write_xml().text;
    const std::string::size_type state_at = written.find("state=\"normal\"");
    require(state_at != std::string::npos, "a normal state is written");
    if (state_at == std::string::npos) return;
    std::string corrupt = written;
    corrupt.replace(state_at, std::string_view("state=\"normal\"").size(), "state=\"rolled\"");
    DockLayout target;
    register_standard(target);
    const DockRestoreResult refused = target.restore_xml(corrupt, DockRestoreOptions{});
    require(refused.status == DockRestoreStatus::kept_current, "an unknown frame state refuses the restore");
    std::string bad_presentation = written;
    const std::string::size_type presentation_at = bad_presentation.find("presentation=\"frames\"");
    if (presentation_at != std::string::npos) {
        bad_presentation.replace(presentation_at, std::string_view("presentation=\"frames\"").size(),
                                 "presentation=\"cards\"");
    }
    const DockRestoreResult refused_presentation = target.restore_xml(bad_presentation, DockRestoreOptions{});
    require(refused_presentation.status == DockRestoreStatus::kept_current, "an unknown presentation refuses it");
    // A frames layout without <frame> memories gets defaults on restore.
    std::string bare = written;
    std::string::size_type frame_at = bare.find("<frame ");
    while (frame_at != std::string::npos) {
        const std::string::size_type end = bare.find("/>", frame_at);
        if (end == std::string::npos) break;
        bare.erase(frame_at, end + 2U - frame_at);
        frame_at = bare.find("<frame ");
    }
    DockLayout defaults;
    register_standard(defaults);
    const DockRestoreResult restored = defaults.restore_xml(bare, DockRestoreOptions{});
    require(restored.status == DockRestoreStatus::restored, "frames without memories restore");
    require(frame_of(defaults, "doc:a") != nullptr && frame_of(defaults, "doc:c") != nullptr,
            "restore gives the area's documents default frames");
}

// Three documents, each with a frame; doc:c in front.
void framed_layout(DockLayout& layout) {
    register_standard(layout);
    show_standard(layout);
    require(apply_one(layout, DockEdit::set_document_presentation(DockDocumentPresentation::frames)).applied,
            "frames");
}

void apply_plan(DockLayout& layout, DockFrameArrangement arrangement, Size area, const DockFrameMetrics& metrics) {
    const DockTransaction plan = gui_forms::plan_frame_arrangement(layout, arrangement, area, metrics);
    require(layout.apply(plan).applied, "the plan applies");
}

void test_cascade() {
    DockLayout layout;
    framed_layout(layout);
    require(apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:a"), DockFrameState::maximized)).applied,
            "doc:a maximizes");
    DockFrameMetrics metrics{};
    metrics.caption_height = 20.0;
    apply_plan(layout, DockFrameArrangement::cascade, Size{900.0, 600.0}, metrics);
    const DockFrameMemory& a = *frame_of(layout, "doc:a");
    const DockFrameMemory& b = *frame_of(layout, "doc:b");
    const DockFrameMemory& c = *frame_of(layout, "doc:c");
    require(a.state == DockFrameState::normal, "a cascade restores a maximized frame");
    // doc:a is at the back (order 1), doc:c in front.
    require(near(a.bounds.x, 0.0) && near(a.bounds.y, 0.0), "the back frame takes the corner");
    require(near(b.bounds.x, 20.0) && near(b.bounds.y, 20.0), "the next steps one caption");
    require(near(c.bounds.x, 40.0) && near(c.bounds.y, 40.0), "the front frame steps furthest");
    require(near(c.bounds.width, 600.0) && near(c.bounds.height, 400.0), "cascaded frames take two thirds");
}

void test_tiles() {
    DockLayout layout;
    framed_layout(layout);
    const DockFrameMetrics metrics{};
    apply_plan(layout, DockFrameArrangement::tile_horizontal, Size{900.0, 600.0}, metrics);
    // Front first: doc:c on top, then doc:b, then doc:a.
    require(same_rect((*frame_of(layout, "doc:c")).bounds, Rect{0.0, 0.0, 900.0, 200.0}), "doc:c tiles on top");
    require(same_rect((*frame_of(layout, "doc:b")).bounds, Rect{0.0, 200.0, 900.0, 200.0}), "doc:b tiles next");
    require(same_rect((*frame_of(layout, "doc:a")).bounds, Rect{0.0, 400.0, 900.0, 200.0}), "doc:a tiles last");
    apply_plan(layout, DockFrameArrangement::tile_vertical, Size{900.0, 600.0}, metrics);
    require(same_rect((*frame_of(layout, "doc:c")).bounds, Rect{0.0, 0.0, 300.0, 600.0}), "doc:c tiles left");
    require(same_rect((*frame_of(layout, "doc:a")).bounds, Rect{600.0, 0.0, 300.0, 600.0}), "doc:a tiles right");
    // A minimized frame keeps its icon, and tiles keep clear of the icons.
    require(apply_one(layout, DockEdit::set_frame_state(DockPanelId("doc:b"), DockFrameState::minimized)).applied,
            "doc:b minimizes");
    apply_plan(layout, DockFrameArrangement::tile_horizontal, Size{900.0, 600.0}, metrics);
    require(same_rect((*frame_of(layout, "doc:c")).bounds, Rect{0.0, 0.0, 900.0, 288.0}), "tiles leave the icon row");
    require(same_rect((*frame_of(layout, "doc:a")).bounds, Rect{0.0, 288.0, 900.0, 288.0}), "the second tile");
    require((*frame_of(layout, "doc:b")).state == DockFrameState::minimized, "the icon stays minimized");
}

void test_tile_grid() {
    DockLayout layout;
    framed_layout(layout);
    require(layout.register_panel(document("doc:d")), "doc:d registers");
    require(layout.register_panel(document("doc:e")), "doc:e registers");
    require(apply_one(layout, DockEdit::show(DockPanelId("doc:d"))).applied, "doc:d shows");
    require(apply_one(layout, DockEdit::show(DockPanelId("doc:e"))).applied, "doc:e shows");
    apply_plan(layout, DockFrameArrangement::tile_horizontal, Size{600.0, 600.0}, DockFrameMetrics{});
    // Five frames: three rows of two, two and one (the last row wider).
    require(same_rect((*frame_of(layout, "doc:e")).bounds, Rect{0.0, 0.0, 300.0, 200.0}), "the front frame first");
    require(same_rect((*frame_of(layout, "doc:a")).bounds, Rect{0.0, 400.0, 600.0, 200.0}),
            "the back frame alone in the last row");
}

void test_arrange_icons_and_bounds() {
    DockLayout layout;
    framed_layout(layout);
    DockTransaction slots{};
    slots.edits.push_back(DockEdit::set_frame_state(DockPanelId("doc:a"), DockFrameState::minimized));
    slots.edits.push_back(DockEdit::set_frame_state(DockPanelId("doc:c"), DockFrameState::minimized));
    slots.edits.push_back(DockEdit::set_frame_icon_slot(DockPanelId("doc:a"), 4U));
    slots.edits.push_back(DockEdit::set_frame_icon_slot(DockPanelId("doc:c"), 7U));
    require(layout.apply(slots).applied, "two icons with gaps");
    DockFrameMetrics metrics{};
    metrics.icon_width = 100.0;
    metrics.caption_height = 20.0;
    apply_plan(layout, DockFrameArrangement::arrange_icons, Size{250.0, 400.0}, metrics);
    require((*frame_of(layout, "doc:a")).icon_slot == 0U, "the lower slot comes first");
    require((*frame_of(layout, "doc:c")).icon_slot == 1U, "the gap closes");
    // Two icons fit a row of 250: slot 2 starts the second row, up from
    // the bottom.
    require(same_rect(gui_forms::dock_frame_icon_bounds(2U, Size{250.0, 400.0}, metrics),
                      Rect{0.0, 360.0, 100.0, 20.0}),
            "the third icon starts the second row");
    metrics.icons_at_bottom = false;
    require(same_rect(gui_forms::dock_frame_icon_bounds(1U, Size{250.0, 400.0}, metrics),
                      Rect{100.0, 0.0, 100.0, 20.0}),
            "icons along the top");
    DockFrameMemory maximized{};
    maximized.state = DockFrameState::maximized;
    maximized.bounds = Rect{5.0, 5.0, 50.0, 50.0};
    require(same_rect(gui_forms::dock_frame_shown_bounds(maximized, Size{250.0, 400.0}, metrics),
                      Rect{0.0, 0.0, 250.0, 400.0}),
            "a maximized frame fills the area");
    maximized.state = DockFrameState::normal;
    require(same_rect(gui_forms::dock_frame_shown_bounds(maximized, Size{250.0, 400.0}, metrics),
                      Rect{5.0, 5.0, 50.0, 50.0}),
            "a normal frame shows at its bounds");
    const DockTransaction empty =
        gui_forms::plan_frame_arrangement(layout, DockFrameArrangement::cascade, Size{0.0, 400.0}, metrics);
    require(empty.edits.empty(), "an empty area plans nothing");
}

} // namespace

int main() {
    test_presentation_and_defaults();
    test_select_brings_to_front();
    test_frame_edits_and_refusals();
    test_states_and_icon_slots();
    test_lock_allows_frame_edits();
    test_change_is_not_frames_only();
    test_save_and_restore();
    test_restore_refuses_bad_frames();
    test_cascade();
    test_tiles();
    test_tile_grid();
    test_arrange_icons_and_bounds();
    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "dock layout frames tests passed\n";
    return 0;
}
