// Lamps and lit keys as baked light planes (skeudoindustrial surfaces,
// phase 3): src/core/engine/light_planes.
//
// - The study's bake file reads back exactly; every malformed file is
//   refused with its defect before anything is replaced.
// - The composite at the bake's own size is panel_lamps' formula pixel for
//   pixel: the room at its gain plus each lamp's colour times its power,
//   unpremultiplied, toned 1 − e^(−1.3 v), encoded, with the coverage as
//   alpha (written premultiplied); no coverage is transparent; a lamp at
//   power 0 leaves the room alone; the press mixes the up and down bakes.
// - Stretched over another size, a uniform bake stays uniform, and only
//   the rectangle's pixels inside the destination are written.
// - The strike and press envelopes and the lean formula hold their values,
//   and reduced motion arrives at once.
// - The failing lamp's factor repeats for a seed, stays in (0, 1], drops out
//   to near dark, holds at most two dips in any second, and is 1 under
//   reduced motion.
// - The shipped store (GFLP, binary16 grey planes) reads back exactly,
//   subnormals included; each malformed store is refused with its defect;
//   the six shipped bakes are embedded with their shapes and the jewel's
//   aperture.
// - The jewel's filter tints an unlit jewel toward its colour; the jewel
//   shape is a dot with a dark rim, transparent outside; the pinpoint is a
//   core with a halo that goes out with the lamp.
// - LightPlaneCache reads each bake once, on first use, from the
//   directory's float planes, then its store, then the shipped store; a
//   missing or defective one is remembered; a name that is not a bake's
//   never reaches the filesystem; with no directory it lends the shipped
//   bakes.
//
// `--preview <out dir> <bake dir>` writes the dialog keys (the baker's
// dialog_cancel and dialog_ok, up and down) lit and unlit, 180 units wide
// at scales 1 and 2, as PNG files, for review without a window. The bakes
// are made by the integrator (`lamps <bake dir> 240 dialog`, the study's
// lamp_keys.cpp); nothing here bakes.

#include "gui_forms/engine/engine_math/engine_math.hpp"
#include "gui_forms/engine/part_raster/part_raster.hpp"
#include "gui_forms/surface_root/surface_root.hpp"

#include "gui_forms/engine/light_planes/light_planes.hpp"
#include "src/core/engine/primitives/finish/finish_kernel.hpp"
#include "support/unique_temporary_directory.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
using namespace gui_forms;
using engine_light::LampDrive;
using engine_light::LightComposite;
using engine_light::LightPlaneRect;
using engine_light::LightPlanes;
using engine_light::LightPlanesDefect;

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "gui_forms_engine_light_planes_tests: " << message << '\n';
    std::exit(1);
}

// -- writing bake files ------------------------------------------------------------

void put_word(std::vector<std::byte>& bytes, std::uint32_t word) {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<std::byte>((word >> shift) & 0xffU));
    }
}

void put_float(std::vector<std::byte>& bytes, float value) {
    std::uint32_t word = 0U;
    std::memcpy(&word, &value, sizeof(word));
    put_word(bytes, word);
}

// `planes` in the baker's layout (lamp_keys.cpp, write_buffers).
std::vector<std::byte> bake_bytes(const LightPlanes& planes) {
    std::vector<std::byte> bytes{};
    put_word(bytes, planes.width);
    put_word(bytes, planes.height);
    put_word(bytes, planes.lamp_count);
    for (const float value : planes.room) put_float(bytes, value);
    for (const float value : planes.lamps) put_float(bytes, value);
    for (const float value : planes.coverage) put_float(bytes, value);
    return bytes;
}

// A `width` x `height` bake with `lamps` lamps: room `room` and each lamp
// `glow` in every channel, both already premultiplied by `coverage`.
LightPlanes uniform_planes(std::uint32_t width, std::uint32_t height, std::uint32_t lamps, float room, float glow,
                           float coverage) {
    LightPlanes planes{};
    planes.width = width;
    planes.height = height;
    planes.lamp_count = lamps;
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    planes.room.assign(pixels * 3U, room);
    planes.lamps.assign(pixels * 3U * lamps, glow);
    planes.coverage.assign(pixels, coverage);
    return planes;
}

// A small bake whose every float differs, for the round trip.
LightPlanes varied_planes() {
    LightPlanes planes = uniform_planes(3U, 2U, 2U, 0.0F, 0.0F, 0.0F);
    for (std::size_t index = 0U; index < planes.room.size(); ++index) planes.room[index] = 0.01F * static_cast<float>(index);
    for (std::size_t index = 0U; index < planes.lamps.size(); ++index) planes.lamps[index] = 0.5F + 0.02F * static_cast<float>(index);
    for (std::size_t index = 0U; index < planes.coverage.size(); ++index) planes.coverage[index] = 0.125F * static_cast<float>(index);
    return planes;
}

// A store (GFLP, version `version`) of `width` x `height` pixels and
// `lamps` lamp planes: the room `room`, each lamp `glow` and the coverage
// `cover`, as binary16 bit patterns, the jewel's hole `aperture`.
std::vector<std::byte> store_bytes(std::uint32_t width, std::uint32_t height, std::uint32_t lamps, float aperture,
                                   std::uint16_t room, std::uint16_t glow, std::uint16_t cover,
                                   std::uint32_t version) {
    std::vector<std::byte> bytes{std::byte{'G'}, std::byte{'F'}, std::byte{'L'}, std::byte{'P'}};
    put_word(bytes, version);
    put_word(bytes, width);
    put_word(bytes, height);
    put_word(bytes, lamps);
    put_float(bytes, aperture);
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    const std::size_t planes = 2U + static_cast<std::size_t>(lamps);
    for (std::size_t plane = 0U; plane < planes; ++plane) {
        std::uint16_t half = glow;
        if (plane == 0U) half = room;
        if (plane + 1U == planes) half = cover;
        for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
            bytes.push_back(static_cast<std::byte>(half & 0xffU));
            bytes.push_back(static_cast<std::byte>((half >> 8U) & 0xffU));
        }
    }
    return bytes;
}

