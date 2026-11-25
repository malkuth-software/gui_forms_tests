// The MCP console's record and colours (MCP 1.0.5, console_log.hpp): the
// owner's formula at its two poles, the tones' clusters, the shades'
// stability across repaints, and the record's lines for calls, replies,
// failures and notes. The overlay's windows are checked from captures by
// the owner (they never appear in the automation's own captures).

#include "src/controls/automation/console/console_log.hpp"
#include "src/controls/automation/console/console_overlay.hpp"
#include "src/controls/automation/console/console_screen.hpp"
#include "src/controls/automation/help/automation_help.hpp"
#include "src/controls/automation/presenter/stock_presenter.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace gui_forms;
using namespace gui_forms::automation;

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "automation console test failed: " << message << '\n';
    std::exit(1);
}

bool near(double left, double right) {
    const bool close = std::abs(left - right) < 1e-9;
    return close;
}

// Within one 8-bit step of `expected` (the dither moves a channel by at
// most one step).
bool near_level(std::uint8_t level, int expected) {
    const int difference = static_cast<int>(level) - expected;
    return difference >= -1 && difference <= 1;
}

std::string joined(const ConsoleLine& line) {
    std::string text{};
    for (const ConsoleSpan& span : line.spans) text += span.text;
    return text;
}

void formula_poles() {
    // c = 0, v = 0.8: the instruction's orange, #CC6600.
    const ConsoleRgb orange = console_formula(ConsoleShade{0.0, 0.8});
    require(near(orange.r, 0.8) && near(orange.g, 0.4) && near(orange.b, 0.0), "c 0 is orange");
    // c = 1, v = 0.8: the values' green, #66CC33.
    const ConsoleRgb green = console_formula(ConsoleShade{1.0, 0.8});
    require(near(green.r, 0.4) && near(green.g, 0.8) && near(green.b, 0.2), "c 1 is green");
    const Color orange_colour = console_colour(ConsoleShade{0.0, 0.8}, 7U);
    require(near_level(orange_colour.red, 0xcc) && near_level(orange_colour.green, 0x66) && orange_colour.blue <= 1U &&
                orange_colour.alpha == 255U,
            "the dithered orange is within a step of #CC6600");
    const Color green_colour = console_colour(ConsoleShade{1.0, 0.8}, 7U);
    require(near_level(green_colour.red, 0x66) && near_level(green_colour.green, 0xcc) && near_level(green_colour.blue, 0x33),
            "the dithered green is within a step of #66CC33");
}

void shades_cluster_and_hold() {
    bool varied = false;
    const ConsoleShade first = console_shade(ConsoleTone::value, 1U, 0U);
    for (std::uint64_t serial = 1U; serial < 200U; ++serial) {
        for (std::size_t run = 0U; run < 8U; ++run) {
            const ConsoleShade instruction = console_shade(ConsoleTone::instruction, serial, run);
            const ConsoleShade value = console_shade(ConsoleTone::value, serial, run);
            require(near(instruction.c, 0.0), "the instruction's c is 0");
            require(near(value.c, 1.0), "a value's c is 1");
            require(value.v >= 0.78 && value.v <= 1.0, "v hovers around 0.9");
            if (!near(value.v, first.v)) varied = true;
        }
    }
    require(varied, "v varies from glyph to glyph");
    const ConsoleShade again = console_shade(ConsoleTone::value, 1U, 0U);
    require(near(again.c, first.c) && near(again.v, first.v), "a glyph's shade is the same at every repaint");
    const Color one = console_colour(first, console_glyph_hash(1U, 0U));
    const Color two = console_colour(first, console_glyph_hash(1U, 0U));
    require(one.red == two.red && one.green == two.green && one.blue == two.blue, "a glyph's dither is the same at every repaint");
}

