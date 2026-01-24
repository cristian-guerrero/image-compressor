#!/usr/bin/env python3
"""
Script para generar iconos para ImageCompressor
Genera iconos de 256x256 y 512x512 píxeles
"""

from PIL import Image, ImageDraw, ImageFont
import os

def create_icon(size):
    """
    Crea un icono de compresión de imágenes
    
    Args:
        size: Tamaño del icono en píxeles (cuadrado)
    
    Returns:
        PIL.Image: El icono generado
    """
    # Crear imagen con fondo degradado azul
    img = Image.new('RGB', (size, size), color=(41, 128, 185))
    draw = ImageDraw.Draw(img)
    
    # Crear degradado de fondo
    for y in range(size):
        r = int(41 + (52 - 41) * y / size)
        g = int(128 + (152 - 128) * y / size)
        b = int(185 + (219 - 185) * y / size)
        draw.rectangle([(0, y), (size, y + 1)], fill=(r, g, b))
    
    # Calcular posiciones relativas al tamaño
    margin = size // 10
    center = size // 2
    icon_size = size // 3
    
    # Dibujar documento/papel (representando una imagen)
    doc_x = center - icon_size // 2
    doc_y = center - icon_size // 2 - margin // 2
    doc_width = icon_size
    doc_height = int(icon_size * 1.3)
    
    # Sombra del documento
    shadow_offset = size // 40
    draw.rectangle(
        [(doc_x + shadow_offset, doc_y + shadow_offset),
         (doc_x + doc_width + shadow_offset, doc_y + doc_height + shadow_offset)],
        fill=(30, 80, 120)
    )
    
    # Documento principal
    draw.rectangle(
        [(doc_x, doc_y), (doc_x + doc_width, doc_y + doc_height)],
        fill=(255, 255, 255),
        outline=(200, 200, 200),
        width=size // 100
    )
    
    # Líneas horizontales dentro del documento (representando contenido de imagen)
    line_height = size // 40
    line_spacing = size // 15
    start_y = doc_y + size // 10
    for i in range(4):
        line_y = start_y + i * line_spacing
        draw.rectangle(
            [(doc_x + size // 20, line_y),
             (doc_x + doc_width - size // 20, line_y + line_height)],
            fill=(220, 220, 220)
        )
    
    # Dibujar flechas de compresión (apuntando hacia el documento)
    arrow_size = size // 8
    
    # Flecha izquierda
    arrow_left_x = doc_x - arrow_size - margin // 2
    arrow_left_y = center
    draw.polygon([
        (arrow_left_x + arrow_size, arrow_left_y - arrow_size // 2),
        (arrow_left_x + arrow_size, arrow_left_y + arrow_size // 2),
        (arrow_left_x, arrow_left_y)
    ], fill=(231, 76, 60))
    
    # Flecha derecha
    arrow_right_x = doc_x + doc_width + margin // 2
    arrow_right_y = center
    draw.polygon([
        (arrow_right_x, arrow_right_y - arrow_size // 2),
        (arrow_right_x + arrow_size, arrow_right_y),
        (arrow_right_x, arrow_right_y + arrow_size // 2)
    ], fill=(231, 76, 60))
    
    # Flechas superiores e inferiores
    arrow_top_x = center
    arrow_top_y = doc_y - arrow_size - margin // 2
    draw.polygon([
        (arrow_top_x - arrow_size // 2, arrow_top_y + arrow_size),
        (arrow_top_x + arrow_size // 2, arrow_top_y + arrow_size),
        (arrow_top_x, arrow_top_y)
    ], fill=(231, 76, 60))
    
    arrow_bottom_x = center
    arrow_bottom_y = doc_y + doc_height + margin // 2
    draw.polygon([
        (arrow_bottom_x - arrow_size // 2, arrow_bottom_y),
        (arrow_bottom_x + arrow_size // 2, arrow_bottom_y),
        (arrow_bottom_x, arrow_bottom_y + arrow_size)
    ], fill=(231, 76, 60))
    
    # Texto "IMG" en el documento
    try:
        # Intentar usar fuente del sistema
        font_size = size // 6
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", font_size)
    except:
        # Usar fuente por defecto si no encuentra la del sistema
        font = ImageFont.load_default()
    
    text = "IMG"
    # Calcular posición del texto
    bbox = draw.textbbox((0, 0), text, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    text_x = doc_x + (doc_width - text_width) // 2
    text_y = doc_y + doc_height - size // 5
    
    draw.text((text_x, text_y), text, fill=(100, 100, 100), font=font)
    
    return img

def main():
    """Función principal para generar los iconos"""
    resources_dir = "resources"
    
    # Crear directorio resources si no existe
    if not os.path.exists(resources_dir):
        os.makedirs(resources_dir)
        print(f"Directorio '{resources_dir}' creado")
    
    # Generar icono de 256x256
    print("Generando icono de 256x256...")
    icon_256 = create_icon(256)
    icon_256_path = os.path.join(resources_dir, "icon.png")
    icon_256.save(icon_256_path, "PNG")
    print(f"✓ Icono guardado en: {icon_256_path}")
    
    # Generar icono de 512x512
    print("Generando icono de 512x512...")
    icon_512 = create_icon(512)
    icon_512_path = os.path.join(resources_dir, "icon512.png")
    icon_512.save(icon_512_path, "PNG")
    print(f"✓ Icono guardado en: {icon_512_path}")
    
    print("\n¡Iconos generados exitosamente!")
    print(f"  - {icon_256_path} (256x256)")
    print(f"  - {icon_512_path} (512x512)")

if __name__ == "__main__":
    main()
