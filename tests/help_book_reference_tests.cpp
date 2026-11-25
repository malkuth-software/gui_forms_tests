// HelpBook as a reference page (Pocket Reference's page view, 2026-11-24):
// leading and one shared baseline per line for body, code and link runs;
// inline code in the monospace role's face on a chip; the hover event; layout only
// when the page or the width changes, never from the pointer; definition
// lists; tables sized to their content (long names wrap inside their cell,
// links work in cells, empty cells stay empty) and pipe tables in markdown;
// code coloured by token; find in page; linked cards; sideways scrolling
// for a wide code block.

#include "gui_forms/control/control/control.hpp"
#include "gui_forms/control/stable_id/stable_id.hpp"
#include "gui_forms/controls/help_book.hpp"
#include "gui_forms/controls/panel.hpp"
#include "gui_forms/event/subscription_token/subscription_token.hpp"
#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/help/help_page/help_page.hpp"
#include "gui_forms/metrics/types/metrics_types.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/types/painter/painter.hpp"
#include "gui_forms/types/paint_types/paint_types.hpp"
#include "gui_forms/typography/font_families/font_families.hpp"
#include "gui_forms/window/window.hpp"

#include <stxlib/optional.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace gui_forms;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

// One text draw: where its baseline starts, the text and its font.
struct TextDraw final {
    Point origin{};
    std::string text{};
    FontSpec font{};
};

// Records the text and the filled boxes a paint draws.
class CapturePainter final : public Painter {
public:
    void save() override {}
    void restore() override {}
    void translate(Point) override {}
    void clip_rect(Rect) override {}
    void fill_rect(Rect rect, Color) override { fills.push_back(rect); }
    void fill_rounded_rect(Rect rect, double, Color) override { fills.push_back(rect); }
    void stroke_rect(Rect, Color, double) override {}
    void draw_line(Point, Point, Color, double) override {}
    void draw_text_utf8(Point origin, std::string_view text, FontSpec font, Color) override {
        texts.push_back(TextDraw{origin, std::string(text), font});
    }
    void draw_image(ImageId, Rect, double) override {}

    // The first draw whose text is `text`, or null.
    [[nodiscard]] const TextDraw* find(std::string_view text) const {
        for (const TextDraw& draw : texts) {
            if (draw.text == text) return &draw;
        }
        return nullptr;
    }

    std::vector<TextDraw> texts;
    std::vector<Rect> fills;
};

struct Fixture final {
    std::shared_ptr<Panel> root;
    std::shared_ptr<HelpBook> book;
    std::unique_ptr<Window> window;
};

Fixture make_fixture(double width, double height) {
    Fixture fixture;
    fixture.root = make_control<Panel>(StableId("app"));
    fixture.book = make_control<HelpBook>(StableId("app.page"));
    (*fixture.book).set_requested_bounds({0.0, 0.0, width, height});
    (*fixture.root).add_child(fixture.book);
    fixture.window = std::make_unique<Window>(fixture.root, Size{width + 40.0, height + 40.0});
    (*fixture.window).perform_layout();
    return fixture;
}

// Appends a run to `page` and returns its index.
std::size_t add_run(HelpPageModel& page, std::string text, bool code, stxlib::Optional<std::size_t> link) {
    HelpPageRun run{};
    run.text = std::move(text);
    run.code = code;
    run.link = link;
    page.runs.push_back(std::move(run));
    return page.runs.size() - 1U;
}

std::size_t add_link(HelpPageModel& page, std::string target) {
    page.links.push_back(HelpPageLink{HelpLinkKind::topic, std::move(target), {}, {}});
    return page.links.size() - 1U;
}

// A block of `kind` over the runs from `first_run` to the end of the runs.
void add_block(HelpPageModel& page, HelpPageBlockKind kind, std::size_t first_run) {
    HelpPageBlock block{};
    block.kind = kind;
    block.first_run = first_run;
    block.run_count = page.runs.size() - first_run;
    page.blocks.push_back(std::move(block));
}

