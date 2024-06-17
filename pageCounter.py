import os
import zipfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from ebooklib import epub
from rarfile import RarFile
from pdfminer.high_level import extract_text
from PIL import Image
import io

class DirectoryInfo:
    def __init__(self, name, total_pages, png_count, jpg_count):
        self.name = name
        self.total_pages = total_pages
        self.png_count = png_count
        self.jpg_count = jpg_count

    def __str__(self):
        return f"{self.name}: {self.total_pages}"

def contar_archivos_imagenes(directory):
    png_count = 0
    jpg_count = 0
    for file in os.listdir(directory):
        if file.endswith('.png'):
            try:
                img = Image.open(os.path.join(directory, file))
                img.close()
                png_count += 1
            except:
                pass
        elif file.endswith('.jpg') or file.endswith('.jpeg'):
            try:
                img = Image.open(os.path.join(directory, file))
                img.close()
                jpg_count += 1
            except:
                pass
    return png_count, jpg_count

def contar_paginas_cbz_cbr(cbz_cbr_file):
    try:
        page_count = 0
        if cbz_cbr_file.endswith('.cbz'):
            with zipfile.ZipFile(cbz_cbr_file, 'r') as archive:
                page_count = len([f for f in archive.namelist() if any(f.endswith(ext) for ext in ('.jpg', '.jpeg', '.png', '.webp'))])
        elif cbz_cbr_file.endswith('.cbr'):
            with RarFile(cbz_cbr_file, 'r') as archive:
                page_count = len([f for f in archive.namelist() if any(f.endswith(ext) for ext in ('.jpg', '.jpeg', '.png', '.webp'))])
        return page_count
    except Exception:
        return 0

def contar_paginas_epub(epub_file):
    try:
        book = epub.read_epub(epub_file)
        return len([item for item in book.get_items() if isinstance(item, epub.EpubHtml)])
    except Exception:
        return 0

def contar_paginas_pdf(pdf_file):
    try:
        return len(list(extract_text(pdf_file).splitlines()))
    except Exception:
        return 0

def contar_paginas_subdirectorios(main_directory):
    subdirectories = [d for d in os.listdir(main_directory) if os.path.isdir(os.path.join(main_directory, d))]
    directory_info = {}

    with ThreadPoolExecutor() as executor:
        futures = [executor.submit(process_subdirectory, os.path.join(main_directory, subdirectory)) for subdirectory in subdirectories]
        for future in as_completed(futures):
            subdirectory, info = future.result()
            directory_info[subdirectory] = info

    sorted_dirs = sorted(directory_info.items(), key=lambda x: x[1].total_pages, reverse=True)
    for subdirectory, info in sorted_dirs:
        print(info)

    with open("diccionario_paginas.txt", "w", encoding="utf-8") as txt_file:
        for subdirectory, info in sorted_dirs:
            txt_file.write(str(info) + "\n")

def process_subdirectory(subdirectory_path):
    files = os.listdir(subdirectory_path)
    total_page_count = 0
    png_count, jpg_count = contar_archivos_imagenes(subdirectory_path)

    with ThreadPoolExecutor() as executor:
        futures = []
        for file in files:
            file_path = os.path.join(subdirectory_path, file)
            if file.endswith('.cbz'):
                futures.append(executor.submit(contar_paginas_cbz_cbr, file_path))
            elif file.endswith('.cbr'):
                futures.append(executor.submit(contar_paginas_cbz_cbr, file_path))
            elif file.endswith('.epub'):
                futures.append(executor.submit(contar_paginas_epub, file_path))
            elif file.endswith('.pdf'):
                futures.append(executor.submit(contar_paginas_pdf, file_path))

        for future in as_completed(futures):
            total_page_count += future.result()

    return os.path.basename(subdirectory_path), DirectoryInfo(os.path.basename(subdirectory_path), total_page_count, png_count, jpg_count)

def main():
    script_directory = os.path.dirname(os.path.abspath(__file__))
    contar_paginas_subdirectorios(script_directory)

if __name__ == "__main__":
    main()