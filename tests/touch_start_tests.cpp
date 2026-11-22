// The touch start demonstrator (demo/touch_start): the touch seams' mouse
// behaviour (a tap within the slop, a direct pan past it, the inertia and
// overscroll stubs that touch fills in), the tile layout and its keyboard
// neighbours, and the field's wheel, click, drag, key, launch, live-tile
// and semantic paths through the window.
#include "touch_input.hpp"
#include "touch_start.hpp"

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/semantics/types/semantic_types.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/window/window.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace gui_forms;
namespace touch = gui_forms::touch_start;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double left, double right) {
    return std::abs(left - right) < 0.001;
}

touch::TouchSample at(double x, double y, std::uint64_t milliseconds = 0U) {
    const touch::TouchSample sample{.position = Point{x, y}, .timestamp_nanoseconds = milliseconds * 1'000'000U};
    return sample;
}

void test_tap_within_slop() {
    touch::GestureClassifier classifier{};
    classifier.press(touch::PointerKind::mouse, at(100.0, 100.0));
    require(classifier.pressed() && near(classifier.slop(), 4.0), "a mouse press takes the mouse slop");
    const touch::PanStep small = classifier.move(at(103.0, 101.0, 10U));
    require(!small.panning && !classifier.panning(), "travel within the slop is no pan");
    // Held long and still: the mouse has no press-and-hold, only a click.
    const touch::GestureClassification gesture = classifier.release(at(103.0, 101.0, 2000U));
    require(gesture.kind == touch::GestureKind::tap && near(gesture.position.x, 103.0),
            "a release within the slop is a tap where it lifted");
    require(!classifier.pressed(), "the classifier is idle after a release");
    require(classifier.release(at(0.0, 0.0)).kind == touch::GestureKind::none, "a release with no press is nothing");
}

void test_pan_past_slop() {
    touch::GestureClassifier classifier{};
    classifier.press(touch::PointerKind::mouse, at(200.0, 50.0));
    const touch::PanStep first = classifier.move(at(190.0, 50.0, 8U));
    require(first.panning && first.phase == touch::TouchPhase::began && near(first.delta.x, -10.0),
            "the first move past the slop begins the pan and catches up the travel");
    const touch::PanStep second = classifier.move(at(170.0, 52.0, 16U));
    require(second.panning && second.phase == touch::TouchPhase::changed && near(second.delta.x, -20.0) &&
                near(second.delta.y, 2.0),
            "later moves pan by the travel since the last");
    const touch::GestureClassification gesture = classifier.release(at(170.0, 52.0, 24U));
    // TODO(touch): with the velocity fit, a fast release here is a flick.
    require(gesture.kind == touch::GestureKind::pan && near(classifier.estimated_velocity().x, 0.0),
            "a pan ends where it was let go while the flick is unfilled");
}

void test_cancel_and_touch_slop() {
    touch::GestureClassifier classifier{};
    classifier.press(touch::PointerKind::mouse, at(0.0, 0.0));
    static_cast<void>(classifier.move(at(30.0, 0.0)));
    classifier.cancel();
    require(!classifier.pressed() && !classifier.panning(), "cancel drops the press and the pan");
    require(classifier.release(at(30.0, 0.0)).kind == touch::GestureKind::none, "a cancelled press reports nothing");

    touch::GestureClassifier finger{};
    finger.press(touch::PointerKind::touch, at(0.0, 0.0));
    require(near(finger.slop(), 10.0), "a touch press takes the finger's slop");
    require(!finger.move(at(8.0, 0.0)).panning, "a finger's eight pixels are still a tap");
    touch::GestureClassifier mouse{};
    mouse.press(touch::PointerKind::mouse, at(0.0, 0.0));
    require(mouse.move(at(8.0, 0.0)).panning, "a mouse's eight pixels pan");
}

