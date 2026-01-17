package main

import (
	"context"
	"fmt"
	"image"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"runtime/debug"
	"strings"
	"sync"
	"time"

	_ "image/gif"
	_ "image/jpeg"
	_ "image/png"

	"github.com/gen2brain/avif"
	"golang.org/x/image/webp"
)

type JobStatus string

const (
	StatusPending    JobStatus = "pending"
	StatusProcessing JobStatus = "processing"
	StatusPaused     JobStatus = "paused"
	StatusCompleted  JobStatus = "completed"
	StatusStopped    JobStatus = "stopped"
	StatusError      JobStatus = "error"
)

// JobDisplay is a clean copy for the UI to avoid data races
type JobDisplay struct {
	ID          string    `json:"id"`
	SourcePath  string    `json:"sourcePath"`
	OutputPath  string    `json:"outputPath"`
	Status      JobStatus `json:"status"`
	Progress    int       `json:"progress"`
	TotalFiles  int       `json:"totalFiles"`
	DoneFiles   int       `json:"doneFiles"`
	CurrentFile string    `json:"currentFile"`
}

type CompressionJob struct {
	ID          string
	SourcePath  string
	OutputPath  string
	Status      JobStatus
	Progress    int
	TotalFiles  int
	DoneFiles   int
	CurrentFile string
	cancel      context.CancelFunc
	pauseChan   chan struct{}
	resumeChan  chan struct{}
	mu          sync.Mutex
	lastUpdate  time.Time
}

type ImageProcessor struct {
	ctx      context.Context
	jobs     map[string]*CompressionJob
	jobQueue chan *CompressionJob
	mu       sync.Mutex
	onUpdate func(JobDisplay)
}

func NewImageProcessor(onUpdate func(JobDisplay)) *ImageProcessor {
	return &ImageProcessor{
		jobs:     make(map[string]*CompressionJob),
		jobQueue: make(chan *CompressionJob, 100),
		onUpdate: onUpdate,
	}
}

func (p *ImageProcessor) startup(ctx context.Context) {
	p.ctx = ctx
	go p.processQueue()
}

func (p *ImageProcessor) processQueue() {
	for {
		select {
		case <-p.ctx.Done():
			return
		case job := <-p.jobQueue:
			p.runJob(job)
			// Small break between folders
			time.Sleep(100 * time.Millisecond)
		}
	}
}

func (p *ImageProcessor) ProcessFolder(folderPath string) string {
	p.mu.Lock()
	defer p.mu.Unlock()

	id := fmt.Sprintf("job_%d", len(p.jobs)+1)
	outputFolder := folderPath + "_translated"

	job := &CompressionJob{
		ID:         id,
		SourcePath: folderPath,
		OutputPath: outputFolder,
		Status:     StatusPending,
		pauseChan:  make(chan struct{}),
		resumeChan: make(chan struct{}),
	}
	p.jobs[id] = job

	p.jobQueue <- job
	p.emitJobUpdateForce(job)

	return id
}

// ResolveFolder returns the directory path for a given file or directory path.
func (p *ImageProcessor) ResolveFolder(path string) string {
	info, err := os.Stat(path)
	if err != nil {
		return ""
	}
	if info.IsDir() {
		return path
	}
	return filepath.Dir(path)
}

func (p *ImageProcessor) runJob(job *CompressionJob) {
	job.mu.Lock()
	job.Status = StatusProcessing
	job.mu.Unlock()
	p.emitJobUpdateForce(job)

	files, err := os.ReadDir(job.SourcePath)
	if err != nil {
		job.Status = StatusError
		p.emitJobUpdateForce(job)
		return
	}

	var imageFiles []string
	for _, f := range files {
		if !f.IsDir() && isImage(f.Name()) {
			imageFiles = append(imageFiles, f.Name())
		}
	}

	job.TotalFiles = len(imageFiles)
	if job.TotalFiles == 0 {
		job.Status = StatusCompleted
		p.emitJobUpdateForce(job)
		return
	}

	if err := os.MkdirAll(job.OutputPath, 0755); err != nil {
		job.Status = StatusError
		p.emitJobUpdateForce(job)
		return
	}

	ctx, cancel := context.WithCancel(p.ctx)
	job.cancel = cancel

	semaphore := make(chan struct{}, 2)
	var wg sync.WaitGroup

	for _, filename := range imageFiles {
		select {
		case <-ctx.Done():
			job.Status = StatusStopped
			p.emitJobUpdateForce(job)
			return
		default:
		}

		job.mu.Lock()
		if job.Status == StatusPaused {
			job.mu.Unlock()
			select {
			case <-job.resumeChan:
			case <-ctx.Done():
				job.Status = StatusStopped
				p.emitJobUpdateForce(job)
				return
			}
			job.mu.Lock()
			job.Status = StatusProcessing
			job.mu.Unlock()
		} else {
			job.mu.Unlock()
		}

		semaphore <- struct{}{}
		wg.Add(1)

		go func(fname string) {
			defer func() {
				<-semaphore
				wg.Done()
			}()

			job.mu.Lock()
			job.CurrentFile = fname
			job.mu.Unlock()
			p.emitJobUpdate(job)

			src := filepath.Join(job.SourcePath, fname)
			p.compressSmart(src, job.OutputPath, fname)

			job.mu.Lock()
			job.DoneFiles++
			job.Progress = int((float64(job.DoneFiles) / float64(job.TotalFiles)) * 100)
			job.mu.Unlock()
			p.emitJobUpdate(job)

			runtime.Gosched()
			time.Sleep(50 * time.Millisecond)
		}(filename)
	}

	wg.Wait()
	job.Status = StatusCompleted
	p.emitJobUpdateForce(job)

	runtime.GC()
	debug.FreeOSMemory()
}

