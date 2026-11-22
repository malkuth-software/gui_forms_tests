// The code-zoom demonstrator (owner, 2026-11-22; demo/code_zoom/
// code_zoom.hpp): its C++ language on tricky lines (raw strings with and
// without delimiters, short and long, across lines; block-comment markers
// that do not nest; line continuations of comments, strings and
// directives; character literals beside digit separators; numbers,
// attributes, directives, keywords and types), the zoom's easing and
// limits (zoom_model.hpp), the bench's sweep, summary and report shape
// (code_bench.hpp), and the sheet: its runs, its culling at the zoom's true
// size, its keys and its wheel. Drawing the runs' glyphs is the raster's,
// covered by the painter and text-engine tests.

#include "code_bench.hpp"
#include "code_highlight.hpp"
#include "code_sheet.hpp"
#include "code_zoom.hpp"
#include "code_zoom_program.hpp"
#include "cpp_language.hpp"
#include "zoom_model.hpp"

#include "gui_forms/events/input_events/input_events.hpp"
#include "gui_forms/types/geometry/geometry.hpp"
#include "gui_forms/types/painter/painter.hpp"
#include "gui_forms/window/window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace gui_forms;
using code_highlight::LexState;
using code_highlight::LineToken;
using code_highlight::TokenKind;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double left, double right, double tolerance) { return std::fabs(left - right) <= tolerance; }

// -- the C++ language ---------------------------------------------------------------------

// Each line's tokens, lexed in order from the initial state, and the state
// each line leaves.
struct LexedLines final {
    std::vector<std::vector<LineToken>> tokens{};
    std::vector<LexState> exits{};
};

LexedLines lex(std::span<const std::string_view> lines) {
    const code_highlight::CppLanguage& language = code_highlight::cpp_language();
    LexedLines lexed{};
    LexState state = language.initial_state();
    for (const std::string_view line : lines) {
        std::vector<LineToken> tokens{};
        state = language.lex_line(line, state, tokens);
        // Increasing columns without overlap, inside the line.
        std::uint32_t end = 0U;
        for (const LineToken& token : tokens) {
            require(token.column >= end && token.length > 0U && token.column + token.length <= line.size(),
                    "tokens are ordered, non-empty and inside the line: " + std::string(line));
            end = token.column + token.length;
        }
        lexed.tokens.push_back(tokens);
        lexed.exits.push_back(state);
    }
    return lexed;
}

// The token at `text`'s first occurrence in `line` (from `from`) covers it
// exactly and is of `kind`.
void expect_token(std::string_view line, const std::vector<LineToken>& tokens, std::string_view text, TokenKind kind,
                  std::size_t from = 0U) {
    const std::size_t column = line.find(text, from);
    require(column != std::string_view::npos, "the test line holds " + std::string(text));
    for (const LineToken& token : tokens) {
        if (token.column != column) continue;
        require(token.length == text.size() && token.kind == kind,
                "'" + std::string(text) + "' in '" + std::string(line) + "' is one " +
                    std::string(code_highlight::token_kind_name(kind)) + " token, not " +
                    std::string(code_highlight::token_kind_name(token.kind)) + " of " +
                    std::to_string(token.length));
        return;
    }
    require(false, "no token starts at '" + std::string(text) + "' in '" + std::string(line) + "'");
}

