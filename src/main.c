/*
 * Image Compressor - Raylib GUI
 * Cross-platform: Windows, Linux, macOS
 * 
 * This file ONLY uses raylib, never vips directly.
 * The processor.c file handles all vips operations.
 * 
 * Build (Windows MSYS2):
 *   gcc main.c processor.c -o compressor.exe $(pkg-config --cflags --libs vips) -lraylib -lgdi32 -lwinmm -lopengl32
 * 
 * Build (Linux):
 *   gcc main.c processor.c -o compressor $(pkg-config --cflags --libs vips) -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
 */

#ifndef MAIN_C_HEADERS
#define MAIN_C_HEADERS
#include "raylib.h"
#include "processor.h"
#include "font_data.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#endif

#define MAX_JOBS 32

// Global job list - array of pointers for memory stability
FolderJob* jobs[MAX_JOBS];
volatile int jobCount = 0;
pthread_mutex_t jobMutex = PTHREAD_MUTEX_INITIALIZER;

// Global font
Font guiFont;

// Worker thread function
void* JobWorker(void* arg) {
    printf("Worker: Thread started\n");
    while (true) {
        FolderJob* currentJob = NULL;

        pthread_mutex_lock(&jobMutex);
        for (int i = 0; i < jobCount; i++) {
            if (jobs[i] && jobs[i]->status == JOB_PENDING) {
                currentJob = jobs[i];
                // Mark as processing immediately to avoid double-processing
                currentJob->status = JOB_PROCESSING;
                printf("Worker: Starting job for %s\n", currentJob->sourcePath);
                break;
            }
        }
        pthread_mutex_unlock(&jobMutex);

        if (currentJob) {
            process_folder(currentJob);
            // NOTE: Do NOT call processor_thread_cleanup() here!
            // It calls vips_thread_shutdown() which destroys libvips structures
            // needed for subsequent jobs, causing GLib errors
        } else {
            // No jobs, sleep a bit to avoid CPU spin
            processor_sleep(500); 
        }
    }
    return NULL;
}

// Use check_is_directory from processor.c (handles unicode paths on Windows)
#define IsPathDirectory check_is_directory

// Add a folder to the job queue
void AddFolder(const char *path, CompressionConfig *config) {
    pthread_mutex_lock(&jobMutex);
    if (jobCount >= MAX_JOBS) {
        pthread_mutex_unlock(&jobMutex);
        return;
    }
    
    FolderJob *job = (FolderJob*)malloc(sizeof(FolderJob));
    if (!job) {
        pthread_mutex_unlock(&jobMutex);
        return;
    }
    memset(job, 0, sizeof(FolderJob));
    
    jobs[jobCount] = job;
    
    // Use the path directly - if user drops a folder, use that folder
    // If user drops a file, use the file's directory
    if (IsPathDirectory(path)) {
        strncpy(job->sourcePath, path, sizeof(job->sourcePath) - 1);
    } else {
        const char *dir = GetDirectoryPath(path);
        if (dir && strlen(dir) > 0) {
            strncpy(job->sourcePath, dir, sizeof(job->sourcePath) - 1);
        } else {
            pthread_mutex_unlock(&jobMutex);
            return;
        }
    }
    
    // Set output path
    get_output_folder_path(job->sourcePath, job->outputPath, sizeof(job->outputPath));
    
    job->status = JOB_PENDING;
    job->progress = 0;
    job->totalFiles = 0;
    job->doneFiles = 0;
    job->currentFile[0] = '\0';
    job->config = *config;
    
    jobCount++;
    printf("AddFolder: Added %s (jobCount: %d)\n", job->sourcePath, jobCount);
    pthread_mutex_unlock(&jobMutex);
}

