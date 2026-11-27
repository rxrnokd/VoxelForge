#include "core/CommandLine.hpp"

#include "raylib.h"

#include <cstdio>

int main(int argc, char** argv) {
    using namespace voxelforge;

    const CommandLineResult parsed = parseCommandLine(argc, argv);
    if (!parsed.ok()) {
        std::fprintf(stderr, "voxelforge: %s\n\n%s", parsed.error.c_str(), commandLineUsage());
        return 2;
    }
    const LaunchOptions& options = parsed.options;
    if (options.showHelp) {
        std::printf("%s", commandLineUsage());
        return 0;
    }

    const bool screenshotMode = !options.screenshotPath.empty();
    SetConfigFlags(screenshotMode ? 0u : (FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE));
    InitWindow(options.windowWidth, options.windowHeight, "VoxelForge");
    SetExitKey(KEY_NULL);

    int frame = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color{135, 180, 235, 255});
        DrawText("VoxelForge", 20, 20, 30, RAYWHITE);
        EndDrawing();

        if (screenshotMode && ++frame >= options.screenshotFrames) {
            Image image = LoadImageFromScreen();
            const bool saved = ExportImage(image, options.screenshotPath.c_str());
            UnloadImage(image);
            CloseWindow();
            return saved ? 0 : 1;
        }
    }
    CloseWindow();
    return 0;
}
