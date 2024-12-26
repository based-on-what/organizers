import os
import zipfile
from ebooklib import epub
from PyPDF2 import PdfReader
from rarfile import RarFile

def contar_paginas_cbz(cbz_file):
    """Cuenta el número de páginas (imágenes) en un archivo .cbz."""
    try:
        with zipfile.ZipFile(cbz_file, 'r') as archive:
            return sum(1 for file_name in archive.namelist() if file_name.lower().endswith(('.jpg', '.jpeg', '.png')))
    except Exception as e:
        print(f"Error al leer {cbz_file}: {e}")
        return 0

def contar_paginas_cbr(cbr_file):
    """Cuenta el número de páginas (imágenes) en un archivo .cbr."""
    try:
        with RarFile(cbr_file, 'r') as archive:
            return sum(1 for file_name in archive.namelist() if file_name.lower().endswith(('.jpg', '.jpeg', '.png')))
    except Exception as e:
        print(f"Error al leer {cbr_file}: {e}")
        return 0

def contar_paginas_epub(epub_file):
    """Cuenta el número de páginas (capítulos HTML) en un archivo .epub."""
    try:
        book = epub.read_epub(epub_file)
        return sum(1 for item in book.get_items() if isinstance(item, epub.EpubHtml))
    except Exception as e:
        print(f"Error al leer {epub_file}: {e}")
        return 0

def contar_paginas_pdf(pdf_file):
    """Cuenta el número de páginas en un archivo .pdf."""
    try:
        reader = PdfReader(pdf_file)
        return len(reader.pages)
    except Exception as e:
        print(f"Error al leer {pdf_file}: {e}")
        return 0

def contar_paginas_directorio(main_directory):
    """Cuenta las páginas en archivos .cbz, .cbr, .epub y .pdf dentro de un directorio y sus subdirectorios."""
    resultados = {}

    # Iterar sobre los elementos en el directorio principal
    for item in os.listdir(main_directory):
        item_path = os.path.join(main_directory, item)

        if os.path.isdir(item_path):
            # Si es un subdirectorio, contar las páginas en los archivos soportados dentro
            total_paginas = 0
            for file in os.listdir(item_path):
                file_path = os.path.join(item_path, file)
                if file.endswith('.cbz'):
                    total_paginas += contar_paginas_cbz(file_path)
                elif file.endswith('.cbr'):
                    total_paginas += contar_paginas_cbr(file_path)
                elif file.endswith('.epub'):
                    total_paginas += contar_paginas_epub(file_path)
                elif file.endswith('.pdf'):
                    total_paginas += contar_paginas_pdf(file_path)
            resultados[item] = total_paginas
            print(f"Subdirectorio analizado: {item}, Páginas: {total_paginas}")
        elif item.endswith(('.cbz', '.cbr', '.epub', '.pdf')):
            # Si es un archivo soportado suelto, contarlo individualmente
            if item.endswith('.cbz'):
                paginas = contar_paginas_cbz(item_path)
            elif item.endswith('.cbr'):
                paginas = contar_paginas_cbr(item_path)
            elif item.endswith('.epub'):
                paginas = contar_paginas_epub(item_path)
            elif item.endswith('.pdf'):
                paginas = contar_paginas_pdf(item_path)
            resultados[item] = paginas
            print(f"Archivo analizado: {item}, Páginas: {paginas}")

    # Ordenar resultados por número de páginas (de menor a mayor)
    resultados_ordenados = sorted(resultados.items(), key=lambda x: x[1])

    # Mostrar resultados
    for nombre, paginas in resultados_ordenados:
        print(f"{nombre}: {paginas} páginas")

    # Guardar resultados en un archivo de texto
    with open("resultados_paginas.txt", "w", encoding="utf-8") as txt_file:
        for nombre, paginas in resultados_ordenados:
            txt_file.write(f"{nombre}: {paginas} páginas\n")

def main():
    # Obtener la ruta del directorio actual del script
    script_directory = os.path.dirname(os.path.abspath(__file__))
    contar_paginas_directorio(script_directory)

if __name__ == "__main__":
    main()
