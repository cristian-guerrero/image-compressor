package main

import (
	"context"
	"embed"
	"fmt"
	"os"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
	"github.com/wailsapp/wails/v2/pkg/runtime"
)

//go:embed all:frontend/dist
var assets embed.FS

func main() {
	// Create instances of the app structures
	app := NewApp()
	processor := NewImageProcessor()

	// Create application with options
	err := wails.Run(&options.App{
		Title:  "image-compressor",
		Width:  1024,
		Height: 768,
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		BackgroundColour: &options.RGBA{R: 27, G: 38, B: 54, A: 1},
		OnStartup: func(ctx context.Context) {
			app.startup(ctx)
			processor.startup(ctx)

			// Register the file drop handler
			runtime.OnFileDrop(ctx, func(x, y int, paths []string) {
				fmt.Printf("Wails: Backend drop detected: %v\n", paths)
				for _, path := range paths {
					info, err := os.Stat(path)
					if err == nil && info.IsDir() {
						processor.ProcessFolder(path)
					}
				}
			})
		},
		Bind: []interface{}{
			app,
			processor,
		},
		DragAndDrop: &options.DragAndDrop{
			EnableFileDrop:     true,
			DisableWebViewDrop: true,
		},
	})

	if err != nil {
		println("Error:", err.Error())
	}
}
