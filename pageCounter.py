import os
import zipfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from ebooklib import epub
from rarfile import RarFile
from pdfminer.high_level import extract_text
from PyPDF2 import PdfReader
from docx import Document

class FileInfo:
    def __init__(self, name, total_pages, png_count, jpg_count):
        self.name = name
        self.total_pages = total_pages
        self.png_count = png_count
        self.jpg_count = jpg_count

    def __str__(self):
        return (f"{self.name}: {self.total_pages} páginas, PNGs: {self.png_count}, "
                f"JPGs: {self.jpg_count}")

def contar_archivos_imagenes(directory):
    """Cuenta la cantidad de imágenes PNG y JPG en un directorio."""
    png_count = sum(1 for f in os.listdir(directory) if f.lower().endswith('.png'))
    jpg_count = sum(1 for f in os.listdir(directory) if f.lower().endswith(('.jpg', '.jpeg')))
    return png_count, jpg_count

def contar_paginas_cbz_cbr(file_path):
    """Cuenta las páginas en archivos CBZ y CBR."""
    try:
        if file_path.lower().endswith('.cbz'):
            with zipfile.ZipFile(file_path, 'r') as archive:
                return len([f for f in archive.namelist() if f.lower().endswith(('.jpg', '.jpeg', '.png', '.webp'))])
        elif file_path.lower().endswith('.cbr'):
            with RarFile(file_path, 'r') as archive:
                return len([f for f in archive.namelist() if f.lower().endswith(('.jpg', '.jpeg', '.png', '.webp'))])
    except Exception:
        pass
    return 0

def contar_paginas_epub(file_path):
    """Cuenta las páginas en archivos EPUB basándose en el número de elementos HTML."""
    try:
        book = epub.read_epub(file_path)
        return len([item for item in book.get_items() if isinstance(item, epub.EpubHtml)])
    except Exception:
        pass
    return 0

def contar_paginas_pdf(file_path):
    """Cuenta las páginas en archivos PDF utilizando PyPDF2."""
    try:
        with open(file_path, 'rb') as f:
            reader = PdfReader(f)
            return len(reader.pages)
    except Exception:
        pass
    return 0

def contar_paginas_docx(file_path):
    """Cuenta las páginas en archivos DOCX utilizando una estrategia específica."""
    try:
        document = Document(file_path)
        page_count = 0
        for para in document.paragraphs:
            if para.text == 'Microsoft Word 2010 - Level 2':
                page_count += 1
        return page_count
    except Exception:
        pass
    return 0

def contar_paginas_archivo(file_path):
    """Cuenta la cantidad de páginas en un archivo de texto compatible."""
    try:
        print(f"Contando páginas del archivo {os.path.basename(file_path)}...")
        if file_path.endswith('.txt'):
            with open(file_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()
                return len(lines) // 30  # Asumimos 30 líneas por página como aproximación
        elif file_path.endswith('.pdf'):
            return contar_paginas_pdf(file_path)
        elif file_path.endswith('.docx'):
            return contar_paginas_docx(file_path)
        elif file_path.endswith('.epub'):
            return contar_paginas_epub(file_path)
        elif file_path.lower().endswith(('.cbz', '.cbr')):
            return contar_paginas_cbz_cbr(file_path)
    except Exception:
        pass
    return 0

def analizar_directorio(directory):
    """Analiza cada archivo en un directorio y sus subdirectorios, generando información detallada."""
    results = []

    with ThreadPoolExecutor(max_workers=min(32, os.cpu_count() + 4)) as executor:
        futures = {}

        for root, _, files in os.walk(directory):
            for file in files:
                file_path = os.path.join(root, file)
                futures[executor.submit(contar_paginas_archivo, file_path)] = file_path

        for future in as_completed(futures):
            file_path = futures[future]
            try:
                total_pages = future.result()
                png_count, jpg_count = contar_archivos_imagenes(os.path.dirname(file_path))
                results.append(FileInfo(os.path.basename(file_path), total_pages, png_count, jpg_count))
            except Exception as e:
                print(f"Error procesando {file_path}: {e}")

    return results

def main():
    """Punto de entrada principal del script."""
    script_directory = os.path.dirname(os.path.abspath(__file__))
    results = analizar_directorio(script_directory)

    for info in results:
        print(info)

    with open("resultados_directorio.txt", "w", encoding="utf-8") as f:
        for info in results:
            f.write(str(info) + "\n")

if __name__ == "__main__":
    main()