CapturePainter paint(HelpBook& book) {
    CapturePainter painter;
    book.on_paint(painter, book.client_rectangle());
    return painter;
}

void shared_baseline_and_chip() {
    Fixture fixture = make_fixture(900.0, 500.0);
    HelpPageModel page{};
    page.title = "Typography";
    const std::size_t link = add_link(page, "options");
    const std::size_t first = page.runs.size();
    add_run(page, "Invalid UTF-8 throws ", false, {});
    add_run(page, "std::invalid_argument", true, {});
    add_run(page, "; see ", false, {});
    add_run(page, "options", true, stxlib::Optional<std::size_t>::of(link));
    add_run(page, " for more.", false, {});
    add_block(page, HelpPageBlockKind::paragraph, first);
    // A paragraph long enough to take two lines, for the line height.
    const std::size_t second = page.runs.size();
    add_run(page, std::string(40U, 'w') + " " + std::string(40U, 'w') + " " + std::string(40U, 'w') + " " + std::string(40U, 'w'), false, {});
    add_block(page, HelpPageBlockKind::paragraph, second);
    (*fixture.book).set_requested_bounds({0.0, 0.0, 860.0, 500.0});
    (*fixture.book).show_page(std::move(page));
    (*fixture.window).perform_layout();
    const CapturePainter painter = paint(*fixture.book);
    const TextDraw* const body = painter.find("Invalid ");
    const TextDraw* const code = painter.find("std::invalid_argument");
    const TextDraw* const linked = painter.find("options");
    require(body != nullptr && code != nullptr && linked != nullptr, "the body, code and link runs are drawn");
    require((*code).origin.y == (*body).origin.y && (*linked).origin.y == (*body).origin.y,
            "body, code and link runs on one line share one baseline");
    require((*body).font.role == FontRole::content, "body text is set in the content role's face");
    require((*code).font.role == FontRole::monospace && (*code).font.family != portsmouth_mono_font_family,
            "inline code is set in the monospace role's face, not the console face");
    require((*body).font.family != portsmouth_mono_font_family && (*linked).font.role == FontRole::monospace,
            "code runs, linked ones included, are mono and prose is not");
    require((*code).font.size <= (*body).font.size, "the code face is sized to the body's x-height, not larger");
    // The chip under the code: around its baseline, padded beside it, and
    // shorter than the line, so chips on neighbouring lines stay apart.
    const double line = std::round((*body).font.size * 1.5);
    bool chip = false;
    for (const Rect& fill : painter.fills) {
        if (fill.x < (*code).origin.x && fill.x > (*code).origin.x - 8.0 && fill.y < (*code).origin.y &&
            fill.bottom() > (*code).origin.y && fill.height < line - 3.0) {
            chip = true;
        }
    }
    require(chip, "inline code sits on a padded chip shorter than the line");
    // Punctuation after code hugs it: the semicolon after
    // "std::invalid_argument" starts at the chip's hairline edge, with no
    // padding between.
    const TextDraw* semicolon = nullptr;
    for (const TextDraw& draw : painter.texts) {
        if (!draw.text.empty() && draw.text[0] == ';') semicolon = &draw;
    }
    double chip_right = 0.0;
    for (const Rect& fill : painter.fills) {
        if (fill.x < (*code).origin.x && fill.right() > (*code).origin.x && fill.y < (*code).origin.y &&
            fill.bottom() > (*code).origin.y) {
            chip_right = fill.right();
        }
    }
    require(semicolon != nullptr && chip_right > 0.0 && (*semicolon).origin.x >= chip_right - 1.5 &&
                (*semicolon).origin.x <= chip_right + 0.5,
            "punctuation after inline code hugs the chip");
    // The title reads as the page's first heading: larger than a level-one
    // heading of the body, and semibold.
    const TextDraw* const title = painter.find("Typography");
    require(title != nullptr && (*title).font.size >= (*body).font.size * 1.5 && (*title).font.weight >= 600,
            "the page title is larger than the body's headings and semibold");
    // Two lines of one paragraph are a line height apart.
    std::vector<double> baselines;
    for (const TextDraw& draw : painter.texts) {
        // Each word may be drawn on its own; a line's words share a baseline.
        if (!draw.text.empty() && draw.text[0] == 'w' &&
            (baselines.empty() || baselines.back() != draw.origin.y)) {
            baselines.push_back(draw.origin.y);
        }
    }
    require(baselines.size() >= 2U && std::abs((baselines[1] - baselines[0]) - line) < 0.5,
            "lines are 1.5 times the body size apart");
}