void record_lines() {
    ConsoleLog log{};
    require(log.serial() == 0U, "an empty record has serial 0");
    log.record_call("act", "{\"action\":\"press\",\n\"target\":\"auto.press\"}");
    log.record_reply(ToolReply::success("ok: press [auto.press]\nchanged: [status] \"Ready\"\nframes: 1 presented"));
    log.record_reply(ToolReply::failure(ToolError::not_found, "no node auto.nothing", "read a snapshot first"));
    log.record_note("disconnected");
    std::vector<ConsoleLine> lines{};
    log.copy_since(0U, lines);
    require(lines.size() == 7U, "a call, three reply lines, a failure with its hint and a note");
    require(lines[0].spans.size() == 2U && lines[0].spans[0].tone == ConsoleTone::instruction &&
                lines[0].spans[0].text.find("act") != std::string::npos &&
                lines[0].spans[1].tone == ConsoleTone::value &&
                lines[0].spans[1].text.find('\n') == std::string::npos,
            "a call is the tool's name as the instruction and its arguments on one line as the value");
    require(joined(lines[1]).find("ok: press [auto.press]") != std::string::npos &&
                lines[1].spans[1].tone == ConsoleTone::instruction && lines[1].spans[1].text == "ok:",
            "a reply's leading word is the instruction");
    require(lines[3].spans.front().tone == ConsoleTone::value, "a reply's later lines are values");
    require(joined(lines[4]).find("not_found:") != std::string::npos &&
                joined(lines[4]).find("no node auto.nothing") != std::string::npos,
            "a failure names its error and message");
    require(joined(lines[5]).find("read a snapshot first") != std::string::npos, "a failure's hint follows it");
    require(joined(lines[6]).find("disconnected") != std::string::npos &&
                lines[6].spans.front().tone == ConsoleTone::instruction,
            "a note is the run's own line");
    std::vector<ConsoleLine> later{};
    log.copy_since(lines[4].serial, later);
    require(later.size() == 2U && later.front().serial == lines[5].serial, "copy_since takes only the newer lines");
}

// One record serves the socket and the overlay (attach revision 3): a
// labelled session's calls carry its label, a console's typed line stays as
// typed, and the transcript heads each session's lines with its label.
void record_sessions() {
    ConsoleLog log{};
    log.set_session("claude-code 3f2a");
    log.record_call("windows", "{}");
    log.record_reply(ToolReply::success("ok: 1 window"));
    log.set_session("console 9b0c");
    log.record_typed("call act action=press target=auto.ok");
    std::vector<ConsoleLine> lines{};
    log.copy_since(0U, lines);
    require(lines.size() == 3U && lines[0].session == "claude-code 3f2a" && lines[2].session == "console 9b0c",
            "each line keeps its session");
    require(lines[0].spans.front().text == "[claude-code 3f2a] " &&
                joined(lines[0]).find("call windows") != std::string::npos,
            "a JSON session's call reads as the console would type it, after its label");
    require(joined(lines[2]).find("call act action=press target=auto.ok") != std::string::npos,
            "a console's line stays as typed");
    const std::string transcript = log.transcript_text();
    require(transcript.find("== session claude-code 3f2a\n") != std::string::npos &&
                transcript.find("== session console 9b0c\n") != std::string::npos &&
                transcript.find("ok: 1 window") != std::string::npos,
            "the transcript heads each session's lines with its label");
}

void record_is_bounded() {
    ConsoleLog log{};
    for (std::size_t index = 0U; index < ConsoleLog::maximum_lines + 50U; ++index) log.record_note("line");
    std::vector<ConsoleLine> lines{};
    log.copy_since(0U, lines);
    require(lines.size() <= ConsoleLog::maximum_lines, "the record keeps at most maximum_lines");
    require(lines.back().serial == ConsoleLog::maximum_lines + 50U, "the newest line is kept");
    const std::string long_text(5000U, 'x');
    const std::string clipped = console_clip(long_text, 100U);
    require(clipped.size() <= 100U + 3U && clipped.substr(clipped.size() - 3U) == "…",
            "long text is clipped with an ellipsis");
    const std::string accented = console_clip("ééé", 3U);
    require(accented == "é…", "a clip never splits a UTF-8 sequence");
}

