import os
import win32com.client as win32
import pywintypes

# Carpeta actual donde se ejecuta el programa
input_folder = os.getcwd()

# Crear una carpeta "output" dentro de la carpeta actual
output_folder = os.path.join(input_folder, "output")
if not os.path.exists(output_folder):
    os.makedirs(output_folder)

# Inicializar Microsoft Word
word = win32.Dispatch("Word.Application")
word.Visible = False

# Procesar los archivos .doc en la carpeta actual
for filename in os.listdir(input_folder):
    if filename.endswith(".doc") and not filename.endswith(".docx"):
        input_path = os.path.join(input_folder, filename)
        output_path = os.path.join(output_folder, os.path.splitext(filename)[0] + ".docx")
        
        try:
            # Abrir y guardar como .docx
            doc = word.Documents.Open(input_path)
            doc.SaveAs(output_path, FileFormat=16)  # 16 = wdFormatXMLDocument
            doc.Close()
            print(f"Convertido exitosamente: {filename}")
        except pywintypes.com_error as e:
            print(f"No se pudo procesar el archivo {filename}: {str(e)}")
            continue
        except Exception as e:
            print(f"Error inesperado con {filename}: {str(e)}")
            continue

# Cerrar Microsoft Word
word.Quit()

print(f"\nArchivos convertidos guardados en: {output_folder}")