// binary16 bit patterns used below.
constexpr std::uint16_t half_zero = 0x0000U;
constexpr std::uint16_t half_smallest = 0x0001U; // 2^-24, a subnormal
constexpr std::uint16_t half_quarter = 0x3400U;
constexpr std::uint16_t half_half = 0x3800U;
constexpr std::uint16_t half_one = 0x3c00U;
constexpr std::uint16_t half_two = 0x4000U;
constexpr std::uint16_t half_infinity = 0x7c00U;

bool all_equal(const std::vector<float>& values, float expected) {
    for (const float value : values) {
        if (value != expected) return false;
    }
    return true;
}

LightPlanesDefect store_defect_of(const std::vector<std::byte>& bytes, LightPlanes& kept) {
    const LightPlanesDefect defect = engine_light::read_light_plane_store(bytes, kept);
    return defect;
}

void reads_the_store() {
    LightPlanes read{};
    const std::vector<std::byte> store = store_bytes(3U, 2U, 1U, 0.5F, half_quarter, half_half, half_one, 1U);
    require(engine_light::read_light_plane_store(store, read) == LightPlanesDefect::none, "a store reads");
    require(read.width == 3U && read.height == 2U && read.lamp_count == 1U && read.aperture == 0.5,
            "its shape and aperture read back");
    require(read.room.size() == 18U && all_equal(read.room, 0.25F), "the grey room is every channel of every pixel");
    require(read.lamps.size() == 18U && all_equal(read.lamps, 0.5F), "the grey lamp is every channel of every pixel");
    require(read.coverage.size() == 6U && read.coverage[5] == 1.0F, "the coverage reads back");
    LightPlanes tiny{};
    const std::vector<std::byte> subnormal = store_bytes(1U, 1U, 0U, 0.0F, half_smallest, half_zero, half_one, 1U);
    require(engine_light::read_light_plane_store(subnormal, tiny) == LightPlanesDefect::none &&
                static_cast<double>(tiny.room[0]) == std::ldexp(1.0, -24),
            "a subnormal half reads exactly");

    LightPlanes kept = uniform_planes(1U, 1U, 0U, 0.0F, 0.0F, 1.0F);
    std::vector<std::byte> short_store = store;
    short_store.resize(10U);
    require(store_defect_of(short_store, kept) == LightPlanesDefect::short_header, "a short header is refused");
    std::vector<std::byte> foreign = store;
    foreign[0] = std::byte{'X'};
    require(store_defect_of(foreign, kept) == LightPlanesDefect::not_a_store, "another file is not a store");
    require(store_defect_of(store_bytes(3U, 2U, 1U, 0.5F, half_quarter, half_half, half_one, 2U), kept) ==
                LightPlanesDefect::unsupported_version,
            "another version is refused");
    require(store_defect_of(store_bytes(3U, 2U, 1U, 1.5F, half_quarter, half_half, half_one, 1U), kept) ==
                LightPlanesDefect::aperture_out_of_range,
            "an aperture past 1 is refused");
    std::vector<std::byte> truncated = store;
    truncated.pop_back();
    require(store_defect_of(truncated, kept) == LightPlanesDefect::length_mismatch, "a truncated store is refused");
    require(store_defect_of(store_bytes(3U, 2U, 1U, 0.5F, half_infinity, half_half, half_one, 1U), kept) ==
                LightPlanesDefect::not_finite,
            "an infinite light is refused");
    require(store_defect_of(store_bytes(3U, 2U, 1U, 0.5F, half_quarter, half_half, half_two, 1U), kept) ==
                LightPlanesDefect::coverage_out_of_range,
            "a coverage past 1 is refused");
    require(kept.width == 1U && kept.room.size() == 3U, "a refused store leaves the planes alone");
}

void ships_the_bakes() {
    constexpr std::array<std::string_view, 4> dialog{"dialog_ok_up", "dialog_ok_down", "dialog_cancel_up",
                                                     "dialog_cancel_down"};
    for (const std::string_view name : dialog) {
        LightPlanes planes{};
        const std::span<const std::byte> shipped = engine_light::shipped_light_plane_store(name);
        require(!shipped.empty(), "each dialog key's bake is shipped");
        require(engine_light::read_light_plane_store(shipped, planes) == LightPlanesDefect::none, "and reads");
        require(planes.width == 240U && planes.height == 105U && planes.lamp_count == 1U && planes.aperture == 0.0,
                "at the baker's dialog size, its lamps summed");
    }
    constexpr std::array<std::string_view, 2> indicator{"indicator_backed_up", "indicator_clear_up"};
    for (const std::string_view name : indicator) {
        LightPlanes planes{};
        require(engine_light::read_light_plane_store(engine_light::shipped_light_plane_store(name), planes) ==
                    LightPlanesDefect::none,
                "each indicator bake is shipped and reads");
        require(planes.width == 112U && planes.height == 112U &&
                    std::fabs(planes.aperture - engine_light::indicator_jewel_aperture) < 1e-6,
                "at the baker's indicator size, with the jewel's hole");
    }
    require(!engine_light::shipped_light_plane_store(engine_light::jewel_lamp_bake).empty(),
            "the jewel lamp's bake is shipped");
    require(engine_light::shipped_light_plane_store("dialog_spare_up").empty(), "no other name is shipped");
}

