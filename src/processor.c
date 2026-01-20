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
#include <pthread.h>
#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
    #include <direct.h>
    #include <psapi.h>
    #define PATH_SEP '\\'
    #define PATH_SEP_STR "\\"
    #define my_mkdir(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #include <dirent.h>
    #include <unistd.h>
    #define PATH_SEP '/'
    #define PATH_SEP_STR "/"
    #define my_mkdir(path) mkdir(path, 0755)
#endif

#ifdef _WIN32
// Helper: Convert UTF-8 string to Wide string
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
#endif

// Supported image extensions
static const char *SUPPORTED_EXTENSIONS[] = {
    ".jpg", ".jpeg", ".png", ".webp", ".gif", ".bmp", ".tiff", ".tif",
    ".avif", ".heic", ".heif",
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
    
    // Disable caching on Windows to prevent file locking issues
    // Using a tiny cache to avoid leaks while maintaining some performance
    vips_cache_set_max(10);
    vips_cache_set_max_mem(50 * 1024 * 1024); // 50MB
    vips_cache_set_max_files(10);
    
    // Enable leak reporting to stdout/stderr
    // vips_leak_set(TRUE); 
    
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

void processor_thread_cleanup(void) {
    vips_thread_shutdown();
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
    
    // Load the image (using sequential access for low memory)
    image = vips_image_new_from_file(inputPath, "access", VIPS_ACCESS_SEQUENTIAL, NULL);
    if (!image) {
        fprintf(stderr, "Error loading: %s\n", inputPath);
        return -1;
    }
    
    // Original size for comparison
    long originalSize = get_file_size(inputPath);
    
    // Speed mapping: UI (0=slow, 10=fast) -> libvips effort (0=slow/best, 9=fast/worst)
    int effort = config->speed;
    if (effort > 9) effort = 9;

    // Save as AVIF with specified quality
    int result = vips_heifsave(image, outputPath,
                               "Q", config->quality,
                               "effort", effort,
                               "compression", VIPS_FOREIGN_HEIF_COMPRESSION_AV1,
                               NULL);
    
    g_object_unref(image);
    
    // Aggressively drop all cache entries to release file handles immediately
    vips_cache_drop_all();
    vips_error_clear();
    
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

// Data passed to image processing threads
typedef struct {
    FolderJob *job;
    char **imageFiles;
    int imageCount;
    int *nextImageIndex;
    pthread_mutex_t *lock;
} ParallelJobData;

// Thread function for processing images in parallel
void* image_worker(void *arg) {
    ParallelJobData *data = (ParallelJobData*)arg;
    
    // For AVIF, we want 1 thread per image to maximize image-level parallelism
    vips_concurrency_set(1);
    
    while (1) {
        int index = -1;
        
        // Pick next image
        pthread_mutex_lock(data->lock);
        if (data->job->status == JOB_STOPPED || data->job->status == JOB_STOPPING) {
            pthread_mutex_unlock(data->lock);
            break;
        }
        
        if (*data->nextImageIndex < data->imageCount) {
            index = (*data->nextImageIndex)++;
        }
        pthread_mutex_unlock(data->lock);
        
        if (index == -1) break; // No more images
        
        // Handle Pause
        while (data->job->status == JOB_PAUSED) {
            processor_sleep(200);
            if (data->job->status == JOB_STOPPED || data->job->status == JOB_STOPPING) break;
        }
        if (data->job->status == JOB_STOPPED || data->job->status == JOB_STOPPING) break;

        char inputPath[1024];
        char outputPath[1024];
        const char *filename = "";
        
        // Get filename for UI display
        const char *lastSlash = strrchr(data->imageFiles[index], PATH_SEP);
        filename = lastSlash ? lastSlash + 1 : data->imageFiles[index];

        snprintf(inputPath, sizeof(inputPath), "%s%c%s", data->job->sourcePath, PATH_SEP, data->imageFiles[index]);
        
        // Build output path by stripping original extension
        char baseName[260];
        strncpy(baseName, data->imageFiles[index], sizeof(baseName) - 1);
        baseName[sizeof(baseName) - 1] = '\0';
        char *dot = strrchr(baseName, '.');
        if (dot) *dot = '\0';
        
        snprintf(outputPath, sizeof(outputPath), "%s%c%s.avif", data->job->outputPath, PATH_SEP, baseName);

        // Update current file status and active count
        pthread_mutex_lock(data->lock);
        strncpy(data->job->currentFile, filename, 255);
        data->job->activeThreads++;
        pthread_mutex_unlock(data->lock);

        // Check if already processed to enable resume
#ifdef _WIN32
        wchar_t wideOutputPath[520];
        utf8_to_wide(outputPath, wideOutputPath, 520);
        if (GetFileAttributesW(wideOutputPath) != INVALID_FILE_ATTRIBUTES) {
#else
        struct stat st;
        if (stat(outputPath, &st) == 0) {
#endif
            pthread_mutex_lock(data->lock);
            data->job->doneFiles++;
            data->job->activeThreads--; // Decrement since we're done here
            if (data->imageCount > 0) {
                data->job->progress = (data->job->doneFiles * 100) / data->imageCount;
            }
            pthread_mutex_unlock(data->lock);
            continue;
        }

        // Parallelism proof: Log before starting
        printf("[Job %p] Thread %p: Starting %s\n", (void*)data->job, (void*)pthread_self(), filename);

        // Compress
        compress_image_to_avif(inputPath, outputPath, data->imageFiles[index], &data->job->config);

        // Update progress and decrement active count
        pthread_mutex_lock(data->lock);
        data->job->doneFiles++;
        data->job->activeThreads--; 
        if (data->imageCount > 0) {
            data->job->progress = (data->job->doneFiles * 100) / data->imageCount;
        }
        pthread_mutex_unlock(data->lock);
    }
    
    vips_thread_shutdown();
    return NULL;
}

#ifdef _WIN32
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
    job->activeThreads = 0;
    
    // Set libvips concurrency for this job
    vips_concurrency_set(job->config.threads);
    printf("Job Concurrency: %d threads\n", job->config.threads);
    
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
    
    job->totalFiles = imageCount;
    
    // Prepare parallel processing
    int nextIndex = 0;
    pthread_mutex_t jobLock;
    pthread_mutex_init(&jobLock, NULL);
    
    int numThreads = job->config.threads;
    if (numThreads < 1) numThreads = 1;
    if (numThreads > imageCount) numThreads = imageCount;
    
    printf("Spawning %d threads for %d images\n", numThreads, imageCount);
    
    pthread_t *threads = (pthread_t*)malloc(numThreads * sizeof(pthread_t));
    ParallelJobData *threadData = (ParallelJobData*)malloc(numThreads * sizeof(ParallelJobData));
    
    for (int i = 0; i < numThreads; i++) {
        threadData[i].job = job;
        threadData[i].imageFiles = imageFiles;
        threadData[i].imageCount = imageCount;
        threadData[i].nextImageIndex = &nextIndex;
        threadData[i].lock = &jobLock;
        pthread_create(&threads[i], NULL, image_worker, &threadData[i]);
    }
    
    // Wait for all threads to finish
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    free(threadData);
    pthread_mutex_destroy(&jobLock);
    
    // Clean up file list
    for (int i = 0; i < imageCount; i++) {
        free(imageFiles[i]);
    }
    
    free(imageFiles);
    
    if (job->status == JOB_STOPPING) {
        job->status = JOB_STOPPED;
    } else if (job->status != JOB_STOPPED) {
        job->status = JOB_COMPLETED;
    }
    
    // Force libvips to release all file handles from cache
    vips_cache_drop_all();
    vips_thread_shutdown();
    
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
    job->activeThreads = 0;
    
    // Set libvips concurrency for this job
    vips_concurrency_set(job->config.threads);
    printf("Job Concurrency: %d threads\n", job->config.threads);
    
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
    
    // Prepare parallel processing
    int nextIndex = 0;
    pthread_mutex_t jobLock;
    pthread_mutex_init(&jobLock, NULL);
    
    int numThreads = job->config.threads;
    if (numThreads < 1) numThreads = 1;
    if (numThreads > imageCount) numThreads = imageCount;
    
    printf("Spawning %d threads for %d images\n", numThreads, imageCount);
    
    pthread_t *threads = (pthread_t*)malloc(numThreads * sizeof(pthread_t));
    ParallelJobData *threadData = (ParallelJobData*)malloc(numThreads * sizeof(ParallelJobData));
    
    for (int i = 0; i < numThreads; i++) {
        threadData[i].job = job;
        threadData[i].imageFiles = imageFiles;
        threadData[i].imageCount = imageCount;
        threadData[i].nextImageIndex = &nextIndex;
        threadData[i].lock = &jobLock;
        pthread_create(&threads[i], NULL, image_worker, &threadData[i]);
    }
    
    // Wait for all threads to finish
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    free(threadData);
    pthread_mutex_destroy(&jobLock);
    
    // Clean up file list
    for (int i = 0; i < imageCount; i++) {
        free(imageFiles[i]);
    }
    free(imageFiles);
    
    if (job->status == JOB_STOPPING) {
        job->status = JOB_STOPPED;
    } else if (job->status != JOB_STOPPED) {
        job->status = JOB_COMPLETED;
    }
    
    // Force libvips to release all file handles from cache
    vips_cache_drop_all();
    vips_thread_shutdown();
    
    printf("Job finished (status %d): %s\n", job->status, job->sourcePath);
    return 0;
}
#endif

char* pick_folder_dialog(void) {
    char *path = (char *)malloc(1024);
    if (!path) return NULL;
    path[0] = '\0';

#ifdef _WIN32
    BROWSEINFOW bi = { 0 };
    bi.lpszTitle = L"Selecciona una carpeta / Select a folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
    
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL) {
        wchar_t widePath[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, widePath)) {
            wide_to_utf8(widePath, path, 1024);
        }
        
        IMalloc *imalloc = NULL;
        if (SUCCEEDED(SHGetMalloc(&imalloc))) {
            imalloc->lpVtbl->Free(imalloc, pidl);
            imalloc->lpVtbl->Release(imalloc);
        }
    }
#else
    // Linux/macOS: Try zenity, then kdialog
    FILE *pipe = popen("zenity --file-selection --directory --title=\"Selecciona una carpeta\" 2>/dev/null", "r");
    if (!pipe) pipe = popen("kdialog --getexistingdirectory . 2>/dev/null", "r");
    
    if (!pipe) {
        free(path);
        return NULL;
    }
    
    if (fgets(path, 1024, pipe) != NULL) {
        size_t len = strlen(path);
        if (len > 0 && path[len-1] == '\n') path[len-1] = '\0';
    }
    pclose(pipe);
#endif

    if (strlen(path) == 0) {
        free(path);
        return NULL;
    }
    
    return path;
}

long long get_process_ram_usage(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return (long long)pmc.WorkingSetSize;
    }
    return 0;
#else
    // Basic Linux implementation using /proc/self/statm
    long RSS = 0;
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp) {
        if (fscanf(fp, "%*s%ld", &RSS) == 1) {
            RSS *= sysconf(_SC_PAGESIZE);
        }
        fclose(fp);
    }
    return (long long)RSS;
#endif
}
