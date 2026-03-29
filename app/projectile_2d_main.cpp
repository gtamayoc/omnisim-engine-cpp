#include "projectile/projectile_simulation.h"
#include "projectile/terrain_profile.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

struct AppState {
    int width{1280};
    int height{720};

    double launch_speed{45.0};
    double launch_angle_deg{38.0};
    double gravity{9.81};

    bool launched{false};
    bool impact{false};

    omnisim::projectile::TerrainProfile terrain{};
    std::unique_ptr<omnisim::projectile::ProjectileSimulation> simulation{};
    std::vector<omnisim::math::Vector2> flight_path{};
    HDC back_dc{nullptr};
    HBITMAP back_bitmap{nullptr};
    HBITMAP back_old_bitmap{nullptr};
};

AppState g_state{};

constexpr double kWorldXMin = 0.0;
constexpr double kWorldXMax = 300.0;
constexpr double kWorldYMin = 0.0;
constexpr double kWorldYMax = 120.0;
constexpr double kLaunchX = 10.0;
constexpr double kLaunchOffsetY = 3.0;

double deg_to_rad(const double deg) {
    return deg * (3.14159265358979323846 / 180.0);
}

int world_to_screen_x(const double x, const int width) {
    const double t = (x - kWorldXMin) / (kWorldXMax - kWorldXMin);
    return static_cast<int>(std::lround(t * static_cast<double>(width - 1)));
}

int world_to_screen_y(const double y, const int height) {
    const double t = (y - kWorldYMin) / (kWorldYMax - kWorldYMin);
    return static_cast<int>(std::lround((1.0 - t) * static_cast<double>(height - 1)));
}

double terrain_world_y(const double x) {
    return (std::clamp)(g_state.terrain.height_at(x), kWorldYMin, kWorldYMax);
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

    g_state.simulation = std::make_unique<omnisim::projectile::ProjectileSimulation>(cfg);
    g_state.simulation->initialize();
    g_state.flight_path.clear();
    g_state.flight_path.push_back(cfg.initial_position);
    g_state.launched = false;
    g_state.impact = false;
}

void update_simulation(const double dt) {
    if (!g_state.launched || !g_state.simulation || g_state.impact) {
        return;
    }

    g_state.simulation->step(dt);
    const auto& s = g_state.simulation->state();
    const omnisim::math::Vector2 clamped_pos{
        s.position.x,
        (std::max)(s.position.y, kWorldYMin),
    };
    g_state.flight_path.push_back(clamped_pos);
    const double ground = terrain_world_y(s.position.x);
    if (s.position.y <= ground || s.position.x > kWorldXMax) {
        g_state.impact = true;
        g_state.launched = false;
    }
}

