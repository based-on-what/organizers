import os
import logging
from moviepy.editor import VideoFileClip

def setup_logging():
    """Configura el registro de logs para depuración."""
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def obtener_duracion_video(archivo):
    """Obtiene la duración de un archivo de video en segundos."""
    try:
        with VideoFileClip(archivo) as clip:
            return clip.duration
    except Exception as e:
        logging.warning(f"Error al procesar el archivo {archivo}: {e}")
        return 0  # Si hay un error, retornamos 0 para evitar interrupciones.

def obtener_duraciones_por_directorio(directorio, extensiones=None, directorios_omitidos=None):
    """
    Calcula la duración total de los videos agrupados por directorio.

    Args:
        directorio (str): Ruta del directorio base.
        extensiones (set): Extensiones de archivo de video a procesar.
        directorios_omitidos (set): Nombres de subdirectorios a ignorar.

    Returns:
        dict: Diccionario con nombres de directorios y sus duraciones totales en segundos.
    """
    if extensiones is None:
        extensiones = {'.mp4', '.avi', '.mkv', '.mov'}

    if directorios_omitidos is None:
        directorios_omitidos = {"Sub", "Subs", "Subtitles", "Featurettes"}

    duracion_por_directorio = {}

    for root, dirs, files in os.walk(directorio):
        # Filtrar directorios omitidos
        dirs[:] = [d for d in dirs if d not in directorios_omitidos]

        # Calcular duración total para el directorio actual
        duracion_total = 0
        for file in files:
            if any(file.lower().endswith(ext) for ext in extensiones):
                archivo_path = os.path.join(root, file)
                duracion_total += obtener_duracion_video(archivo_path)

        # Asociar la duración total al directorio actual
        duracion_por_directorio[root] = duracion_total

    return duracion_por_directorio

def guardar_duraciones_en_archivo(duraciones, archivo_salida):
    """Guarda las duraciones por directorio en un archivo de texto."""
    with open(archivo_salida, 'w', encoding='utf-8') as f:
        for directorio, duracion in duraciones.items():
            horas = int(duracion // 3600)
            minutos = int((duracion % 3600) // 60)
            segundos = int(duracion % 60)
            f.write(f"{directorio} : {horas}h {minutos}m {segundos}s\n")

if __name__ == "__main__":
    setup_logging()

    directorio_base = os.getcwd()  # Obtiene el directorio de trabajo actual

    # Obtener las duraciones agrupadas por directorio
    logging.info("Procesando archivos de video en los directorios...")
    duraciones = obtener_duraciones_por_directorio(directorio_base)

    # Ordenar las duraciones por valor
    duraciones_ordenadas = dict(sorted(duraciones.items(), key=lambda x: x[1]))

    # Mostrar resultados en consola
    for directorio, duracion in duraciones_ordenadas.items():
        horas = int(duracion // 3600)
        minutos = int((duracion % 3600) // 60)
        segundos = int(duracion % 60)
        logging.info(f"{directorio} : {horas}h {minutos}m {segundos}s")

    # Guardar resultados en un archivo
    archivo_salida = "duraciones_por_directorio.txt"
    guardar_duraciones_en_archivo(duraciones_ordenadas, archivo_salida)
    logging.info(f"Resultados guardados en {archivo_salida}")