void reply_lines_are_bounded() {
    ConsoleLog log{};
    std::string text = "ok: snapshot";
    for (int line = 0; line < 30; ++line) text += "\n[node." + std::to_string(line) + "] button";
    log.record_reply(ToolReply::success(text));
    std::vector<ConsoleLine> lines{};
    log.copy_since(0U, lines);
    require(lines.size() == ConsoleLog::maximum_reply_lines + 1U, "a long reply shows its first lines and a count");
    require(joined(lines.back()).find("27 more lines") != std::string::npos, "the count names the lines left out");
}

// A probe whose one covered rectangle is `covered` (window-local).
class CoveredProbe final : public ConsoleButtonProbe {
public:
    explicit CoveredProbe(Rect covered) noexcept : covered_(covered) {}
    [[nodiscard]] bool covers(Rect local) const override {
        const bool apart = local.x + local.width <= covered_.x || covered_.x + covered_.width <= local.x ||
                           local.y + local.height <= covered_.y || covered_.y + covered_.height <= local.y;
        return !apart;
    }

private:
    Rect covered_{};
};

// 1.0.5 review: the thinking button covered the ribbon's help button. It
// sits in the title area when there is one, else where it covers no control.
void button_covers_no_content() {
    HostWindowFrame titled{};
    titled.content = Rect{100.0, 128.0, 800.0, 600.0};
    titled.outer = Rect{100.0, 100.0, 800.0, 628.0};
    titled.outer_measured = true;
    const Rect in_title = ConsoleOverlay::button_place(titled, 0.0, nullptr);
    require(in_title.y >= titled.outer.y && in_title.y + in_title.height <= titled.content.y,
            "with a title bar, the button sits in it");
    require(in_title.x + in_title.width <= titled.outer.x + titled.outer.width,
            "and inside the window's width");
    const Rect reserved = ConsoleOverlay::button_place(titled, 146.0, nullptr);
    require(reserved.x + reserved.width <= titled.outer.x + titled.outer.width - 146.0,
            "short of the host's caption buttons");

    HostWindowFrame chromeless{};
    chromeless.content = Rect{0.0, 0.0, 800.0, 600.0};
    chromeless.outer = chromeless.content;
    const Rect corner = ConsoleOverlay::button_place(chromeless, 0.0, nullptr);
    require(corner.y < 40.0 && corner.x + corner.width > 700.0, "with nothing to avoid, the upper right");
    // A help button at the upper right, where the button would go.
    const Rect help{760.0, 8.0, 32.0, 32.0};
    const CoveredProbe probe(help);
    const Rect aside = ConsoleOverlay::button_place(chromeless, 0.0, &probe);
    require(!probe.covers(Rect{aside.x, aside.y, aside.width, aside.height}), "the button moves off the control");
    require(aside.y < 40.0, "along the same row");
    const CoveredProbe everything(Rect{0.0, 0.0, 800.0, 600.0});
    const Rect fallback = ConsoleOverlay::button_place(chromeless, 0.0, &everything);
    require(fallback.width > 0.0, "with no free spot, it still has a place");
    const Rect console = ConsoleOverlay::console_place(chromeless.content, aside);
    require(console.x == chromeless.content.x && console.width == chromeless.content.width &&
                console.height == chromeless.content.height,
            "the console's layer covers the whole client area");
    require(ConsoleOverlay::console_text_top(chromeless.content, aside) >= aside.y + aside.height,
            "its text begins below a button in the client area");
    require(ConsoleOverlay::console_text_top(titled.content, in_title) < 20.0,
            "and at the top when the button is in the title");
    require(ConsoleOverlay::console_font().family == portsmouth_mono_font_family,
            "the console asks for Portsmouth Mono by family");
}

