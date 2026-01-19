/*
 * processor.c - Image Processing with libvips
 * AVIF compression with minimal memory footprint
 * 
 * This file ONLY includes vips.h, never raylib.h to avoid conflicts.
 */

#include "processor.h"
#include <vips/vips.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define PATH_SEP '\\'
    #define PATH_SEP_STR "\\"
    #define my_mkdir(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #include <dirent.h>
    #define PATH_SEP '/'
    #define PATH_SEP_STR "/"
    #define my_mkdir(path) mkdir(path, 0755)
#endif

// Supported image extensions
static const char *SUPPORTED_EXTENSIONS[] = {
    ".jpg", ".jpeg", ".png", ".webp", ".gif", ".bmp", ".tiff", ".tif",
    NULL
};

static int vips_initialized = 0;

int processor_init(void) {
    if (vips_initialized) return 1;
    
    if (VIPS_INIT("image-compressor")) {
        fprintf(stderr, "Error: Failed to initialize libvips\n");
        return 0;
    }
    
    // Set concurrency
    vips_concurrency_set(4);
    
    // Enable caching for better performance
    vips_cache_set_max(100);
    vips_cache_set_max_mem(50 * 1024 * 1024);  // 50MB cache
    
    printf("libvips %s initialized\n", vips_version_string());
    vips_initialized = 1;
    return 1;
}

void processor_shutdown(void) {
    if (vips_initialized) {
        vips_shutdown();
        vips_initialized = 0;
    }
}