void test_inertia_and_overscroll_seams() {
    touch::PanInertia inertia{};
    inertia.start(900.0);
    // TODO(touch): active, decaying, with a positive first step.
    require(!inertia.active() && near(inertia.step(0.016), 0.0), "the inertia stub never carries a pan on");
    require(near(touch::settle_pan_offset(-40.0, 500.0, 1200.0), 0.0) &&
                near(touch::settle_pan_offset(560.0, 500.0, 1200.0), 500.0) &&
                near(touch::settle_pan_offset(120.0, 500.0, 1200.0), 120.0) &&
                near(touch::settle_pan_offset(30.0, -10.0, 1200.0), 0.0),
            "the pan clamps hard at both ends");
    require(near(touch::spring_back_target(-12.0, 500.0), 0.0) && near(touch::spring_back_target(520.0, 500.0), 500.0) &&
                near(touch::spring_back_target(250.0, 500.0), 250.0),
            "spring back returns to the nearer end");
    const touch::GestureClassification flick = touch::GestureClassification::flick(Point{1.0, 2.0}, Point{-800.0, 0.0});
    require(flick.kind == touch::GestureKind::flick && near(flick.velocity.x, -800.0),
            "the flick factory carries its velocity");
}

bool overlaps(Rect left, Rect right) {
    return left.x < right.x + right.width && right.x < left.x + left.width && left.y < right.y + right.height &&
           right.y < left.y + left.height;
}

void test_layout() {
    require(touch::rows_for_height(720.0) == 7U, "a 720 field holds seven rows");
    require(touch::rows_for_height(200.0) == touch::minimum_rows, "a short field keeps the minimum rows");
    require(touch::rows_for_height(4000.0) == touch::maximum_rows, "a tall field stops at the maximum rows");
    const std::span<const touch::TileGroup> groups = touch::start_groups();
    const touch::TileLayout layout = touch::lay_out_tiles(groups, 720.0);
    std::size_t total = 0U;
    for (const touch::TileGroup& group : groups) total += group.tiles.size();
    require(layout.tiles.size() == total && layout.groups.size() == groups.size(), "every tile and group is placed");
    for (std::size_t index = 0U; index < layout.tiles.size(); ++index) {
        const touch::TilePlacement& placement = layout.tiles[index];
        const touch::TileSpec& spec = touch::tile_spec(groups, placement);
        const double width = static_cast<double>(touch::tile_columns(spec.size)) * (touch::tile_unit + touch::tile_gap) -
                             touch::tile_gap;
        require(near(placement.bounds.width, width), "a tile is as wide as its units");
        const touch::GroupPlacement& group = layout.groups[placement.group];
        require(placement.bounds.x >= group.header.x && placement.bounds.x + placement.bounds.width <= group.right + 0.001,
                "a tile lies within its group");
        require(placement.bounds.y >= touch::surface_top + touch::group_header, "tiles start under the group names");
        for (std::size_t other = index + 1U; other < layout.tiles.size(); ++other) {
            require(!overlaps(placement.bounds, layout.tiles[other].bounds), "tiles never overlap");
        }
    }
    for (std::size_t group = 1U; group < layout.groups.size(); ++group) {
        require(near(layout.groups[group].header.x, layout.groups[group - 1U].right + touch::group_gap),
                "groups follow each other a group gap apart");
    }
    require(layout.extent > touch::window_size.width, "the surface is wider than the window: it pans");
    require(touch::tile_id(groups, layout, 0U) == "weather" && touch::face_count(touch::tile_spec(groups, layout.tiles[0])) == 3U &&
                touch::live_tile(touch::tile_spec(groups, layout.tiles[0])),
            "the first tile is the live weather tile");
}