// 1.0.5 review: the questions opened at the screen's upper right. They are
// centred on the program window and kept in its screen's work area.
void dialogs_centre_on_the_program() {
    HostWindowFrame owner{};
    owner.content = Rect{400.0, 228.0, 800.0, 600.0};
    owner.outer = Rect{400.0, 200.0, 800.0, 628.0};
    owner.outer_measured = true;
    HostWindowFrame dialog{};
    dialog.content = Rect{1600.0, 28.0, 480.0, 200.0};
    dialog.outer = Rect{1600.0, 0.0, 480.0, 228.0};
    dialog.outer_measured = true;
    HostMonitor screen{};
    screen.frame = Rect{0.0, 0.0, 1920.0, 1080.0};
    screen.work_area = Rect{0.0, 25.0, 1920.0, 1055.0};
    screen.primary = true;
    const std::vector<HostMonitor> screens{screen};
    const Rect placed = presenter_dialog_place(owner, dialog, screens);
    const double centre_x = placed.x + placed.width / 2.0;
    const double outer_centre_y = (placed.y - 28.0) + 228.0 / 2.0;
    require(std::abs(centre_x - 800.0) < 1.0 && std::abs(outer_centre_y - 514.0) < 1.0,
            "a dialog is centred on the program window");
    require(placed.width == 480.0 && placed.height == 200.0, "and keeps its size");
    HostWindowFrame edge = owner;
    edge.outer = Rect{1700.0, 0.0, 300.0, 200.0};
    edge.content = Rect{1700.0, 28.0, 300.0, 172.0};
    const Rect kept = presenter_dialog_place(edge, dialog, screens);
    require(kept.x + kept.width <= 1920.0 && kept.y - 28.0 >= 25.0, "a dialog stays in the work area");
}

} // namespace

// The owner's console brief: each glyph takes its token's c (the verb and
// the tool's name 0, the arguments 1, a reply's leading word 0, wrapped rows
// 1, a label 0 and dimmer) and the formula's colour; the same line wraps to
// the same shades every time.
void tokens_take_their_tone() {
    ConsoleLog log{};
    log.set_session("console ec4c");
    log.record_call("plan_paint_draw", "{\"shape\":\"oval\"}");
    log.set_session("");
    log.record_reply(ToolReply::success("drew a oval from 120,100 to 420,300"));
    std::vector<ConsoleLine> lines{};
    log.copy_since(0U, lines);
    require(lines.size() == 2U, "a call and its reply");
    std::vector<ConsoleGlyphRow> rows{};
    console_wrap(lines[0], 200U, rows);
    require(rows.size() == 1U, "a wide screen keeps the call on one row");
    std::string instruction{};
    std::string value{};
    bool label_dim = true;
    for (const ConsoleGlyph& glyph : rows[0].glyphs) {
        const ConsoleRgb rgb = console_formula(glyph.shade);
        if (glyph.tone == ConsoleTone::label) {
            if (!(glyph.shade.v < 0.75 && near(glyph.shade.c, 0.0))) label_dim = false;
            continue;
        }
        if (glyph.tone == ConsoleTone::instruction) {
            instruction += glyph.text;
            require(near(glyph.shade.c, 0.0) && rgb.r > rgb.g && near(rgb.b, 0.0),
                    "an instruction glyph is the formula at c 0: red over green, no blue");
            require(near(rgb.r, glyph.shade.v) && near(rgb.g, 0.5 * glyph.shade.v), "r = v and g = v/2 at c 0");
        } else {
            value += glyph.text;
            require(near(glyph.shade.c, 1.0) && rgb.g > rgb.r && rgb.r > rgb.b,
                    "a value glyph is the formula at c 1: green over red over blue");
            require(near(rgb.r, 0.5 * glyph.shade.v) && near(rgb.g, glyph.shade.v) && near(rgb.b, 0.25 * glyph.shade.v),
                    "r = v/2, g = v and b = v/4 at c 1");
        }
    }
    require(label_dim, "a session label is c 0 and dimmer");
    require(instruction == "callplan_paint_draw{\"\"\"\"}",
            "the verb, the tool's name and the arguments' braces and quotes are the instruction");
    require(value == "shape:oval", "the arguments between them are values");
    std::vector<ConsoleGlyphRow> reply{};
    console_wrap(lines[1], 200U, reply);
    std::string lead{};
    for (const ConsoleGlyph& glyph : reply[0].glyphs) {
        if (glyph.tone == ConsoleTone::instruction) lead += glyph.text;
    }
    require(lead == "\xe2\x80\xb9" "drew", "a reply's mark and leading word are the instruction");
    // Brackets, braces, parentheses and quotes are the instruction wherever
    // they stand (owner): in a reply's text too.
    ConsoleLog marks{};
    marks.record_reply(ToolReply::success("ok: [status] name \"Untitled\" (it's {new})"));
    std::vector<ConsoleLine> mark_lines{};
    marks.copy_since(0U, mark_lines);
    std::vector<ConsoleGlyphRow> mark_rows{};
    console_wrap(mark_lines[0], 200U, mark_rows);
    for (const ConsoleGlyph& glyph : mark_rows[0].glyphs) {
        const bool structural = std::string_view("[](){}'\"").find(glyph.text) != std::string_view::npos;
        if (structural) {
            require(glyph.tone == ConsoleTone::instruction && near(glyph.shade.c, 0.0),
                    "a bracket, brace, parenthesis or quote is c 0");
        }
    }
    std::vector<ConsoleGlyphRow> narrow{};
    console_wrap(lines[0], 20U, narrow);
    require(narrow.size() > 1U, "a narrow screen wraps the call");
    for (std::size_t index = 1U; index < narrow.size(); ++index) {
        for (const ConsoleGlyph& glyph : narrow[index].glyphs) {
            const bool structural = std::string_view("[](){}'\"").find(glyph.text) != std::string_view::npos;
            require(glyph.tone == (structural ? ConsoleTone::instruction : ConsoleTone::value),
                    "a wrapped row is values, its marks aside");
        }
    }
    std::vector<ConsoleGlyphRow> again{};
    console_wrap(lines[0], 200U, again);
    for (std::size_t index = 0U; index < rows[0].glyphs.size(); ++index) {
        require(near(again[0].glyphs[index].shade.v, rows[0].glyphs[index].shade.v) &&
                    again[0].glyphs[index].column == rows[0].glyphs[index].column,
                "a line wraps to the same shades at every repaint");
    }
}

