#include "projectile/projectile_simulation.h"
#include "projectile/projectile_state.h"
#include "projectile/terrain_profile.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <psapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace {

constexpr int kTopBarH = 48;
constexpr int kLeftPanelW = 268;
constexpr int kRightPanelW = 292;
constexpr int kBottomBarH = 58;

constexpr double kWorldXMin = 0.0;
constexpr double kWorldXMax = 300.0;
constexpr double kWorldYMin = 0.0;
constexpr double kWorldYMax = 120.0;
constexpr double kLaunchX = 10.0;
constexpr double kLaunchOffsetY = 3.0;

constexpr double kAirDragCoeff = 0.038;
constexpr double kZoomMin = 0.35;
constexpr double kZoomMax = 48.0;

struct ClientLayout {
    RECT client{};
    RECT top_bar{};
    RECT left_panel{};
    RECT right_panel{};
    RECT bottom_bar{};
    RECT viewport{};
    RECT speed_track{};
    RECT angle_track{};
    RECT gravity_track{};
    RECT btn_launch{};
    RECT btn_reset{};
    RECT btn_step{};
    RECT cb_vectors{};
    RECT cb_trace{};
    RECT cb_air{};
    RECT preset_earth{};
    RECT preset_moon{};
    RECT preset_mars{};
    RECT zoom_in{};
    RECT zoom_out{};
    RECT btn_fit{};
    RECT sat_view{};
};

enum class DragKind { None, Pan, SpeedSlider, AngleSlider, GravitySlider };

struct AppState {
    int width{1280};
    int height{720};
    ClientLayout layout{};

    double launch_speed{45.0};
    double launch_angle_deg{38.0};
    double gravity{9.81};

    bool launched{false};
    bool impact{false};
    bool paused{false};

    bool show_vectors{true};
    bool trace_path{true};
    bool air_resistance{false};

    omnisim::projectile::ProjectileType proj_type{omnisim::projectile::ProjectileType::Simple};

    int preset_selected{0};

    double camera_center_x{150.0};
    double camera_center_y{55.0};
    double pixels_per_world{3.2};

    double max_height_world{0.0};

    DragKind drag{DragKind::None};
    int drag_last_x{0};
    int drag_last_y{0};

    std::chrono::steady_clock::time_point fps_sample_start{};
    int fps_frame_count{0};
    double fps_display{0.0};

    omnisim::projectile::TerrainProfile terrain{};
    std::unique_ptr<omnisim::projectile::ProjectileSimulation> simulation{};
    std::vector<omnisim::math::Vector2> flight_path{};

    HDC back_dc{nullptr};
    HBITMAP back_bitmap{nullptr};
    HBITMAP back_old_bitmap{nullptr};
    HFONT font_ui{nullptr};
    HFONT font_title{nullptr};
};

AppState g_state{};

void update_layout() {
    const int w = g_state.width;
    const int h = g_state.height;
    ClientLayout& L = g_state.layout;
    L.client = {0, 0, w, h};
    L.top_bar = {0, 0, w, kTopBarH};
    L.bottom_bar = {0, h - kBottomBarH, w, h};
    L.left_panel = {0, kTopBarH, kLeftPanelW, h - kBottomBarH};
    L.right_panel = {w - kRightPanelW, kTopBarH, w, h - kBottomBarH};
    L.viewport = {kLeftPanelW, kTopBarH, w - kRightPanelW, h - kBottomBarH};

    const int lx = L.left_panel.left + 14;
    const int lw = kLeftPanelW - 28;
    int y = L.left_panel.top + 44;
    L.speed_track = {lx, y, lx + lw, y + 22};
    y += 72;
    L.angle_track = {lx, y, lx + lw, y + 22};
    y += 72;
    L.gravity_track = {lx, y, lx + lw, y + 22};
    y += 56;
    L.cb_vectors = {lx, y, lx + 18, y + 18};
    y += 28;
    L.cb_trace = {lx, y, lx + 18, y + 18};
    y += 28;
    L.cb_air = {lx, y, lx + 18, y + 18};
    y += 36;
    const int ph = 26;
    L.preset_earth = {lx, y, lx + lw, y + ph};
    y += ph + 6;
    L.preset_moon = {lx, y, lx + lw, y + ph};
    y += ph + 6;
    L.preset_mars = {lx, y, lx + lw, y + ph};

    const int bx = L.bottom_bar.left + 16;
    const int by = L.bottom_bar.top + 10;
    L.btn_launch = {bx, by, bx + 140, by + 36};
    L.btn_reset = {bx + 156, by, bx + 280, by + 36};
    L.btn_step = {bx + 296, by, bx + 420, by + 36};

    const RECT& vp = L.viewport;
    const int margin = 10;
    L.zoom_in = {vp.right - margin - 92, vp.bottom - margin - 36, vp.right - margin - 62, vp.bottom - margin};
    L.zoom_out = {vp.right - margin - 58, vp.bottom - margin - 36, vp.right - margin - 28, vp.bottom - margin};
    L.btn_fit = {vp.right - margin - 26, vp.bottom - margin - 36, vp.right - margin, vp.bottom - margin};

    const int rw = L.right_panel.right - L.right_panel.left - 20;
    L.sat_view = {L.right_panel.left + 10, L.right_panel.top + 36, L.right_panel.left + 10 + rw,
                  L.right_panel.top + 36 + 110};
}