void test_neighbours_and_reveal() {
    const std::span<const touch::TileGroup> groups = touch::start_groups();
    const touch::TileLayout layout = touch::lay_out_tiles(groups, 720.0);
    require(touch::neighbour_tile(layout, 0U, touch::TileDirection::left).absent() &&
                touch::neighbour_tile(layout, 0U, touch::TileDirection::up).absent(),
            "the first tile has nothing left of or above it");
    const stxlib::Optional<std::size_t> below = touch::neighbour_tile(layout, 0U, touch::TileDirection::down);
    require(below.present() && layout.tiles[*below.value()].bounds.y > layout.tiles[0].bounds.y &&
                layout.tiles[*below.value()].group == 0U,
            "down from the first tile stays in its group, lower");
    const stxlib::Optional<std::size_t> right = touch::neighbour_tile(layout, 0U, touch::TileDirection::right);
    require(right.present() && layout.tiles[*right.value()].group == 1U,
            "right from the wide first tile crosses into the next group");
    const stxlib::Optional<std::size_t> hit =
        touch::tile_at(layout, Point{layout.tiles[2].bounds.x + 4.0, layout.tiles[2].bounds.y + 4.0});
    require(hit == stxlib::Optional<std::size_t>::of(2U), "a point on a tile finds it");
    require(touch::tile_at(layout, Point{10.0, 10.0}).absent(), "the title band holds no tile");
    const std::size_t last = layout.tiles.size() - 1U;
    const double width = touch::window_size.width;
    const double revealed = touch::offset_revealing(layout, last, 0.0, width);
    require(near(revealed, touch::maximum_pan_offset(layout, width)), "revealing the last tile pans to the end");
    require(near(touch::offset_revealing(layout, 0U, revealed, width), 0.0), "revealing the first tile pans home");
}

std::shared_ptr<touch::TileField> field_of(Window& window) {
    std::shared_ptr<touch::TileField> field = std::dynamic_pointer_cast<touch::TileField>(window.find("touch.tiles"));
    require(field != nullptr, "the window holds the tile field");
    return field;
}

PointerEvent pointer(PointerAction action, Point position, PointerButton button = PointerButton::none) {
    PointerEvent event{};
    event.action = action;
    event.position = position;
    event.button = button;
    event.pointer_id = 1U;
    return event;
}

void click(Window& window, Point position) {
    static_cast<void>(window.dispatch_pointer(pointer(PointerAction::down, position, PointerButton::primary)));
    static_cast<void>(window.dispatch_pointer(pointer(PointerAction::up, position, PointerButton::primary)));
}

bool press(Window& window, std::uint32_t key) {
    const KeyEvent event{KeyAction::down, key};
    return window.dispatch_key(event);
}

void test_staging_and_keys() {
    std::unique_ptr<Window> window = touch::make_touch_start();
    const std::shared_ptr<touch::TileField> field = field_of(*window);
    require(touch::check_touch_start_tile(*window, "map") && (*field).selected_tile_id() == "map" &&
                near((*field).pan_offset(), 0.0),
            "--check selects the tile it names, without panning one already in view");
    require(touch::check_touch_start_tile(*window, "network") && (*field).selected_tile_id() == "network" &&
                (*field).pan_offset() > 0.0,
            "a staged tile past the window's edge is panned into view at once");
    require(!touch::check_touch_start_tile(*window, "no-such-tile"), "--check refuses an unknown tile");

    require(press(*window, PhysicalKey::home) && (*field).selected_tile_id() == "weather", "Home selects the first tile");
    require(near((*field).pan_target(), 0.0), "the first tile glides home");
    require(press(*window, PhysicalKey::right) && (*field).selected_tile_id() != "weather",
            "an arrow moves the selection");
    require(press(*window, PhysicalKey::end) && (*field).selected_tile_id() == "network", "End selects the last tile");
    require(press(*window, PhysicalKey::enter) && (*field).launch_stage() != touch::LaunchStage::tiles,
            "Enter launches the selected tile");
    require(!press(*window, PhysicalKey::right) || (*field).selected_tile_id() == "network",
            "arrows do not move the selection behind a launched tile");
    require(press(*window, PhysicalKey::escape) && ((*field).launch_stage() == touch::LaunchStage::closing ||
                                                    (*field).launch_stage() == touch::LaunchStage::tiles),
            "Escape returns to the tiles");
}

