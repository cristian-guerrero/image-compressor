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
    
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, (int)(bounds.x + (bounds.width - textWidth)/2), (int)(bounds.y + (bounds.height - fontSize)/2), fontSize, WHITE);
    
    return clicked;
}

// Draw Help Dialog
void DrawHelpDialog(int screenWidth, int screenHeight, bool *showHelp) {
    Rectangle modal = { 50, 50, (float)screenWidth - 100, (float)screenHeight - 100 };
    DrawRectangleRec(modal, (Color){ 30, 30, 35, 250 });
    DrawRectangleLinesEx(modal, 2, (Color){ 80, 80, 90, 255 });
    
    DrawText("Guía de Usuario / Help", (int)modal.x + 20, (int)modal.y + 20, 20, WHITE);
    DrawRectangle((int)modal.x + 20, (int)modal.y + 45, (int)modal.width - 40, 1, GRAY);
    
    int y = (int)modal.y + 60;
    DrawText("Ajustes de Compresión:", 70, y, 15, YELLOW); y += 20;
    DrawText("- Calidad: Fidelidad visual (55-65 recomendado).", 70, y, 13, LIGHTGRAY); y += 18;
    DrawText("- Velocidad: 0 (mejor compresión) a 10 (más rápido).", 70, y, 13, LIGHTGRAY); y += 18;
    DrawText("- Hilos: Imágenes procesadas a la vez (# de CPUs).", 70, y, 13, LIGHTGRAY); y += 30;
    
    DrawText("Gestión de Procesos:", 70, y, 15, YELLOW); y += 20;
    DrawText("- Pausar/Reanudar: Detiene/continúa el trabajo.", 70, y, 13, LIGHTGRAY); y += 18;
    DrawText("- Parar: Cancela el trabajo definitivamente.", 70, y, 13, LIGHTGRAY); y += 18;
    DrawText("- Eliminar: Quita el registro (disponible al terminar).", 70, y, 13, LIGHTGRAY); y += 35;
    
    DrawText("Salida:", 70, y, 15, YELLOW); y += 20;
    DrawText("- Crea una carpeta con sufijo '(compressed)'.", 70, y, 13, LIGHTGRAY);
    
    if (GuiButton((Rectangle){ modal.x + modal.width - 100, modal.y + modal.height - 40, 80, 25 }, "Cerrar", 13, (Color){ 80, 40, 40, 255 })) {
        *showHelp = false;
    }
}