// -- reading ------------------------------------------------------------------------

void reads_the_bake_file() {
    const LightPlanes written = varied_planes();
    LightPlanes read{};
    require(engine_light::read_light_planes(bake_bytes(written), read) == LightPlanesDefect::none, "a bake file reads");
    require(read.width == 3U && read.height == 2U && read.lamp_count == 2U, "its shape reads back");
    require(read.room == written.room && read.lamps == written.lamps && read.coverage == written.coverage,
            "every plane reads back exactly");
}

LightPlanesDefect defect_of(const std::vector<std::byte>& bytes, LightPlanes& kept) {
    const LightPlanesDefect defect = engine_light::read_light_planes(bytes, kept);
    return defect;
}

void refuses_malformed_files() {
    LightPlanes kept = varied_planes();
    const LightPlanes before = kept;
    const std::vector<std::byte> good = bake_bytes(varied_planes());

    const std::vector<std::byte> short_header(good.begin(), good.begin() + 8);
    require(defect_of(short_header, kept) == LightPlanesDefect::short_header, "a short header is refused");

    std::vector<std::byte> empty = good;
    empty[0] = std::byte{0};
    require(defect_of(empty, kept) == LightPlanesDefect::empty_size, "a width of 0 is refused");

    std::vector<std::byte> too_large{};
    put_word(too_large, 3000U);
    put_word(too_large, 1U);
    put_word(too_large, 0U);
    require(defect_of(too_large, kept) == LightPlanesDefect::too_large, "a side past 2048 is refused");

    std::vector<std::byte> lamps = good;
    lamps[8] = std::byte{9};
    require(defect_of(lamps, kept) == LightPlanesDefect::too_many_lamps, "nine lamps are refused");
    std::vector<std::byte> negative = good;
    negative[8] = std::byte{0xff};
    negative[9] = std::byte{0xff};
    negative[10] = std::byte{0xff};
    negative[11] = std::byte{0xff};
    require(defect_of(negative, kept) == LightPlanesDefect::too_many_lamps, "a negative lamp count is refused");

    std::vector<std::byte> longer = good;
    put_float(longer, 0.0F);
    require(defect_of(longer, kept) == LightPlanesDefect::length_mismatch, "trailing bytes are refused");
    const std::vector<std::byte> shorter(good.begin(), good.end() - 4);
    require(defect_of(shorter, kept) == LightPlanesDefect::length_mismatch, "missing bytes are refused");

    LightPlanes infinite = varied_planes();
    infinite.lamps[5] = std::numeric_limits<float>::quiet_NaN();
    require(defect_of(bake_bytes(infinite), kept) == LightPlanesDefect::not_finite, "a NaN is refused");

    LightPlanes over = varied_planes();
    over.coverage[2] = 1.5F;
    require(defect_of(bake_bytes(over), kept) == LightPlanesDefect::coverage_out_of_range,
            "a coverage past 1 is refused");

    require(kept.room == before.room && kept.lamps == before.lamps && kept.coverage == before.coverage &&
                kept.width == before.width,
            "a refused file leaves the planes as they were");
}

// -- compositing --------------------------------------------------------------------

std::shared_ptr<PartRaster> make_raster(std::uint32_t width, std::uint32_t height) {
    std::shared_ptr<PartRaster> raster = PartRaster::create(DeviceExtent{width, height}, DeviceScale{1.0});
    require(raster != nullptr, "a test raster is made");
    return raster;
}

std::array<std::uint8_t, 4> pixel_of(const PartRaster& raster, std::uint32_t column, std::uint32_t row) {
    const std::span<const std::uint8_t> pixels = raster.pixels();
    const std::size_t at = static_cast<std::size_t>(row) * raster.row_bytes() + static_cast<std::size_t>(column) * 4U;
    const std::array<std::uint8_t, 4> pixel{pixels[at], pixels[at + 1U], pixels[at + 2U], pixels[at + 3U]};
    return pixel;
}

// panel_lamps' pixel for straight linear light `v` per channel and
// coverage `cover`, as premultiplied BGRA.
std::array<std::uint8_t, 4> expected_pixel(double red, double green, double blue, double cover) {
    const std::uint32_t alpha = static_cast<std::uint32_t>(std::floor(cover * 255.0 + 0.5));
    const std::array<double, 3> channels{blue, green, red};
    std::array<std::uint8_t, 4> pixel{};
    for (std::size_t at = 0U; at < 3U; ++at) {
        const double toned = 1.0 - engine_exponential(-channels[at] * 1.3);
        const std::uint32_t straight = engine_primitives::encode_linear(toned);
        pixel[at] = static_cast<std::uint8_t>((straight * alpha + 127U) / 255U);
    }
    pixel[3] = static_cast<std::uint8_t>(alpha);
    return pixel;
}

std::size_t composite(const LightPlanes& up, const LightPlanes& down, std::span<const LampDrive> drives,
                      const LightComposite& settings, LightPlaneRect rect, PartRaster& raster) {
    const PartRasterView view = raster.view();
    const std::size_t written = engine_light::composite_light_planes(up, down, drives, settings, rect, view);
    return written;
}

