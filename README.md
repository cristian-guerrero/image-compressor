# Manga Optimizer ⛩️

Manga Optimizer is a high-performance desktop application designed to bridge the gap between automatic translation tools and efficient storage. It specializes in converting "bloated" images from translators into ultra-optimized **AVIF** files without losing visual fidelity.

![Manga Optimizer](https://raw.githubusercontent.com/cristian-guerrero/image-compressor/main/build/appicon.png)

## ✨ Features

- **Native Go Performance**: Built with Go and Fyne for a fast, resource-efficient experience. No heavy browser engines.
- **Stable Drag & Drop**: Robust support for dragging folders and files directly from your Linux file manager.
- **Smart AVIF Compression**: Uses the state-of-the-art AV1 codec for maximum space saving.
- **Smart Decision Engine**: Automatically compares file sizes. If the optimized version isn't at least 15% smaller, the original file is preserved.
- **Batch Processing**: Processes images in parallel and manages a sequential folder queue.
- **Manga-Tuned Quality**: Optimized quality settings (Quality: 55, Speed: 8) specifically balanced for high-contrast drawings and text.

## 🚀 Getting Started

### Prerequisites (Linux)

To compile and run the application, you need the following system libraries:

```bash
sudo apt-get update && sudo apt-get install -y libgl1-mesa-dev xorg-dev libxxf86vm-dev
```

- [Go](https://golang.org/dl/) (1.18 or later)

### Development

We use [Air](https://github.com/air-verse/air) for live-reloading during development:

```bash
# Install Air
go install github.com/air-verse/air@latest

# Run with live reload
air
```

### Building for Production

To create a standalone executable:

```bash
go build -o image-compressor .
```
```powershell
 go build -ldflags "-H windowsgui" -o image-compressor.exe .
```
## 🛠️ Technology Stack

- **Backend & UI**: Go (Golang) + [Fyne](https://fyne.io/)
- **Live Reload**: Air
- **Image Processing**: 
  - `github.com/gen2brain/avif` (AV1/AVIF)
  - `golang.org/x/image/webp` (WebP)
  - Standard `image` package for JPEG/PNG

## 📜 License

This project is open-source. Feel free to contribute!