// A new line grows in place at a human pace (owner, 2026-11-25): its
// instruction glyphs type left to right at about twelve keystrokes a second
// with a seeded jitter; its values appear as a lit block that eases to the
// formula's colour; a newer entry completes the older one at once; once all
// is shown and settled, the screen asks for nothing.
void lines_type_in() {
    // The keystroke rhythm: about 1/12 s, within 30% either way, varied and
    // the same every time.
    bool varied = false;
    for (std::size_t glyph = 0U; glyph < 64U; ++glyph) {
        const double seconds = ConsoleScreen::keystroke_seconds(3U, glyph);
        require(seconds >= 0.7 / 12.0 - 1e-9 && seconds <= 1.3 / 12.0 + 1e-9, "a keystroke takes 1/12 s, +-30%");
        require(near(seconds, ConsoleScreen::keystroke_seconds(3U, glyph)), "the rhythm is seeded");
        if (!near(seconds, ConsoleScreen::keystroke_seconds(3U, 0U))) varied = true;
    }
    require(varied, "keystrokes vary");
    // The glow's endpoints: lit is brighter, settled is the formula.
    const ConsoleShade value_shade{1.0, 0.85};
    const ConsoleRgb settled = console_glow(value_shade, 0.0);
    const ConsoleRgb formula = console_formula(value_shade);
    require(near(settled.r, formula.r) && near(settled.g, formula.g) && near(settled.b, formula.b),
            "a settled glow is exactly the formula's colour");
    const ConsoleRgb lit = console_glow(value_shade, 1.0);
    require(lit.r > formula.r && lit.g > formula.g && lit.b > formula.b && lit.g >= lit.r,
            "a lit value is brighter, its hue kept");

    ConsoleLog log{};
    log.record_call("act", "{\"a\":1}");
    log.record_call("windows", "{}");
    std::vector<ConsoleLine> lines{};
    log.copy_since(0U, lines);
    std::vector<ConsoleGlyphRow> rows{};
    console_wrap(lines[0], 80U, rows);
    std::vector<ConsoleGlyphRow> second{};
    console_wrap(lines[1], 80U, second);
    ConsoleScreen screen{};
    screen.set_visible_rows(4U);
    require(!screen.busy() && !screen.advance(0.016), "an empty screen is idle and changes nothing");
    screen.queue_line(rows, true);
    require(screen.busy(), "a queued line wants frames");
    const std::vector<ConsoleGlyph>& glyphs = rows[0].glyphs;
    const std::size_t total = glyphs.size();
    std::size_t instruction = 0U;
    for (const ConsoleGlyph& glyph : glyphs) {
        if (glyph.tone == ConsoleTone::instruction) ++instruction;
    }
    require(instruction == 11U, "call act and the braces and quotes are eleven typed glyphs");
    std::size_t shown = 0U;
    std::size_t frames = 0U;
    double elapsed = 0.0;
    bool block = false;
    bool lit_seen = false;
    while ((screen.unshown() > 0U || screen.rows().empty()) && frames < 5000U) {
        static_cast<void>(screen.advance(0.004));
        ++frames;
        elapsed += 0.004;
        const std::size_t now = screen.rows().empty() ? 0U : screen.rows().back().shown;
        // Each step shows at most one keystroke; a value's run comes whole.
        std::size_t keystrokes = 0U;
        std::size_t at = shown;
        while (at < now) {
            if (glyphs[at].tone == ConsoleTone::instruction) {
                ++keystrokes;
                ++at;
                continue;
            }
            std::size_t end = at;
            while (end < total && glyphs[end].tone == ConsoleTone::value) ++end;
            require(now >= end, "a value's run appears at once, as a block");
            require(near(screen.glow(screen.rows().back().key, at), 1.0), "a value's run appears lit");
            lit_seen = true;
            if (end - at > 1U) block = true;
            at = end;
        }
        require(keystrokes <= 1U, "the instruction types a glyph at a time");
        shown = now;
    }
    require(block && lit_seen && shown == total, "the whole line shows");
    // Eleven keystrokes, the first at once: ten intervals of 1/12 s +-30%.
    require(elapsed >= 10.0 * 0.7 / 12.0 - 0.01 && elapsed <= 10.0 * 1.3 / 12.0 + 0.01,
            "the line types at a human pace");
    // The glow settles, then the screen is idle.
    std::size_t settle = 0U;
    while (screen.busy() && settle < 1000U) {
        static_cast<void>(screen.advance(0.016));
        ++settle;
    }
    require(settle * 0.016 <= ConsoleScreen::fade_seconds + 0.05, "the glow settles within its fade");
    require(screen.fades().empty(), "every run has settled to its colour");
    require(!screen.busy() && !screen.advance(0.016) && !screen.advance(1.0),
            "after the line, the screen is idle and asks for no frames");

    // A newer entry completes the one still typing.
    screen.queue_line(rows, true);
    static_cast<void>(screen.advance(0.2));
    require(screen.rows().back().shown < total, "the first entry is still typing");
    const std::uint64_t older = screen.rows().back().key;
    screen.queue_line(second, true);
    bool completed = false;
    for (const ConsoleScreenRow& row : screen.rows()) {
        if (row.key == older) completed = row.shown == row.row.glyphs.size();
    }
    require(completed, "a newer entry shows the older one whole at once");
    require(screen.unshown() == second[0].glyphs.size(), "only the newest entry is left to type");
    // A burst of forty entries leaves only the last one to type.
    for (int index = 0; index < 40; ++index) screen.queue_line(rows, true);
    require(screen.unshown() == total, "a burst never falls behind: only the newest entry animates");
    std::size_t burst_frames = 0U;
    while (screen.busy() && burst_frames < 100000U) {
        static_cast<void>(screen.advance(0.016));
        ++burst_frames;
    }
    require(burst_frames * 0.016 < 2.0, "the burst settles as fast as one line");
    require(screen.rows().size() <= 5U, "the screen keeps only the rows in view and one above");
}

