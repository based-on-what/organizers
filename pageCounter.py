import os
from PyPDF2 import PdfReader
import ebooklib
from ebooklib import epub
from mobi import Mobi
from docx import Document

def contar_paginas_epub(ruta_archivo):
    """
    Cuenta las páginas aproximadas de un archivo EPUB
    """
    try:
        book = epub.read_epub(ruta_archivo)
        # Contamos los documentos HTML como páginas
        paginas = len(list(book.get_items_of_type(ebooklib.ITEM_DOCUMENT)))
        return paginas
    except Exception as e:
        raise Exception(f"Error al procesar EPUB: {str(e)}")

def contar_paginas_mobi(ruta_archivo):
    """
    Cuenta las páginas aproximadas de un archivo MOBI
    """
    try:
        book = Mobi(ruta_archivo)
        book.parse()
        # Estimamos páginas basándonos en el número de caracteres (aprox. 2000 por página)
        caracteres_por_pagina = 2000
        paginas = max(1, len(book.get_book_content()) // caracteres_por_pagina)
        return paginas
    except Exception as e:
        raise Exception(f"Error al procesar MOBI: {str(e)}")

def contar_paginas_word(ruta_archivo):
    """
    Cuenta las páginas de un archivo DOC/DOCX usando los saltos de página
    """
    try:
        doc = Document(ruta_archivo)
        # Contar los saltos de página y añadir 1 para la primera página
        paginas = sum(p.runs[-1].element.xpath("./w:br[@w:type='page']") for p in doc.paragraphs if p.runs) + 1
        return paginas
    except Exception as e:
        raise Exception(f"Error al procesar archivo Word: {str(e)}")

def contar_paginas_archivo(ruta_archivo):
    """
    Cuenta las páginas de un archivo según su extensión
    """
    extension = os.path.splitext(ruta_archivo)[1].lower()
    
    if extension == '.pdf':
        reader = PdfReader(ruta_archivo, strict=False)
        return len(reader.pages)
    elif extension == '.epub':
        return contar_paginas_epub(ruta_archivo)
    elif extension == '.mobi':
        return contar_paginas_mobi(ruta_archivo)
    elif extension in ['.docx']:
        return contar_paginas_word(ruta_archivo)
    else:
        raise Exception(f"Formato de archivo no soportado: {extension}")

def contar_paginas_archivos(directorio):
    """
    Cuenta las páginas de los archivos PDF, EPUB, MOBI y DOC/DOCX en el directorio especificado.
    """
    archivos_paginas = []
    archivos_error = []

    # Listar todos los archivos con extensiones soportadas
    extensiones = ('.pdf', '.epub', '.mobi', '.doc', '.docx')
    archivos = [archivo for archivo in os.listdir(directorio) 
                if archivo.lower().endswith(extensiones)]
    total_archivos = len(archivos)
    
    print(f"\nIniciando procesamiento de {total_archivos} archivos...\n")

    for indice, archivo in enumerate(archivos, 1):
        print(f"Procesando archivo {indice}/{total_archivos}: {archivo}...", end=" ")
        ruta_archivo = os.path.join(directorio, archivo)
        try:
            # Verificar si el archivo está vacío
            if os.path.getsize(ruta_archivo) == 0:
                print("ERROR: Archivo vacío")
                archivos_error.append((archivo, "Archivo vacío"))
                continue

            # Contar páginas según el tipo de archivo
            num_paginas = contar_paginas_archivo(ruta_archivo)
            archivos_paginas.append((archivo, num_paginas))
            print(f"COMPLETADO ({num_paginas} páginas)")

        except Exception as e:
            print(f"ERROR: {str(e)}")
            archivos_error.append((archivo, str(e)))

    print("\nProcesamiento de archivos completado!")

    # Imprimir resumen de errores
    if archivos_error:
        print("\nResumen de archivos con errores:")
        for archivo, error in archivos_error:
            print(f"Error en {archivo}:")
            print(f"  - Tipo de error: {error}")
            print(f"  - Ruta completa: {os.path.join(directorio, archivo)}")
            print()

    return archivos_paginas

def ordenar_y_guardar(archivos_paginas, archivo_salida):
    """
    Ordena los archivos por número de páginas, imprime el resultado y lo guarda en un archivo.
    """
    # Ordenar los archivos por número de páginas
    archivos_ordenados = sorted(archivos_paginas, key=lambda x: x[1])

    # Mostrar en consola
    print("Archivos ordenados por número de páginas:")
    for nombre, paginas in archivos_ordenados:
        print(f"{nombre}: {paginas} páginas")

    # Guardar en el archivo de salida usando UTF-8
    with open(archivo_salida, 'w', encoding='utf-8') as f:
        f.write("Archivos ordenados por número de páginas:\n")
        for nombre, paginas in archivos_ordenados:
            f.write(f"{nombre}: {paginas} páginas\n")

if __name__ == "__main__":
    # Directorio actual
    directorio_actual = os.getcwd()

    # Contar páginas de los archivos
    archivos_paginas = contar_paginas_archivos(directorio_actual)

    # Ordenar y guardar el resultado
    archivo_salida = "resultado.txt"
    ordenar_y_guardar(archivos_paginas, archivo_salida)

    print(f"\nEl resultado se ha guardado en el archivo '{archivo_salida}'.")
