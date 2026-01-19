package main

import (
	"context"
	"fmt"
	"path/filepath"
	"runtime"
	"sync"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

type JobCard struct {
	container *fyne.Container
	progress  *widget.ProgressBar
	status    *widget.Label
	info      *widget.Label
	current   *widget.Label
	controls  *fyne.Container
	pauseBtn  *widget.Button
	stopBtn   *widget.Button
	clearBtn  *widget.Button
}

func main() {
	myApp := app.New()
	myApp.Settings().SetTheme(theme.DarkTheme())
	myWindow := myApp.NewWindow("Manga Optimizer - Fyne Edition")
	myWindow.Resize(fyne.NewSize(600, 800))

	var processor *ImageProcessor
	var mu sync.Mutex
	jobCards := make(map[string]*JobCard)
	jobsListContainer := container.NewVBox()

	// Update UI from Processor updates
	updateUI := func(job JobDisplay) {
		fyne.Do(func() {
			mu.Lock()
			card, ok := jobCards[job.ID]
			mu.Unlock()

			if !ok {
				// Create new card
				card = createJobCard(job, processor, func(id string) {
					processor.DeleteJob(id) // Remove from backend
					mu.Lock()
					delete(jobCards, id)
					mu.Unlock()
					refreshJobList(jobsListContainer, jobCards, &mu)
				})
				mu.Lock()
				jobCards[job.ID] = card
				mu.Unlock()
				refreshJobList(jobsListContainer, jobCards, &mu)
			}

			// Update existing card
			card.progress.SetValue(float64(job.Progress))
			card.status.SetText(string(job.Status))

			if job.Status == StatusCompleted {
				card.info.SetText(fmt.Sprintf("Completado: %d imágenes", job.TotalFiles))
				card.current.Hide()
				card.pauseBtn.Hide()
				card.stopBtn.Hide()
				card.clearBtn.Show()
			} else {
				card.info.SetText(fmt.Sprintf("Procesando: %d / %d", job.DoneFiles, job.TotalFiles))
				if job.CurrentFile != "" {
					card.current.SetText("Actual: " + job.CurrentFile)
					card.current.Show()
				}

				if job.Status == StatusProcessing {
					card.pauseBtn.SetText("Pausar")
					card.pauseBtn.Show()
					card.stopBtn.Show()
					card.clearBtn.Hide()
				} else if job.Status == StatusPaused {
					card.pauseBtn.SetText("Reanudar")
					card.pauseBtn.Show()
					card.stopBtn.Show()
					card.clearBtn.Hide()
				} else {
					card.pauseBtn.Hide()
					card.stopBtn.Hide()
					card.clearBtn.Show()
				}
			}
		})
	}

	processor = NewImageProcessor(updateUI)
	processor.startup(context.Background())

	header := container.NewVBox(
		container.NewHBox(
			layoutSpacer(),
			widget.NewLabelWithStyle("Manga Optimizer", fyne.TextAlignCenter, fyne.TextStyle{Bold: true}),
			layoutSpacer(),
			widget.NewButtonWithIcon("", theme.HelpIcon(), func() {
				showHelpDialog(myWindow)
			}),
		),
		widget.NewLabelWithStyle("AVIF Smart Compression", fyne.TextAlignCenter, fyne.TextStyle{Italic: true}),
	)

	// Settings UI
	qualityLabel := widget.NewLabel("Calidad: 55")
	qualitySlider := widget.NewSlider(0, 100)
	qualitySlider.Value = 55
	qualitySlider.OnChanged = func(v float64) {
		qualityLabel.SetText(fmt.Sprintf("Calidad: %d", int(v)))
	}

	speedLabel := widget.NewLabel("Velocidad: 8")
	speedSlider := widget.NewSlider(0, 10)
	speedSlider.Value = 8
	speedSlider.OnChanged = func(v float64) {
		speedLabel.SetText(fmt.Sprintf("Velocidad: %d", int(v)))
	}

	maxThreads := runtime.NumCPU()
	if maxThreads < 1 {
		maxThreads = 1
	}
	threadsLabel := widget.NewLabel(fmt.Sprintf("Hilos: %d", runtime.NumCPU()))
	threadsSlider := widget.NewSlider(1, float64(maxThreads))
	threadsSlider.Value = float64(runtime.NumCPU())
	threadsSlider.OnChanged = func(v float64) {
		threadsLabel.SetText(fmt.Sprintf("Hilos: %d", int(v)))
	}

	settingsCard := widget.NewCard("Ajustes de Compresión", "Afecta a las nuevas carpetas añadidas",
		container.NewVBox(
			container.NewBorder(nil, nil, widget.NewLabel("Calidad (0-100)"), qualityLabel, qualitySlider),
			container.NewBorder(nil, nil, widget.NewLabel("Velocidad (0-10)"), speedLabel, speedSlider),
			container.NewBorder(nil, nil, widget.NewLabel("Hilos (CPU)"), threadsLabel, threadsSlider),
		),
	)

	getConfig := func() CompressionConfig {
		return CompressionConfig{
			Quality: int(qualitySlider.Value),
			Speed:   int(speedSlider.Value),
			Threads: int(threadsSlider.Value),
		}
	}

	addFolderBtn := widget.NewButtonWithIcon("Añadir Carpeta", theme.ContentAddIcon(), func() {
		dialog.ShowFolderOpen(func(list fyne.ListableURI, err error) {
			if err == nil && list != nil {
				processor.ProcessFolder(list.Path(), getConfig())
			}
		}, myWindow)
	})

	// Drop Zone
	dropLabel := widget.NewLabel("Arrastra carpetas aquí")
	dropLabel.Alignment = fyne.TextAlignCenter
	dropZone := container.NewPadded(container.NewStack(
		widget.NewCard("", "", dropLabel),
	))

	myWindow.SetOnDropped(func(p fyne.Position, uris []fyne.URI) {
		for _, u := range uris {
			path := u.Path()
			resolved := processor.ResolveFolder(path)
			if resolved != "" {
				processor.ProcessFolder(resolved, getConfig())
			}
		}
	})

	scroll := container.NewVScroll(jobsListContainer)

	mainContent := container.NewBorder(
		container.NewVBox(header, settingsCard, addFolderBtn, dropZone, widget.NewSeparator()),
		nil, nil, nil,
		scroll,
	)

	myWindow.SetContent(mainContent)
	myWindow.ShowAndRun()
}

func createJobCard(job JobDisplay, p *ImageProcessor, onDelete func(string)) *JobCard {
	name := filepath.Base(job.SourcePath)
	title := widget.NewLabelWithStyle(name, fyne.TextAlignLeading, fyne.TextStyle{Bold: true})
	status := widget.NewLabel(string(job.Status))
	info := widget.NewLabel("")
	current := widget.NewLabel("")
	current.Hide()

	progress := widget.NewProgressBar()
	progress.Max = 100

	pauseBtn := widget.NewButton("Pausar", func() {
		if status.Text == string(StatusProcessing) {
			p.PauseJob(job.ID)
		} else {
			p.ResumeJob(job.ID)
		}
	})

	stopBtn := widget.NewButton("Parar", func() {
		p.StopJob(job.ID)
	})

	clearBtn := widget.NewButton("Eliminar", func() {
		onDelete(job.ID)
	})
	clearBtn.Hide()

	controls := container.NewHBox(pauseBtn, stopBtn, clearBtn)

	card := container.NewVBox(
		container.NewHBox(title, layoutSpacer(), status),
		progress,
		container.NewHBox(info, layoutSpacer(), controls),
		current,
		widget.NewSeparator(),
	)

	return &JobCard{
		container: card,
		progress:  progress,
		status:    status,
		info:      info,
		current:   current,
		controls:  controls,
		pauseBtn:  pauseBtn,
		stopBtn:   stopBtn,
		clearBtn:  clearBtn,
	}
}

func refreshJobList(c *fyne.Container, cards map[string]*JobCard, mu *sync.Mutex) {
	c.Objects = nil
	mu.Lock()
	defer mu.Unlock()
	// Sort by ID or just add them
	for _, card := range cards {
		c.Add(card.container)
	}
	c.Refresh()
}

func showHelpDialog(w fyne.Window) {
	content := container.NewVBox(
		widget.NewLabelWithStyle("Guía de Usuario", fyne.TextAlignCenter, fyne.TextStyle{Bold: true}),
		widget.NewSeparator(),

		widget.NewLabelWithStyle("Ajustes de Compresión", fyne.TextAlignLeading, fyne.TextStyle{Bold: true}),
		widget.NewLabel("- Calidad: Define la fidelidad visual. 55-65 es recomendado."),
		widget.NewLabel("- Velocidad: 0 es más lento (mejor compresión), 10 es más rápido."),
		widget.NewLabel("- Hilos: Cuántas imágenes se procesan a la vez. Recomendado: # de CPUs."),

		widget.NewSeparator(),
		widget.NewLabelWithStyle("Uso de la Aplicación", fyne.TextAlignLeading, fyne.TextStyle{Bold: true}),
		widget.NewLabel("- Añadir Carpeta: Selecciona una carpeta para empezar a procesar."),
		widget.NewLabel("- Drag & Drop: Arrastra carpetas directamente a la zona punteada."),
		widget.NewLabel("- Salida: Crea una nueva carpeta con el sufijo '(compressed)'."),

		widget.NewSeparator(),
		widget.NewLabelWithStyle("Gestión de Procesos", fyne.TextAlignLeading, fyne.TextStyle{Bold: true}),
		widget.NewLabel("- Pausar/Reanudar: Detiene temporalmente el trabajo actual."),
		widget.NewLabel("- Parar: Cancela el trabajo definitivamente."),
		widget.NewLabel("- Eliminar: Quita el registro de la lista (solo disponible al terminar/parar)."),
	)

	d := dialog.NewCustom("Ayuda e Información", "Cerrar", content, w)
	d.Resize(fyne.NewSize(450, 400))
	d.Show()
}

func layoutSpacer() fyne.CanvasObject {
	return container.NewStack()
}