// Draw a styled slider and return new value
int DrawSlider(Rectangle bounds, int value, int minVal, int maxVal, Color barColor) {
    // Background
    DrawRectangleRec(bounds, (Color){ 50, 50, 55, 255 });
    
    // Fill
    float ratio = (float)(value - minVal) / (float)(maxVal - minVal);
    DrawRectangle((int)bounds.x, (int)bounds.y, (int)(bounds.width * ratio), (int)bounds.height, barColor);
    
    // Border
    DrawRectangleLinesEx(bounds, 1, (Color){ 70, 70, 75, 255 });
    
    // Handle mouse input
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds)) {
        float mouseRatio = (GetMousePosition().x - bounds.x) / bounds.width;
        if (mouseRatio < 0) mouseRatio = 0;
        if (mouseRatio > 1) mouseRatio = 1;
        // Use rounding instead of truncation for accurate slider values
        value = minVal + (int)(mouseRatio * (maxVal - minVal) + 0.5f);
    }
    
    return value;
}

// Simple button helper
bool GuiButton(Rectangle bounds, const char *text, int fontSize, Color baseColor) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    
    DrawRectangleRec(bounds, hovered ? ColorAlpha(baseColor, 0.8f) : baseColor);
    DrawRectangleLinesEx(bounds, 1, (Color){ 100, 100, 110, 255 });
    
    Vector2 textSize = MeasureTextEx(guiFont, text, (float)fontSize, 0);
    DrawTextEx(guiFont, text, (Vector2){ (float)bounds.x + (bounds.width - textSize.x)/2, (float)bounds.y + (bounds.height - textSize.y)/2 }, (float)fontSize, 0, WHITE);
    
    return clicked;
}

// Draw Help Dialog
void DrawHelpDialog(int screenWidth, int screenHeight, bool *showHelp) {
    Rectangle modal = { 50, 50, (float)screenWidth - 100, (float)screenHeight - 100 };
    DrawRectangleRec(modal, (Color){ 30, 30, 35, 250 });
    DrawRectangleLinesEx(modal, 2, (Color){ 80, 80, 90, 255 });
    
    DrawTextEx(guiFont, "Guía de Usuario / Help", (Vector2){ (float)modal.x + 20, (float)modal.y + 20 }, 22, 0, WHITE);
    DrawRectangle((int)modal.x + 20, (int)modal.y + 45, (int)modal.width - 40, 1, GRAY);
    
    int y = (int)modal.y + 60;
    DrawTextEx(guiFont, "Ajustes de Compresión:", (Vector2){ 70, (float)y }, 16, 0, YELLOW); y += 22;
    DrawTextEx(guiFont, "- Calidad: Fidelidad visual (55-65 recomendado).", (Vector2){ 70, (float)y }, 15, 0, LIGHTGRAY); y += 20;
    DrawTextEx(guiFont, "- Compresión (CPU): 0 (rápido) a 10 (mejor/lento).", (Vector2){ 70, (float)y }, 15, 0, LIGHTGRAY); y += 20;
    DrawTextEx(guiFont, "- Hilos: Imágenes procesadas a la vez (# de CPUs).", (Vector2){ 70, (float)y }, 15, 0, LIGHTGRAY); y += 35;
    
    DrawTextEx(guiFont, "Gestión de Procesos:", (Vector2){ 70, (float)y }, 16, 0, YELLOW); y += 22;
    DrawTextEx(guiFont, "- Pausar/Reanudar: Detiene/continúa el trabajo.", (Vector2){ 70, (float)y }, 15, 0, LIGHTGRAY); y += 20;
    DrawTextEx(guiFont, "- Parar: Cancela el trabajo definitivamente.", (Vector2){ 70, (float)y }, 15, 0, LIGHTGRAY); y += 20;
    DrawTextEx(guiFont, "- Eliminar: Quita el registro (disponible al terminar).", (Vector2){ 70, (float)y }, 15, 0, LIGHTGRAY); y += 38;
    
    DrawTextEx(guiFont, "Salida:", (Vector2){ 70, (float)y }, 16, 0, YELLOW); y += 22;
    DrawTextEx(guiFont, "- Crea una carpeta con sufijo '(compressed)'.", (Vector2){ 70, (float)y }, 15, 0, LIGHTGRAY);
    
    if (GuiButton((Rectangle){ modal.x + modal.width - 100, modal.y + modal.height - 40, 80, 25 }, "Cerrar", 13, (Color){ 80, 40, 40, 255 })) {
        *showHelp = false;
    }
}