struct RecordLink final {
    std::vector<std::string>* seen{};
    void operator()(HelpLinkActivation& activation) const {
        (*seen).push_back(activation.target);
        activation.handled = true;
    }
};

struct RecordHover final {
    std::vector<std::string>* seen{};
    void operator()(const HelpLinkHover& hover) const { (*seen).push_back(hover.link.present() ? hover.target : "-"); }
};

void hover_costs_no_layout() {
    Fixture fixture = make_fixture(600.0, 400.0);
    HelpPageModel page{};
    page.title = "Hover";
    const std::size_t link = add_link(page, "type/Binding");
    const std::size_t first = page.runs.size();
    add_run(page, "See ", false, {});
    add_run(page, "Binding", false, stxlib::Optional<std::size_t>::of(link));
    add_run(page, " for the rest of the story, told at some length so the page has text.", false, {});
    add_block(page, HelpPageBlockKind::paragraph, first);
    (*fixture.book).show_page(std::move(page));
    (*fixture.window).perform_layout();
    std::vector<std::string> hovers;
    SubscriptionToken token = (*fixture.book).link_hovered().subscribe(RecordHover{&hovers});
    const stxlib::Optional<Rect> found = (*fixture.book).link_bounds(link);
    require(found.present(), "the link is laid out");
    const Rect bounds = (*found.value());
    const std::uint64_t passes = (*fixture.book).layout_passes();
    PointerEvent move{};
    move.action = PointerAction::move;
    for (std::size_t step = 0U; step < 40U; ++step) {
        move.position = Point{bounds.x + 2.0 + static_cast<double>(step % 5U), bounds.y + bounds.height * 0.5};
        static_cast<void>((*fixture.window).dispatch_pointer(move));
        (*fixture.window).perform_layout();
        move.position = Point{bounds.x + 2.0, bounds.y + 120.0};
        static_cast<void>((*fixture.window).dispatch_pointer(move));
        (*fixture.window).perform_layout();
    }
    require((*fixture.book).layout_passes() == passes, "moving the pointer over the page lays nothing out");
    require(hovers.size() == 80U && hovers[0] == "type/Binding" && hovers[1] == "-",
            "link_hovered names the link entered and reports leaving it");
}

void definition_lists() {
    Fixture fixture = make_fixture(600.0, 400.0);
    HelpPageModel page{};
    page.title = "Terms";
    std::size_t first = page.runs.size();
    add_run(page, "checked", false, {});
    add_block(page, HelpPageBlockKind::term, first);
    first = page.runs.size();
    add_run(page, "Whether the switch is on.", false, {});
    add_block(page, HelpPageBlockKind::definition, first);
    (*fixture.book).show_page(std::move(page));
    (*fixture.window).perform_layout();
    const CapturePainter painter = paint(*fixture.book);
    const TextDraw* const term = painter.find("checked");
    const TextDraw* const definition = painter.find("Whether ");
    require(term != nullptr && definition != nullptr && (*definition).origin.x > (*term).origin.x + 10.0 &&
                (*definition).origin.y > (*term).origin.y,
            "a definition is indented under its term");
}