void test_raw_strings() {
    {
        // A ")\"" inside a delimited raw string does not close it.
        const std::array<std::string_view, 1> lines{R"cpp(auto s = R"x(say ")" here)x" + 1;)cpp"};
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], R"cpp(R"x(say ")" here)x")cpp", TokenKind::string);
        expect_token(lines[0], lexed.tokens[0], "+", TokenKind::operator_symbol);
        expect_token(lines[0], lexed.tokens[0], "1", TokenKind::number);
        require(lexed.exits[0] == LexState{}, "a closed raw string leaves the line in code");
    }
    {
        // Across lines, with a false close (')sql'') on the way.
        const std::array<std::string_view, 3> lines{
            R"cpp(const char* q = R"sql(SELECT ")" FROM)cpp",
            R"cpp(t WHERE x = ')sql' )sql"; int after;)cpp",
            "int next;",
        };
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], R"cpp(R"sql(SELECT ")" FROM)cpp", TokenKind::string);
        require(lexed.exits[0] != LexState{}, "an open raw string carries its delimiter to the next line");
        expect_token(lines[1], lexed.tokens[1], R"cpp(t WHERE x = ')sql' )sql")cpp", TokenKind::string);
        expect_token(lines[1], lexed.tokens[1], ";", TokenKind::punctuation);
        expect_token(lines[1], lexed.tokens[1], "after", TokenKind::identifier);
        require(lexed.exits[1] == LexState{}, "the close returns to code");
        expect_token(lines[2], lexed.tokens[2], "int", TokenKind::type);
    }
    {
        // A sixteen-byte delimiter (kept as its hash): a near miss does not
        // close it, the exact one does.
        const std::array<std::string_view, 3> lines{
            R"cpp(R"0123456789abcdef(body)cpp",
            R"cpp()0123456789abcdee" still inside)cpp",
            R"cpp(end)0123456789abcdef"_raw x)cpp",
        };
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], lines[0], TokenKind::string);
        expect_token(lines[1], lexed.tokens[1], lines[1], TokenKind::string);
        require(lexed.exits[1] == lexed.exits[0], "the near miss keeps the raw string open");
        expect_token(lines[2], lexed.tokens[2], R"cpp(end)0123456789abcdef"_raw)cpp", TokenKind::string);
        expect_token(lines[2], lexed.tokens[2], "x", TokenKind::identifier, 25U);
        require(lexed.exits[2] == LexState{}, "the exact close returns to code");
    }
    {
        // Prefixes and a suffix; a seventeen-byte delimiter is invalid.
        const std::array<std::string_view, 2> lines{
            R"cpp(f(u8R"(x)"_sv, LR"--(y)--");)cpp",
            R"cpp(R"0123456789abcdefg(z)0123456789abcdefg")cpp",
        };
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], R"cpp(u8R"(x)"_sv)cpp", TokenKind::string);
        expect_token(lines[0], lexed.tokens[0], R"cpp(LR"--(y)--")cpp", TokenKind::string);
        expect_token(lines[1], lexed.tokens[1], lines[1], TokenKind::invalid);
        require(lexed.exits[1] == LexState{}, "an invalid raw string does not carry over");
    }
}

void test_comments() {
    {
        // Block-comment markers do not nest: the first */ ends it.
        const std::array<std::string_view, 1> lines{"/* a /* b */ c */"};
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], "/* a /* b */", TokenKind::comment);
        expect_token(lines[0], lexed.tokens[0], "c", TokenKind::identifier);
        expect_token(lines[0], lexed.tokens[0], "*/", TokenKind::operator_symbol, 13U);
    }
    {
        const std::array<std::string_view, 4> lines{
            "int x; /* open",
            "// not a line comment, nor \"a string",
            "still */ int y; // done",
            "int z;",
        };
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], "/* open", TokenKind::comment);
        expect_token(lines[1], lexed.tokens[1], lines[1], TokenKind::comment);
        require(lexed.tokens[1].size() == 1U, "a line inside a block comment is one comment");
        expect_token(lines[2], lexed.tokens[2], "still */", TokenKind::comment);
        expect_token(lines[2], lexed.tokens[2], "y", TokenKind::identifier);
        expect_token(lines[2], lexed.tokens[2], "// done", TokenKind::comment);
        require(lexed.exits[2] == LexState{}, "a closed block comment and an unspliced line comment end");
        expect_token(lines[3], lexed.tokens[3], "z", TokenKind::identifier);
    }
}

