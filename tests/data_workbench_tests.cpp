// The data-workbench demonstrator (demo/data_workbench/data_workbench.hpp):
// fifty thousand virtual orders, the find panel's filter and its summaries,
// the program's sort, the column chooser, layout save and restore, and the
// dashboard view.
#include "data_workbench.hpp"

#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/checked_list_box.hpp"
#include "gui_forms/controls/choice_group.hpp"
#include "gui_forms/controls/data_grid_view.hpp"
#include "gui_forms/controls/label.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/controls/search_field.hpp"
#include "gui_forms/controls/text_box.hpp"
#include "gui_forms/window/window.hpp"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
using namespace gui_forms;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

template <typename Kind>
std::shared_ptr<Kind> find_as(Window& window, std::string_view id) {
    const std::shared_ptr<Kind> found = std::dynamic_pointer_cast<Kind>(window.find(id));
    require(found != nullptr, "the workbench holds every control it names");
    return found;
}

void press(Window& window, std::string_view id) {
    const std::shared_ptr<Button> button = find_as<Button>(window, id);
    (*button).clicked().emit(*button);
    window.perform_layout();
}

// The orders generated from their index match the grid's text.
void test_generated_orders() {
    const data_workbench::Order first = data_workbench::order_at(0U);
    require(first.number == 100000 && first.quantity >= 1 && first.total_cents == first.quantity * first.price_cents,
            "an order's total is its quantity times its price");
    require(data_workbench::money_text(123456) == "1234.56" && data_workbench::money_text(5) == "0.05",
            "money is written with two decimals");
    require(data_workbench::order_text(first, 0U) == "100000", "the first column is the order number");
    require(data_workbench::order_text(first, 1U).starts_with("2026-"), "dates fall in 2026");
}

void test_find_sort_and_summaries(Window& window) {
    const std::shared_ptr<DataGridView> grid = find_as<DataGridView>(window, "workbench.grid");
    require((*grid).row_count() == data_workbench::order_count, "the grid shows every order");
    require((*find_as<Label>(window, "workbench.summary.0")).text.read() == "Count 50000",
            "the count summary starts at every order");

    std::size_t expected = 0U;
    for (std::size_t index = 0; index < data_workbench::order_count; ++index) {
        if (data_workbench::order_text(data_workbench::order_at(index), 2U) == "Kestrel Marine") ++expected;
    }
    const std::shared_ptr<SearchField> find = find_as<SearchField>(window, "workbench.find");
    (*(*find).editor()).text = "kestrel";
    (*find).search_now();
    window.perform_layout();
    require((*grid).row_count() == expected, "the find panel keeps the matching orders");
    require((*find_as<Label>(window, "workbench.summary.0")).text.read() == "Count " + std::to_string(expected),
            "the count summary follows the filter");

    (*grid).column_header_click().emit(5U);
    window.perform_layout();
    const std::string first = (*grid).cell_text(DataGridViewRowPosition{0U}, 5U);
    const std::string last = (*grid).cell_text(DataGridViewRowPosition{expected - 1U}, 5U);
    require(std::stoi(first) <= std::stoi(last), "a header click sorts the quantities up");

    (*(*find).editor()).text = "";
    (*find).search_now();
    window.perform_layout();
    require((*grid).row_count() == data_workbench::order_count, "an empty find shows every order again");
}

void test_chooser_and_layouts(Window& window) {
    const std::shared_ptr<DataGridView> grid = find_as<DataGridView>(window, "workbench.grid");
    const std::shared_ptr<CheckedListBox> chooser = find_as<CheckedListBox>(window, "workbench.chooser.list");
    press(window, "workbench.columns");
    require((*find_as<Panel>(window, "workbench.chooser")).visible.read(), "Columns opens the chooser");
    (*chooser).set_item_checked(2U, false);
    window.perform_layout();
    require(!(*grid).columns()[2].visible, "clearing a column hides it");

    press(window, "workbench.layout.save");
    (*grid).set_column_width(4U, 260.0);
    (*chooser).set_item_checked(2U, true);
    window.perform_layout();
    press(window, "workbench.layout.restore");
    require((*grid).columns()[4].width == 140.0 && !(*grid).columns()[2].visible,
            "Restore puts back the saved widths and visibility");
    require(!(*chooser).item_checked(2U), "the chooser follows a restored layout");
    press(window, "workbench.layout.reset");
    require((*grid).columns()[2].visible, "Reset shows every column again");
}

void test_dashboard(Window& window) {
    const std::shared_ptr<ChoiceGroup> views = find_as<ChoiceGroup>(window, "workbench.view");
    (*views).selected_index = 1;
    window.perform_layout();
    require((*find_as<Panel>(window, "workbench.dashboard")).visible.read() &&
                !(*find_as<Panel>(window, "workbench.orders")).visible.read(),
            "the view switch shows the dashboard");
    require((*find_as<Label>(window, "workbench.dashboard.figure.0.value")).text.read() == "50000",
            "the dashboard counts every order");
}

} // namespace

int main() {
    test_generated_orders();
    std::unique_ptr<Window> window = data_workbench::make_data_workbench();
    test_find_sort_and_summaries(*window);
    test_chooser_and_layouts(*window);
    test_dashboard(*window);
    std::cout << "gui_forms_data_workbench_tests: passed\n";
    return EXIT_SUCCESS;
}