void composites_as_panel_lamps() {
    // Coverage 0.5, room 0.25 and a lamp plane of 0.15, premultiplied: the
    // straight room is 0.5 and the lamp 0.3.
    const LightPlanes planes = uniform_planes(4U, 3U, 1U, 0.25F, 0.15F, 0.5F);
    const LightPlaneRect native{0, 0, 4, 3};
    const std::array<LampDrive, 1> off{LampDrive{engine_light::lamp_red, 0.0}};
    std::shared_ptr<PartRaster> raster = make_raster(4U, 3U);
    require(composite(planes, planes, off, LightComposite{}, native, *raster) == 12U, "every pixel is written");
    const double room = static_cast<double>(0.25F) / static_cast<double>(0.5F);
    require(pixel_of(*raster, 2U, 1U) == expected_pixel(room, room, room, 0.5), "an unlit lamp leaves the room alone");

    const std::array<LampDrive, 1> lit{LampDrive{engine_light::lamp_red, 1.0}};
    require(composite(planes, planes, lit, LightComposite{}, native, *raster) == 12U, "the lit key is written");
    const double glow = static_cast<double>(0.15F);
    const double red = (0.25 + (1.0 * 1.0) * glow) / 0.5;
    const double green = (0.25 + (0.12 * 1.0) * glow) / 0.5;
    const double blue = (0.25 + (0.04 * 1.0) * glow) / 0.5;
    require(pixel_of(*raster, 3U, 2U) == expected_pixel(red, green, blue, 0.5),
            "the lamp adds its colour times its power, unpremultiplied and toned");

    LightComposite dim{};
    dim.room = 0.5;
    require(composite(planes, planes, off, dim, native, *raster) == 12U, "the dimmed room is written");
    const double half_room = (0.5 * static_cast<double>(0.25F)) / 0.5;
    require(pixel_of(*raster, 0U, 0U) == expected_pixel(half_room, half_room, half_room, 0.5), "the room's gain applies");

    const LightPlanes uncovered = uniform_planes(4U, 3U, 1U, 0.0F, 0.0F, 0.0F);
    require(composite(uncovered, uncovered, lit, LightComposite{}, native, *raster) == 12U, "an empty bake is written");
    const std::array<std::uint8_t, 4> clear{0U, 0U, 0U, 0U};
    require(pixel_of(*raster, 1U, 1U) == clear, "no coverage is transparent");

    // Light is never local (owner, 2026-11-12): a bake whose lamp spills
    // onto the panel round the key, where its coverage is 0, shows none of
    // that spill.
    const LightPlanes spill = uniform_planes(4U, 3U, 1U, 0.3F, 2.0F, 0.0F);
    require(composite(spill, spill, lit, LightComposite{}, native, *raster) == 12U, "a spilling bake is written");
    require(pixel_of(*raster, 2U, 2U) == clear, "a lamp's spill past the key's coverage never shows");

    // A-gf-core-05-F4: a caller's view whose extent claims more rows than
    // its buffer holds is refused before any write, as is a rectangle whose
    // far edge overflows.
    std::vector<std::uint8_t> short_buffer(4U * 4U * 2U, std::uint8_t{0xAB});
    PartRasterView short_view{};
    short_view.pixels = std::span<std::uint8_t>(short_buffer);
    short_view.extent = DeviceExtent{4U, 3U};
    short_view.row_bytes = 16U;
    require(engine_light::composite_light_planes(planes, planes, lit, LightComposite{}, native, short_view) == 0U &&
                short_buffer[0] == std::uint8_t{0xAB},
            "a view whose buffer is short of its extent is refused, untouched");
    const LightPlaneRect overflowing{std::numeric_limits<std::int64_t>::max() - 1, 0, 4, 3};
    require(composite(planes, planes, lit, LightComposite{}, overflowing, *raster) == 0U,
            "a rectangle whose far edge overflows is refused");
}

void press_mixes_the_bakes() {
    const LightPlanes up = uniform_planes(4U, 3U, 1U, 0.2F, 0.1F, 1.0F);
    const LightPlanes down = uniform_planes(4U, 3U, 1U, 0.1F, 0.4F, 1.0F);
    const std::array<LampDrive, 1> lit{LampDrive{engine_light::lamp_white, 1.0}};
    const LightPlaneRect native{0, 0, 4, 3};
    std::shared_ptr<PartRaster> pressed = make_raster(4U, 3U);
    std::shared_ptr<PartRaster> alone = make_raster(4U, 3U);
    LightComposite settings{};
    static_cast<void>(composite(up, down, lit, settings, native, *pressed));
    static_cast<void>(composite(up, up, lit, settings, native, *alone));
    require(pixel_of(*pressed, 1U, 1U) == pixel_of(*alone, 1U, 1U), "unpressed, the up bake alone");
    settings.press_mix = 1.0;
    static_cast<void>(composite(up, down, lit, settings, native, *pressed));
    static_cast<void>(composite(down, down, lit, settings, native, *alone));
    require(pixel_of(*pressed, 1U, 1U) == pixel_of(*alone, 1U, 1U), "pressed, the down bake alone");
    settings.press_mix = 0.5;
    static_cast<void>(composite(up, down, lit, settings, native, *pressed));
    const double red = static_cast<double>(0.2F) + (1.0 * 1.0) * static_cast<double>(0.1F);
    const double red_down = static_cast<double>(0.1F) + (1.0 * 1.0) * static_cast<double>(0.4F);
    const double mixed_red = red + (red_down - red) * 0.5;
    require(pixel_of(*pressed, 1U, 1U)[2] == expected_pixel(mixed_red, 0.0, 0.0, 1.0)[2],
            "half pressed, the light halfway between the bakes");

    const LightPlanes other = uniform_planes(5U, 3U, 1U, 0.2F, 0.1F, 1.0F);
    require(composite(up, other, lit, settings, native, *pressed) == 0U, "bakes of different shapes are refused");
    const std::array<LampDrive, 0> none{};
    require(composite(up, up, none, settings, native, *pressed) == 0U, "fewer drives than lamps are refused");
}