void tables() {
    // "Application and hosts": a long monospace name in a narrow first
    // column, a description, links in cells, and rows with an empty cell.
    Fixture fixture = make_fixture(420.0, 500.0);
    HelpPageModel page{};
    page.title = "Application and hosts";
    const std::size_t link = add_link(page, "type/ApplicationWindowHandle");
    const char* const names[3] = {"ApplicationWindowHandleWithAVeryLongName", "ApplicationCreateResult", "Shown"};
    for (std::size_t row = 0U; row < 3U; ++row) {
        HelpPageBlock block{};
        block.kind = HelpPageBlockKind::table_row;
        block.header = row == 0U;
        block.first_cell = page.cells.size();
        HelpPageCell name{};
        name.first_run = page.runs.size();
        add_run(page, names[row], row > 0U, row == 1U ? stxlib::Optional<std::size_t>::of(link)
                                                      : stxlib::Optional<std::size_t>::none());
        name.run_count = page.runs.size() - name.first_run;
        page.cells.push_back(name);
        HelpPageCell text{};
        text.first_run = page.runs.size();
        if (row != 2U) add_run(page, row == 0U ? "Description" : "What the application made, or why it failed.", false, {});
        text.run_count = page.runs.size() - text.first_run;
        page.cells.push_back(text);
        block.cell_count = 2U;
        page.blocks.push_back(std::move(block));
    }
    (*fixture.book).show_page(std::move(page));
    (*fixture.window).perform_layout();
    const CapturePainter painter = paint(*fixture.book);
    const TextDraw* const description = painter.find("What ");
    require(description != nullptr, "the description cell is drawn");
    for (const TextDraw& draw : painter.texts) {
        if (draw.text.rfind("Application", 0U) == 0U && draw.origin.y >= (*description).origin.y - 0.5) {
            require(draw.origin.x < (*description).origin.x, "a name stays in its own column");
        }
    }
    std::size_t name_pieces = 0U;
    for (const TextDraw& draw : painter.texts) {
        if (draw.text.find("ApplicationWindow") != std::string::npos || draw.text.find("VeryLongName") != std::string::npos ||
            draw.text.find("Long") != std::string::npos) {
            ++name_pieces;
        }
    }
    require(painter.find("ApplicationWindowHandleWithAVeryLongName") == nullptr && name_pieces >= 2U,
            "a name wider than its column wraps inside the cell rather than running over the next");
    require((*fixture.book).link_bounds(link).present(), "a link in a cell is laid out as a link");
    for (const TextDraw& draw : painter.texts) require(draw.text != "-", "an empty cell stays empty");

    // A pipe table in markdown becomes table rows, its cells' links links.
    HelpPageModel parsed{};
    const MarkdownParseResult result = build_help_page(
        "tables", "Tables",
        "Intro.\n\n| Type | Use |\n|---|---|\n| [Binding](type.binding) | Binds `a` to b. |\n| Plain | |\n\nAfter.\n", parsed);
    require(result.succeeded(), "the page with a table parses");
    std::size_t rows = 0U;
    bool header = false;
    for (const HelpPageBlock& block : parsed.blocks) {
        if (block.kind != HelpPageBlockKind::table_row) continue;
        ++rows;
        header = header || block.header;
        require(block.cell_count == 2U, "each row has its two cells");
    }
    require(rows == 3U && header, "the table has a header row and two body rows");
    require(parsed.blocks.front().kind == HelpPageBlockKind::paragraph &&
                parsed.blocks.back().kind == HelpPageBlockKind::paragraph,
            "the table stands between the paragraphs around it");
    require(parsed.links.size() == 1U && parsed.links[0].target == "type.binding", "a link in a cell is a page link");
    const HelpPageCell& empty = parsed.cells[parsed.cells.size() - 1U];
    require(empty.run_count == 0U, "an empty cell has no runs");
}