void test_line_continuations() {
    {
        const std::array<std::string_view, 3> lines{"x = 1; // note \\", "continued note", "y = 2;"};
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], "// note \\", TokenKind::comment);
        expect_token(lines[1], lexed.tokens[1], lines[1], TokenKind::comment);
        expect_token(lines[2], lexed.tokens[2], "y", TokenKind::identifier);
    }
    {
        const std::array<std::string_view, 3> lines{"s = \"abc\\", "def\" + t;", "u;"};
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], "\"abc\\", TokenKind::string);
        expect_token(lines[1], lexed.tokens[1], "def\"", TokenKind::string);
        expect_token(lines[1], lexed.tokens[1], "t", TokenKind::identifier);
        require(lexed.exits[1] == LexState{}, "the continued string closes");
    }
    {
        // A directive continued: its next line's '#' is an operator, not a
        // directive; the line after it is code again.
        const std::array<std::string_view, 4> lines{
            "#define TWICE(x) \\",
            "  #x ((x) + (x)) \\",
            "  /* tail */",
            "#endif",
        };
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], "#define", TokenKind::preprocessor);
        expect_token(lines[0], lexed.tokens[0], "TWICE", TokenKind::identifier);
        require(lexed.exits[0] != LexState{}, "a spliced directive carries over");
        expect_token(lines[1], lexed.tokens[1], "#", TokenKind::operator_symbol);
        require(lexed.exits[1] == lexed.exits[0], "and carries over again");
        expect_token(lines[2], lexed.tokens[2], "/* tail */", TokenKind::comment);
        require(lexed.exits[2] == LexState{}, "an unspliced line ends the directive");
        expect_token(lines[3], lexed.tokens[3], "#endif", TokenKind::preprocessor);
    }
}

void test_literals_and_names() {
    {
        const std::array<std::string_view, 1> lines{R"cpp(n = 1'000'000 + 'a' + u8'b' + '\'' + L'\\';)cpp"};
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], "1'000'000", TokenKind::number);
        expect_token(lines[0], lexed.tokens[0], "'a'", TokenKind::character);
        expect_token(lines[0], lexed.tokens[0], "u8'b'", TokenKind::character);
        expect_token(lines[0], lexed.tokens[0], R"cpp('\'')cpp", TokenKind::character);
        expect_token(lines[0], lexed.tokens[0], R"cpp(L'\\')cpp", TokenKind::character);
    }
    {
        const std::array<std::string_view, 1> lines{"v = 0x1.8p+3f + 1e-5 + .5 + 0b1010'1010u + 42_km;"};
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], "0x1.8p+3f", TokenKind::number);
        expect_token(lines[0], lexed.tokens[0], "1e-5", TokenKind::number);
        expect_token(lines[0], lexed.tokens[0], ".5", TokenKind::number);
        expect_token(lines[0], lexed.tokens[0], "0b1010'1010u", TokenKind::number);
        expect_token(lines[0], lexed.tokens[0], "42_km", TokenKind::number);
    }
    {
        const std::array<std::string_view, 4> lines{
            R"cpp([[nodiscard, deprecated("old")]] constexpr std::size_t f() noexcept;)cpp",
            "  #  include <vector> // the header",
            "Painter painter; MAX_SIZE; char32_t c = nullptr;",
            R"cpp(bad = "unterminated)cpp",
        };
        const LexedLines lexed = lex(lines);
        expect_token(lines[0], lexed.tokens[0], "[[", TokenKind::attribute);
        expect_token(lines[0], lexed.tokens[0], "nodiscard", TokenKind::attribute);
        expect_token(lines[0], lexed.tokens[0], "deprecated", TokenKind::attribute);
        expect_token(lines[0], lexed.tokens[0], "\"old\"", TokenKind::string);
        expect_token(lines[0], lexed.tokens[0], "]]", TokenKind::attribute);
        expect_token(lines[0], lexed.tokens[0], "constexpr", TokenKind::keyword);
        expect_token(lines[0], lexed.tokens[0], "std", TokenKind::identifier);
        expect_token(lines[0], lexed.tokens[0], "::", TokenKind::operator_symbol);
        expect_token(lines[0], lexed.tokens[0], "size_t", TokenKind::type);
        expect_token(lines[0], lexed.tokens[0], "noexcept", TokenKind::keyword);
        expect_token(lines[1], lexed.tokens[1], "#  include", TokenKind::preprocessor);
        expect_token(lines[1], lexed.tokens[1], "<vector>", TokenKind::string);
        expect_token(lines[1], lexed.tokens[1], "// the header", TokenKind::comment);
        expect_token(lines[2], lexed.tokens[2], "Painter", TokenKind::type);
        expect_token(lines[2], lexed.tokens[2], "painter", TokenKind::identifier);
        expect_token(lines[2], lexed.tokens[2], "MAX_SIZE", TokenKind::identifier);
        expect_token(lines[2], lexed.tokens[2], "char32_t", TokenKind::type);
        expect_token(lines[2], lexed.tokens[2], "nullptr", TokenKind::keyword);
        expect_token(lines[3], lexed.tokens[3], "\"unterminated", TokenKind::invalid);
        require(lexed.exits[3] == LexState{}, "an unterminated string does not carry over");
    }
    require(code_highlight::cpp_language().name() == "cpp" && code_highlight::cpp_language().display_name() == "C++",
            "the language's names");
    require(code_highlight::token_kind_name(TokenKind::operator_symbol) == "operator_symbol" &&
                code_highlight::token_kind_name(static_cast<TokenKind>(200)) == "plain",
            "kind names, plain outside the enumeration");
}

