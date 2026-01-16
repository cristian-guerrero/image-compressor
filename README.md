# Manga Optimizer ⛩️

Manga Optimizer is a high-performance desktop application designed to bridge the gap between automatic translation tools and efficient storage. It specializes in converting "bloated" images from translators into ultra-optimized **AVIF** files without losing visual fidelity.

![Manga Optimizer](https://raw.githubusercontent.com/cristian-guerrero/image-compressor/main/build/appicon.png)

## ✨ Features

- **Smart AVIF Compression**: Uses the state-of-the-art AV1 codec for maximum space saving.
- **Smart Decision Engine**: Automatically compares file sizes. If the optimized version isn't at least 15% smaller, the original file is preserved to ensure no quality is wasted.
- **Batch Processing**: Processes images in parallel (2 at a time) and manages a sequential folder queue.
- **Manga-Tuned Quality**: Optimized quality settings (Quality: 55, Speed: 8) specifically balanced for high-contrast drawings and text.
- **Modern UI**: A premium, responsive dashboard built with React and glassmorphism aesthetics.
- **Resource Efficient**: Implements aggressive memory management and event throttling to keep your system smooth during heavy tasks.

## 🚀 Getting Started

### Prerequisites

- [Go](https://golang.org/dl/) (1.18 or later)
- [Wails CLI](https://wails.io/docs/gettingstarted/installation)
- [Node.js & npm](https://nodejs.org/en/download/)

### Development

To run the application in development mode with live-reloading:

```bash
wails dev
```

### Building for Production

To create a standalone executable for your operating system:

```bash
wails build
```

The output will be located in the `build/bin/` directory.

## 🛠️ Technology Stack

- **Backend**: Go (Golang)
- **Frontend**: React.js
- **Bridge**: Wails v2
- **Image Processing**: 
  - `github.com/gen2brain/avif` (AV1/AVIF)
  - `golang.org/x/image/webp` (WebP)
  - Standard `image` package for JPEG/PNG

## 📜 License

This project is open-source. Feel free to contribute!