int main(int argc, char **argv) {
#ifdef _WIN32
    // Disable memory-mapped files in libvips on Windows to prevent folder locking
    _putenv("VIPS_MMAP=0");
#endif
    
    // Initialize libvips first
    if (!processor_init()) {
        printf("ERROR: Failed to initialize libvips!\n");
        printf("Make sure libvips is installed.\n");
        return 1;
    }
    
    // Initialize window
    const int screenWidth = 700;
    const int screenHeight = 550;
    
    InitWindow(screenWidth, screenHeight, "Manga Optimizer - AVIF Compressor");
    SetTargetFPS(60);

    // Load custom font from memory (embedded)
    guiFont = LoadFontFromMemory(".ttf", font_data, font_data_size, 64, 0, 250);
    if (guiFont.texture.id == 0) {
        printf("WARNING: Failed to load embedded font, using default\n");
        guiFont = GetFontDefault();
    } else {
        SetTextureFilter(guiFont.texture, TEXTURE_FILTER_BILINEAR);
    }
    
    // Get CPU count for thread limit
    int maxThreads = get_cpu_count();
    if (maxThreads < 1) maxThreads = 4;
    if (maxThreads > 32) maxThreads = 32;
    
    // Start background worker thread
    pthread_t workerThread;
    if (pthread_create(&workerThread, NULL, JobWorker, NULL) != 0) {
        TraceLog(LOG_ERROR, "Failed to create worker thread!");
    } else {
        pthread_detach(workerThread);
    }

    // Settings
    CompressionConfig config = {
        .quality = 55,
        .speed = 6,
        .threads = maxThreads / 2 > 0 ? maxThreads / 2 : 1  // Default to half of CPU count
    };
    
    bool isDragging = false;
    bool showHelp = false;
    float jobScrollY = 0.0f;
    int totalJobsHeight = 0;
    
    while (!WindowShouldClose()) {
        // Check for dropped files/folders
        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            
            for (int i = 0; i < (int)droppedFiles.count; i++) {
                AddFolder(droppedFiles.paths[i], &config);
            }
            
            UnloadDroppedFiles(droppedFiles);
        }
        
        // Handle scrolling if mouse is over the jobs panel
        Rectangle jobsPanelRec = { 15, 305, (float)screenWidth - 30, 210 };
        if (CheckCollisionPointRec(GetMousePosition(), jobsPanelRec)) {
            jobScrollY += GetMouseWheelMove() * 30.0f;
            
            // Clamp scroll
            int maxScroll = totalJobsHeight - 170; // 170 is approx visible height
            if (maxScroll < 0) maxScroll = 0;
            if (jobScrollY < -maxScroll) jobScrollY = -maxScroll;
            if (jobScrollY > 0) jobScrollY = 0;
        }
        
        // Drawing
        BeginDrawing();
        ClearBackground((Color){ 25, 25, 30, 255 });
        
        // Title bar
        DrawRectangle(0, 0, screenWidth, 70, (Color){ 35, 35, 42, 255 });
        DrawTextEx(guiFont, "Manga Optimizer", (Vector2){ 20, 15 }, 28, 0, WHITE);
        DrawTextEx(guiFont, "AVIF Smart Compression - Low Memory Mode", (Vector2){ 20, 48 }, 16, 0, (Color){ 150, 150, 160, 255 });
        
        // Help button in header
        if (GuiButton((Rectangle){ (float)screenWidth - 50, 15, 30, 30 }, "?", 18, (Color){ 60, 60, 70, 255 })) {
            showHelp = true;
        }
        
        // Memory indicator (Dynamic)
        long long ramUsed = get_process_ram_usage();
        const char *ramText = TextFormat("RAM: %lld MB", ramUsed / (1024 * 1024));
        if (ramUsed > 1024LL * 1024LL * 1024LL) {
            ramText = TextFormat("RAM: %.2f GB", (double)ramUsed / (1024.0 * 1024.0 * 1024.0));
        }
        DrawTextEx(guiFont, ramText, (Vector2){ (float)screenWidth - 190, 48 }, 14, 0, (ramUsed > 800LL*1024*1024) ? ORANGE : (Color){ 100, 220, 100, 255 });
        
        // Drop zone
        Rectangle dropZone = { 20, 85, screenWidth - 40, 70 };
        isDragging = CheckCollisionPointRec(GetMousePosition(), dropZone);
        
        Color dropBg = isDragging ? (Color){ 50, 90, 50, 255 } : (Color){ 40, 40, 48, 255 };
        Color dropBorder = isDragging ? (Color){ 100, 200, 100, 255 } : (Color){ 70, 70, 80, 255 };
        
        DrawRectangleRounded(dropZone, 0.1f, 8, dropBg);
#if defined(RAYLIB_VERSION_MAJOR) && (RAYLIB_VERSION_MAJOR > 5 || (RAYLIB_VERSION_MAJOR == 5 && RAYLIB_VERSION_MINOR >= 1))
        DrawRectangleRoundedLinesEx(dropZone, 0.1f, 8, 2.0f, dropBorder);
#else
        DrawRectangleRoundedLines(dropZone, 0.1f, 8, 2.0f, dropBorder);
#endif
        
        // Click to open folder picker
        if (isDragging && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            char *pickedPath = pick_folder_dialog();
            if (pickedPath) {
                AddFolder(pickedPath, &config);
                free(pickedPath);
            }
        }

        const char *dropText = "Click o arrastra carpetas aqui / Click or Drop folders here";
        Vector2 dropTextSize = MeasureTextEx(guiFont, dropText, 20, 0);
        DrawTextEx(guiFont, dropText, (Vector2){ (float)dropZone.x + (dropZone.width - dropTextSize.x) / 2, (float)dropZone.y + 25 }, 20, 0,
                 isDragging ? WHITE : (Color){ 180, 180, 190, 255 });
        
        // Settings panel
        DrawRectangle(15, 170, screenWidth - 30, 120, (Color){ 35, 35, 42, 255 });
        DrawRectangleLines(15, 170, screenWidth - 30, 120, (Color){ 50, 50, 58, 255 });
        DrawTextEx(guiFont, "Ajustes / Settings", (Vector2){ 25, 178 }, 16, 0, WHITE);
        
        // Quality slider
        DrawTextEx(guiFont, TextFormat("Calidad: %d", config.quality), (Vector2){ 30, 205 }, 16, 0, (Color){ 200, 200, 210, 255 });
        config.quality = DrawSlider((Rectangle){ 200, 203, 180, 16 }, config.quality, 0, 100, (Color){ 80, 160, 80, 255 });
        DrawTextEx(guiFont, "(0=min, 100=max)", (Vector2){ 400, 205 }, 14, 0, GRAY);
        
        // Speed/Effort slider
        DrawTextEx(guiFont, TextFormat("Compresión (CPU): %d", config.speed), (Vector2){ 30, 230 }, 16, 0, (Color){ 200, 200, 210, 255 });
        config.speed = DrawSlider((Rectangle){ 200, 228, 180, 16 }, config.speed, 0, 10, (Color){ 80, 140, 200, 255 });
        DrawTextEx(guiFont, "(0=rapido, 10=mejor)", (Vector2){ 400, 230 }, 14, 0, GRAY);
        
        // Threads slider
        DrawTextEx(guiFont, TextFormat("Hilos: %d", config.threads), (Vector2){ 30, 255 }, 16, 0, (Color){ 200, 200, 210, 255 });
        config.threads = DrawSlider((Rectangle){ 200, 253, 180, 16 }, config.threads, 1, maxThreads, (Color){ 200, 140, 80, 255 });
        DrawTextEx(guiFont, TextFormat("(max: %d CPUs)", maxThreads), (Vector2){ 400, 255 }, 14, 0, GRAY);
        
        // Jobs panel
        DrawRectangle(15, 305, screenWidth - 30, 210, (Color){ 35, 35, 42, 255 });
        DrawRectangleLines(15, 305, screenWidth - 30, 210, (Color){ 50, 50, 58, 255 });
        DrawTextEx(guiFont, TextFormat("Trabajos / Jobs (%d)", jobCount), (Vector2){ 25, 313 }, 16, 0, WHITE);
        
        // Button to clear all finished jobs (Done, Error, Stopped)
        if (jobCount > 0) {
            if (GuiButton((Rectangle){ (float)screenWidth - 135, 310, 110, 22 }, "Limpiar Listos", 12, (Color){ 60, 60, 70, 255 })) {
                pthread_mutex_lock(&jobMutex);
                for (int i = 0; i < jobCount; ) {
                    if (jobs[i] && (jobs[i]->status == JOB_COMPLETED || jobs[i]->status == JOB_ERROR || jobs[i]->status == JOB_STOPPED)) {
                        FolderJob* jobToFree = jobs[i];
                        for (int j = i; j < jobCount - 1; j++) {
                            jobs[j] = jobs[j+1];
                        }
                        jobCount--;
                        free(jobToFree);
                    } else {
                        i++;
                    }
                }
                pthread_mutex_unlock(&jobMutex);
            }
        }
        
        if (jobCount == 0) {
            DrawTextEx(guiFont, "No hay trabajos. Arrastra una carpeta para comenzar.", (Vector2){ 40, 360 }, 15, 0, GRAY);
            totalJobsHeight = 0;
        } else {
            // Recorte para el área de la lista (clipping)
            BeginScissorMode(16, 335, screenWidth - 32, 175);
            
            int yOffset = 345 + (int)jobScrollY;
            int startY = yOffset;
            
            pthread_mutex_lock(&jobMutex);
            for (int i = 0; i < jobCount; i++) {
                FolderJob *job = jobs[i];
                if (!job) continue;
                
                // Skip rendering if far outside view for performance (optional)
                if (yOffset > 550) { 
                    yOffset += 60; 
                    continue; 
                }
                
                // Get folder name (simple extraction)
                const char *folderName = strrchr(job->sourcePath, '\\');
                if (!folderName) folderName = strrchr(job->sourcePath, '/');
                if (folderName) folderName++; else folderName = job->sourcePath;
                
                // Status mapping
                const char *statusText = "Pending";
                Color statusColor = YELLOW;
                
                if (job->status == JOB_PROCESSING) {
                    statusText = "Processing";
                    statusColor = (Color){ 100, 180, 255, 255 };
                } else if (job->status == JOB_COMPLETED) {
                    statusText = "Done";
                    statusColor = (Color){ 100, 220, 100, 255 };
                } else if (job->status == JOB_ERROR) {
                    statusText = "Error";
                    statusColor = (Color){ 255, 100, 100, 255 };
                } else if (job->status == JOB_STOPPED) {
                    statusText = "Stopped";
                    statusColor = (Color){ 200, 150, 100, 255 };
                } else if (job->status == JOB_STOPPING) {
                    statusText = "Stopping...";
                    statusColor = (Color){ 200, 150, 100, 255 };
                }
                
                // Folder name - Truncate and clean non-ASCII for display
                char displayPath[128];
                int k = 0;
                for (int j = 0; folderName[j] && k < 80; j++) {
                    unsigned char c = (unsigned char)folderName[j];
                    if (c < 128) displayPath[k++] = folderName[j];
                    else if (k < 77) {
                         displayPath[k++] = '.';
                         while ((folderName[j+1] & 0xC0) == 0x80) j++;
                    }
                }
                displayPath[k] = '\0';
                if (strlen(folderName) > (size_t)k && k >= 80) {
                    displayPath[77] = '.'; displayPath[78] = '.'; displayPath[79] = '.';
                }
                
                // Row 1: Folder name
                DrawTextEx(guiFont, displayPath, (Vector2){ 35, (float)yOffset }, 16, 0, WHITE);
                
                // Row 2: Progress bar + details
                int detailsY = yOffset + 22;
                Rectangle progressBar = { 35.0f, (float)(detailsY + 2), 400.0f, 10.0f };
                
                DrawRectangleRec(progressBar, (Color){ 45, 45, 50, 255 });
                if (job->totalFiles > 0) {
                    DrawRectangle((int)progressBar.x, (int)progressBar.y, 
                                 (int)(progressBar.width * job->progress / 100.0f), 
                                 (int)progressBar.height, statusColor);
                }
                
                // Progress text
                if (job->status == JOB_PROCESSING || job->status == JOB_STOPPING) {
                    DrawTextEx(guiFont, TextFormat("%d/%d (Threads: %d)", job->doneFiles, job->totalFiles, job->activeThreads), (Vector2){ 440, (float)detailsY }, 14, 0, LIGHTGRAY);
                } else {
                    DrawTextEx(guiFont, TextFormat("%d/%d", job->doneFiles, job->totalFiles), (Vector2){ 460, (float)detailsY }, 14, 0, LIGHTGRAY);
                }
                
                // Status label
                DrawTextEx(guiFont, statusText, (Vector2){ (float)screenWidth - 105, (float)detailsY }, 14, 0, statusColor);
                
                // Controls
                int btnX = 490;
                if (job->status == JOB_PROCESSING || job->status == JOB_PAUSED || job->status == JOB_PENDING || job->status == JOB_STOPPING) {
                    if (job->status != JOB_STOPPING && job->status != JOB_PENDING) {
                        const char *pText = (job->status == JOB_PAUSED) ? "Resume" : "Pause";
                        if (GuiButton((Rectangle){ (float)btnX, (float)detailsY - 2, 55, 20 }, pText, 11, (Color){ 60, 60, 80, 255 })) {
                            if (job->status == JOB_PAUSED) job->status = JOB_PROCESSING;
                            else job->status = JOB_PAUSED;
                        }
                    }
                    
                    if (job->status != JOB_STOPPING) {
                        if (GuiButton((Rectangle){ (float)btnX + 60, (float)detailsY - 2, 45, 20 }, "Stop", 11, (Color){ 80, 40, 40, 255 })) {
                            job->status = JOB_STOPPING;
                        }
                    }
                } else {
                    // Delete button - only if not processing
                    if (GuiButton((Rectangle){ (float)btnX + 60, (float)detailsY - 2, 45, 20 }, "Del", 11, (Color){ 100, 40, 40, 255 })) {
                        FolderJob* jobToFree = jobs[i];
                        for (int j = i; j < jobCount - 1; j++) {
                            jobs[j] = jobs[j+1];
                        }
                        jobCount--;
                        free(jobToFree);
                        // Break to avoid double-triggering buttons in the same frame
                        // Mutex will be unlocked by the call at the end of the loop scope
                        break;
                    }
                }
                
                // Current file (if processing or paused)
                if ((job->status == JOB_PROCESSING || job->status == JOB_PAUSED || job->status == JOB_STOPPING) && job->currentFile[0] != '\0') {
                    DrawTextEx(guiFont, TextFormat("  > %s", job->currentFile), (Vector2){ 35, (float)detailsY + 18 }, 12, 0, GRAY);
                    yOffset += 65;
                } else {
                    yOffset += 55;
                }
            }
            pthread_mutex_unlock(&jobMutex);
            
            EndScissorMode();
            totalJobsHeight = yOffset - startY - (int)jobScrollY;
            
            // Draw visual scrollbar if needed
            if (totalJobsHeight > 170) {
                float scrollRatio = 170.0f / (float)totalJobsHeight;
                float scrollThumbHeight = 170.0f * scrollRatio;
                float scrollThumbY = 335.0f + (-jobScrollY / (float)totalJobsHeight) * 170.0f;
                DrawRectangle(screenWidth - 12, (int)scrollThumbY, 4, (int)scrollThumbHeight, (Color){ 100, 100, 120, 255 });
            }
        }
        
        // Footer
        DrawTextEx(guiFont, "v1.8 - raylib + libvips | UI Font: Cascadia Mono (Embedded)", (Vector2){ 20, (float)screenHeight - 22 }, 13, 0, DARKGRAY);
        
        // Help Dialog (top layer)
        if (showHelp) DrawHelpDialog(screenWidth, screenHeight, &showHelp);
        
        EndDrawing();
    }
    
    UnloadFont(guiFont);
    CloseWindow();
    processor_shutdown();
    
    // Final cleanup of remaining jobs
    for (int i = 0; i < jobCount; i++) {
        if (jobs[i]) free(jobs[i]);
    }
    
    return 0;
}