// -- the zoom -----------------------------------------------------------------------------

void test_zoom_easing_and_limits() {
    using namespace gui_forms::code_zoom;
    require(ease_out_cubic(0.0) == 0.0 && ease_out_cubic(1.0) == 1.0 && near(ease_out_cubic(0.5), 0.875, 1e-12),
            "the ease-out cubic's ends and middle");
    require(ease_out_cubic(-1.0) == 0.0 && ease_out_cubic(2.0) == 1.0, "progress is clamped");
    double previous = 0.0;
    for (int step = 1; step <= 100; ++step) {
        const double eased = ease_out_cubic(static_cast<double>(step) / 100.0);
        require(eased > previous, "the ease rises throughout");
        previous = eased;
    }
    require(glide_zoom(1.0, 4.0, 0.0) == 1.0 && glide_zoom(1.0, 4.0, 1.0) == 4.0, "a glide's ends are exact");
    require(near(glide_zoom(1.0, 4.0, 0.5), std::exp(std::log(4.0) * 0.875), 1e-12),
            "a glide interpolates in log scale after easing");
    require(clamp_zoom(0.01) == minimum_zoom && clamp_zoom(1000.0) == maximum_zoom && clamp_zoom(std::nan("")) == 1.0,
            "the zoom's limits");

    // Content far larger than the viewport, scrolled into its middle, so
    // the anchor is never clamped.
    ZoomModel model{};
    model.set_extent(Size{100000.0, 100000.0}, Size{800.0, 600.0});
    model.scroll_by(Point{5000.0, 5000.0});
    const Point anchor{200.0, 150.0};
    const Point under = model.content_at(anchor);
    model.zoom_at(3.0, anchor);
    require(model.zoom() == 3.0 && !model.gliding(), "zoom_at sets the zoom at once");
    require(near(model.content_at(anchor).x, under.x, 1e-9) && near(model.content_at(anchor).y, under.y, 1e-9),
            "zoom_at keeps the content under the anchor");
    model.zoom_at(0.0001, anchor);
    require(model.zoom() == minimum_zoom, "zoom_at clamps to the minimum");
    model.zoom_at(1.0, anchor);

    // A glide: nothing moves until its first frame, then it eases to the
    // target and ends there exactly.
    model.glide_by(zoom_step, anchor);
    model.glide_by(zoom_step, anchor);
    require(model.gliding() && near(model.target_zoom(), zoom_step * zoom_step, 1e-12) && model.zoom() == 1.0,
            "presses during a glide accumulate from its target");
    const Point glide_under = model.content_at(anchor);
    require(model.advance(10.0) && model.zoom() == 1.0, "the first frame starts the glide's clock");
    require(model.advance(10.0 + glide_seconds * 0.5), "the glide goes on at its middle");
    require(near(model.zoom(), glide_zoom(1.0, zoom_step * zoom_step, 0.5), 1e-12), "and eases in log scale");
    require(near(model.content_at(anchor).x, glide_under.x, 1e-9), "the anchor holds through the glide");
    require(!model.advance(10.0 + glide_seconds + 0.001) && model.zoom() == zoom_step * zoom_step &&
                !model.gliding(),
            "the glide ends at its target");
    require(!model.advance(11.0), "no glide, no frames");

    model.glide_to(1000.0, anchor);
    require(model.target_zoom() == maximum_zoom, "a glide's target is clamped");
    model.finish();
    require(model.zoom() == maximum_zoom && !model.gliding(), "finish lands on the target");

    // Content smaller than the viewport stays inside it.
    ZoomModel small{};
    small.set_extent(Size{400.0, 300.0}, Size{800.0, 600.0});
    small.scroll_by(Point{1000.0, 1000.0});
    require(small.origin().x == 0.0 && small.origin().y == 0.0, "small content cannot be scrolled out");
    small.scroll_by(Point{-1000.0, -1000.0});
    require(small.origin().x == -400.0 && small.origin().y == -300.0, "it may sit anywhere inside the viewport");
}

