// What GUI.Forms takes from STXlib's math (stxlib/math.hpp) since its own
// engine_math was removed (the STXlib shield, 2026-11-22). STXlib's tests
// own the functions' accuracy; these check the facts GUI.Forms' callers rely
// on, through GUI.Forms' own entry points where it has them:
// - a rotation by a multiple of 90 degrees is exact, with every zero +0, so
//   the same quarter turn reached two ways has the same bits;
// - the effect vocabulary's sine is STXlib's sine, bit for bit;
// - a log scale's decades are exact, and other powers of ten are STXlib's
//   pow;
// - the angle at a point on the axes, as the conic and border paths ask
//   for it (negations written 0.0 - v).

#include "gui_forms/engine/effect/effect.hpp"
#include "gui_forms/plots/scale/scale.hpp"
#include "gui_forms/types/painter/painter.hpp"
#include "stxlib/math.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "gui_forms_math_seat_tests: " << message << '\n';
    std::exit(1);
}

bool same_bits(double left, double right) {
    const bool same = std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
    return same;
}

bool same_transform(const gui_forms::PaintTransform& left, const gui_forms::PaintTransform& right) {
    const bool same = same_bits(left.xx, right.xx) && same_bits(left.yx, right.yx) && same_bits(left.xy, right.xy) &&
                      same_bits(left.yy, right.yy) && same_bits(left.dx, right.dx) && same_bits(left.dy, right.dy);
    return same;
}

void test_quarter_turns() {
    // sincosd itself: every multiple of 90 exact, every zero +0.
    const double quarter_sine[4] = {0.0, 1.0, 0.0, -1.0};
    const double quarter_cosine[4] = {1.0, 0.0, -1.0, 0.0};
    for (int k = -8; k <= 8; ++k) {
        const int quarter = ((k % 4) + 4) % 4;
        double sine = 0.0;
        double cosine = 0.0;
        stxlib::math::sincosd(90.0 * static_cast<double>(k), sine, cosine);
        require(same_bits(sine, quarter_sine[quarter]) && same_bits(cosine, quarter_cosine[quarter]),
                "a multiple of 90 degrees is exact, its zeros +0");
    }
    // Through the painter: a quarter turn about (10, 20) maps whole numbers
    // to whole numbers, and -90 and 270 give the same bits.
    const gui_forms::Point centre{10.0, 20.0};
    const gui_forms::PaintTransform turned = gui_forms::paint_rotation_about(centre, 90.0);
    require(same_bits(turned.xx, 0.0) && turned.xy == 1.0 && turned.yx == -1.0 && same_bits(turned.yy, 0.0) &&
                turned.dx == -10.0 && turned.dy == 30.0,
            "a quarter turn about (10, 20) is exact");
    require(same_transform(gui_forms::paint_rotation_about(centre, -90.0),
                           gui_forms::paint_rotation_about(centre, 270.0)),
            "-90 and 270 degrees give the same bits");
    require(same_transform(gui_forms::paint_rotation_about(centre, -180.0),
                           gui_forms::paint_rotation_about(centre, 180.0)),
            "-180 and 180 degrees give the same bits");
}

void test_effect_sine() {
    const double samples[] = {0.0, 0.5, -1.25, 3.14159265358979323846, 100.0, -2048.75};
    for (const double x : samples) {
        require(same_bits(gui_forms::effect_sine(x), stxlib::math::sin(x)), "effect_sine is STXlib's sine");
    }
}

void test_scale_decades() {
    require(gui_forms::plots::engine_log10(1000.0) == 3.0 && gui_forms::plots::engine_log10(1e-3) == -3.0 &&
                gui_forms::plots::engine_log10(1.0) == 0.0,
            "a decade's logarithm is exact");
    require(gui_forms::plots::engine_exp10(-3.0) == 1.0 / 1000.0 && gui_forms::plots::engine_exp10(22.0) == 1e22,
            "a whole exponent's power of ten is exact");
    require(same_bits(gui_forms::plots::engine_exp10(2.5), stxlib::math::pow(10.0, 2.5)),
            "another power of ten is STXlib's pow");
}

void test_axis_angles() {
    // The conic and border paths write their negations 0.0 - v, so a point
    // at the centre asks atan2(+0, +0) and keeps angle +0 (atan2(+0, -0)
    // would be pi under Annex F).
    const double dy = 0.0;
    require(same_bits(stxlib::math::atan2(0.0, 0.0 - dy), 0.0), "the centre's angle is +0");
    require(stxlib::math::atan2(0.0, -1.0) == stxlib::math::atan2(0.0 - 0.0, -1.0),
            "a point on the negative axis keeps +pi");
}

} // namespace

int main() {
    test_quarter_turns();
    test_effect_sine();
    test_scale_decades();
    test_axis_angles();
    std::cout << "gui_forms_math_seat_tests passed\n";
    return 0;
}
