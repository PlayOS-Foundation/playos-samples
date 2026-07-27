// input-debug — controller button & axis tester for PlayOS.
//
// Draws a visual map of a ROG Ally / Xbox-style controller and highlights
// buttons as they are pressed. Shows analog stick positions and keeps a
// scrolling event log.  Useful for verifying which physical buttons are
// reaching the platform.
//
// Controls:
//   Any button → highlighted on the visual map + logged to the event list
//   B (or Esc) → quit (return to shell)
//   Y → also quits (since Y is remapped as Home on some builds)

#include "raylib.h"
#include "playos/playos.h"

#include <deque>
#include <string>
#include <cstdio>

// ── helpers ────────────────────────────────────────────────────────────────

static bool Pressed(PlayOS::Button b) { return PlayOS::Input::Pressed(b); }
static bool Down(PlayOS::Button b)    { return PlayOS::Input::Down(b); }

static const char* ButtonName(PlayOS::Button b) {
    switch (b) {
        case PlayOS::Button::A:            return "A";
        case PlayOS::Button::B:            return "B";
        case PlayOS::Button::X:            return "X";
        case PlayOS::Button::Y:            return "Y";
        case PlayOS::Button::DPadUp:       return "D-Up";
        case PlayOS::Button::DPadDown:     return "D-Down";
        case PlayOS::Button::DPadLeft:     return "D-Left";
        case PlayOS::Button::DPadRight:    return "D-Right";
        case PlayOS::Button::L1:           return "LB";
        case PlayOS::Button::R1:           return "RB";
        case PlayOS::Button::L2:           return "LT";
        case PlayOS::Button::R2:           return "RT";
        case PlayOS::Button::Start:        return "Start";
        case PlayOS::Button::Select:       return "Select";
        case PlayOS::Button::Home:         return "Home";
        case PlayOS::Button::QuickSettings:return "QS";
        default: return "?";
    }
}

struct LogEntry {
    std::string text;
    float age = 0;  // seconds since logged
    Color color;
};

int main() {
    auto displayInfo = PlayOS::Display::Current();
#ifdef __linux__
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "Input Debug — PlayOS");
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();
#else
    const int W = displayInfo.width > 0 ? displayInfo.width : 1280;
    const int H = displayInfo.height > 0 ? displayInfo.height : 720;
    InitWindow(W, H, "Input Debug — PlayOS");
