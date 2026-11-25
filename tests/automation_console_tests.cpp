// The MCP console's record and colours (MCP 1.0.5, console_log.hpp): the
// owner's formula at its two poles, the tones' clusters, the shades'
// stability across repaints, and the record's lines for calls, replies,
// failures and notes. The overlay's windows are checked from captures by
// the owner (they never appear in the automation's own captures).

#include "src/controls/automation/console/console_log.hpp"

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
    require(near_level(orange_colour.red(), 0xcc) && near_level(orange_colour.green(), 0x66) && orange_colour.blue() <= 1U &&
                orange_colour.alpha() == 255U,
            "the dithered orange is within a step of #CC6600");
    const Color green_colour = console_colour(ConsoleShade{1.0, 0.8}, 7U);
    require(near_level(green_colour.red(), 0x66) && near_level(green_colour.green(), 0xcc) && near_level(green_colour.blue(), 0x33),
            "the dithered green is within a step of #66CC33");
}

void shades_cluster_and_hold() {
    bool varied = false;
    const ConsoleShade first = console_shade(ConsoleTone::value, 1U, 0U);
    for (std::uint64_t serial = 1U; serial < 200U; ++serial) {
        for (std::size_t run = 0U; run < 8U; ++run) {
            const ConsoleShade instruction = console_shade(ConsoleTone::instruction, serial, run);
            const ConsoleShade value = console_shade(ConsoleTone::value, serial, run);
            require(instruction.c >= 0.0 && instruction.c <= 0.10, "the instruction's c is near 0");
            require(value.c >= 0.90 && value.c <= 1.0, "a value's c is near 1");
            require(value.v >= 0.76 && value.v <= 0.84, "v hovers around 0.8");
            if (!near(value.v, first.v)) varied = true;
        }
    }
    require(varied, "v varies from run to run");
    const ConsoleShade again = console_shade(ConsoleTone::value, 1U, 0U);
    require(near(again.c, first.c) && near(again.v, first.v), "a run's shade is the same at every repaint");
    const Color one = console_colour(first, console_run_hash(1U, 0U));
    const Color two = console_colour(first, console_run_hash(1U, 0U));
    require(one.red() == two.red() && one.green() == two.green() && one.blue() == two.blue(), "a run's dither is the same at every repaint");
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

} // namespace

int main() {
    formula_poles();
    shades_cluster_and_hold();
    record_lines();
    record_is_bounded();
    reply_lines_are_bounded();
    std::cout << "automation console tests passed\n";
    return 0;
}
