// IC3: the property families' value rules, moved out of prose into
// declarations (gui_forms include/**/<family>.ic.toml), are what the setters
// check. prepare_write now calls the predicate generated from each clause
// (admits_<property> in <family>_ic_checks.hpp); these cases hold each
// boundary the clause states, from both sides, through the public property:
// a value the clause admits is kept, one it refuses throws
// std::invalid_argument and changes nothing, NaN included where the clause
// says finite.

#include "gui_forms/components/context_menu/context_menu.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/badge.hpp"
#include "gui_forms/controls/label.hpp"
#include "gui_forms/controls/raster_canvas.hpp"
#include "gui_forms/controls/sparkline.hpp"
#include "gui_forms/controls/split_container.hpp"
#include "gui_forms/controls/table_layout_panel_compat.hpp"
#include "gui_forms/controls/track_bar.hpp"
#include "gui_forms/types/geometry/geometry.hpp"

#include <stxlib/optional.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using namespace gui_forms;

int failures = 0;

void require(bool condition, const std::string& message) {
    if (condition) return;
    ++failures;
    std::cerr << "gui_forms_ic_property_clauses_tests: " << message << '\n';
}

constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinite = std::numeric_limits<double>::infinity();

// ContextMenu.preferred_width: finite(incoming) and incoming >= 120 and incoming <= 1024.
bool context_menu_width_admits(double width) {
    const std::shared_ptr<ContextMenu> menu = std::make_shared<ContextMenu>("ic.clauses.context");
    const double before = (*menu).preferred_width.read();
    bool admitted = true;
    try {
        (*menu).preferred_width = width;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    if (!admitted && (*menu).preferred_width.read() != before) {
        require(false, "a refused ContextMenu width changed the property");
    }
    return admitted;
}

// Badge.maximum: incoming != 0.
bool badge_maximum_admits(std::uint32_t maximum) {
    const std::shared_ptr<Badge> badge = make_control<Badge>(StableId("ic.clauses.badge"));
    bool admitted = true;
    try {
        (*badge).maximum = maximum;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    return admitted;
}

// TrackBar.tick_frequency: finite(incoming) and incoming > 0 (the finite
// half is require_finite's, which runs first).
bool track_bar_frequency_admits(double frequency) {
    const std::shared_ptr<TrackBar> bar = make_control<TrackBar>(StableId("ic.clauses.track-bar"));
    bool admitted = true;
    try {
        (*bar).tick_frequency = frequency;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    return admitted;
}

// TableLayoutPanelCompat.track_spacing: each side finite and in [0, 256].
bool table_spacing_admits(Size spacing) {
    const std::shared_ptr<TableLayoutPanelCompat> table =
        make_control<TableLayoutPanelCompat>(StableId("ic.clauses.table"));
    bool admitted = true;
    try {
        (*table).track_spacing = spacing;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    return admitted;
}

void test_context_menu_width() {
    require(context_menu_width_admits(120.0), "ContextMenu width 120 is the clause's lower bound");
    require(context_menu_width_admits(1024.0), "ContextMenu width 1024 is the clause's upper bound");
    require(!context_menu_width_admits(119.5), "ContextMenu width below 120 is refused");
    require(!context_menu_width_admits(1024.5), "ContextMenu width above 1024 is refused");
    require(!context_menu_width_admits(not_a_number), "ContextMenu width NaN is refused (finite)");
    require(!context_menu_width_admits(infinite), "ContextMenu width infinity is refused (finite)");
}

void test_badge_maximum() {
    require(badge_maximum_admits(1U), "Badge maximum 1 is admitted");
    require(!badge_maximum_admits(0U), "Badge maximum 0 is refused");
}

void test_track_bar_frequency() {
    require(track_bar_frequency_admits(0.5), "TrackBar tick frequency 0.5 is admitted");
    require(!track_bar_frequency_admits(0.0), "TrackBar tick frequency 0 is refused");
    require(!track_bar_frequency_admits(-1.0), "TrackBar tick frequency below 0 is refused");
    require(!track_bar_frequency_admits(not_a_number), "TrackBar tick frequency NaN is refused");
}

void test_table_spacing() {
    require(table_spacing_admits(Size{0.0, 256.0}), "TableLayoutPanelCompat spacing 0 by 256 is admitted");
    require(!table_spacing_admits(Size{-0.5, 4.0}), "a negative TableLayoutPanelCompat width is refused");
    require(!table_spacing_admits(Size{4.0, 256.5}), "a TableLayoutPanelCompat height above 256 is refused");
    require(!table_spacing_admits(Size{not_a_number, 4.0}), "a NaN TableLayoutPanelCompat width is refused");
}

// Label.line_spacing: finite(incoming) and 4 * incoming >= 3 and incoming <= 3 (0.75 to 3).
bool label_spacing_admits(double spacing) {
    const std::shared_ptr<Label> label = make_control<Label>(StableId("ic.clauses.label"));
    bool admitted = true;
    try {
        (*label).line_spacing = spacing;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    return admitted;
}

// RasterCanvas.transparency_cell_size: finite and 0, or within [2, 128].
bool canvas_cell_admits(double size) {
    const std::shared_ptr<RasterCanvas> canvas = make_control<RasterCanvas>(StableId("ic.clauses.canvas"));
    bool admitted = true;
    try {
        (*canvas).transparency_cell_size = size;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    return admitted;
}

// Sparkline.capacity: incoming >= 2 and incoming <= 100000.
bool sparkline_capacity_admits(std::uint32_t capacity) {
    const std::shared_ptr<Sparkline> line = make_control<Sparkline>(StableId("ic.clauses.sparkline"));
    bool admitted = true;
    try {
        (*line).capacity = capacity;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    return admitted;
}

// SplitContainer.first_minimum: finite and nonnegative, and at most first_maximum when one is
// set (the term first_maximum_bound, unbounded when it is unset).
bool split_minimum_admits(SplitContainer& split, double minimum) {
    bool admitted = true;
    try {
        split.first_minimum = minimum;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    return admitted;
}

// Control.minimum_size: each side finite and nonnegative, and within a maximum side above zero.
bool minimum_size_admits(Control& control, Size size) {
    bool admitted = true;
    try {
        control.minimum_size = size;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    return admitted;
}

// Control.maximum_size: each side finite and nonnegative, and a side above zero at least the minimum's.
bool maximum_size_admits(Control& control, Size size) {
    bool admitted = true;
    try {
        control.maximum_size = size;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    return admitted;
}

void test_label_line_spacing() {
    require(label_spacing_admits(0.75), "Label line spacing 0.75 is the clause's lower bound");
    require(label_spacing_admits(3.0), "Label line spacing 3 is the clause's upper bound");
    require(!label_spacing_admits(0.7499), "Label line spacing below 0.75 is refused");
    require(!label_spacing_admits(3.0001), "Label line spacing above 3 is refused");
    require(!label_spacing_admits(not_a_number), "Label line spacing NaN is refused");
}

void test_raster_canvas_cell_size() {
    require(canvas_cell_admits(0.0), "a zero transparency cell is admitted");
    require(canvas_cell_admits(2.0), "a transparency cell of 2 is admitted");
    require(canvas_cell_admits(128.0), "a transparency cell of 128 is admitted");
    require(!canvas_cell_admits(1.0), "a transparency cell of 1 is refused");
    require(!canvas_cell_admits(128.5), "a transparency cell above 128 is refused");
    require(!canvas_cell_admits(infinite), "an infinite transparency cell is refused");
}

void test_sparkline_capacity() {
    require(sparkline_capacity_admits(2U), "Sparkline capacity 2 is admitted");
    require(sparkline_capacity_admits(100000U), "Sparkline capacity 100000 is admitted");
    require(!sparkline_capacity_admits(1U), "Sparkline capacity 1 is refused");
    require(!sparkline_capacity_admits(100001U), "Sparkline capacity above 100000 is refused");
}

void test_split_container_minimum() {
    const std::shared_ptr<SplitContainer> split = make_control<SplitContainer>(StableId("ic.clauses.split"));
    require(split_minimum_admits(*split, 1000.0), "an unbounded first minimum takes any finite extent");
    require(!split_minimum_admits(*split, -1.0), "a negative first minimum is refused");
    require(split_minimum_admits(*split, 40.0), "first minimum 40 is admitted");
    (*split).first_maximum = stxlib::Optional<double>::of(100.0);
    require(split_minimum_admits(*split, 100.0), "a first minimum equal to its maximum is admitted");
    require(!split_minimum_admits(*split, 100.5), "a first minimum above its maximum is refused");
}

void test_control_size_bounds() {
    const std::shared_ptr<Label> control = make_control<Label>(StableId("ic.clauses.control"));
    require(maximum_size_admits(*control, Size{200.0, 0.0}), "a maximum width with an unbounded height");
    require(minimum_size_admits(*control, Size{200.0, 5000.0}), "a minimum at the maximum width, any height");
    require(!minimum_size_admits(*control, Size{200.5, 10.0}), "a minimum wider than the maximum is refused");
    require(!maximum_size_admits(*control, Size{199.0, 0.0}), "a maximum narrower than the minimum is refused");
    require(maximum_size_admits(*control, Size{0.0, 0.0}), "a zero maximum is unbounded");
    require(!minimum_size_admits(*control, Size{-1.0, 0.0}), "a negative minimum is refused");
    require(!maximum_size_admits(*control, Size{not_a_number, 0.0}), "a NaN maximum is refused");
}

} // namespace

int main() {
    test_context_menu_width();
    test_badge_maximum();
    test_track_bar_frequency();
    test_table_spacing();
    test_label_line_spacing();
    test_raster_canvas_cell_size();
    test_sparkline_capacity();
    test_split_container_minimum();
    test_control_size_bounds();
    if (failures != 0) {
        std::cerr << failures << " property clause failures\n";
        return EXIT_FAILURE;
    }
    std::cout << "gui_forms_ic_property_clauses_tests: every clause holds at its bounds\n";
    return EXIT_SUCCESS;
}