void coloured_code_and_scrolling() {
    Fixture fixture = make_fixture(500.0, 400.0);
    HelpPageModel page{};
    page.title = "Code";
    const std::size_t first = page.runs.size();
    add_run(page,
            "int value = 42; // the answer\nconst char* text = \"a string that runs well past the right edge of the page\";",
            false, {});
    add_block(page, HelpPageBlockKind::code, first);
    page.blocks.back().language = HelpCodeLanguage::cpp;
    (*fixture.book).show_page(std::move(page));
    (*fixture.window).perform_layout();
    const CapturePainter painter = paint(*fixture.book);
    const TextDraw* const keyword = painter.find("int");
    const TextDraw* const number = painter.find("42");
    const TextDraw* const comment = painter.find("// the answer");
    require(keyword != nullptr && number != nullptr && comment != nullptr,
            "a coloured code block draws its tokens apart");
    require((*comment).font.italic && (*keyword).origin.y == (*comment).origin.y, "a comment is italic, on its line");
    require((*fixture.book).content_width() > 500.0, "a code block wider than the page widens the content");
}

void find_in_page() {
    Fixture fixture = make_fixture(500.0, 200.0);
    HelpPageModel page{};
    const MarkdownParseResult result = build_help_page(
        "find", "Find",
        "A needle here.\n\n{collapsible}\n## Hidden\n\nAnother Needle, folded away.\n\nThe last needle.\n", page);
    require(result.succeeded(), "the find page parses");
    (*fixture.book).show_page(std::move(page));
    (*fixture.window).perform_layout();
    require((*fixture.book).find("needle", false) == 3U && (*fixture.book).find_position() == 1U,
            "find counts every match, case folded, and makes the first current");
    require((*fixture.book).find("needle", true) == 2U, "matching case leaves the capitalised one out");
    static_cast<void>((*fixture.book).find("needle", false));
    (*fixture.book).find_next();
    require((*fixture.book).find_position() == 2U && (*fixture.book).section_open(0U),
            "the next match is current, and its folded section opens");
    (*fixture.book).find_next();
    (*fixture.book).find_next();
    require((*fixture.book).find_position() == 1U, "find_next wraps to the first match");
    (*fixture.book).find_previous();
    require((*fixture.book).find_position() == 3U, "find_previous wraps to the last");
    (*fixture.book).clear_find();
    require((*fixture.book).find_count() == 0U && (*fixture.book).find_position() == 0U, "clear_find marks nothing");
}

