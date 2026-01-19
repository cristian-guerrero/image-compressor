/*
 * processor.h - Image Processing API
 * This header is designed to NOT include any vips or raylib headers
 * to avoid conflicts between them.
 */

#ifndef PROCESSOR_H
#define PROCESSOR_H

// Job status codes
#define JOB_PENDING    0
#define JOB_PROCESSING 1
#define JOB_COMPLETED  2
#define JOB_ERROR      3
#define JOB_STOPPED    4

// Compression settings
typedef struct {
    int quality;      // 0-100 (default: 55)
    int speed;        // 0-10 (default: 8, higher = faster)
    int threads;      // Number of worker threads
} CompressionConfig;

// Single folder job
typedef struct {
    char sourcePath[512];
    char outputPath[512];
    volatile int status;       // Use JOB_* constants
    int progress;
    int totalFiles;
    int doneFiles;
    char currentFile[256];
    CompressionConfig config;
} FolderJob;

// Initialize libvips (call once at startup)
// Returns: 1 on success, 0 on error
int processor_init(void);

// Shutdown libvips (call before exit)
void processor_shutdown(void);

// Process an entire folder
// Updates job->progress, job->doneFiles, job->currentFile during processing
int process_folder(FolderJob *job);

// Get output path for compressed folder
void get_output_folder_path(const char *inputPath, char *outputPath, int maxLen);

// Check if a path is a directory (handles unicode on Windows)
int check_is_directory(const char *path);

// Get the number of CPU cores available
int get_cpu_count(void);

// Sleep current thread
void processor_sleep(int ms);

#endif // PROCESSOR_H