void stretches_over_other_sizes() {
    const LightPlanes planes = uniform_planes(4U, 3U, 2U, 0.3F, 0.05F, 0.75F);
    const std::array<LampDrive, 2> lit{LampDrive{engine_light::lamp_amber, 0.7}, LampDrive{engine_light::lamp_cyan, 1.0}};
    std::shared_ptr<PartRaster> native = make_raster(4U, 3U);
    static_cast<void>(composite(planes, planes, lit, LightComposite{}, LightPlaneRect{0, 0, 4, 3}, *native));
    const std::array<std::uint8_t, 4> reference = pixel_of(*native, 0U, 0U);

    std::shared_ptr<PartRaster> large = make_raster(9U, 7U);
    require(composite(planes, planes, lit, LightComposite{}, LightPlaneRect{0, 0, 9, 7}, *large) == 63U,
            "a larger rectangle is written whole");
    bool uniform = true;
    for (std::uint32_t row = 0U; row < 7U; ++row) {
        for (std::uint32_t column = 0U; column < 9U; ++column) {
            if (!(pixel_of(*large, column, row) == reference)) uniform = false;
        }
    }
    require(uniform, "a uniform bake stays uniform, stretched");

    // Partly outside: 5 x 4 at (4, 3) in a 6 x 6 destination writes its
    // 2 x 3 overlap; a sentinel elsewhere is left alone.
    std::shared_ptr<PartRaster> clipped = make_raster(6U, 6U);
    const PartRasterView view = (*clipped).view();
    std::fill(view.pixels.begin(), view.pixels.end(), std::uint8_t{0xcd});
    require(composite(planes, planes, lit, LightComposite{}, LightPlaneRect{4, 3, 5, 4}, *clipped) == 6U,
            "only the rectangle's pixels inside the destination are written");
    const std::array<std::uint8_t, 4> sentinel{0xcdU, 0xcdU, 0xcdU, 0xcdU};
    require(pixel_of(*clipped, 3U, 3U) == sentinel && pixel_of(*clipped, 4U, 2U) == sentinel,
            "nothing outside the rectangle is written");
    require(pixel_of(*clipped, 4U, 3U) == reference, "inside it the bake's light");
}

// -- motion and the bake's defaults --------------------------------------------------

bool near(double a, double b) {
    const bool close = std::abs(a - b) < 1e-12;
    return close;
}

// -- jewels and pinpoints -----------------------------------------------------------

void jewel_filter_tints() {
    const engine_light::LampColour red = engine_light::jewel_filter(engine_light::lamp_red);
    require(red.red == 1.0 && near(red.green, 0.4 + 0.6 * 0.12) && near(red.blue, 0.4 + 0.6 * 0.04),
            "a red jewel passes red and tints the rest");
    // The study's colours are filters over a white-hot lamp (panel_lamps.html:
    // "Lamp colours as filters over a white-hot lamp"), its white the warm
    // [1, 0.92, 0.78]: a white jewel is faintly warm glass, not clear. Only
    // a neutral colour gives a clear jewel.
    const engine_light::LampColour white = engine_light::jewel_filter(engine_light::lamp_white);
    require(white.red == 1.0 && near(white.green, 0.4 + 0.6 * 0.92) && near(white.blue, 0.4 + 0.6 * 0.78),
            "the study's warm white jewel is faintly warm glass");
    const engine_light::LampColour neutral = engine_light::jewel_filter(engine_light::LampColour{1.0, 1.0, 1.0});
    require(neutral.red == 1.0 && neutral.green == 1.0 && neutral.blue == 1.0, "a neutral jewel is clear");
    const engine_light::LampColour dark = engine_light::jewel_filter(engine_light::LampColour{0.0, 0.0, 0.0});
    require(dark.red == 1.0 && dark.green == 1.0 && dark.blue == 1.0, "a colourless lamp's jewel is clear");
}