func (p *ImageProcessor) compressSmart(src, outDir, filename string) error {
	f, err := os.Open(src)
	if err != nil {
		return err
	}
	defer f.Close()

	originalInfo, _ := f.Stat()
	originalSize := originalInfo.Size()

	avifName := strings.TrimSuffix(filename, filepath.Ext(filename)) + ".avif"
	dstAvif := filepath.Join(outDir, avifName)

	img, _, err := image.Decode(f)
	if err != nil {
		if strings.ToLower(filepath.Ext(src)) == ".webp" {
			f.Seek(0, 0)
			img, err = webp.Decode(f)
		}
		if err != nil {
			return p.copyFile(src, filepath.Join(outDir, filename))
		}
	}

	out, err := os.Create(dstAvif)
	if err != nil {
		return err
	}

	err = avif.Encode(out, img, avif.Options{Quality: 55, Speed: 8})
	out.Close()

	if err != nil {
		return p.copyFile(src, filepath.Join(outDir, filename))
	}

	compressedInfo, _ := os.Stat(dstAvif)
	compressedSize := compressedInfo.Size()

	if float64(compressedSize) > float64(originalSize)*0.85 {
		os.Remove(dstAvif)
		return p.copyFile(src, filepath.Join(outDir, filename))
	}

	return nil
}

func (p *ImageProcessor) copyFile(src, dst string) error {
	sourceFile, err := os.Open(src)
	if err != nil {
		return err
	}
	defer sourceFile.Close()

	destFile, err := os.Create(dst)
	if err != nil {
		return err
	}
	defer destFile.Close()

	_, err = io.Copy(destFile, sourceFile)
	return err
}

func (p *ImageProcessor) emitJobUpdate(job *CompressionJob) {
	job.mu.Lock()
	now := time.Now()
	if now.Sub(job.lastUpdate) < 300*time.Millisecond {
		job.mu.Unlock()
		return
	}
	job.lastUpdate = now

	display := JobDisplay{
		ID:          job.ID,
		SourcePath:  job.SourcePath,
		OutputPath:  job.OutputPath,
		Status:      job.Status,
		Progress:    job.Progress,
		TotalFiles:  job.TotalFiles,
		DoneFiles:   job.DoneFiles,
		CurrentFile: job.CurrentFile,
	}
	job.mu.Unlock()

	if p.onUpdate != nil {
		p.onUpdate(display)
	}
}

func (p *ImageProcessor) emitJobUpdateForce(job *CompressionJob) {
	job.mu.Lock()
	job.lastUpdate = time.Now()
	display := JobDisplay{
		ID:          job.ID,
		SourcePath:  job.SourcePath,
		OutputPath:  job.OutputPath,
		Status:      job.Status,
		Progress:    job.Progress,
		TotalFiles:  job.TotalFiles,
		DoneFiles:   job.DoneFiles,
		CurrentFile: job.CurrentFile,
	}
	job.mu.Unlock()
	if p.onUpdate != nil {
		p.onUpdate(display)
	}
}

func (p *ImageProcessor) PauseJob(id string) {
	p.mu.Lock()
	job, ok := p.jobs[id]
	p.mu.Unlock()
	if ok {
		job.mu.Lock()
		job.Status = StatusPaused
		job.mu.Unlock()
		p.emitJobUpdateForce(job)
	}
}

func (p *ImageProcessor) ResumeJob(id string) {
	p.mu.Lock()
	job, ok := p.jobs[id]
	p.mu.Unlock()
	if ok {
		select {
		case job.resumeChan <- struct{}{}:
		default:
		}
	}
}

func (p *ImageProcessor) StopJob(id string) {
	p.mu.Lock()
	job, ok := p.jobs[id]
	p.mu.Unlock()
	if ok {
		if job.cancel != nil {
			job.cancel()
		}
	}
}

func (p *ImageProcessor) DeleteJob(id string) {
	p.mu.Lock()
	delete(p.jobs, id)
	p.mu.Unlock()
}

func (p *ImageProcessor) GetJobs() []JobDisplay {
	p.mu.Lock()
	defer p.mu.Unlock()
	list := make([]JobDisplay, 0)
	for _, job := range p.jobs {
		job.mu.Lock()
		list = append(list, JobDisplay{
			ID:          job.ID,
			SourcePath:  job.SourcePath,
			OutputPath:  job.OutputPath,
			Status:      job.Status,
			Progress:    job.Progress,
			TotalFiles:  job.TotalFiles,
			DoneFiles:   job.DoneFiles,
			CurrentFile: job.CurrentFile,
		})
		job.mu.Unlock()
	}
	return list
}

func isImage(name string) bool {
	ext := strings.ToLower(filepath.Ext(name))
	return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp" || ext == ".avif"
}
