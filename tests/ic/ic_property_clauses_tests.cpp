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
#include "gui_forms/controls/table_layout_panel_compat.hpp"
#include "gui_forms/controls/track_bar.hpp"
#include "gui_forms/types/geometry/geometry.hpp"

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
    bool admitted = true;
    try {
        (*menu).preferred_width = width;
    } catch (const std::invalid_argument&) {
        admitted = false;
    }
    if (!admitted && (*menu).preferred_width.read() != 268.0) {
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

} // namespace

int main() {
    test_context_menu_width();
    test_badge_maximum();
    test_track_bar_frequency();
    test_table_spacing();
    if (failures != 0) {
        std::cerr << failures << " property clause failures\n";
        return EXIT_FAILURE;
    }
    std::cout << "gui_forms_ic_property_clauses_tests: every clause holds at its bounds\n";
    return EXIT_SUCCESS;
}