void jewels_and_pinpoints() {
    LightPlanes planes = uniform_planes(16U, 16U, 1U, 0.2F, 0.3F, 1.0F);
    planes.aperture = 0.5;
    const std::array<LampDrive, 1> lit{LampDrive{engine_light::lamp_red, engine_light::jewel_lamp_power}};
    const std::array<LampDrive, 1> off{LampDrive{engine_light::lamp_red, 0.0}};
    // A 20 x 20 jewel: half 10, rim 10 × 2/14, dot radius about 8.57.
    LightComposite jewel{};
    jewel.shape = engine_light::LightShape::jewel;
    jewel.filter = engine_light::jewel_filter(engine_light::lamp_red);
    const LightPlaneRect square{0, 0, 20, 20};
    std::shared_ptr<PartRaster> raster = make_raster(20U, 20U);
    require(composite(planes, planes, lit, jewel, square, *raster) == 400U, "the jewel is written");
    require(pixel_of(*raster, 0U, 0U)[3] == 0U, "outside the jewel is transparent");
    const std::array<std::uint8_t, 4> lit_centre = pixel_of(*raster, 10U, 10U);
    require(lit_centre[3] == 255U && lit_centre[2] > lit_centre[0], "the lit jewel's centre is opaque and red");
    const std::array<std::uint8_t, 4> rim = pixel_of(*raster, 10U, 0U);
    require(rim[3] > 0U && rim[3] < 255U && rim[2] < 8U && rim[1] < 8U && rim[0] < 8U,
            "the rim round the dot is dark and part covered");
    require(composite(planes, planes, off, jewel, square, *raster) == 400U, "the unlit jewel is written");
    const std::array<std::uint8_t, 4> off_centre = pixel_of(*raster, 10U, 10U);
    require(off_centre[2] > off_centre[0] && off_centre[2] < lit_centre[2], "unlit, the jewel is tinted and dimmer");
    LightComposite clear = jewel;
    clear.filter = engine_light::LampColour{1.0, 1.0, 1.0};
    require(composite(planes, planes, off, clear, square, *raster) == 400U, "a clear jewel is written");
    const std::array<std::uint8_t, 4> grey = pixel_of(*raster, 10U, 10U);
    require(grey[0] == grey[1] && grey[1] == grey[2], "a clear jewel over a grey bake is grey");

    // A 30 x 30 pinpoint: half 15, core radius 5, the halo from 3.
    LightComposite pinpoint{};
    pinpoint.shape = engine_light::LightShape::pinpoint;
    pinpoint.filter = engine_light::jewel_filter(engine_light::lamp_red);
    const LightPlaneRect wide{0, 0, 30, 30};
    std::shared_ptr<PartRaster> point = make_raster(30U, 30U);
    require(composite(planes, planes, lit, pinpoint, wide, *point) == 900U, "the pinpoint is written");
    require(pixel_of(*point, 15U, 15U)[3] == 255U, "its core is opaque");
    const std::array<std::uint8_t, 4> glow = pixel_of(*point, 23U, 15U);
    require(glow[3] > 0U && glow[3] < 255U && glow[2] > glow[0], "its halo glows red past the core");
    require(pixel_of(*point, 0U, 0U)[3] == 0U, "the halo fades out before the corner");
    require(composite(planes, planes, off, pinpoint, wide, *point) == 900U, "the unlit pinpoint is written");
    require(pixel_of(*point, 23U, 15U)[3] == 0U, "unlit, the pinpoint has no halo");
    require(pixel_of(*point, 15U, 15U)[3] == 255U, "unlit, its core still shows");
}


void envelopes_hold() {
    require(engine_light::lamp_power(false, 0.5, false) == 0.0, "an unlit lamp has no power, at once");
    require(near(engine_light::lamp_power(true, 0.0, false), 0.4), "a lamp strikes at 40%");
    require(near(engine_light::lamp_power(true, 0.03, false), 0.7), "halfway through the strike, 70%");
    require(engine_light::lamp_power(true, 0.06, false) == 1.0, "full power from 60 ms");
    require(engine_light::lamp_power(true, 0.0, true) == 1.0, "under reduced motion, full power at once");
    require(!engine_light::lamp_settled(0.05) && engine_light::lamp_settled(0.06), "the strike settles at 60 ms");
    // The cool-down (principle 7): the filament's inertia, then fully off.
    require(engine_light::lamp_cooling_power(1.0, 0.0, false) == 1.0, "a lamp switched off starts at its power");
    require(near(engine_light::lamp_cooling_power(1.0, 0.06, false), 0.25), "halfway through the cool-down, 25%");
    require(near(engine_light::lamp_cooling_power(0.7, 0.03, false), 0.7 * 0.5625),
            "a lamp switched off mid-strike cools from where it was");
    require(engine_light::lamp_cooling_power(1.0, 0.12, false) == 0.0, "fully off from 120 ms");
    require(engine_light::lamp_cooling_power(1.0, 0.0, true) == 0.0, "under reduced motion, off at once");
    require(!engine_light::lamp_cooled(0.119) && engine_light::lamp_cooled(0.12), "the cool-down ends at 120 ms");
    double previous = 1.0;
    bool falling = true;
    for (int step = 1; step <= 12; ++step) {
        const double power = engine_light::lamp_cooling_power(1.0, 0.01 * static_cast<double>(step), false);
        if (!(power < previous)) falling = false;
        previous = power;
    }
    require(falling && previous == 0.0, "the cool-down falls every frame and ends at 0, so the frames stop");

    require(engine_light::key_press_mix(0.0, 1.0, 0.0, false) == 0.0, "a press starts where it was");
    require(near(engine_light::key_press_mix(0.0, 1.0, 0.035, false), 0.75), "halfway, eased to 75%");
    require(engine_light::key_press_mix(0.0, 1.0, 0.07, false) == 1.0, "down at 70 ms");
    require(near(engine_light::key_press_mix(0.5, 0.0, 0.035, false), 0.125), "a release eases from where it was");
    require(engine_light::key_press_mix(0.2, 1.0, 0.0, true) == 1.0, "under reduced motion, at once");
    require(!engine_light::key_press_settled(0.069) && engine_light::key_press_settled(0.07),
            "the press settles at 70 ms");

    require(near(engine_light::lit_key_lean_degrees(engine_light::dialog_key_study_bake.height_share),
                 3.0 + 17.0 * ((0.57 - 0.25) / 0.75)),
            "the study's 0.57 share leans its sides about 10.25 degrees");
    require(engine_light::lit_key_lean_degrees(0.1) == 3.0 && engine_light::lit_key_lean_degrees(2.0) == 20.0,
            "the lean runs from 3 to 20 degrees");
    require(engine_light::dialog_key_study_bake.camera_tilt_degrees == 16.0 &&
                engine_light::dialog_key_study_bake.bake_width == 240U &&
                engine_light::dialog_key_study_bake.display_width == 180.0 && engine_light::dialog_key_study_bake.room == 1.0,
            "the study's dialog bake and display values");
    require(engine_light::lamp_red.red == 1.0 && engine_light::lamp_red.green == 0.12 &&
                engine_light::lamp_red.blue == 0.04,
            "the owner's red");
}