void cards() {
    Fixture fixture = make_fixture(600.0, 400.0);
    HelpPageModel page{};
    page.title = "Search: binding";
    for (std::size_t card = 0U; card < 3U; ++card) {
        const std::size_t link = add_link(page, "type/Card" + std::to_string(card));
        HelpPageBlock block{};
        block.kind = HelpPageBlockKind::card;
        block.first_run = page.runs.size();
        add_run(page, "Card " + std::to_string(card), false, stxlib::Optional<std::size_t>::of(link));
        block.run_count = 1U;
        block.first_cell = page.cells.size();
        HelpPageCell line{};
        line.first_run = page.runs.size();
        add_run(page, "3 matches", false, {});
        line.run_count = 1U;
        page.cells.push_back(line);
        block.cell_count = 1U;
        page.blocks.push_back(std::move(block));
    }
    (*fixture.book).show_page(std::move(page));
    (*fixture.window).perform_layout();
    const CapturePainter painter = paint(*fixture.book);
    // A title's words may be drawn one by one: the first two titles' first
    // words stand on one baseline, the second to the right.
    std::vector<Point> titles;
    for (const TextDraw& draw : painter.texts) {
        if (draw.text.rfind("Card", 0U) == 0U) titles.push_back(draw.origin);
    }
    require(titles.size() >= 2U && titles[1].y == titles[0].y && titles[1].x > titles[0].x,
            "cards stand side by side in a grid");
    std::vector<std::string> followed;
    SubscriptionToken token = (*fixture.book).link_activated().subscribe(RecordLink{&followed});
    // A click on the card's line, away from its title, follows the card.
    const TextDraw* const line = painter.find("3 ");
    require(line != nullptr, "the card's line is drawn");
    PointerEvent press{};
    press.button = PointerButton::primary;
    press.position = Point{(*line).origin.x + 60.0, (*line).origin.y - 4.0};
    press.action = PointerAction::move;
    static_cast<void>((*fixture.window).dispatch_pointer(press));
    press.action = PointerAction::down;
    static_cast<void>((*fixture.window).dispatch_pointer(press));
    press.action = PointerAction::up;
    static_cast<void>((*fixture.window).dispatch_pointer(press));
    require(followed.size() == 1U && followed[0] == "type/Card0", "a click anywhere on a card follows its link");
}
// A long page, scrolled to its middle, in a book away from the window's
// origin: re-arranging keeps the scroll position and lays nothing out, and
// 100 pointer moves over it arrange, measure and flush nothing. The moves
// hit the link at its place in the book, not at the same point of the
// window, so the book reads the pointer's local location.
void scrolled_pointer_motion_costs_nothing() {
    Fixture fixture;
    fixture.root = make_control<Panel>(StableId("app"));
    fixture.book = make_control<HelpBook>(StableId("app.page"));
    (*fixture.book).set_requested_bounds({60.0, 80.0, 600.0, 400.0});
    (*fixture.root).add_child(fixture.book);
    fixture.window = std::make_unique<Window>(fixture.root, Size{700.0, 520.0});
    (*fixture.window).perform_layout();
    HelpPageModel page{};
    page.title = "Geometry and appearance";
    std::size_t middle_link = 0U;
    for (std::size_t index = 0U; index < 200U; ++index) {
        const std::size_t link = add_link(page, "type/Item" + std::to_string(index));
        if (index == 100U) middle_link = link;
        const std::size_t first = page.runs.size();
        add_run(page, "Paragraph " + std::to_string(index) + " has ", false, {});
        add_run(page, "a link", false, stxlib::Optional<std::size_t>::of(link));
        add_run(page, ", some ", false, {});
        add_run(page, "inline_code", true, {});
        add_run(page, " and enough words after them to wrap onto a second line in the book.", false, {});
        add_block(page, HelpPageBlockKind::paragraph, first);
    }
    (*fixture.book).show_page(std::move(page));
    (*fixture.window).perform_layout();
    const stxlib::Optional<double> top = (*fixture.book).block_top(100U);
    require(top.present() && (*top.value()) > 1000.0, "the page is long");
    // An anchor far down the page is reached by the show itself, before the
    // next arrange states the range.
    HelpPageModel anchored = (*fixture.book).page();
    anchored.blocks[150U].anchor = "far";
    (*fixture.book).show_page(std::move(anchored), "far");
    require((*fixture.book).scroll_position().y > 1000.0, "showing a page at a far anchor scrolls there at once");
    (*fixture.window).perform_layout();
    require((*fixture.book).scroll_position().y > 1000.0, "the next arrange keeps the anchor's position");
    static_cast<void>((*fixture.book).scroll_to(Point{0.0, (*top.value()) - 40.0}));
    (*fixture.window).perform_layout();
    const Point scrolled = (*fixture.book).scroll_position();
    require(scrolled.y > 1000.0, "the book scrolls to the middle of the page");
    const std::uint64_t passes = (*fixture.book).layout_passes();

    // A taller book re-arranges at the same width: the position stays and
    // the page is not laid out again.
    (*fixture.book).set_requested_bounds({60.0, 80.0, 600.0, 420.0});
    (*fixture.window).perform_layout();
    require((*fixture.book).scroll_position() == scrolled, "re-arranging keeps the scroll position");
    require((*fixture.book).layout_passes() == passes, "re-arranging at the same width lays nothing out");

    const stxlib::Optional<Rect> found = (*fixture.book).link_bounds(middle_link);
    require(found.present(), "the middle link is laid out");
    const Rect bounds = (*found.value());
    const Rect book_bounds = (*fixture.book).absolute_bounds();
    require(bounds.y > 0.0 && bounds.y < 380.0, "the middle link shows in the scrolled viewport");
    (*fixture.window).reset_activity_metrics();
    const MetricsSnapshot before = (*fixture.window).metrics_snapshot();
    PointerEvent move{};
    move.action = PointerAction::move;
    for (std::size_t step = 0U; step < 100U; ++step) {
        const double x = book_bounds.x + bounds.x + 2.0 + static_cast<double>(step % 7U);
        const double y = step % 2U == 0U ? book_bounds.y + bounds.y + bounds.height * 0.5
                                         : book_bounds.y + bounds.y + 60.0 + static_cast<double>(step);
        move.position = Point{x, y};
        static_cast<void>((*fixture.window).dispatch_pointer(move));
        (*fixture.window).perform_layout();
    }
    const MetricsSnapshot after = (*fixture.window).metrics_snapshot();
    require(after.controls_arranged == before.controls_arranged, "pointer motion arranges no control");
    require(after.measure_passes == before.measure_passes, "pointer motion measures nothing");
    require(after.read_barrier_flushes == before.read_barrier_flushes, "pointer motion flushes no layout");
    require((*fixture.book).layout_passes() == passes, "pointer motion lays nothing out");
    require((*fixture.book).scroll_position() == scrolled, "pointer motion keeps the scroll position");
    std::vector<std::string> followed;
    SubscriptionToken token = (*fixture.book).link_activated().subscribe(RecordLink{&followed});
    PointerEvent press{};
    press.button = PointerButton::primary;
    press.position = Point{book_bounds.x + bounds.x + 2.0, book_bounds.y + bounds.y + bounds.height * 0.5};
    press.action = PointerAction::down;
    static_cast<void>((*fixture.window).dispatch_pointer(press));
    press.action = PointerAction::up;
    static_cast<void>((*fixture.window).dispatch_pointer(press));
    require(followed.size() == 1U && followed[0] == "type/Item100", "the pointer hits the link at its place in the book");
}