[[nodiscard]] int viewport_center_x() {
    const RECT& vp = g_state.layout.viewport;
    return (vp.left + vp.right) / 2;
}

[[nodiscard]] int viewport_center_y() {
    const RECT& vp = g_state.layout.viewport;
    return (vp.top + vp.bottom) / 2;
}

[[nodiscard]] double deg_to_rad(const double deg) {
    return deg * (std::numbers::pi / 180.0);
}

void world_to_screen(const double wx, const double wy, int& sx, int& sy) {
    const int cx = viewport_center_x();
    const int cy = viewport_center_y();
    const double p = g_state.pixels_per_world;
    sx = cx + static_cast<int>(std::lround((wx - g_state.camera_center_x) * p));
    sy = cy - static_cast<int>(std::lround((wy - g_state.camera_center_y) * p));
}

void screen_to_world(const int sx, const int sy, double& wx, double& wy) {
    const int cx = viewport_center_x();
    const int cy = viewport_center_y();
    const double p = g_state.pixels_per_world;
    wx = g_state.camera_center_x + static_cast<double>(sx - cx) / p;
    wy = g_state.camera_center_y - static_cast<double>(sy - cy) / p;
}

[[nodiscard]] bool point_in_viewport(const int x, const int y) {
    const RECT& vp = g_state.layout.viewport;
    return x >= vp.left && x < vp.right && y >= vp.top && y < vp.bottom;
}

[[nodiscard]] double terrain_world_y(const double x) {
    return (std::clamp)(g_state.terrain.height_at(x), kWorldYMin, kWorldYMax);
}

void fit_camera_to_scene() {
    const RECT& vp = g_state.layout.viewport;
    const int vw = vp.right - vp.left;
    const int vh = vp.bottom - vp.top;
    if (vw <= 0 || vh <= 0) {
        return;
    }
    g_state.camera_center_x = (kWorldXMin + kWorldXMax) * 0.5;
    g_state.camera_center_y = (kWorldYMin + kWorldYMax) * 0.5;
    const double scale_x = static_cast<double>(vw) / (kWorldXMax - kWorldXMin);
    const double scale_y = static_cast<double>(vh) / (kWorldYMax - kWorldYMin);
    g_state.pixels_per_world = (std::min)(scale_x, scale_y) * 0.92;
}

void ensure_backbuffer(HWND hwnd) {
    if (g_state.width <= 0 || g_state.height <= 0) {
        return;
    }
    HDC window_dc = GetDC(hwnd);
    if (!g_state.back_dc) {
        g_state.back_dc = CreateCompatibleDC(window_dc);
    }
    if (g_state.back_bitmap) {
        SelectObject(g_state.back_dc, g_state.back_old_bitmap);
        DeleteObject(g_state.back_bitmap);
        g_state.back_bitmap = nullptr;
    }
    g_state.back_bitmap = CreateCompatibleBitmap(window_dc, g_state.width, g_state.height);
    g_state.back_old_bitmap = static_cast<HBITMAP>(SelectObject(g_state.back_dc, g_state.back_bitmap));
    ReleaseDC(hwnd, window_dc);
}

void release_backbuffer() {
    if (g_state.back_dc && g_state.back_old_bitmap) {
        SelectObject(g_state.back_dc, g_state.back_old_bitmap);
        g_state.back_old_bitmap = nullptr;
    }
    if (g_state.back_bitmap) {
        DeleteObject(g_state.back_bitmap);
        g_state.back_bitmap = nullptr;
    }
    if (g_state.back_dc) {
        DeleteDC(g_state.back_dc);
        g_state.back_dc = nullptr;
    }
    if (g_state.font_ui) {
        DeleteObject(g_state.font_ui);
        g_state.font_ui = nullptr;
    }
    if (g_state.font_title) {
        DeleteObject(g_state.font_title);
        g_state.font_title = nullptr;
    }
}