// -- the cache ----------------------------------------------------------------------

void write_file(const std::filesystem::path& path, const std::vector<std::byte>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(file), "a test bake is written");
}

void cache_reads_on_first_use() {
    const std::filesystem::path directory =
        test_support::make_unique_temporary_directory("gui_forms_engine_light_planes_tests");
    write_file(directory / "dialog_ok_up.f32", bake_bytes(varied_planes()));
    std::vector<std::byte> broken = bake_bytes(varied_planes());
    broken.pop_back();
    write_file(directory / "dialog_ok_down.f32", broken);

    engine_light::LightPlaneCache cache(directory);
    require(cache.reads() == 0U, "nothing is read before it is asked for");
    const std::shared_ptr<const LightPlanes> first = cache.lend("dialog_ok_up");
    require(first != nullptr && (*first).width == 3U && cache.reads() == 1U, "the first ask reads the bake");
    const std::shared_ptr<const LightPlanes> again = cache.lend("dialog_ok_up");
    require(again == first && cache.reads() == 1U, "the second is lent from the cache");

    require(cache.lend("dialog_ok_down") == nullptr && cache.reads() == 2U, "a defective bake is refused");
    require(cache.defect("dialog_ok_down") == LightPlanesDefect::length_mismatch, "with its defect");
    require(cache.lend("dialog_ok_down") == nullptr && cache.reads() == 2U, "and not read again");
    require(cache.lend("dialog_spare_up") == nullptr && cache.defect("dialog_spare_up") == LightPlanesDefect::unreadable,
            "a bake neither in the directory nor shipped is refused");
    const std::shared_ptr<const LightPlanes> shipped = cache.lend("dialog_cancel_up");
    require(shipped != nullptr && (*shipped).width == 240U && cache.reads() == 4U,
            "a bake missing from the directory is lent from the shipped store");
    write_file(directory / "indicator_clear_up.gflp",
               store_bytes(3U, 2U, 1U, 0.5F, half_quarter, half_half, half_one, 1U));
    const std::shared_ptr<const LightPlanes> stored = cache.lend("indicator_clear_up");
    require(stored != nullptr && (*stored).width == 3U && cache.reads() == 5U,
            "the directory's store stands before the shipped one");
    require(cache.lend("../dialog_ok_up") == nullptr && cache.lend("Dialog") == nullptr && cache.reads() == 5U,
            "a name that is not a bake's reads nothing");
    test_support::remove_temporary_directory(directory);

    engine_light::LightPlaneCache shipped_only{std::filesystem::path{}};
    const std::shared_ptr<const LightPlanes> jewel = shipped_only.lend(engine_light::jewel_lamp_bake);
    require(jewel != nullptr && (*jewel).width == 112U, "with no directory, the shipped bakes are lent");
}

// -- previewing ---------------------------------------------------------------------

bool write_png(const PartRaster& raster, double scale, const std::filesystem::path& path) {
    WindowCapture capture{};
    capture.pixel_width = raster.extent().width;
    capture.pixel_height = raster.extent().height;
    capture.scale = scale;
    capture.logical_size = Size{static_cast<double>(capture.pixel_width) / scale,
                                static_cast<double>(capture.pixel_height) / scale};
    const std::size_t live = static_cast<std::size_t>(capture.pixel_width) * 4U;
    capture.bgra.resize(live * capture.pixel_height);
    const std::span<const std::uint8_t> pixels = raster.pixels();
    for (std::uint32_t row = 0U; row < capture.pixel_height; ++row) {
        const std::size_t source = static_cast<std::size_t>(row) * raster.row_bytes();
        std::memcpy(capture.bgra.data() + static_cast<std::size_t>(row) * live, pixels.data() + source, live);
    }
    std::vector<std::byte> png{};
    if (!encode_window_capture_png(capture, png)) return false;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    const bool written = static_cast<bool>(file);
    return written;
}

struct PreviewKey final {
    const char* stem{};
    engine_light::LampColour colour{};
};