#endif
    SetTargetFPS(displayInfo.refreshRate > 0 ? displayInfo.refreshRate : 60);
    PlayOS::Lifecycle::Init();

    std::deque<LogEntry> log;
    const int kMaxLog = 40;

    // ── Layout constants ───────────────────────────────────────────────────
    // Controller visual lives in left 60% of screen; log in right 40%.
    const int kCtrlLeft   = 120;
    const int kCtrlTop    = 100;
    const int kCtrlW      = (int)(W * 0.52f);
    const int kCtrlH      = H - 200;
    const int kLogLeft    = kCtrlLeft + kCtrlW + 60;
    const int kLogW       = W - kLogLeft - 40;
    const int kLogH       = kCtrlH;

    // Button layout positions (relative to controller box, centred at ctrl mid)
    const float cx = kCtrlLeft + kCtrlW / 2.0f;
    const float cy = kCtrlTop + kCtrlH / 2.0f;

    // Face buttons (right cluster — A B X Y)
    const float faceCX = cx + 160;
    const float faceCY = cy - 10;
    struct { float x, y; PlayOS::Button btn; } faceBtns[] = {
        {faceCX + 52, faceCY + 52, PlayOS::Button::A},
        {faceCX + 52, faceCY - 52, PlayOS::Button::Y},
        {faceCX - 52, faceCY + 52, PlayOS::Button::X},
        {faceCX - 52, faceCY - 52, PlayOS::Button::B},
    };

    // D-Pad (left cluster)
    const float dpadCX = cx - 160;
    const float dpadCY = cy - 10;
    struct { float x, y; PlayOS::Button btn; } dpadBtns[] = {
        {dpadCX,       dpadCY - 40, PlayOS::Button::DPadUp},
        {dpadCX,       dpadCY + 40, PlayOS::Button::DPadDown},
        {dpadCX - 40,  dpadCY,      PlayOS::Button::DPadLeft},
        {dpadCX + 40,  dpadCY,      PlayOS::Button::DPadRight},
    };

    // Bumpers & triggers (top)
    const float bumperY = cy - 110;
    struct { float x, y; PlayOS::Button btn; } bumperBtns[] = {
        {cx - 100, bumperY,      PlayOS::Button::L1},
        {cx + 100, bumperY,      PlayOS::Button::R1},
        {cx - 100, bumperY - 30, PlayOS::Button::L2},
        {cx + 100, bumperY - 30, PlayOS::Button::R2},
    };

    // Centre buttons (Start, Select, Home, QuickSettings)
    const float centreY = cy + 60;
    struct { float x, y; PlayOS::Button btn; const char* label; } centreBtns[] = {
        {cx - 70, centreY, PlayOS::Button::Select,       "Sel"},
        {cx + 70, centreY, PlayOS::Button::Start,        "Sta"},
        {cx,      centreY - 30, PlayOS::Button::Home,    "Home"},
        {cx,      centreY + 30, PlayOS::Button::QuickSettings, "QS"},
    };

    auto drawBtn = [&](float bx, float by, float r, PlayOS::Button btn,
                       const char* label, const char* sub) {
        bool held = Down(btn);
        Color fill = held ? RED : Color{60, 60, 80, 255};
        Color border = held ? Color{255, 80, 80, 255} : Color{120, 120, 150, 255};
        DrawCircleV({bx, by}, r, fill);
        DrawCircleLines(bx, by, r, border);
        int tw = MeasureText(label, 16);
        DrawText(label, bx - tw / 2, by - 25, 16, held ? WHITE : GRAY);
        if (sub) {
            int sw = MeasureText(sub, 12);
            DrawText(sub, bx - sw / 2, by + 8, 12, Color{100, 100, 140, 255});
        }
    };

    // ── Main loop ──────────────────────────────────────────────────────────
    while (!WindowShouldClose() && !Pressed(PlayOS::Button::B)
           && !IsKeyPressed(KEY_ESCAPE)) {
        float dt = GetFrameTime();
        PlayOS::Lifecycle::Update();

        // Log any pressed buttons
        for (int i = 0; i < static_cast<int>(PlayOS::Button::Count); ++i) {
            auto btn = static_cast<PlayOS::Button>(i);
            if (Pressed(btn)) {
                char buf[128];
                snprintf(buf, sizeof(buf), "PRESS: %s", ButtonName(btn));
                log.push_back({buf, 0.0f, GREEN});
                if ((int)log.size() > kMaxLog) log.pop_front();
            }
        }
        // Age log entries
        for (auto& e : log) e.age += dt;
        // Remove old entries
        while (!log.empty() && log.front().age > 8.0f) log.pop_front();

        // ── Draw ──────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(Color{8, 10, 20, 255});

        // ── Left panel: controller visual ──────────────────────────────────
        DrawRectangleRounded(
            {(float)kCtrlLeft, (float)kCtrlTop, (float)kCtrlW, (float)kCtrlH},
            0.15f, 10, Color{16, 18, 32, 255});
        DrawRectangleRoundedLines(
            {(float)kCtrlLeft, (float)kCtrlTop, (float)kCtrlW, (float)kCtrlH},
            0.15f, 10, 1.5f, Color{50, 50, 80, 255});

        // Controller body silhouette (simplified Xbox shape)
        DrawEllipse(cx, cy, 220, 140, Color{22, 26, 42, 255});
        DrawEllipseLines(cx, cy, 220, 140, Color{80, 80, 120, 255});

        // D-Pad cross background
        DrawRectangle(dpadCX - 43, dpadCY - 3, 86, 6, Color{40, 40, 60, 255});
        DrawRectangle(dpadCX - 3, dpadCY - 43, 6, 86, Color{40, 40, 60, 255});

        // Face button cluster background
        for (const auto& fb : faceBtns) {
            drawBtn(fb.x, fb.y, 24, fb.btn, ButtonName(fb.btn), nullptr);
        }
        for (const auto& db : dpadBtns) {
            drawBtn(db.x, db.y, 14, db.btn, nullptr, nullptr);
        }
        for (const auto& bb : bumperBtns) {
            // Wider rectangles for bumpers/triggers
            bool held = Down(bb.btn);
            Color fill = held ? RED : Color{50, 50, 70, 255};
            Color border = held ? Color{255, 80, 80, 255} : Color{100, 100, 140, 255};
            float bw = (bb.btn == PlayOS::Button::L2 || bb.btn == PlayOS::Button::R2) ? 60 : 70;
            float bh = (bb.btn == PlayOS::Button::L2 || bb.btn == PlayOS::Button::R2) ? 18 : 22;
            DrawRectangleRounded({bb.x - bw / 2, bb.y - bh / 2, bw, bh}, 0.4f, 6, fill);
            DrawRectangleRoundedLines({bb.x - bw / 2, bb.y - bh / 2, bw, bh}, 0.4f, 6, 1.5f, border);
            int tw = MeasureText(ButtonName(bb.btn), 14);
            DrawText(ButtonName(bb.btn), bb.x - tw / 2, bb.y - 7, 14,
                     held ? WHITE : Color{120, 120, 160, 255});
        }
        for (const auto& cb : centreBtns) {
            drawBtn(cb.x, cb.y, 16, cb.btn, cb.label, nullptr);
        }

        // Analog stick indicators
        float lx = PlayOS::Input::GetAxis(PlayOS::Axis::LeftX);
        float ly = PlayOS::Input::GetAxis(PlayOS::Axis::LeftY);
        float rx = PlayOS::Input::GetAxis(PlayOS::Axis::RightX);
        float ry = PlayOS::Input::GetAxis(PlayOS::Axis::RightY);

        auto drawStick = [&](float scx, float scy, float sx, float sy, const char* name) {
            const float R = 55;
            DrawCircleLines(scx, scy, R, Color{80, 80, 110, 255});
            DrawCircle(scx + sx * R, scy + sy * R, 10,
                       Color{80, 160, 255, 200});
            int tw = MeasureText(name, 14);
            DrawText(name, scx - tw / 2, scy + R + 8, 14, Color{100, 100, 140, 255});
        };

        drawStick(cx - 160, cy - 115, lx, ly, "L Stick");
        drawStick(cx + 160, cy - 115, rx, ry, "R Stick");

        // Trigger bar indicators
        auto drawTriggerBar = [&](float tx, float ty, float val, const char* name) {
            const float barH = 60;
            const float barW = 14;
            DrawRectangleLines(tx - barW/2 - 1, ty - 1, barW + 2, barH + 2, Color{100, 100, 140, 255});
            float fillH = val * barH;
            DrawRectangle(tx - barW/2, ty + barH - fillH, barW, fillH,
                          val > 0.05f ? Color{80, 200, 120, 255} : Color{40, 40, 60, 255});
            int tw = MeasureText(name, 12);
            DrawText(name, tx - tw / 2, ty + barH + 6, 12, Color{100, 100, 140, 255});
        };

        float ltVal = PlayOS::Input::GetAxis(PlayOS::Axis::LeftTrigger);
        float rtVal = PlayOS::Input::GetAxis(PlayOS::Axis::RightTrigger);
        drawTriggerBar(cx - 100, cy - 40, ltVal, "LT");
        drawTriggerBar(cx + 100, cy - 40, rtVal, "RT");

        // Left shoulder button labels (L/R)
        DrawText("L", cx - 180, cy - 136, 14, Color{80, 80, 110, 255});
        DrawText("R", cx + 170, cy - 136, 14, Color{80, 80, 110, 255});

        // ── Right panel: event log ─────────────────────────────────────────
        DrawRectangleRounded(
            {(float)kLogLeft, (float)kCtrlTop, (float)kLogW, (float)kLogH},
            0.15f, 10, Color{16, 18, 32, 255});
        DrawRectangleRoundedLines(
            {(float)kLogLeft, (float)kCtrlTop, (float)kLogW, (float)kLogH},
            0.15f, 10, 1.5f, Color{50, 50, 80, 255});

        DrawText("EVENT LOG", kLogLeft + 16, kCtrlTop + 16, 26,
                 Color{120, 120, 160, 255});
        DrawLine(kLogLeft + 16, kCtrlTop + 52, kLogLeft + kLogW - 16, kCtrlTop + 52,
                 Color{50, 50, 80, 255});

        for (int i = 0; i < (int)log.size(); ++i) {
            const auto& e = log[i];
            float alpha = 1.0f;
            if (e.age > 6.0f) alpha = 1.0f - (e.age - 6.0f) / 2.0f;
            Color c = e.color;
            c.a = (unsigned char)(c.a * alpha);
            DrawText(e.text.c_str(), kLogLeft + 20,
                     kCtrlTop + 64 + i * 24, 18, c);
        }

        // Status bar
        const char* conn = PlayOS::Input::ControllerConnected()
            ? "Controller: CONNECTED" : "Controller: DISCONNECTED";
        DrawText(conn, 80, H - 80, 28,
                 PlayOS::Input::ControllerConnected()
                     ? Color{80, 200, 120, 255} : Color{200, 80, 80, 255});

        const char* help = "Y/Home: overlay   B: quit   D-Pad: nav   A: confirm";
        int hw = MeasureText(help, 24);
        DrawText(help, W - hw - 80, H - 80, 24, Color{100, 100, 130, 255});

        EndDrawing();
    }

    PlayOS::Lifecycle::Shutdown();
    CloseWindow();
    return 0;
}
