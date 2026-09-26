/* PlayOS touch demo: proves GetTouchPosition() reaches a real game, through
 * raylib -> libplayos -> evdev (ADR-0013). */
#include <raylib.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    InitWindow(1920, 1080, "PlayOS Touch Demo");
    SetTargetFPS(60);
    printf("[touchdemo] window %dx%d\n", GetScreenWidth(), GetScreenHeight());
    fflush(stdout);

    int prev_n = -1;

    while (!WindowShouldClose())
    {
        int n = GetTouchPointCount();

        if (n != prev_n)
        {
            printf("[touchdemo] points=%d", n);
            for (int i = 0; i < n; i++)
            {
                Vector2 p = GetTouchPosition(i);
                printf(" [id=%d %.0f,%.0f]", GetTouchPointId(i), p.x, p.y);
            }
            printf("\n");
            fflush(stdout);
            prev_n = n;
        }

        BeginDrawing();
        ClearBackground((Color){16, 18, 28, 255});
        DrawText("TOUCH THE SCREEN", 60, 60, 48, RAYWHITE);
        DrawText("B exits", 60, 130, 28, (Color){140,150,170,255});
        for (int i = 0; i < n; i++)
        {
            Vector2 p = GetTouchPosition(i);
            DrawCircleV(p, 70, (Color){80, 200, 255, 200});
            DrawCircleLinesV(p, 70, RAYWHITE);
        }
        EndDrawing();      /* always: this is where the Wayland/evdev pump runs */
    }

    CloseWindow();
    return 0;
}