void test_pointer_and_wheel() {
    std::unique_ptr<Window> window = touch::make_touch_start();
    const std::shared_ptr<touch::TileField> field = field_of(*window);
    const touch::TileLayout layout = (*field).current_layout();
    const Rect calendar = layout.tiles[1].bounds;
    const Point on_calendar{calendar.x + 10.0, calendar.y + 10.0};
    click(*window, on_calendar);
    require((*field).selected_tile_id() == "calendar" && (*field).launch_stage() == touch::LaunchStage::tiles,
            "a click selects the tile under it");
    click(*window, on_calendar);
    require((*field).launch_stage() != touch::LaunchStage::tiles, "a click on the selected tile launches it");
    require((*field).return_to_tiles() && !(*field).return_to_tiles(), "return_to_tiles leaves the launched tile once");

    std::unique_ptr<Window> second = touch::make_touch_start();
    const std::shared_ptr<touch::TileField> panned = field_of(*second);
    const Point start{600.0, 400.0};
    static_cast<void>((*second).dispatch_pointer(pointer(PointerAction::down, start, PointerButton::primary)));
    static_cast<void>((*second).dispatch_pointer(pointer(PointerAction::move, Point{560.0, 400.0})));
    static_cast<void>((*second).dispatch_pointer(pointer(PointerAction::move, Point{500.0, 400.0})));
    static_cast<void>((*second).dispatch_pointer(pointer(PointerAction::up, Point{500.0, 400.0}, PointerButton::primary)));
    require(near((*panned).pan_offset(), 100.0), "a drag pans the surface with the pointer");
    require((*panned).selected_tile_id().empty(), "a drag selects nothing");

    std::unique_ptr<Window> third = touch::make_touch_start();
    const std::shared_ptr<touch::TileField> wheeled = field_of(*third);
    PointerEvent wheel = pointer(PointerAction::wheel, Point{600.0, 400.0});
    wheel.wheel_delta = Point{0.0, -2.0};
    wheel.wheel_unit = WheelDeltaUnit::lines;
    static_cast<void>((*third).dispatch_pointer(wheel));
    require(near((*wheeled).pan_target(), 2.0 * touch::wheel_line_pan), "a wheel notch down pans right by its lines");
    wheel.wheel_delta = Point{0.0, 30.0};
    wheel.wheel_unit = WheelDeltaUnit::pixels;
    static_cast<void>((*third).dispatch_pointer(wheel));
    require((*wheeled).pan_offset() >= 0.0, "pixel deltas pan at once and stay within the ends");
}

void test_live_tiles_and_semantics() {
    std::unique_ptr<Window> window = touch::make_touch_start();
    const std::shared_ptr<touch::TileField> field = field_of(*window);
    require((*field).tile_face(0U) == 0U, "a live tile starts on its first face");
    (*field).turn_next_live_tile(FrameClock::now());
    require((*field).tile_face(0U) == 1U && (*field).tile_face(1U) == 0U, "live tiles turn one at a time, in order");
    (*field).turn_next_live_tile(FrameClock::now());
    require((*field).tile_face(1U) == 1U, "the next live tile turns next");
    require((*field).tile_face(3U) == 0U, "a small tile never turns");

    const std::vector<SemanticNode> nodes = (*field).semantic_virtual_children();
    require(nodes.size() == (*field).current_layout().tiles.size(), "every tile is a semantic child");
    require(nodes[0].stable_id == "touch.tiles.tile.weather" && nodes[0].role == SemanticRole::list_item &&
                nodes[0].name == "Weather" && !nodes[0].description.empty(),
            "a tile's node names it and its live face");
    require((*field).on_semantic_child_action("touch.tiles.tile.tides", SemanticAction::select, {}) &&
                (*field).selected_tile_id() == "tides",
            "the select action selects the tile");
    require((*field).on_semantic_child_action("touch.tiles.tile.tides", SemanticAction::press, {}) &&
                (*field).launch_stage() != touch::LaunchStage::tiles,
            "the press action launches it");
    require(!(*field).on_semantic_child_action("touch.tiles.tile.none", SemanticAction::select, {}),
            "an unknown tile refuses actions");
    require((*field).semantic_descriptor().role == SemanticRole::list, "the field is a list of tiles");
}

} // namespace

int main() {
    try {
        test_tap_within_slop();
        test_pan_past_slop();
        test_cancel_and_touch_slop();
        test_inertia_and_overscroll_seams();
        test_layout();
        test_neighbours_and_reveal();
        test_staging_and_keys();
        test_pointer_and_wheel();
        test_live_tiles_and_semantics();
        std::cout << "gui_forms_touch_start_tests: all tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "gui_forms_touch_start_tests: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
