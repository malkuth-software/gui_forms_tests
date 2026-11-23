#include "support.hpp"
#include "gui_forms/controls/button.hpp"
#include "gui_forms/controls/callout.hpp"
#include "gui_forms/controls/card.hpp"
#include "gui_forms/controls/choice_group.hpp"
#include "gui_forms/controls/data_grid_view.hpp"
#include "gui_forms/controls/drawing_surface.hpp"
#include "gui_forms/controls/expandable_sections.hpp"
#include "gui_forms/controls/label.hpp"
#include "gui_forms/controls/list_box.hpp"
#include "gui_forms/controls/numeric_up_down.hpp"
#include "gui_forms/controls/progress_bar.hpp"
#include "gui_forms/controls/scaled_panel.hpp"
#include "gui_forms/controls/search_field.hpp"
#include "gui_forms/controls/text_box.hpp"
#include "gui_forms/controls/track_bar.hpp"

#include <memory>
#include <string>

// The DevExpress "demo only" composition pages (notes: planning/audits/
// DEVEXPRESS_OVERKILL_SURVEY_2026-11-22.md) and the page header's Source
// button: each page builds from stock parts and its controls do what the
// page says.
namespace gallery_pages_tests {
namespace {

template <typename Kind>
std::shared_ptr<Kind> find_as(Window& window, std::string_view id) {
    const std::shared_ptr<Kind> found = std::dynamic_pointer_cast<Kind>(window.find(id));
    if (!found) throw std::runtime_error("missing control " + std::string(id));
    return found;
}

std::string label_text(Window& window, std::string_view id) {
    return (*find_as<Label>(window, id)).text.read();
}

} // namespace

// Zoom stops, the duration carry, the rating and the grid lookup's popup.
void test_lookups_and_pickers(Window& window) {
    select_page(window, "lookups-and-pickers");
    const std::shared_ptr<TrackBar> zoom = find_as<TrackBar>(window, "gallery.lookups.zoom.track");
    require((*zoom).value.read() == 100.0, "the zoom starts at 100%");
    click(window, window.find("gallery.lookups.zoom.in"));
    require((*zoom).value.read() == 125.0, "zoom in steps to the next stop");
    require(label_text(window, "gallery.lookups.zoom.value") == "125%", "the zoom label follows the slider");
    click(window, window.find("gallery.lookups.zoom.out"));
    click(window, window.find("gallery.lookups.zoom.out"));
    require((*zoom).value.read() == 75.0, "zoom out steps back through the stops");

    const std::shared_ptr<NumericUpDown> seconds = find_as<NumericUpDown>(window, "gallery.lookups.duration.seconds");
    const std::shared_ptr<NumericUpDown> minutes = find_as<NumericUpDown>(window, "gallery.lookups.duration.minutes");
    (*seconds).value = 60.0;
    window.perform_layout();
    require((*seconds).value.read() == 0.0 && (*minutes).value.read() == 3.0,
            "seconds past 59 carry into minutes");
    require(label_text(window, "gallery.lookups.duration.total").find("3780 s in all") != std::string::npos,
            "the duration box totals its parts");

    click(window, window.find("gallery.lookups.rating.clear"));
    require(label_text(window, "gallery.lookups.rating.value") == "0 of 5 stars", "Clear empties the rating");

    const std::shared_ptr<Button> customer = find_as<Button>(window, "gallery.lookups.grid.button");
    click(window, customer);
    window.perform_layout();
    require((*customer).expanded_state.read().present() && *(*customer).expanded_state.read().value(),
            "the grid lookup opens its popup");
    const std::shared_ptr<DataGridView> grid = find_as<DataGridView>(window, "gallery.lookups.grid.rows");
    require((*grid).row_count() == 40U, "the lookup lists forty customers");
    const std::shared_ptr<SearchField> search = find_as<SearchField>(window, "gallery.lookups.grid.search");
    (*(*search).editor()).text = "porto";
    (*search).search_now();
    window.perform_layout();
    require((*grid).row_count() == 10U, "the search keeps the Porto customers");
    click(window, customer);
    window.perform_layout();
    require(!*(*customer).expanded_state.read().value(), "a second click closes the popup");
}

// The slider steps, the transition effect choice and the carousel dots.
void test_media_and_motion(Window& window) {
    select_page(window, "media-and-motion");
    require(label_text(window, "gallery.media.slider.caption").find("1 of 8") != std::string::npos,
            "the slider starts on the first slide");
    click(window, window.find("gallery.media.slider.next"));
    // A tick-free test window lands the move only on later ticks; the
    // caption names the target at once.
    require(label_text(window, "gallery.media.slider.caption").find("2 of 8") != std::string::npos,
            "Next moves to the second slide");
    const std::shared_ptr<ChoiceGroup> effects = find_as<ChoiceGroup>(window, "gallery.media.transitions.effect");
    (*effects).selected_index = 3;
    require(label_text(window, "gallery.media.status") == "Effect: wipe", "choosing an effect reports it");
    click(window, window.find("gallery.media.carousel.dot.2"));
    require((*find_as<DrawingSurface>(window, "gallery.media.carousel.view")).accessible_name.read() ==
                "Carousel, card 3 of 5",
            "a dot brings its card to the front");
    click(window, window.find("gallery.media.carousel.autoplay"));
    require((*find_as<Button>(window, "gallery.media.carousel.autoplay")).text.read() == "Autoplay: on",
            "autoplay toggles on");
    click(window, window.find("gallery.media.carousel.autoplay"));
}

// The accordion mode, the drawer, command search and the tour's first step.
void test_navigation_patterns(Window& window) {
    select_page(window, "navigation-patterns");
    const std::shared_ptr<ExpandableSections> accordion =
        find_as<ExpandableSections>(window, "gallery.navigation.accordion.sections");
    require((*accordion).single_open.read() && (*accordion).expanded(0), "the accordion opens its first section");
    click(window, window.find("gallery.navigation.accordion.mode"));
    require(!(*accordion).single_open.read(), "the mode button allows several sections");

    const std::shared_ptr<Button> drawer = find_as<Button>(window, "gallery.navigation.drawer.toggle");
    click(window, drawer);
    require(*(*drawer).expanded_state.read().value(), "the menu button opens the drawer");

    const std::shared_ptr<SearchField> search = find_as<SearchField>(window, "gallery.navigation.commands.search");
    const std::shared_ptr<ListBox> commands = find_as<ListBox>(window, "gallery.navigation.commands.list");
    (*(*search).editor()).text = "zoom";
    (*search).search_now();
    window.perform_layout();
    require((*commands).items.read().size() == 2U, "command search narrows to the zoom commands");
    (*commands).item_activated().emit(0U);
    require(label_text(window, "gallery.navigation.status") == "Ran \"Zoom in\"", "activating a command runs it");

    click(window, window.find("gallery.navigation.tour.start"));
    window.perform_layout();
    require(label_text(window, "gallery.navigation.tour.progress") == "Step 1: Accordion",
            "the tour starts at the accordion");
    const std::shared_ptr<Callout> tour = find_as<Callout>(window, "gallery.navigation.tour.callout");
    static_cast<void>((*tour).close());
    window.perform_layout();
}

// The widget layouts and maximize, the gauge, card paging and the template.
void test_dashboard_pieces(Window& window) {
    select_page(window, "dashboard-pieces");
    const std::shared_ptr<Card> revenue = find_as<Card>(window, "gallery.dashboard.widgets.card.0");
    const std::shared_ptr<Card> orders = find_as<Card>(window, "gallery.dashboard.widgets.card.1");
    (*revenue).activated().emit(*revenue);
    require((*revenue).visible.read() && !(*orders).visible.read(), "activating a widget maximizes it");
    (*revenue).activated().emit(*revenue);
    require((*orders).visible.read(), "activating it again restores the others");

    const std::shared_ptr<TrackBar> cpu = find_as<TrackBar>(window, "gallery.dashboard.gauge.cpu");
    (*cpu).value = 90.0;
    require(label_text(window, "gallery.dashboard.gauge.cpu.label") == "CPU  90%", "the gauge follows its slider");

    require(label_text(window, "gallery.dashboard.cards.page") == "Page 1 of 2", "the card view pages six at a time");
    click(window, window.find("gallery.dashboard.cards.next"));
    require(label_text(window, "gallery.dashboard.cards.page") == "Page 2 of 2", "Next shows the second page");

    const std::shared_ptr<TextBox> markup = find_as<TextBox>(window, "gallery.dashboard.template.markup");
    (*markup).text = "<card><title>{name}</title><blink>{city}</blink></card>";
    click(window, window.find("gallery.dashboard.template.apply"));
    require(label_text(window, "gallery.dashboard.template.status").find("unknown element <blink>") !=
                std::string::npos,
            "an unknown template element is refused");
    (*markup).text = "<card><title>{name}</title><line>{city}</line></card>";
    click(window, window.find("gallery.dashboard.template.apply"));
    require(label_text(window, "gallery.dashboard.template.status") == "Applied: 2 elements per card.",
            "a good template is applied");
    require(window.find("gallery.dashboard.cards.card.6.1") != nullptr, "the cards are rebuilt from the template");
    click(window, window.find("gallery.dashboard.template.reset"));
}

// The workspace presets and the chat panel's offline reply.
void test_workspaces_and_themes(Window& window) {
    select_page(window, "workspaces-and-themes");
    const std::shared_ptr<ChoiceGroup> workspace = find_as<ChoiceGroup>(window, "gallery.workspaces.dock.choice");
    (*workspace).selected_index = 1;
    require(label_text(window, "gallery.workspaces.status").starts_with("Workspace Debugging"),
            "choosing a workspace applies its named layout");
    click(window, window.find("gallery.workspaces.dock.save"));
    require((*workspace).selected_index.read() == 3, "Save as Custom selects Custom");

    const std::shared_ptr<TextBox> prompt = find_as<TextBox>(window, "gallery.workspaces.chat.prompt");
    (*prompt).text = "tell me about docking";
    click(window, window.find("gallery.workspaces.chat.send"));
    const std::shared_ptr<TextBox> transcript = find_as<TextBox>(window, "gallery.workspaces.chat.transcript");
    require((*transcript).text.read().find("You: tell me about docking") != std::string::npos,
            "the message joins the transcript");
    require((*prompt).text.read().empty(), "sending clears the prompt");
}

// The template switch, the sign-in check and the record navigation.
void test_ui_templates(Window& window) {
    select_page(window, "ui-templates");
    require((*window.find("gallery.templates.sign-in.document")).visible.read(), "the sign-in template shows first");
    click(window, window.find("gallery.templates.sign-in.submit"));
    require(label_text(window, "gallery.templates.sign-in.message") == "Enter an email address",
            "signing in without an email asks for one");
    const std::shared_ptr<ChoiceGroup> chooser = find_as<ChoiceGroup>(window, "gallery.templates.choice");
    (*chooser).selected_index = 2;
    require((*window.find("gallery.templates.record.document")).visible.read() &&
                !(*window.find("gallery.templates.sign-in.document")).visible.read(),
            "choosing Record shows the record screen alone");
    require(label_text(window, "gallery.templates.record.position") == "Record 1 of 3", "the record screen starts at one");
    click(window, window.find("gallery.templates.record.next"));
    require((*find_as<TextBox>(window, "gallery.templates.record.name")).text.read() == "Bao Tran",
            "Next shows the second record");
    (*chooser).selected_index = 3;
    const std::shared_ptr<ProgressBar> quota = find_as<ProgressBar>(window, "gallery.templates.dashboard.quota.bar");
    require((*quota).value.read() == 68.0, "the dashboard's quota starts at 68%");
}

// The page header's Source button swaps the body for the page's files.
void test_page_source_button(Window& window) {
    select_page(window, "lookups-and-pickers");
    const std::shared_ptr<Button> source = find_as<Button>(window, "gallery.surface.lookups-and-pickers.source");
    click(window, source);
    window.perform_layout();
    require((*source).text.read() == "Back to page", "Source shows the page's files");
    require(!(*window.find("gallery.body.lookups-and-pickers")).visible.read(), "the page body hides");
    const std::shared_ptr<TextBox> text = find_as<TextBox>(window, "gallery.surface.lookups-and-pickers.source-view.text");
    require((*text).text.read().find("class LookupsAndPickersPage") != std::string::npos,
            "the viewer shows the page's builder first");
    const std::shared_ptr<ChoiceGroup> files =
        find_as<ChoiceGroup>(window, "gallery.surface.lookups-and-pickers.source-view.files");
    (*files).selected_index = 1;
    require((*text).text.read().find("class PopupPanel") != std::string::npos,
            "the shared demo parts are the page's second file");
    click(window, source);
    window.perform_layout();
    require((*window.find("gallery.body.lookups-and-pickers")).visible.read(), "Back to page shows the body again");

    select_page(window, "basics");
    click(window, window.find("gallery.surface.basics.source"));
    window.perform_layout();
    require((*find_as<Label>(window, "gallery.surface.basics.source-view.path")).text.read().starts_with(
                "demo/gallery_form_pages.form"),
            "a window form page shows the window's form pages document");
    click(window, window.find("gallery.surface.basics.source"));
}

} // namespace gallery_pages_tests