// Check if filename has a supported image extension
static int is_supported_image(const char *filename) {
    if (!filename) return 0;
    
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    
    // Convert to lowercase for comparison
    char lowerExt[16];
    int i = 0;
    for (; ext[i] && i < 15; i++) {
        lowerExt[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
    }
    lowerExt[i] = '\0';
    
    for (int j = 0; SUPPORTED_EXTENSIONS[j] != NULL; j++) {
        if (strcmp(lowerExt, SUPPORTED_EXTENSIONS[j]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Output path: source + " (compressed)"
void get_output_folder_path(const char *inputPath, char *outputPath, int maxLen) {
    snprintf(outputPath, maxLen, "%s (compressed)", inputPath);
}

// Get the number of logical CPU cores available
int get_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

// Sleep current thread
void processor_sleep(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

// Check if a path is a directory - works with unicode paths on Windows
int check_is_directory(const char *path) {
#ifdef _WIN32
    // raylib passes UTF-8 paths, convert to Wide for Windows API
    wchar_t widePath[520];
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, 520);
    
    if (len == 0) {
        // UTF-8 conversion failed, try ANSI fallback
        DWORD attrs = GetFileAttributesA(path);
        if (attrs == INVALID_FILE_ATTRIBUTES) return 0;
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    
    DWORD attrs = GetFileAttributesW(widePath);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
#endif
}

// Get file size in bytes
static long get_file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

// Copy file (for cases where compression doesn't help)
static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    
    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        fwrite(buffer, 1, bytes, out);
    }
    
    fclose(in);
    fclose(out);
    return 0;
}

// Compress a single image to AVIF
static int compress_image_to_avif(const char *inputPath, const char *outputPath, 
                                   const char *originalName, CompressionConfig *config) {
    VipsImage *image = NULL;
    
    // Load the image (libvips uses streaming, low memory)
    image = vips_image_new_from_file(inputPath, "access", VIPS_ACCESS_SEQUENTIAL, NULL);
    if (!image) {
        fprintf(stderr, "Error loading: %s\n", inputPath);
        return -1;
    }
    
    // Get original file size for comparison
    long originalSize = get_file_size(inputPath);
    
    // Save as AVIF with specified quality
    int result = vips_heifsave(image, outputPath,
                               "Q", config->quality,
                               "speed", config->speed,
                               "compression", VIPS_FOREIGN_HEIF_COMPRESSION_AV1,
                               NULL);
    
    g_object_unref(image);
    
    if (result != 0) {
        fprintf(stderr, "Error saving AVIF: %s - %s\n", outputPath, vips_error_buffer());
        vips_error_clear();
        return -1;
    }
    
    // Check if compression was worthwhile (>15% reduction)
    long compressedSize = get_file_size(outputPath);
    if (compressedSize > 0 && originalSize > 0) {
        double ratio = (double)compressedSize / (double)originalSize;
        if (ratio > 0.85) {
            // Compression didn't help much, keep original format
            remove(outputPath);
            
            // Build path for original copy
            char outputDir[512];
            strncpy(outputDir, outputPath, sizeof(outputDir) - 1);
            char *lastSlash = strrchr(outputDir, PATH_SEP);
            if (lastSlash) *lastSlash = '\0';
            
            char originalDest[520];
            snprintf(originalDest, sizeof(originalDest), "%s%s%s", 
                     outputDir, PATH_SEP_STR, originalName);
            copy_file(inputPath, originalDest);
            printf("Kept original (%.0f%%): %s\n", ratio * 100, originalName);
        } else {
            printf("Compressed to %.0f%%: %s\n", ratio * 100, originalName);
        }
    }
    
    return 0;
}

#ifdef _WIN32
// Helper: Convert UTF-8 string to Wide string
// Note: raylib passes paths in UTF-8 encoding
static int utf8_to_wide(const char *utf8, wchar_t *wide, int wideSize) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wideSize);
}

// Helper: Convert Wide string to UTF-8
static int wide_to_utf8(const wchar_t *wide, char *utf8, int utf8Size) {
    return WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Size, NULL, NULL);
}

// Helper: Create directory with unicode support
static int create_dir_unicode(const char *path) {
    wchar_t widePath[520];
    if (utf8_to_wide(path, widePath, 520) == 0) {
        return _mkdir(path);
    }
    return _wmkdir(widePath) == 0 ? 0 : -1;
}

// Windows directory iteration with Unicode support
int process_folder(FolderJob *job) {
    WIN32_FIND_DATAW findData;
    HANDLE hFind;
    wchar_t wideSearchPath[520];
    wchar_t wideSourcePath[520];
    char **imageFiles = NULL;
    int imageCount = 0;
    int capacity = 64;
    
    printf("Processing: %s\n", job->sourcePath);
    
    job->status = JOB_PROCESSING;
    job->progress = 0;
    job->doneFiles = 0;
    
    // Convert source path to wide (raylib uses UTF-8)
    if (utf8_to_wide(job->sourcePath, wideSourcePath, 520) == 0) {
        fprintf(stderr, "Error: Failed to convert path to unicode\n");
        job->status = JOB_ERROR;
        return -1;
    }
    
    // Create output directory
    create_dir_unicode(job->outputPath);
    
    // Build wide search pattern
    wcscpy(wideSearchPath, wideSourcePath);
    wcscat(wideSearchPath, L"\\*");
    
    // Allocate file list
    imageFiles = (char **)malloc(capacity * sizeof(char *));
    if (!imageFiles) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        job->status = JOB_ERROR;
        return -1;
    }
    
    // Find all image files using Wide API
    hFind = FindFirstFileW(wideSearchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: Cannot open directory\n");
        job->status = JOB_ERROR;
        free(imageFiles);
        return -1;
    }
    
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // Convert wide filename to UTF-8
            char utf8Filename[260];
            wide_to_utf8(findData.cFileName, utf8Filename, sizeof(utf8Filename));
            
            if (is_supported_image(utf8Filename)) {
                if (imageCount >= capacity) {
                    capacity *= 2;
                    imageFiles = (char **)realloc(imageFiles, capacity * sizeof(char *));
                }
                imageFiles[imageCount] = _strdup(utf8Filename);
                imageCount++;
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    
    printf("Found %d images\n", imageCount);
    
    job->totalFiles = imageCount;
    
    if (imageCount == 0) {
        job->status = JOB_COMPLETED;
        free(imageFiles);
        return 0;
    }
    
    // Process each image
    for (int i = 0; i < imageCount; i++) {
        // Check for Stop/Pause
        while (job->status == JOB_PAUSED) {
            processor_sleep(200);
        }
        if (job->status == JOB_STOPPED || job->status == JOB_STOPPING) break;
        
        char inputPath[1024];
        char outputPath[1024];
        char avifName[260];
        
        // Update current file
        strncpy(job->currentFile, imageFiles[i], sizeof(job->currentFile) - 1);
        
        // Build paths
        snprintf(inputPath, sizeof(inputPath), "%s\\%s", job->sourcePath, imageFiles[i]);
        
        // Change extension to .avif
        strncpy(avifName, imageFiles[i], sizeof(avifName) - 1);
        char *dot = strrchr(avifName, '.');
        if (dot) {
            strcpy(dot, ".avif");
        } else {
            strcat(avifName, ".avif");
        }
        snprintf(outputPath, sizeof(outputPath), "%s\\%s", job->outputPath, avifName);
        
        // Check if already processed (using wide API)
        wchar_t wideOutputPath[520];
        utf8_to_wide(outputPath, wideOutputPath, 520);
        if (GetFileAttributesW(wideOutputPath) != INVALID_FILE_ATTRIBUTES) {
            // Already exists, skip
            job->doneFiles++;
            job->progress = (job->doneFiles * 100) / job->totalFiles;
            free(imageFiles[i]);
            continue;
        }
        
        printf("Compressing: %s\n", imageFiles[i]);
        
        // Compress
        compress_image_to_avif(inputPath, outputPath, imageFiles[i], &job->config);
        
        job->doneFiles++;
        job->progress = (job->doneFiles * 100) / job->totalFiles;
        
        free(imageFiles[i]);
    }
    
    free(imageFiles);
    
    if (job->status == JOB_STOPPING) {
        job->status = JOB_STOPPED;
    } else if (job->status != JOB_STOPPED) {
        job->status = JOB_COMPLETED;
    }
    
    printf("Job finished (status %d): %s\n", job->status, job->sourcePath);
    return 0;
}

#else
// Linux/POSIX directory iteration
int process_folder(FolderJob *job) {
    DIR *dir;
    struct dirent *entry;
    char **imageFiles = NULL;
    int imageCount = 0;
    int capacity = 64;
    
    job->status = JOB_PROCESSING;
    job->progress = 0;
    job->doneFiles = 0;
    
    // Create output directory
    my_mkdir(job->outputPath);
    
    // Allocate file list
    imageFiles = (char **)malloc(capacity * sizeof(char *));
    if (!imageFiles) {
        job->status = JOB_ERROR;
        return -1;
    }
    
    // Open directory
    dir = opendir(job->sourcePath);
    if (!dir) {
        job->status = JOB_ERROR;
        free(imageFiles);
        return -1;
    }
    
    // Find all image files
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && is_supported_image(entry->d_name)) {
            if (imageCount >= capacity) {
                capacity *= 2;
                imageFiles = (char **)realloc(imageFiles, capacity * sizeof(char *));
            }
            imageFiles[imageCount] = strdup(entry->d_name);
            imageCount++;
        }
    }
    closedir(dir);
    
    job->totalFiles = imageCount;
    printf("Found %d images in %s\n", imageCount, job->sourcePath);
    
    if (imageCount == 0) {
        job->status = JOB_COMPLETED;
        free(imageFiles);
        return 0;
    }
    
    // Process each image
    for (int i = 0; i < imageCount; i++) {
        // Check for Stop/Pause
        while (job->status == JOB_PAUSED) {
            processor_sleep(200);
        }
        if (job->status == JOB_STOPPED || job->status == JOB_STOPPING) break;
        
        char inputPath[520];
        char outputPath[520];
        char avifName[260];
        
        // Update current file
        strncpy(job->currentFile, imageFiles[i], sizeof(job->currentFile) - 1);
        
        // Build paths
        snprintf(inputPath, sizeof(inputPath), "%s/%s", job->sourcePath, imageFiles[i]);
        
        // Change extension to .avif
        strncpy(avifName, imageFiles[i], sizeof(avifName) - 1);
        char *dot = strrchr(avifName, '.');
        if (dot) {
            strcpy(dot, ".avif");
        } else {
            strcat(avifName, ".avif");
        }
        snprintf(outputPath, sizeof(outputPath), "%s/%s", job->outputPath, avifName);
        
        // Check if already processed
        struct stat st;
        if (stat(outputPath, &st) == 0) {
            // Already exists, skip
            job->doneFiles++;
            job->progress = (job->doneFiles * 100) / job->totalFiles;
            free(imageFiles[i]);
            continue;
        }
        
        // Compress
        compress_image_to_avif(inputPath, outputPath, imageFiles[i], &job->config);
        
        job->doneFiles++;
        job->progress = (job->doneFiles * 100) / job->totalFiles;
        
        free(imageFiles[i]);
    }
    
    free(imageFiles);
    
    if (job->status == JOB_STOPPING) {
        job->status = JOB_STOPPED;
    } else if (job->status != JOB_STOPPED) {
        job->status = JOB_COMPLETED;
    }
    
    printf("Job finished (status %d): %s\n", job->status, job->sourcePath);
    return 0;
}
#endif