void create_ui_fonts() {
    if (!g_state.font_ui) {
        g_state.font_ui =
            CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    }
    if (!g_state.font_title) {
        g_state.font_title = CreateFontA(-18, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    }
}

[[nodiscard]] double value_from_track_x(const RECT& track, const double vmin, const double vmax, const int x) {
    const double t = static_cast<double>(x - track.left) / static_cast<double>(track.right - track.left);
    return vmin + (std::clamp)(t, 0.0, 1.0) * (vmax - vmin);
}

void reset_projectile() {
    const double launch_angle = deg_to_rad(g_state.launch_angle_deg);
    const double launch_x = kLaunchX;
    const double launch_y = terrain_world_y(launch_x) + kLaunchOffsetY;

    omnisim::projectile::ProjectileConfig cfg{};
    cfg.initial_position = {launch_x, launch_y};
    cfg.initial_velocity = {
        g_state.launch_speed * std::cos(launch_angle),
        g_state.launch_speed * std::sin(launch_angle),
    };
    cfg.gravity = {0.0, -g_state.gravity};
    cfg.max_duration_seconds = 60.0;
    cfg.enable_console_output = false;
    cfg.air_drag_coefficient = g_state.air_resistance ? kAirDragCoeff : 0.0;
    cfg.terrain = &g_state.terrain;
    cfg.type = g_state.proj_type;

    g_state.simulation = std::make_unique<omnisim::projectile::ProjectileSimulation>(cfg);
    g_state.simulation->initialize();
    g_state.flight_path.clear();
    g_state.flight_path.push_back(cfg.initial_position);
    g_state.launched = false;
    g_state.impact = false;
    g_state.paused = false;
    g_state.max_height_world = launch_y;
}

void update_simulation(const double dt) {
    if (g_state.paused || !g_state.launched || !g_state.simulation || g_state.impact) {
        return;
    }

    g_state.simulation->step(dt);
    const auto& s = g_state.simulation->state();
    g_state.max_height_world = (std::max)(g_state.max_height_world, s.position.y);

    const omnisim::math::Vector2 clamped_pos{
        s.position.x,
        (std::max)(s.position.y, kWorldYMin),
    };
    if (g_state.trace_path) {
        g_state.flight_path.push_back(clamped_pos);
    } else {
        if (g_state.flight_path.empty()) {
            g_state.flight_path.push_back(clamped_pos);
        } else {
            g_state.flight_path.back() = clamped_pos;
        }
    }
    
    if (g_state.simulation->is_finished()) {
        g_state.impact = true;
        g_state.launched = false;
    }
}

void step_one_frame() {
    if (!g_state.simulation || g_state.impact) {
        return;
    }
    if (!g_state.launched) {
        g_state.launched = true;
    }
    update_simulation(1.0 / 60.0);
}

void fill_rect(HDC hdc, const RECT& r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(hdc, &r, b);
    DeleteObject(b);
}

void frame_rect(HDC hdc, const RECT& r, COLORREF c) {
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ old = SelectObject(hdc, p);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, old);
    DeleteObject(p);
}

void draw_text_left(HDC hdc, int x, int y, const char* text, COLORREF color) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    TextOutA(hdc, x, y, text, static_cast<int>(std::strlen(text)));
}

void draw_slider(HDC hdc, const RECT& track, const char* label, const double value, const char* suffix) {
    fill_rect(hdc, track, RGB(14, 18, 28));
    frame_rect(hdc, track, RGB(55, 65, 85));
    if (label[0] == 'S') {
        const double tt = (g_state.launch_speed - 5.0) / (150.0 - 5.0);
        const int knob = track.left + static_cast<int>(tt * static_cast<double>(track.right - track.left - 8)) + 4;
        RECT knob_r{knob - 5, track.top - 2, knob + 5, track.bottom + 2};
        fill_rect(hdc, knob_r, RGB(0, 180, 220));
    } else if (label[0] == 'A') {
        const double tt = (g_state.launch_angle_deg - 5.0) / (85.0 - 5.0);
        const int knob = track.left + static_cast<int>(tt * static_cast<double>(track.right - track.left - 8)) + 4;
        RECT knob_r{knob - 5, track.top - 2, knob + 5, track.bottom + 2};
        fill_rect(hdc, knob_r, RGB(0, 180, 220));
    } else {
        const double tt = (g_state.gravity - 0.1) / (25.0 - 0.1);
        const int knob = track.left + static_cast<int>((std::clamp)(tt, 0.0, 1.0) * static_cast<double>(track.right - track.left - 8)) + 4;
        RECT knob_r{knob - 5, track.top - 2, knob + 5, track.bottom + 2};
        fill_rect(hdc, knob_r, RGB(0, 180, 220));
    }
    char buf[64];
    if (label[0] == 'G') {
        std::snprintf(buf, sizeof(buf), "%s  %.2f", label, value);
    } else {
        std::snprintf(buf, sizeof(buf), "%s  %.2f%s", label, value, suffix);
    }
    draw_text_left(hdc, track.left, track.top - 20, buf, RGB(190, 200, 220));
}

void draw_checkbox(HDC hdc, const RECT& box, const char* label, const bool checked) {
    fill_rect(hdc, box, RGB(20, 24, 34));
    frame_rect(hdc, box, RGB(80, 90, 110));
    if (checked) {
        draw_text_left(hdc, box.left + 3, box.top + 1, "x", RGB(0, 220, 240));
    }
    draw_text_left(hdc, box.right + 10, box.top + 1, label, RGB(200, 210, 230));
}

void draw_button(HDC hdc, const RECT& r, const char* text, const bool primary) {
    SelectObject(hdc, g_state.font_ui ? g_state.font_ui : static_cast<HFONT>(GetStockObject(ANSI_FIXED_FONT)));
    fill_rect(hdc, r, primary ? RGB(0, 110, 180) : RGB(45, 52, 68));
    frame_rect(hdc, r, RGB(90, 100, 120));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(235, 240, 250));
    SIZE sz{};
    GetTextExtentPoint32A(hdc, text, static_cast<int>(std::strlen(text)), &sz);
    const int tx = r.left + (r.right - r.left - sz.cx) / 2;
    const int ty = r.top + (r.bottom - r.top - sz.cy) / 2;
    TextOutA(hdc, tx, ty, text, static_cast<int>(std::strlen(text)));
}

