import os
from moviepy.editor import VideoFileClip

def obtener_duracion_video(archivo):
   with VideoFileClip(archivo) as clip:
       return clip.duration

def obtener_duracion_por_subdirectorios(ruta_base, directorios_omitidos=None):
   if directorios_omitidos is None:
       directorios_omitidos = {"Sub", "Subs", "Subtitles", "Featurettes"}

   duracion_por_subdirectorios = {}
   for root, dirs, files in os.walk(ruta_base):
       # Filtrar subdirectorios omitidos
       dirs[:] = [d for d in dirs if d not in directorios_omitidos]

       for file in files:
           if file.endswith(('.mp4', '.avi', '.mkv', '.mov')): # Asegúrate de incluir todas las extensiones de video que necesitas
               archivo_path = os.path.join(root, file)
               duracion = obtener_duracion_video(archivo_path)
               duracion_por_subdirectorios[file] = duracion

   return duracion_por_subdirectorios

def guardar_diccionario_en_archivo(diccionario, archivo):
   with open(archivo, 'w', encoding='utf-8') as f:
       for subdirectorio, duracion in sorted(diccionario.items(), key=lambda x: x[1]):
           horas = int(duracion // 3600)
           minutos = int((duracion % 3600) // 60)
           segundos = int(duracion % 60)
           f.write(f"Nombre: {subdirectorio}\n")
           f.write(f"Duración total: {horas} horas, {minutos} minutos, {segundos} segundos\n\n")

if __name__ == "__main__":
   directorio_base = os.getcwd() # Obtiene el directorio de trabajo actual
   duracion_por_subdirectorios = obtener_duracion_por_subdirectorios(directorio_base)
   
   archivo_salida = "duracion_por_subdirectorios.txt" # Nombre del archivo de salida
   guardar_diccionario_en_archivo(duracion_por_subdirectorios, archivo_salida)
   
   print(f"El diccionario ordenado se ha guardado en '{archivo_salida}'.")