int main(void) {
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
        .speed = 8,
        .threads = maxThreads / 2 > 0 ? maxThreads / 2 : 1  // Default to half of CPU count
    };
    
    bool isDragging = false;
    bool showHelp = false;
    
    while (!WindowShouldClose()) {
        // Check for dropped files/folders
        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            
            for (int i = 0; i < (int)droppedFiles.count; i++) {
                AddFolder(droppedFiles.paths[i], &config);
            }
            
            UnloadDroppedFiles(droppedFiles);
        }
        
        // Drawing
        BeginDrawing();
        ClearBackground((Color){ 25, 25, 30, 255 });
        
        // Title bar
        DrawRectangle(0, 0, screenWidth, 70, (Color){ 35, 35, 42, 255 });
        DrawText("Manga Optimizer", 20, 15, 28, WHITE);
        DrawText("AVIF Smart Compression - Low Memory Mode", 20, 48, 14, (Color){ 150, 150, 160, 255 });
        
        // Help button in header
        if (GuiButton((Rectangle){ (float)screenWidth - 50, 15, 30, 30 }, "?", 18, (Color){ 60, 60, 70, 255 })) {
            showHelp = true;
        }
        
        // Memory indicator
        DrawText("libvips: ~50MB RAM", screenWidth - 150, 48, 12, (Color){ 100, 200, 100, 255 });
        
        // Drop zone
        Rectangle dropZone = { 20, 85, screenWidth - 40, 70 };
        isDragging = CheckCollisionPointRec(GetMousePosition(), dropZone);
        
        Color dropBg = isDragging ? (Color){ 50, 90, 50, 255 } : (Color){ 40, 40, 48, 255 };
        Color dropBorder = isDragging ? (Color){ 100, 200, 100, 255 } : (Color){ 70, 70, 80, 255 };
        
        DrawRectangleRounded(dropZone, 0.1f, 8, dropBg);
        DrawRectangleRoundedLinesEx(dropZone, 0.1f, 8, 2.0f, dropBorder);
        
        const char *dropText = "Arrastra carpetas aqui / Drop folders here";
        int textWidth = MeasureText(dropText, 18);
        DrawText(dropText, (int)(dropZone.x + (dropZone.width - textWidth) / 2), (int)(dropZone.y + 26), 18, 
                 isDragging ? WHITE : (Color){ 180, 180, 190, 255 });
        
        // Settings panel
        DrawRectangle(15, 170, screenWidth - 30, 120, (Color){ 35, 35, 42, 255 });
        DrawRectangleLines(15, 170, screenWidth - 30, 120, (Color){ 50, 50, 58, 255 });
        DrawText("Ajustes / Settings", 25, 178, 16, WHITE);
        
        // Quality slider
        DrawText(TextFormat("Calidad: %d", config.quality), 30, 205, 14, (Color){ 200, 200, 210, 255 });
        config.quality = DrawSlider((Rectangle){ 180, 203, 200, 16 }, config.quality, 0, 100, (Color){ 80, 160, 80, 255 });
        DrawText("(0=min, 100=max)", 390, 205, 12, GRAY);
        
        // Speed slider
        DrawText(TextFormat("Velocidad: %d", config.speed), 30, 230, 14, (Color){ 200, 200, 210, 255 });
        config.speed = DrawSlider((Rectangle){ 180, 228, 200, 16 }, config.speed, 0, 10, (Color){ 80, 140, 200, 255 });
        DrawText("(0=lento, 10=rapido)", 390, 230, 12, GRAY);
        
        // Threads slider
        DrawText(TextFormat("Hilos: %d", config.threads), 30, 255, 14, (Color){ 200, 200, 210, 255 });
        config.threads = DrawSlider((Rectangle){ 180, 253, 200, 16 }, config.threads, 1, maxThreads, (Color){ 200, 140, 80, 255 });
        DrawText(TextFormat("(max: %d CPUs)", maxThreads), 390, 255, 12, GRAY);
        
        // Jobs panel
        DrawRectangle(15, 305, screenWidth - 30, 210, (Color){ 35, 35, 42, 255 });
        DrawRectangleLines(15, 305, screenWidth - 30, 210, (Color){ 50, 50, 58, 255 });
        DrawText("Trabajos / Jobs", 25, 313, 16, WHITE);
        
        if (jobCount == 0) {
            DrawText("No hay trabajos. Arrastra una carpeta para comenzar.", 40, 360, 14, GRAY);
        } else {
            int yOffset = 345;
            pthread_mutex_lock(&jobMutex);
            for (int i = 0; i < jobCount && i < 4; i++) {
                FolderJob *job = jobs[i];
                if (!job) continue;
                
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
                // Wider truncation (approx match to progress bar width)
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
                DrawText(displayPath, 35, yOffset, 14, WHITE);
                
                // Row 2: Progress bar + details
                int detailsY = yOffset + 20;
                Rectangle progressBar = { 35.0f, (float)(detailsY + 2), 400.0f, 10.0f };
                
                DrawRectangleRec(progressBar, (Color){ 45, 45, 50, 255 });
                if (job->totalFiles > 0) {
                    DrawRectangle((int)progressBar.x, (int)progressBar.y, 
                                 (int)(progressBar.width * job->progress / 100.0f), 
                                 (int)progressBar.height, statusColor);
                }
                
                // Progress text
                DrawText(TextFormat("%d/%d", job->doneFiles, job->totalFiles), 450, detailsY, 13, LIGHTGRAY);
                
                // Status label
                DrawText(statusText, screenWidth - 100, detailsY, 13, statusColor);
                
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
                        // Delete job: free memory and shift pointers
                        FolderJob* jobToFree = jobs[i];
                        for (int j = i; j < jobCount - 1; j++) {
                            jobs[j] = jobs[j+1];
                        }
                        jobCount--;
                        free(jobToFree);
                        pthread_mutex_unlock(&jobMutex);
                        break; // Exit loop, next frame will redraw correctly
                    }
                }
                
                // Current file (if processing or paused)
                if ((job->status == JOB_PROCESSING || job->status == JOB_PAUSED || job->status == JOB_STOPPING) && job->currentFile[0] != '\0') {
                    DrawText(TextFormat("  > %s", job->currentFile), 35, detailsY + 16, 11, GRAY);
                    yOffset += 55; // Enough space for next job
                } else {
                    yOffset += 45; // Standard space per job
                }
            }
            pthread_mutex_unlock(&jobMutex);
        }
        
        // Footer
        DrawText("v1.0 - raylib + libvips | Cross-platform Windows/Linux", 20, screenHeight - 22, 11, DARKGRAY);
        
        // Help Dialog (top layer)
        if (showHelp) DrawHelpDialog(screenWidth, screenHeight, &showHelp);
        
        EndDrawing();
    }
    
    CloseWindow();
    processor_shutdown();
    
    // Final cleanup of remaining jobs
    for (int i = 0; i < jobCount; i++) {
        if (jobs[i]) free(jobs[i]);
    }
    
    return 0;
}