void draw_scene(HDC hdc) {
    RECT rect{0, 0, g_state.width, g_state.height};
    HBRUSH bg = CreateSolidBrush(RGB(9, 12, 20));
    FillRect(hdc, &rect, bg);
    DeleteObject(bg);

    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(28, 40, 56));
    HGDIOBJ old_pen = SelectObject(hdc, grid_pen);
    for (int x = 0; x <= 300; x += 25) {
        const int sx = world_to_screen_x(static_cast<double>(x), g_state.width);
        MoveToEx(hdc, sx, 0, nullptr);
        LineTo(hdc, sx, g_state.height);
    }
    for (int y = 0; y <= 120; y += 20) {
        const int sy = world_to_screen_y(static_cast<double>(y), g_state.height);
        MoveToEx(hdc, 0, sy, nullptr);
        LineTo(hdc, g_state.width, sy);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(grid_pen);

    HPEN mountain_pen = CreatePen(PS_SOLID, 2, RGB(79, 125, 77));
    HBRUSH mountain_fill = CreateSolidBrush(RGB(38, 66, 43));
    old_pen = SelectObject(hdc, mountain_pen);
    HGDIOBJ old_brush = SelectObject(hdc, mountain_fill);
    POINT terrain_pts[304]{};
    terrain_pts[0] = {0, g_state.height};
    for (int i = 0; i <= 301; ++i) {
        const double x = kWorldXMin + (kWorldXMax - kWorldXMin) * (static_cast<double>(i) / 301.0);
        const double y = terrain_world_y(x);
        terrain_pts[i + 1] = {
            world_to_screen_x(x, g_state.width),
            world_to_screen_y(y, g_state.height),
        };
    }
    terrain_pts[303] = {g_state.width, g_state.height};
    Polygon(hdc, terrain_pts, 304);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(mountain_fill);
    DeleteObject(mountain_pen);

    const double cannon_x = kLaunchX;
    const double cannon_y = terrain_world_y(cannon_x) + kLaunchOffsetY;
    const double cannon_angle = deg_to_rad(g_state.launch_angle_deg);
    const double cannon_len = 7.0 + (g_state.launch_speed / 8.0);
    const int cx = world_to_screen_x(cannon_x, g_state.width);
    const int cy = world_to_screen_y(cannon_y, g_state.height);
    const int ctip_x = world_to_screen_x(cannon_x + cannon_len * std::cos(cannon_angle), g_state.width);
    const int ctip_y = world_to_screen_y(cannon_y + cannon_len * std::sin(cannon_angle), g_state.height);

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
        const int px = world_to_screen_x(s.position.x, g_state.width);
        const int py = world_to_screen_y(s.position.y, g_state.height);

        // Pre-launch prediction guide (static) until expected impact.
        if (!g_state.launched && !g_state.impact) {
            HPEN pred_pen = CreatePen(PS_DOT, 1, RGB(130, 170, 255));
            old_pen = SelectObject(hdc, pred_pen);

            const double vx = g_state.launch_speed * std::cos(deg_to_rad(g_state.launch_angle_deg));
            const double vy = g_state.launch_speed * std::sin(deg_to_rad(g_state.launch_angle_deg));
            const double y0 = cannon_y;
            bool has_prev = false;
            int prev_sx = 0;
            int prev_sy = 0;
            for (double t = 0.0; t <= 30.0; t += 0.06) {
                const double x = cannon_x + vx * t;
                const double y = y0 + vy * t - 0.5 * g_state.gravity * t * t;
                const double ground = terrain_world_y(x);
                if (x > kWorldXMax) {
                    break;
                }
                const int sx = world_to_screen_x(x, g_state.width);
                const int sy = world_to_screen_y(y, g_state.height);
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

        // Real path during/after flight.
        if (g_state.flight_path.size() > 1) {
            HPEN real_pen = CreatePen(PS_SOLID, 2, RGB(220, 230, 255));
            old_pen = SelectObject(hdc, real_pen);
            for (std::size_t i = 1; i < g_state.flight_path.size(); ++i) {
                const auto& p0 = g_state.flight_path[i - 1];
                const auto& p1 = g_state.flight_path[i];
                MoveToEx(hdc, world_to_screen_x(p0.x, g_state.width),
                         world_to_screen_y(p0.y, g_state.height), nullptr);
                LineTo(hdc, world_to_screen_x(p1.x, g_state.width),
                       world_to_screen_y(p1.y, g_state.height));
            }
            SelectObject(hdc, old_pen);
            DeleteObject(real_pen);
        }

        HBRUSH projectile = CreateSolidBrush(RGB(185, 210, 255));
        old_obj = SelectObject(hdc, projectile);
        Ellipse(hdc, px - 8, py - 8, px + 8, py + 8);
        SelectObject(hdc, old_obj);
        DeleteObject(projectile);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(231, 236, 255));

    std::string help = "SPACE launch | R reset | Left/Right speed | Up/Down angle | 1 Earth | 2 Moon | 3 Mars | G/H gravity";
    TextOutA(hdc, 12, 10, help.c_str(), static_cast<int>(help.size()));

    char info[256];
    std::snprintf(info, sizeof(info), "speed=%.1f m/s  angle=%.1f deg  gravity=%.2f m/s^2",
                  g_state.launch_speed, g_state.launch_angle_deg, g_state.gravity);
    TextOutA(hdc, 12, 32, info, static_cast<int>(std::strlen(info)));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        g_state.width = LOWORD(lParam);
        g_state.height = HIWORD(lParam);
        ensure_backbuffer(hwnd);
        return 0;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_SPACE:
            reset_projectile();
            g_state.launched = true;
            g_state.impact = false;
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
            g_state.gravity = (std::max)(1.0, g_state.gravity - 0.2);
            reset_projectile();
            return 0;
        case 'H':
            g_state.gravity = (std::min)(30.0, g_state.gravity + 0.2);
            reset_projectile();
            return 0;
        case '1':
            g_state.gravity = 9.81;
            reset_projectile();
            return 0;
        case '2':
            g_state.gravity = 1.62;
            reset_projectile();
            return 0;
        case '3':
            g_state.gravity = 3.71;
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
        0, CLASS_NAME, "OmniSim Projectile 2D",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        g_state.width, g_state.height, nullptr, nullptr, hInstance, nullptr);
    if (hwnd == nullptr) {
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    ensure_backbuffer(hwnd);
    reset_projectile();

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

        InvalidateRect(hwnd, nullptr, FALSE);
        Sleep(1);
    }
}