void draw_preset_row(HDC hdc, const RECT& r, const char* title, const char* sub, const bool selected) {
    fill_rect(hdc, r, selected ? RGB(30, 55, 75) : RGB(28, 32, 44));
    frame_rect(hdc, r, selected ? RGB(0, 200, 230) : RGB(60, 70, 90));
    draw_text_left(hdc, r.left + 8, r.top + 5, title, RGB(220, 230, 245));
    draw_text_left(hdc, r.left + 8, r.top + 18, sub, RGB(140, 155, 175));
}

void draw_satellite_stub(HDC hdc, const RECT& r) {
    fill_rect(hdc, r, RGB(12, 14, 20));
    frame_rect(hdc, r, RGB(55, 65, 80));
    HPEN p = CreatePen(PS_SOLID, 1, RGB(60, 70, 85));
    HGDIOBJ op = SelectObject(hdc, p);
    const int w = r.right - r.left;
    const int h = r.bottom - r.top;
    for (int i = 0; i < w; i += 6) {
        MoveToEx(hdc, r.left + i, r.top, nullptr);
        LineTo(hdc, r.left + i, r.bottom);
    }
    for (int j = 0; j < h; j += 6) {
        MoveToEx(hdc, r.left, r.top + j, nullptr);
        LineTo(hdc, r.right, r.top + j);
    }
    HPEN tp = CreatePen(PS_SOLID, 2, RGB(90, 120, 95));
    SelectObject(hdc, tp);
    DeleteObject(p);
    for (int i = 0; i <= 40; ++i) {
        const double u = static_cast<double>(i) / 40.0;
        const double wx = kWorldXMin + u * (kWorldXMax - kWorldXMin);
        const double wy = terrain_world_y(wx);
        const int sx = r.left + static_cast<int>(u * static_cast<double>(w - 4)) + 2;
        const int sy = r.bottom - 4 - static_cast<int>((wy - kWorldYMin) / (kWorldYMax - kWorldYMin) * static_cast<double>(h - 8));
        if (i == 0) {
            MoveToEx(hdc, sx, sy, nullptr);
        } else {
            LineTo(hdc, sx, sy);
        }
    }
    SelectObject(hdc, op);
    DeleteObject(tp);
}