// -- the bench ----------------------------------------------------------------------------

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines{};
    std::istringstream stream(text);
    std::string line{};
    while (std::getline(stream, line)) lines.push_back(line);
    return lines;
}

std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> fields{};
    std::istringstream stream(line);
    std::string field{};
    while (stream >> field) fields.push_back(field);
    return fields;
}

void test_bench() {
    using namespace gui_forms::code_zoom;
    std::vector<double> scales{};
    fill_bench_scales(scales, bench_first_zoom, bench_last_zoom, bench_frame_count);
    require(scales.size() == bench_frame_count && scales.front() == bench_first_zoom &&
                scales.back() == bench_last_zoom,
            "the sweep's frames and ends");
    const double ratio = scales[1] / scales[0];
    for (std::size_t index = 1U; index < scales.size(); ++index) {
        require(scales[index] > scales[index - 1U], "every frame draws at a new, larger size");
        require(near(scales[index] / scales[index - 1U], ratio, 1e-9), "in equal ratios");
    }
    fill_bench_scales(scales, 1.0, 2.0, 1U);
    require(scales.size() == 1U && scales[0] == 1.0, "one frame is the first zoom");
    fill_bench_scales(scales, 1.0, 2.0, 0U);
    require(scales.empty(), "no frames, no zooms");

    const std::array<double, 10> sorted{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    require(nearest_rank(sorted, 0.5) == 5.0 && nearest_rank(sorted, 0.95) == 10.0 && nearest_rank(sorted, 0.0) == 1.0,
            "nearest-rank percentiles");
    require(nearest_rank(std::span<const double>{}, 0.5) == 0.0, "no times, zero");

    // Ten frames in the first band (1 to 10 ms, 100 glyphs each), one at the
    // very end (the last band includes its end), one outside every band.
    std::vector<BenchSample> samples{};
    for (int frame = 1; frame <= 10; ++frame) samples.push_back(BenchSample{0.15, static_cast<double>(frame), 100U});
    samples.push_back(BenchSample{40.0, 20.0, 4U});
    samples.push_back(BenchSample{50.0, 99.0, 1U});
    std::vector<BenchBand> bands{};
    summarize_bench(samples, bench_band_edges, bands);
    require(bands.size() == bench_band_edges.size() - 1U, "one band per pair of edges");
    require(bands[0].frames == 10U && bands[0].median_milliseconds == 5.0 && bands[0].p95_milliseconds == 10.0 &&
                bands[0].mean_glyphs == 100U,
            "the first band's summary");
    require(bands[1].frames == 0U && bands[1].median_milliseconds == 0.0, "an empty band is kept, zero");
    require(bands.back().frames == 1U && bands.back().median_milliseconds == 20.0, "the last band holds its end");
    const BenchBand whole = summarize_whole(samples, bench_first_zoom, bench_last_zoom);
    require(whole.frames == 11U && whole.p95_milliseconds == 20.0, "the whole sweep leaves out what is outside it");

    const BenchSetting setting{"src/example.cpp", 1280.0, 800.0, 2.0};
    const std::string report = format_bench_report(setting, bands, whole);
    const std::vector<std::string> lines = split_lines(report);
    require(!report.empty() && report.back() == '\n', "the report ends its last line");
    require(lines.size() == 2U + bands.size() + 1U, "a header, a column line, the bands and the whole sweep");
    require(lines[0].rfind("code_zoom bench: 11 frames, 0.10x to 40.00x, src/example.cpp, 1280x800 at scale 2.00", 0) ==
                0U,
            "the header names the frames, the zooms, the file and the window: " + lines[0]);
    require(lines[0].size() >= 30U && lines[0].substr(lines[0].size() - 30U) == ", refresh: unknown (offscreen)",
            "and says the refresh is unknown offscreen: " + lines[0]);
    require(lines[1] == "label zoom_from zoom_to frames median_ms p95_ms glyphs", "the column line");
    for (std::size_t index = 2U; index < lines.size(); ++index) {
        const std::vector<std::string> fields = split_fields(lines[index]);
        require(fields.size() == 7U, "every band line has seven columns: " + lines[index]);
        require(fields[0] == (index + 1U == lines.size() ? "all" : "band"), "its label");
    }
    require(lines[2] == "band 0.10 0.20 10 5.000 10.000 100", "the first band's line: " + lines[2]);
    require(lines.back() == "all 0.10 40.00 11 6.000 20.000 91", "the whole sweep's line: " + lines.back());
}

// -- the program's arguments --------------------------------------------------------------

void test_arguments() {
    using namespace gui_forms::code_zoom;
    std::array<std::string, 4> storage{"gui-forms-code-zoom", "--size", "640x400", "--bench"};
    std::array<char*, 4> argv{storage[0].data(), storage[1].data(), storage[2].data(), storage[3].data()};
    const CodeZoomArguments taken = take_code_zoom_arguments(4, argv.data());
    require(taken.bench && taken.passed.size() == 3U && taken.passed[2] == argv[2],
            "--bench is taken out, the rest passed on in order");
    require(capture_path_for("shot.png", 0.5) == "shot-0.5x.png" &&
                capture_path_for("shot.png", 40.0) == "shot-40x.png",
            "each still is named for its zoom");
}

// -- the sheet ----------------------------------------------------------------------------

// Records the one draw_text_runs a paint makes; measures every code point
// 0.55 em wide (Painter's own estimate).
class RunsPainter final : public Painter {
public:
    void save() override {}
    void restore() override {}
    void translate(Point) override {}
    void clip_rect(Rect) override {}
    void fill_rect(Rect, Color) override {}
    void stroke_rect(Rect, Color, double) override {}
    void draw_line(Point, Point, Color, double) override {}
    void draw_text_utf8(Point, std::string_view, FontSpec, Color) override {}
    void draw_image(ImageId, Rect, double) override {}
    void draw_text_runs(FontSpec font, std::span<const TextRun> runs) override {
        ++calls;
        size = font.size;
        role = font.role;
        origins.clear();
        texts.clear();
        for (const TextRun& run : runs) {
            origins.push_back(run.origin);
            texts.emplace_back(run.text);
        }
    }

    int calls{};
    double size{};
    FontRole role{FontRole::control};
    std::vector<Point> origins{};
    std::vector<std::string> texts{};
};

void test_sheet_runs() {
    using namespace gui_forms::code_zoom;
    const std::shared_ptr<CodeSheet> sheet = make_control<CodeSheet>(StableId("code-zoom.test-sheet"));
    require((*sheet).load_source("test.cpp", "int a = 1; // c\n\tx = \"s\"\r\n"), "a small source loads");
    require((*sheet).line_count() == 2U, "a final newline starts no line");
    const std::span<const SheetLine> lines = (*sheet).lines();
    const std::span<const SheetRun> runs = (*sheet).runs();
    // Line 1: its number, then int, a, =, 1, ;, the comment.
    require(lines[0].run_count == 7U, "line 1's runs: " + std::to_string(lines[0].run_count));
    const SheetRun& number = runs[lines[0].first_run];
    require((*sheet).run_text(number) == "1" && number.colour == SheetColour::gutter &&
                number.column + number.columns + 2U == gutter_columns,
            "the line number, right-aligned before the gutter's space");
    const SheetRun& keyword = runs[lines[0].first_run + 1U];
    require((*sheet).run_text(keyword) == "int" && keyword.colour == SheetColour::type &&
                keyword.column == gutter_columns && keyword.glyphs == 3U,
            "int, a type, at the text's first column");
    const SheetRun& comment = runs[lines[0].first_run + 6U];
    require((*sheet).run_text(comment) == "// c" && comment.colour == SheetColour::comment && comment.glyphs == 3U &&
                comment.columns == 4U && comment.column == gutter_columns + 11U,
            "the comment: one run across its space, three glyphs over four columns");
    // Line 2: the tab moves x to the fourth column; '=' and the string
    // follow; the CR is dropped.
    const SheetRun& name = runs[lines[1].first_run + 1U];
    require((*sheet).run_text(name) == "x" && name.column == gutter_columns + tab_columns, "a tab is a tab stop");
    const SheetRun& last = runs[lines[1].first_run + lines[1].run_count - 1U];
    require((*sheet).run_text(last) == "\"s\"" && last.colour == SheetColour::string, "the string ends the line");
    require(colour_of(TokenKind::operator_symbol) == SheetColour::punctuation &&
                colour_of(TokenKind::identifier) == SheetColour::text &&
                colour_of(TokenKind::plain) == SheetColour::text,
            "operators share punctuation's colour, names the text's");
}

void test_sheet_paints_at_the_true_size(Window& window, CodeSheet& sheet) {
    using namespace gui_forms::code_zoom;
    const Size size = window.client_size();
    const Point centre{size.width * 0.5, size.height * 0.5};
    const Point content = sheet.content_point(capture_line, capture_column);
    RunsPainter painter{};
    std::array<std::uint64_t, 3> glyphs{};
    const std::array<double, 3> zooms{0.1, 1.0, 40.0};
    for (std::size_t index = 0U; index < zooms.size(); ++index) {
        sheet.show_at(zooms[index], content, centre);
        sheet.on_paint(painter, Rect{0.0, 0.0, size.width, size.height});
        require(painter.calls == static_cast<int>(index) + 1, "one draw_text_runs a paint");
        require(painter.role == FontRole::monospace && near(painter.size, base_font_size * zooms[index], 1e-9),
                "the runs are drawn in monospace at the zoom's true size");
        glyphs[index] = sheet.glyphs_drawn();
        // Every run drawn starts within a column of the sheet's right edge
        // and reaches past its left edge (Painter's estimate: 0.55 em a
        // column), and its baseline within three lines of the sheet (the
        // cull keeps a line of slack each way).
        const double advance = painter.size * 0.55;
        const double line_height = base_line_height * zooms[index];
        for (std::size_t run = 0U; run < painter.origins.size(); ++run) {
            const Point origin = painter.origins[run];
            const double width = static_cast<double>(painter.texts[run].size()) * advance;
            require(origin.x < size.width + advance && origin.x + width > 0.0,
                    "only runs crossing the sheet are drawn");
            require(origin.y > -line_height && origin.y < size.height + line_height * 3.0,
                    "only lines crossing the sheet are drawn");
        }
    }
    require(glyphs[0] > glyphs[1] && glyphs[1] > glyphs[2] && glyphs[2] > 0U,
            "zooming in draws fewer glyphs, and some at 40x");
    // At 0.1x every line of the source is on the sheet (each draws its number).
    sheet.show_at(minimum_zoom, Point{}, Point{});
    sheet.on_paint(painter, Rect{0.0, 0.0, size.width, size.height});
    require(painter.origins.size() >= sheet.line_count(), "the minimap shows every line");
}

void test_sheet_keys_and_wheel(Window& window, CodeSheet& sheet) {
    using namespace gui_forms::code_zoom;
    const Size size = window.client_size();
    const Point centre{size.width * 0.5, size.height * 0.5};
    sheet.show_at(1.0, sheet.content_point(capture_line, 0U), centre);
    // Under reduced motion a glide lands at once.
    PresentationOverrides still{};
    still.reduced_motion.set(true);
    window.set_presentation_overrides(still);
    KeyEvent plus{};
    plus.action = KeyAction::down;
    plus.physical_key = PhysicalKey::equal;
    static_cast<void>(window.dispatch_key(plus));
    require(near(sheet.zoom_model().zoom(), zoom_step, 1e-12) && !sheet.zoom_model().gliding(),
            "= zooms in a step, at once under reduced motion");
    KeyEvent minus = plus;
    minus.physical_key = PhysicalKey::keypad_minus;
    static_cast<void>(window.dispatch_key(minus));
    static_cast<void>(window.dispatch_key(minus));
    require(near(sheet.zoom_model().zoom(), 1.0 / zoom_step, 1e-12), "the keypad's - zooms out");
    KeyEvent zero = plus;
    zero.physical_key = 0x27U;
    static_cast<void>(window.dispatch_key(zero));
    require(sheet.zoom_model().zoom() == 1.0, "0 resets the zoom");
    const bool shown = sheet.readout_shown();
    KeyEvent f = plus;
    f.physical_key = PhysicalKey::f;
    static_cast<void>(window.dispatch_key(f));
    require(sheet.readout_shown() != shown, "F toggles the readout");
    static_cast<void>(window.dispatch_key(f));
    require(sheet.readout_shown() == shown, "and back");

    // A precise Ctrl+wheel zooms at once about the pointer; a notch glides
    // a step.
    const Point pointer{size.width * 0.25, size.height * 0.25};
    const Point under = sheet.zoom_model().content_at(pointer);
    PointerEvent wheel{};
    wheel.action = PointerAction::wheel;
    wheel.position = pointer;
    wheel.wheel_delta = Point{0.0, 100.0};
    wheel.modifiers = Modifier::control;
    wheel.wheel_unit = WheelDeltaUnit::pixels;
    static_cast<void>(window.dispatch_pointer(wheel));
    require(near(sheet.zoom_model().zoom(), std::exp(100.0 * pixel_zoom_rate), 1e-9),
            "a precise Ctrl+wheel zooms by exp(delta * rate)");
    // The source is narrower than the sheet, so only the vertical anchor
    // is free of the origin's limits.
    require(near(sheet.zoom_model().content_at(pointer).y, under.y, 1e-6), "about the pointer");
    const double before = sheet.zoom_model().zoom();
    wheel.wheel_delta = Point{0.0, -1.0};
    wheel.wheel_unit = WheelDeltaUnit::lines;
    wheel.modifiers = Modifier::meta;
    static_cast<void>(window.dispatch_pointer(wheel));
    require(near(sheet.zoom_model().zoom(), before / zoom_step, 1e-9), "a Cmd+wheel notch glides a step out");
    // The wheel alone scrolls.
    const double zoom = sheet.zoom_model().zoom();
    const Point origin = sheet.zoom_model().origin();
    wheel.modifiers = Modifier::none;
    wheel.wheel_delta = Point{0.0, -1.0};
    static_cast<void>(window.dispatch_pointer(wheel));
    require(sheet.zoom_model().zoom() == zoom && sheet.zoom_model().origin().y > origin.y,
            "the wheel alone scrolls down");
}

void test_window() {
    using namespace gui_forms::code_zoom;
    const std::string_view text = embedded_source_text();
    require(!text.empty() && embedded_source_name().size() > 4U, "a source is embedded");
    std::unique_ptr<Window> window = make_code_zoom();
    (*window).set_active(true);
    (*window).perform_layout();
    const std::shared_ptr<CodeSheet> sheet = find_code_sheet(*window);
    require(sheet != nullptr, "the window holds the sheet");
    const std::size_t newlines = static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
    const std::size_t expected = text.back() == '\n' ? newlines : newlines + 1U;
    require((*sheet).line_count() == expected, "every line of the embedded source is on the sheet");
    require((*sheet).source_name() == embedded_source_name(), "named for its file");
    test_sheet_paints_at_the_true_size(*window, *sheet);
    test_sheet_keys_and_wheel(*window, *sheet);
}

} // namespace

int main() {
    try {
        test_raw_strings();
        test_comments();
        test_line_continuations();
        test_literals_and_names();
        test_zoom_easing_and_limits();
        test_bench();
        test_arguments();
        test_sheet_runs();
        test_window();
    } catch (const std::exception& error) {
        std::cerr << "gui_forms_code_zoom_tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "gui_forms_code_zoom_tests passed\n";
    return EXIT_SUCCESS;
}