int preview(const std::filesystem::path& output, const std::filesystem::path& bakes) {
    std::error_code made{};
    std::filesystem::create_directories(output, made);
    if (made) {
        std::cerr << "gui_forms_engine_light_planes_tests: cannot make " << output << '\n';
        return 1;
    }
    engine_light::LightPlaneCache cache(bakes);
    // CANCEL under the warm white lamps, OK under the owner's red.
    const std::array<PreviewKey, 2> keys{PreviewKey{"dialog_cancel", engine_light::lamp_white},
                                         PreviewKey{"dialog_ok", engine_light::lamp_red}};
    constexpr std::array<double, 2> scales{1.0, 2.0};
    for (const PreviewKey& key : keys) {
        const std::shared_ptr<const LightPlanes> up = cache.lend(std::string(key.stem) + "_up");
        const std::shared_ptr<const LightPlanes> down = cache.lend(std::string(key.stem) + "_down");
        if (up == nullptr || down == nullptr) {
            std::cerr << "gui_forms_engine_light_planes_tests: " << key.stem << " has no bakes in " << bakes << '\n';
            return 1;
        }
        std::vector<LampDrive> drives((*up).lamp_count, LampDrive{key.colour, 1.0});
        std::vector<LampDrive> dark((*up).lamp_count, LampDrive{key.colour, 0.0});
        for (const double scale : scales) {
            const double width = engine_light::dialog_key_study_bake.display_width * scale;
            const double height = width * static_cast<double>((*up).height) / static_cast<double>((*up).width);
            const std::uint32_t pixel_width = static_cast<std::uint32_t>(std::floor(width + 0.5));
            const std::uint32_t pixel_height = static_cast<std::uint32_t>(std::floor(height + 0.5));
            const LightPlaneRect rect{0, 0, static_cast<std::int64_t>(pixel_width),
                                      static_cast<std::int64_t>(pixel_height)};
            for (int state = 0; state < 3; ++state) {
                // unlit, lit, lit and pressed
                LightComposite settings{};
                settings.press_mix = state == 2 ? 1.0 : 0.0;
                const std::span<const LampDrive> chosen = state == 0 ? std::span<const LampDrive>(dark)
                                                                     : std::span<const LampDrive>(drives);
                std::shared_ptr<PartRaster> raster = make_raster(pixel_width, pixel_height);
                static_cast<void>(composite(*up, *down, chosen, settings, rect, *raster));
                const char* const state_name = state == 0 ? "unlit" : (state == 1 ? "lit" : "pressed");
                const std::string name = std::string(key.stem) + "-" + state_name + "-" +
                                         std::to_string(static_cast<int>(scale)) + "x.png";
                const std::filesystem::path path = output / name;
                if (!write_png(*raster, scale, path)) {
                    std::cerr << "gui_forms_engine_light_planes_tests: cannot write " << path << '\n';
                    return 1;
                }
                std::cout << path.string() << '\n';
            }
        }
    }
    return 0;
}

} // namespace

// The failing lamp (owner, 2026-11-22): deterministic, steady under reduced
// motion, dark in its dropouts, and never more than two dips in any second.
void fault_flickers() {
    require(engine_light::lamp_fault_flicker(0.0, 7U, false) == 1.0, "a fault starts lit");
    require(engine_light::lamp_fault_flicker(-1.0, 7U, false) == 1.0 &&
                engine_light::lamp_fault_flicker(std::numeric_limits<double>::quiet_NaN(), 7U, false) == 1.0 &&
                engine_light::lamp_fault_flicker(1.0e16, 7U, false) == 1.0,
            "before its start, for no time, and past any lamp's life the factor is 1");
    std::size_t worst_dips = 0U;
    std::size_t seeds_reaching_dark = 0U;
    bool in_range = true;
    bool repeats = true;
    bool steady_when_reduced = true;
    bool seeds_differ = false;
    constexpr std::size_t frames_per_second = 240U;
    constexpr std::size_t frames = frames_per_second * 20U;
    for (std::uint32_t seed = 0U; seed < 200U; ++seed) {
        std::vector<double> powers;
        powers.reserve(frames);
        double lowest = 1.0;
        for (std::size_t frame = 0U; frame < frames; ++frame) {
            const double seconds = static_cast<double>(frame) / static_cast<double>(frames_per_second);
            const double power = engine_light::lamp_fault_flicker(seconds, seed, false);
            if (!(power > 0.0 && power <= 1.0)) in_range = false;
            if (engine_light::lamp_fault_flicker(seconds, seed, false) != power) repeats = false;
            if (engine_light::lamp_fault_flicker(seconds, seed, true) != 1.0) steady_when_reduced = false;
            if (engine_light::lamp_fault_flicker(seconds, seed + 1000U, false) != power) seeds_differ = true;
            lowest = std::min(lowest, power);
            powers.push_back(power);
        }
        if (lowest < 0.15) ++seeds_reaching_dark;
        // A dip is a fall through 70%; count them in every one-second window.
        std::vector<std::size_t> dips;
        for (std::size_t frame = 1U; frame < frames; ++frame) {
            if (powers[frame] < 0.7 && powers[frame - 1U] >= 0.7) dips.push_back(frame);
        }
        for (std::size_t first = 0U; first < dips.size(); ++first) {
            std::size_t count = 0U;
            for (std::size_t later = first; later < dips.size() && dips[later] < dips[first] + frames_per_second;
                 ++later) {
                ++count;
            }
            worst_dips = std::max(worst_dips, count);
        }
    }
    require(in_range, "the factor stays in (0, 1]");
    require(repeats, "the same seed and time give the same factor: captures repeat");
    require(steady_when_reduced, "under reduced motion a failing lamp is steadily lit");
    require(seeds_differ, "two seeds fail differently");
    require(worst_dips >= 1U && worst_dips <= 2U,
            "no second holds more than two dips, under three flashes a second");
    require(seeds_reaching_dark == 200U, "within 20 s every seed drops out to near dark");
    require(engine_light::lamp_fault_slot_seconds - engine_light::lamp_fault_latest_start > 0.5,
            "two events start more than half a second apart");
}

int main(int argc, char** argv) {
    if (argc > 3 && std::strcmp(argv[1], "--preview") == 0) return preview(argv[2], argv[3]);
    reads_the_bake_file();
    refuses_malformed_files();
    reads_the_store();
    ships_the_bakes();
    jewel_filter_tints();
    jewels_and_pinpoints();
    composites_as_panel_lamps();
    press_mixes_the_bakes();
    stretches_over_other_sizes();
    envelopes_hold();
    fault_flickers();
    cache_reads_on_first_use();
    std::cout << "gui_forms_engine_light_planes_tests passed\n";
    return 0;
}