void draw_viewport_world(HDC hdc) {
    const RECT& vp = g_state.layout.viewport;
    const int vw = vp.right - vp.left;
    const int vh = vp.bottom - vp.top;

    const double wx0 = g_state.camera_center_x - static_cast<double>(vw) / (2.0 * g_state.pixels_per_world);
    const double wx1 = g_state.camera_center_x + static_cast<double>(vw) / (2.0 * g_state.pixels_per_world);
    const double wy0 = g_state.camera_center_y - static_cast<double>(vh) / (2.0 * g_state.pixels_per_world);
    const double wy1 = g_state.camera_center_y + static_cast<double>(vh) / (2.0 * g_state.pixels_per_world);

    const double grid_step = g_state.pixels_per_world > 6.0 ? 5.0 : (g_state.pixels_per_world > 2.5 ? 10.0 : 25.0);

    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(32, 42, 58));
    HGDIOBJ old_pen = SelectObject(hdc, grid_pen);
    for (double gx = std::floor(wx0 / grid_step) * grid_step; gx <= wx1 + grid_step; gx += grid_step) {
        int ax = 0;
        int ay = 0;
        world_to_screen(gx, wy0, ax, ay);
        int bx = 0;
        int by = 0;
        world_to_screen(gx, wy1, bx, by);
        MoveToEx(hdc, ax, ay, nullptr);
        LineTo(hdc, bx, by);
    }
    for (double gy = std::floor(wy0 / grid_step) * grid_step; gy <= wy1 + grid_step; gy += grid_step) {
        int ax = 0;
        int ay = 0;
        world_to_screen(wx0, gy, ax, ay);
        int bx = 0;
        int by = 0;
        world_to_screen(wx1, gy, bx, by);
        MoveToEx(hdc, ax, ay, nullptr);
        LineTo(hdc, bx, by);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(grid_pen);

    constexpr int kTerrainSamples = 400;
    std::vector<POINT> terrain_pts(static_cast<std::size_t>(kTerrainSamples + 2));
    terrain_pts[0] = {vp.left, vp.bottom};
    for (int i = 0; i <= kTerrainSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kTerrainSamples);
        const double x = wx0 + t * (wx1 - wx0);
        const double y = terrain_world_y(x);
        int tx = 0;
        int ty = 0;
        world_to_screen(x, y, tx, ty);
        terrain_pts[static_cast<std::size_t>(i) + 1] = {tx, ty};
    }
    terrain_pts[terrain_pts.size() - 1] = {vp.right, vp.bottom};

    HPEN mountain_pen = CreatePen(PS_SOLID, 2, RGB(79, 125, 77));
    HBRUSH mountain_fill = CreateSolidBrush(RGB(38, 66, 43));
    old_pen = SelectObject(hdc, mountain_pen);
    HGDIOBJ old_brush = SelectObject(hdc, mountain_fill);
    Polygon(hdc, terrain_pts.data(), static_cast<int>(terrain_pts.size()));
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(mountain_fill);
    DeleteObject(mountain_pen);

    const double cannon_x = kLaunchX;
    const double cannon_y = terrain_world_y(cannon_x) + kLaunchOffsetY;
    const double cannon_angle = deg_to_rad(g_state.launch_angle_deg);
    const double cannon_len = 7.0 + (g_state.launch_speed / 8.0);
    int cx = 0;
    int cy = 0;
    int ctip_x = 0;
    int ctip_y = 0;
    world_to_screen(cannon_x, cannon_y, cx, cy);
    world_to_screen(cannon_x + cannon_len * std::cos(cannon_angle), cannon_y + cannon_len * std::sin(cannon_angle), ctip_x, ctip_y);

    HPEN cannon_pen = CreatePen(PS_SOLID, 4, RGB(214, 154, 63));
    old_pen = SelectObject(hdc, cannon_pen);
    MoveToEx(hdc, cx, cy, nullptr);
    LineTo(hdc, ctip_x, ctip_y);
    SelectObject(hdc, old_pen);
    DeleteObject(cannon_pen);

    HBRUSH cannon_base = CreateSolidBrush(RGB(130, 90, 44));
    HGDIOBJ old_obj = SelectObject(hdc, cannon_base);
    Ellipse(hdc, cx - 10, cy - 10, cx + 10, cy + 10);
    SelectObject(hdc, old_obj);
    DeleteObject(cannon_base);

    if (g_state.simulation) {
        const auto& s = g_state.simulation->state();
        const double vmag = std::sqrt(s.velocity.x * s.velocity.x + s.velocity.y * s.velocity.y);
        int px = 0;
        int py = 0;
        world_to_screen(s.position.x, s.position.y, px, py);

        if (!g_state.launched && !g_state.impact) {
            HPEN pred_pen = CreatePen(PS_DOT, 1, RGB(130, 170, 255));
            old_pen = SelectObject(hdc, pred_pen);
            const double vx = g_state.launch_speed * std::cos(deg_to_rad(g_state.launch_angle_deg));
            const double vy = g_state.launch_speed * std::sin(deg_to_rad(g_state.launch_angle_deg));
            const double y0 = cannon_y;
            bool has_prev = false;
            int prev_sx = 0;
            int prev_sy = 0;
            for (double t = 0.0; t <= 30.0; t += 0.05) {
                double x = cannon_x + vx * t;
                double y = y0 + vy * t - 0.5 * g_state.gravity * t * t;
                const double ground = terrain_world_y(x);
                int sx = 0;
                int sy = 0;
                world_to_screen(x, y, sx, sy);
                if (has_prev) {
                    MoveToEx(hdc, prev_sx, prev_sy, nullptr);
                    LineTo(hdc, sx, sy);
                }
                has_prev = true;
                prev_sx = sx;
                prev_sy = sy;
                if (y <= ground) {
                    break;
                }
            }
            SelectObject(hdc, old_pen);
            DeleteObject(pred_pen);
        }

        if (g_state.flight_path.size() > 1) {
            HPEN real_pen = CreatePen(PS_SOLID, 2, RGB(120, 190, 255));
            old_pen = SelectObject(hdc, real_pen);
            for (std::size_t i = 1; i < g_state.flight_path.size(); ++i) {
                const auto& p0 = g_state.flight_path[i - 1];
                const auto& p1 = g_state.flight_path[i];
                int ax = 0;
                int ay = 0;
                int bx = 0;
                int by = 0;
                world_to_screen(p0.x, p0.y, ax, ay);
                world_to_screen(p1.x, p1.y, bx, by);
                MoveToEx(hdc, ax, ay, nullptr);
                LineTo(hdc, bx, by);
            }
            SelectObject(hdc, old_pen);
            DeleteObject(real_pen);
        }

        if (g_state.show_vectors) {
            if (vmag > 1e-6) {
                const double scale = (std::min)(18.0, vmag * 0.35);
                const double dx = (s.velocity.x / vmag) * scale;
                const double dy = (s.velocity.y / vmag) * scale;
                int vx1 = 0;
                int vy1 = 0;
                world_to_screen(s.position.x + dx, s.position.y + dy, vx1, vy1);
                HPEN vp = CreatePen(PS_SOLID, 2, RGB(255, 160, 60));
                old_pen = SelectObject(hdc, vp);
                MoveToEx(hdc, px, py, nullptr);
                LineTo(hdc, vx1, vy1);
                SelectObject(hdc, old_pen);
                DeleteObject(vp);
            }
        }

        HBRUSH projectile = CreateSolidBrush(RGB(220, 235, 255));
        old_obj = SelectObject(hdc, projectile);
        Ellipse(hdc, px - 8, py - 8, px + 8, py + 8);
        SelectObject(hdc, old_obj);
        DeleteObject(projectile);

        char vlabel[48];
        std::snprintf(vlabel, sizeof(vlabel), "V: %.2f m/s", vmag);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        TextOutA(hdc, px + 12, py - 18, vlabel, static_cast<int>(std::strlen(vlabel)));
    }

    draw_button(hdc, g_state.layout.zoom_in, "+", false);
    draw_button(hdc, g_state.layout.zoom_out, "-", false);
    draw_button(hdc, g_state.layout.btn_fit, "#", false);
}