// A page shown at an anchor before the book has a size (an application
// that opens a page while it builds its window) is scrolled there by the
// first arrange that gives the book one.
void anchor_before_size() {
    std::shared_ptr<Panel> root = make_control<Panel>(StableId("app"));
    std::shared_ptr<HelpBook> book = make_control<HelpBook>(StableId("app.page"));
    HelpPageModel page{};
    page.title = "Anchors";
    for (std::size_t index = 0U; index < 120U; ++index) {
        const std::size_t first = page.runs.size();
        add_run(page, "Paragraph " + std::to_string(index) + " of a page long enough to scroll.", false, {});
        add_block(page, HelpPageBlockKind::paragraph, first);
    }
    page.blocks[90U].anchor = "late";
    (*book).show_page(std::move(page), "late");
    (*book).set_requested_bounds({0.0, 0.0, 500.0, 300.0});
    (*root).add_child(book);
    Window window(root, Size{540.0, 340.0});
    window.perform_layout();
    const stxlib::Optional<double> top = (*book).block_top(90U);
    require(top.present() && (*book).scroll_position().y > (*top.value()) - 40.0,
            "the first arrange with a size scrolls to the anchor asked for before");
}

} // namespace

int main() {
    shared_baseline_and_chip();
    hover_costs_no_layout();
    scrolled_pointer_motion_costs_nothing();
    anchor_before_size();
    definition_lists();
    tables();
    coloured_code_and_scrolling();
    find_in_page();
    cards();
    std::cout << "help book reference tests passed\n";
    return 0;
}