namespace {

// Round 4, item 6: the console shows the window's timing only when it is
// news, and a counters line shows its start.
void timing_lines_are_quiet() {
    ConsoleLog log{};
    log.record_reply(ToolReply::success("drew an oval\nok: plan_paint_draw\nframes: 2 presented; idle after 56 ms"));
    log.record_reply(ToolReply::success("ok: test_draw\nframes: 0 presented; replied after 101 ms"));
    log.record_reply(ToolReply::success("ok: act\nframes: 9 presented; still busy after 5001 ms: a frame is wanted"));
    std::string counters = "{";
    for (int index = 0; index < 40; ++index) counters += "\"counter_" + std::to_string(index) + "\":1,";
    counters += "\"end\":0}";
    log.record_reply(ToolReply::success(counters));
    std::vector<ConsoleLine> lines{};
    log.copy_since(0U, lines);
    std::string all{};
    for (const ConsoleLine& line : lines) all += joined(line) + "\n";
    require(all.find("idle after") == std::string::npos && all.find("replied after") == std::string::npos,
            "settled timing is left out");
    require(all.find("still busy after 5001 ms") != std::string::npos, "real busy time is shown");
    require(all.find("counter_39") == std::string::npos && all.find("counter_0") != std::string::npos,
            "a counters line shows its start only");
}

// The tools a test program serves: one documented with arguments, one not.
std::vector<ToolSpec> help_tools() {
    std::vector<ToolSpec> tools{};
    ToolSpec draw{};
    draw.name = "plan_paint_draw";
    draw.title = "Draw";
    draw.description = "Draws a shape on the picture. Units are picture pixels.";
    draw.input_schema =
        "{\"type\":\"object\",\"properties\":{\"shape\":{\"type\":\"string\",\"enum\":[\"line\",\"oval\"],"
        "\"description\":\"The shape.\"},\"from\":{\"type\":\"object\",\"description\":\"A corner, in picture "
        "pixels.\",\"properties\":{\"x\":{\"type\":\"number\"},\"y\":{\"type\":\"number\"}}},\"size\":{\"type\":"
        "\"integer\",\"minimum\":1,\"maximum\":100,\"description\":\"The outline's width in pixels.\"}},"
        "\"required\":[\"shape\",\"from\"]}";
    draw.access = ToolAccess::acting;
    tools.push_back(draw);
    ToolSpec bare{};
    bare.name = "plan_paint_mystery";
    tools.push_back(bare);
    return tools;
}

bool has(const std::string& text, std::string_view part) {
    const bool found = text.find(part) != std::string::npos;
    return found;
}

// Round 4: help from one source: the tools' own documentation and
// GUI.Forms' compiled-in pages.
void help_comes_from_the_documentation() {
    const std::vector<ToolSpec> tools = help_tools();
    HelpContext context{};
    context.access = std::string(access_summary);
    context.note = "Plan Paint draws pictures.";
    context.console = true;
    const std::string overview = help_text("", tools, context);
    require(has(overview, "help [path]") && has(overview, "your own user account only") &&
                has(overview, "`help mcp`") && has(overview, "Plan Paint draws pictures."),
            "help gives the access line, the commands, the topics and the program's note");
    require(!has(overview, "token") && !has(overview, "resume"), "help names no token and no resume");
    const std::string listing = help_text("tools", tools, context);
    const std::string draw_line = listing.substr(listing.find("plan_paint_draw"), listing.find('\n'));
    require(has(draw_line, "Draws a shape on the picture.") && !has(draw_line, "Units") &&
                has(listing, "plan_paint_mystery") && has(listing, "(not documented)"),
            "help tools lists each tool on one line and says which has no documentation");
    const std::string draw = help_text("plan_paint_draw", tools, context);
    require(has(draw, "Units are picture pixels.") && has(draw, "shape  one of line, oval, required") &&
                has(draw, "size  integer, 1 to 100, optional: The outline's width in pixels.") &&
                has(draw, "call plan_paint_draw {\"shape\":\"line\",\"from\":{\"x\":10,\"y\":10}}"),
            "help <tool> gives the description, typed arguments with their ranges and units, and an example");
    require(has(help_text("plan_paint_mystery", tools, context), "not documented"),
            "an undocumented tool says so");
    require(undocumented_tools(tools) == std::vector<std::string>{"plan_paint_mystery"},
            "the check names the undocumented tool");
    const std::string mcp = help_text("mcp", tools, context);
    require(has(mcp, "Talk to a running program") && has(mcp, "-- Connections --"),
            "help mcp is the compiled-in page on talking to a program");
    const std::string connections = help_text("mcp.connections", tools, context);
    require(has(connections, "-- Connections --") && !has(connections, "-- The console --"),
            "a topic is one section, by dotted path");
    require(help_text("connections", tools, context) == connections, "the bare section name works too");
    require(!has(mcp, "session token") && !has(mcp, "resume <token>"), "the page names no token");
    const std::vector<std::string> topics = help_topics();
    for (const std::string_view topic : {"console", "connections", "control", "disconnect", "help"}) {
        bool listed = false;
        for (const std::string& known : topics) listed = listed || known == topic;
        require(listed, "the documentation answers every topic help names");
    }
    require(has(help_text("gui.forms", tools, context), "https://gui-forms.readthedocs.io/en/latest/user/"),
            "help gui.forms reaches the published guide");
    const std::string unknown = help_text("plan_paint_drw", tools, context);
    require(has(unknown, "no help on \"plan_paint_drw\"") && has(unknown, "`help plan_paint_draw`"),
            "an unknown topic suggests the nearest");
}

// Round 5, the owner's UCI model: help is a dotted tree. The top lists the
// groups, a group its tools as dotted paths, a path one tool.
void help_is_a_dotted_tree() {
    const std::vector<ToolSpec> tools = help_tools();
    HelpContext context{};
    context.console = true;
    const std::string overview = help_text("", tools, context);
    require(has(overview, "Groups:") && has(overview, "plan_paint  (2 tools: `help plan_paint`)"),
            "the top lists the group and how to open it");
    const std::string group = help_text("plan_paint", tools, context);
    require(has(group, "plan_paint.draw") && has(group, "plan_paint.mystery") &&
                has(group, "Draws a shape on the picture.") && has(group, "`help plan_paint.<name>`"),
            "a group lists its tools as dotted paths");
    const std::string leaf = help_text("plan_paint.draw", tools, context);
    require(leaf.rfind("plan_paint.draw = plan_paint_draw", 0U) == 0U && has(leaf, "Units are picture pixels."),
            "a dotted path gives one tool, naming its tool");
    require(has(help_text("PLAN_PAINT.Draw", tools, context), "Units are picture pixels."), "paths ignore case");
    const std::string near = help_text("plan_paint.drw", tools, context);
    require(has(near, "`help plan_paint.draw`"), "an unknown path names the nearest");
}

// Round 5: the instructions say who may connect and how control works, and
// where help is, with no token and no prompt cost.
void the_instructions_say_who_may_connect() {
    HelpContext context{};
    context.access = std::string(access_summary);
    context.note = "Plan Paint's note.";
    const std::string instructions = session_instructions("A GUI.Forms program.", context);
    require(instructions.substr(0U, instructions.find('\n')) == "A GUI.Forms program.", "what every program says first");
    const std::string second = instructions.substr(instructions.find('\n') + 1U);
    require(second.rfind(std::string(access_summary), 0U) == 0U, "the access line is the second line");
    require(has(instructions, std::string(mcp_help_pointer)) && has(instructions, "--mcp-help") &&
                has(instructions, "Plan Paint's note."),
            "the help pointer, with --mcp-help, and the program's note follow");
    require(!has(instructions, "token") && !has(instructions, "prompt") && !has(instructions, "resume"),
            "no token, prompt cost or resume");
}

} // namespace

int main() {
    formula_poles();
    shades_cluster_and_hold();
    record_lines();
    record_sessions();
    record_is_bounded();
    reply_lines_are_bounded();
    button_covers_no_content();
    dialogs_centre_on_the_program();
    tokens_take_their_tone();
    lines_type_in();
    timing_lines_are_quiet();
    help_comes_from_the_documentation();
    help_is_a_dotted_tree();
    the_instructions_say_who_may_connect();
    std::cout << "automation console tests passed\n";
    return 0;
}