void draw_scene(HDC hdc) {
    SelectObject(hdc, g_state.font_ui ? g_state.font_ui : static_cast<HFONT>(GetStockObject(ANSI_FIXED_FONT)));

    fill_rect(hdc, g_state.layout.client, RGB(12, 14, 18));

    fill_rect(hdc, g_state.layout.top_bar, RGB(22, 26, 34));
    frame_rect(hdc, g_state.layout.top_bar, RGB(50, 58, 72));
    SelectObject(hdc, g_state.font_title ? g_state.font_title : g_state.font_ui);
    draw_text_left(hdc, 16, 14, "OMNISIM  KINEMATICS_CORE  v1.0", RGB(200, 210, 230));
    SelectObject(hdc, g_state.font_ui);
    char top[256];
    const double t = g_state.simulation ? g_state.simulation->state().elapsed_seconds : 0.0;
    const char* p_name = "SIMPLE";
    if (g_state.proj_type == omnisim::projectile::ProjectileType::Grenade) p_name = "GRENADE";
    if (g_state.proj_type == omnisim::projectile::ProjectileType::Missile) p_name = "MISSILE";

    std::snprintf(top, sizeof(top), "ENGINE: PHYSX_STABLE     MODE: %s     TIME: %.3fs     G: %.2f m/s^2", p_name, t, g_state.gravity);
    draw_text_left(hdc, g_state.width - 550, 16, top, RGB(0, 210, 230));

    fill_rect(hdc, g_state.layout.left_panel, RGB(24, 28, 38));
    frame_rect(hdc, g_state.layout.left_panel, RGB(55, 62, 78));
    draw_text_left(hdc, g_state.layout.left_panel.left + 14, g_state.layout.left_panel.top + 12, "INITIAL CONDITIONS",
                   RGB(0, 200, 230));

    draw_slider(hdc, g_state.layout.speed_track, "SPEED (m/s)", g_state.launch_speed, "");
    draw_slider(hdc, g_state.layout.angle_track, "ANGLE (deg)", g_state.launch_angle_deg, "");
    draw_slider(hdc, g_state.layout.gravity_track, "GRAVITY m/s^2", g_state.gravity, "");

    draw_checkbox(hdc, g_state.layout.cb_vectors, "SHOW VECTORS", g_state.show_vectors);
    draw_checkbox(hdc, g_state.layout.cb_trace, "TRACE PATH", g_state.trace_path);
    draw_checkbox(hdc, g_state.layout.cb_air, "AIR RESISTANCE", g_state.air_resistance);

    draw_preset_row(hdc, g_state.layout.preset_earth, "[1] EARTH", "g = 9.81", g_state.preset_selected == 0);
    draw_preset_row(hdc, g_state.layout.preset_moon, "[2] MOON", "g = 1.62", g_state.preset_selected == 1);
    draw_preset_row(hdc, g_state.layout.preset_mars, "[3] MARS", "g = 3.71", g_state.preset_selected == 2);

    fill_rect(hdc, g_state.layout.right_panel, RGB(24, 28, 38));
    frame_rect(hdc, g_state.layout.right_panel, RGB(55, 62, 78));
    draw_text_left(hdc, g_state.layout.right_panel.left + 12, g_state.layout.right_panel.top + 10, "TELEMETRY",
                   RGB(0, 200, 230));
    draw_text_left(hdc, g_state.layout.right_panel.left + 12, g_state.layout.right_panel.top + 28, "SAT_VIEW_TR-01",
                   RGB(140, 150, 165));
    draw_satellite_stub(hdc, g_state.layout.sat_view);

    const auto& s = g_state.simulation ? g_state.simulation->state() : omnisim::projectile::ProjectileState{};
    const double vmag =
        g_state.simulation ? std::sqrt(s.velocity.x * s.velocity.x + s.velocity.y * s.velocity.y) : 0.0;
    char tel[512];
    std::snprintf(tel, sizeof(tel),
                  "STATUS: %s\n"
                  "POS_X:  %.2f\n"
                  "POS_Y:  %.2f\n"
                  "VEL_X:  %.2f\n"
                  "VEL_Y:  %.2f\n"
                  "SPEED:  %.2f m/s\n"
                  "MAX_H:  %.2f m\n"
                  "ZOOM:   %.2f px/m\n"
                  "CAM:    (%.1f, %.1f)",
                  g_state.paused                        ? "PAUSED"
                  : g_state.launched && !g_state.impact ? "ACTIVE"
                  : g_state.impact                      ? "IMPACT"
                                                        : "IDLE",
                  s.position.x,
                  s.position.y, s.velocity.x, s.velocity.y, vmag, g_state.max_height_world, g_state.pixels_per_world,
                  g_state.camera_center_x, g_state.camera_center_y);
    RECT tr = g_state.layout.right_panel;
    tr.top = g_state.layout.sat_view.bottom + 12;
    tr.left += 12;
    tr.right -= 10;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(210, 220, 235));
    SelectObject(hdc, g_state.font_ui);
    DrawTextA(hdc, tel, -1, &tr, DT_LEFT | DT_EXPANDTABS | DT_WORDBREAK);

    fill_rect(hdc, g_state.layout.bottom_bar, RGB(20, 24, 32));
    frame_rect(hdc, g_state.layout.bottom_bar, RGB(50, 58, 72));
    draw_button(hdc, g_state.layout.btn_launch, "LAUNCH SEQUENCE", true);
    draw_button(hdc, g_state.layout.btn_reset, "RESET ENGINE", false);
    draw_button(hdc, g_state.layout.btn_step, "STEP FRAME", false);

    char bot[160];
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    double ram_mb = 0.0;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        ram_mb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
    std::snprintf(bot, sizeof(bot), "RAM_USE: %.1f MB   FPS: %.1f   FRAME: %.4fs   RENDER: HARDWARE_GDI", ram_mb,
                  g_state.fps_display, 1.0 / (std::max)(g_state.fps_display, 1.0));
    draw_text_left(hdc, g_state.layout.btn_step.right + 24, g_state.layout.bottom_bar.top + 18, bot, RGB(130, 145, 165));

    fill_rect(hdc, g_state.layout.viewport, RGB(9, 12, 20));
    frame_rect(hdc, g_state.layout.viewport, RGB(45, 52, 65));

    HRGN clip = CreateRectRgnIndirect(&g_state.layout.viewport);
    SelectClipRgn(hdc, clip);
    draw_viewport_world(hdc);
    SelectClipRgn(hdc, nullptr);
    DeleteObject(clip);

    draw_text_left(hdc, g_state.layout.left_panel.left + 12, g_state.layout.left_panel.bottom - 42,
                   "Mode: 4=Simple, 5=Grenade, 6=Missile", RGB(110, 125, 145));
    draw_text_left(hdc, g_state.layout.left_panel.left + 12, g_state.layout.left_panel.bottom - 22,
                   "Wheel: zoom   Right-drag: pan   Space: launch", RGB(110, 125, 145));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        create_ui_fonts();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        g_state.width = LOWORD(lParam);
        g_state.height = HIWORD(lParam);
        update_layout();
        ensure_backbuffer(hwnd);
        return 0;
    case WM_MOUSEWHEEL: {
        POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &pt);
        if (!point_in_viewport(pt.x, pt.y)) {
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        double wx = 0.0;
        double wy = 0.0;
        screen_to_world(pt.x, pt.y, wx, wy);
        const short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        const double zoom_factor = 1.0 + static_cast<double>(delta) / 1200.0;
        const double new_ppw = (std::clamp)(g_state.pixels_per_world * zoom_factor, kZoomMin, kZoomMax);
        const double actual_factor = new_ppw / g_state.pixels_per_world;
        g_state.camera_center_x = wx - (wx - g_state.camera_center_x) / actual_factor;
        g_state.camera_center_y = wy - (wy - g_state.camera_center_y) / actual_factor;
        g_state.pixels_per_world = new_ppw;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_RBUTTONDOWN: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (point_in_viewport(x, y)) {
            g_state.drag = DragKind::Pan;
            g_state.drag_last_x = x;
            g_state.drag_last_y = y;
            SetCapture(hwnd);
        }
        return 0;
    }
    case WM_RBUTTONUP:
        if (g_state.drag == DragKind::Pan) {
            ReleaseCapture();
            g_state.drag = DragKind::None;
        }
        return 0;
    case WM_MOUSEMOVE: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (g_state.drag == DragKind::Pan) {
            const int dx = x - g_state.drag_last_x;
            const int dy = y - g_state.drag_last_y;
            g_state.camera_center_x -= static_cast<double>(dx) / g_state.pixels_per_world;
            g_state.camera_center_y -= static_cast<double>(dy) / g_state.pixels_per_world;
            g_state.drag_last_x = x;
            g_state.drag_last_y = y;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (g_state.drag == DragKind::SpeedSlider) {
            g_state.launch_speed = value_from_track_x(g_state.layout.speed_track, 5.0, 150.0, x);
            reset_projectile();
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (g_state.drag == DragKind::AngleSlider) {
            g_state.launch_angle_deg = value_from_track_x(g_state.layout.angle_track, 5.0, 85.0, x);
            reset_projectile();
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (g_state.drag == DragKind::GravitySlider) {
            g_state.gravity = value_from_track_x(g_state.layout.gravity_track, 0.1, 25.0, x);
            if (std::abs(g_state.gravity - 9.81) < 0.15) {
                g_state.preset_selected = 0;
            } else if (std::abs(g_state.gravity - 1.62) < 0.15) {
                g_state.preset_selected = 1;
            } else if (std::abs(g_state.gravity - 3.71) < 0.15) {
                g_state.preset_selected = 2;
            } else {
                g_state.preset_selected = -1;
            }
            reset_projectile();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (g_state.drag != DragKind::None) {
            ReleaseCapture();
            g_state.drag = DragKind::None;
        }
        return 0;
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        POINT p{x, y};

        if (PtInRect(&g_state.layout.speed_track, p)) {
            g_state.drag = DragKind::SpeedSlider;
            g_state.launch_speed = value_from_track_x(g_state.layout.speed_track, 5.0, 150.0, x);
            reset_projectile();
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.angle_track, p)) {
            g_state.drag = DragKind::AngleSlider;
            g_state.launch_angle_deg = value_from_track_x(g_state.layout.angle_track, 5.0, 85.0, x);
            reset_projectile();
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.gravity_track, p)) {
            g_state.drag = DragKind::GravitySlider;
            g_state.gravity = value_from_track_x(g_state.layout.gravity_track, 0.1, 25.0, x);
            reset_projectile();
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (PtInRect(&g_state.layout.cb_vectors, p)) {
            g_state.show_vectors = !g_state.show_vectors;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.cb_trace, p)) {
            g_state.trace_path = !g_state.trace_path;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.cb_air, p)) {
            g_state.air_resistance = !g_state.air_resistance;
            reset_projectile();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (PtInRect(&g_state.layout.preset_earth, p)) {
            g_state.gravity = 9.81;
            g_state.preset_selected = 0;
            reset_projectile();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.preset_moon, p)) {
            g_state.gravity = 1.62;
            g_state.preset_selected = 1;
            reset_projectile();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.preset_mars, p)) {
            g_state.gravity = 3.71;
            g_state.preset_selected = 2;
            reset_projectile();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (PtInRect(&g_state.layout.btn_launch, p)) {
            reset_projectile();
            g_state.launched = true;
            g_state.impact = false;
            g_state.paused = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.btn_reset, p)) {
            reset_projectile();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.btn_step, p)) {
            step_one_frame();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (PtInRect(&g_state.layout.zoom_in, p)) {
            g_state.pixels_per_world = (std::min)(g_state.pixels_per_world * 1.12, kZoomMax);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.zoom_out, p)) {
            g_state.pixels_per_world = (std::max)(g_state.pixels_per_world / 1.12, kZoomMin);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&g_state.layout.btn_fit, p)) {
            fit_camera_to_scene();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_SPACE:
            reset_projectile();
            g_state.launched = true;
            g_state.impact = false;
            g_state.paused = false;
            return 0;
        case 'P':
            g_state.paused = !g_state.paused;
            return 0;
        case 'R':
            reset_projectile();
            return 0;
        case VK_LEFT:
            g_state.launch_speed = (std::max)(5.0, g_state.launch_speed - 1.0);
            reset_projectile();
            return 0;
        case VK_RIGHT:
            g_state.launch_speed = (std::min)(150.0, g_state.launch_speed + 1.0);
            reset_projectile();
            return 0;
        case VK_UP:
            g_state.launch_angle_deg = (std::min)(85.0, g_state.launch_angle_deg + 1.0);
            reset_projectile();
            return 0;
        case VK_DOWN:
            g_state.launch_angle_deg = (std::max)(5.0, g_state.launch_angle_deg - 1.0);
            reset_projectile();
            return 0;
        case 'G':
            g_state.gravity = (std::max)(0.1, g_state.gravity - 0.2);
            reset_projectile();
            return 0;
        case 'H':
            g_state.gravity = (std::min)(25.0, g_state.gravity + 0.2);
            reset_projectile();
            return 0;
        case '1':
            g_state.gravity = 9.81;
            g_state.preset_selected = 0;
            reset_projectile();
            return 0;
        case '2':
            g_state.gravity = 1.62;
            g_state.preset_selected = 1;
            reset_projectile();
            return 0;
        case '3':
            g_state.gravity = 3.71;
            g_state.preset_selected = 2;
            reset_projectile();
            return 0;
        case '4':
            g_state.proj_type = omnisim::projectile::ProjectileType::Simple;
            reset_projectile();
            return 0;
        case '5':
            g_state.proj_type = omnisim::projectile::ProjectileType::Grenade;
            reset_projectile();
            return 0;
        case '6':
            g_state.proj_type = omnisim::projectile::ProjectileType::Missile;
            reset_projectile();
            return 0;
        default:
            return 0;
        }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (!g_state.back_dc) {
            ensure_backbuffer(hwnd);
        }
        if (g_state.back_dc) {
            draw_scene(g_state.back_dc);
            BitBlt(hdc, 0, 0, g_state.width, g_state.height, g_state.back_dc, 0, 0, SRCCOPY);
        } else {
            draw_scene(hdc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        release_backbuffer();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const char CLASS_NAME[] = "OmniSimProjectile2DWindow";

    WNDCLASSA wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0, CLASS_NAME, "OmniSim Kinematics Core",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        g_state.width, g_state.height, nullptr, nullptr, hInstance, nullptr);
    if (hwnd == nullptr) {
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    update_layout();
    fit_camera_to_scene();
    ensure_backbuffer(hwnd);
    reset_projectile();
    g_state.fps_sample_start = std::chrono::steady_clock::now();

    MSG msg{};
    auto previous = std::chrono::steady_clock::now();
    constexpr double dt = 1.0 / 60.0;
    double accumulator = 0.0;

    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        const auto now = std::chrono::steady_clock::now();
        const double frame_dt = std::chrono::duration<double>(now - previous).count();
        previous = now;
        accumulator += (std::min)(frame_dt, 0.1);

        while (accumulator >= dt) {
            update_simulation(dt);
            accumulator -= dt;
        }

        ++g_state.fps_frame_count;
        const double fps_elapsed = std::chrono::duration<double>(now - g_state.fps_sample_start).count();
        if (fps_elapsed >= 0.5) {
            g_state.fps_display = static_cast<double>(g_state.fps_frame_count) / fps_elapsed;
            g_state.fps_frame_count = 0;
            g_state.fps_sample_start = now;
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        Sleep(1);
    }
}
