# Organizers

Coleccion de scripts Python para organizar y gestionar archivos segun sus propiedades: duracion de video, formatos de documentos y analisis de contenido multimedia. El proyecto sigue una arquitectura por capas: puntos de entrada CLI delgados que delegan en modulos de analisis respaldados por paquetes de lectura y utilidades base.

## Tabla de Contenidos
- [Descripcion General](#descripcion-general)
- [Funcionalidades](#funcionalidades)
- [Tecnologias](#tecnologias)
- [Requisitos Previos](#requisitos-previos)
- [Instalacion](#instalacion)
- [Uso](#uso)
  - [length.py](#lengthpy)
  - [pageCounter.py](#pagecounterpy)
  - [steamSorter.py](#steamsorterpy)
  - [seriesLength.py](#serieslengthpy)
  - [comanga.py](#comangapy)
  - [doc2docx.py](#doc2docxpy)
- [Estructura del Proyecto](#estructura-del-proyecto)
- [Configuracion](#configuracion)
- [Solucion de Problemas](#solucion-de-problemas)
- [Contribucion](#contribucion)
- [Licencia](#licencia)
- [Creditos](#creditos)

## Descripcion General

Seis scripts CLI que ayudan a organizar distintos tipos de archivos:

- **Videos** por duracion
- **Libros y documentos** por cantidad de paginas (PDF, EPUB, DOCX)
- **Juegos de Steam** por tiempo de completado segun HowLongToBeat
- **Series de TV** por duracion total, agrupadas por subcarpeta
- **Comics y manga** por cantidad de paginas (CBZ, CBR, EPUB, PDF)
- **Documentos Word legacy** convertidos al formato DOCX moderno

## Funcionalidades

- **Analisis de video**: duracion para MP4, AVI, MKV, MOV, WMV, FLV, WEBM
- **Procesamiento de documentos**: conteo de paginas para PDF, EPUB, DOCX
- **Analisis de biblioteca Steam**: integracion con Steam API y HowLongToBeat
- **Organizacion de series**: duracion total por carpeta de serie
- **Gestion de comics/manga**: conteo de paginas para CBZ, CBR, EPUB, PDF
- **Conversion de documentos**: DOC a DOCX con Word COM (Windows) o LibreOffice como alternativa
- **Reporte de progreso**: barras de progreso en stderr para no contaminar stdout al usar pipes
- **Arquitectura por capas**: capas CLI, analizador, lector y core con responsabilidades claras
- **Dependencias diferidas**: las librerias pesadas se cargan la primera vez que se usan; si falta alguna, el error indica el comando de instalacion
- **Multiplataforma**: Windows, macOS, Linux

## Tecnologias

### Dependencias Principales
- **Python 3.8+**
- **pypdf 3.0+** — lectura de PDF (usa PyPDF2 como alternativa si pypdf no esta instalado)
- **python-docx 0.8.11+** — lectura de DOCX y estimacion de paginas
- **ebooklib 0.18+** — procesamiento de EPUB
- **ffprobe** (parte de ffmpeg) — lectura rapida de duracion de video; **moviepy 1.0.3+** se usa como alternativa cuando ffprobe no esta en el PATH
- **rarfile 4.0+** — lectura de archivos CBR

### Integracion con APIs
- **requests 2.28+** — llamadas a la Steam Web API
- **howlongtobeatpy 1.0+** — datos de HowLongToBeat

### Especificas por Plataforma
- **pywin32 305+** (solo Windows) — interfaz COM de Microsoft Word para conversion DOC
- **LibreOffice** (Linux/macOS) — alternativa de conversion DOC

## Requisitos Previos

- **Python 3.8 o superior**
- **pip**

### Opcionales
- **ffmpeg** (provee `ffprobe`) para el analisis rapido de duracion de video — muy recomendado; sin el se usa moviepy, que es aproximadamente 10x mas lento
- Clave de Steam Web API para `steamSorter.py` — obtenerla en el [Portal de Desarrolladores de Steam](https://steamcommunity.com/dev/apikey)
- Microsoft Word o LibreOffice para `doc2docx.py`
- Herramientas RAR (`unrar`) para archivos CBR

## Instalacion

```bash
git clone https://github.com/based-on-what/organizers.git
cd organizers
pip install -r requirements.txt
```

### Paquetes del sistema

#### Linux/Ubuntu
```bash
sudo apt-get install unrar-free libreoffice
```

#### macOS
```bash
brew install unrar libreoffice
```

#### Windows
Instalar Microsoft Word o LibreOffice. `pywin32` esta incluido en `requirements.txt` y se instala automaticamente en Windows.

## Uso

Todas las herramientas comparten los mismos flags base: un argumento posicional
opcional `directory` (por defecto: directorio actual), `-o/--output`,
`-f/--format txt|json` y `-l/--log-level`. Los resultados se imprimen en stdout
(compatible con pipes); los diagnosticos y el progreso van a stderr.

Instalar el paquete (`pip install -e .`) provee un unico comando `organizers`
con un subcomando por herramienta:

```bash
organizers videos /ruta/a/peliculas -o reporte.txt
organizers series          # seriesLength.py
organizers pages           # pageCounter.py
organizers comics          # comanga.py
organizers steam           # steamSorter.py
organizers doc2docx        # doc2docx.py
```

Las invocaciones independientes `python <script>.py` de abajo siguen funcionando sin cambios.

### length.py

Analiza las duraciones de archivos de video en un arbol de directorios.

```bash
# Directorio actual
python length.py

# Directorio especifico con archivo de salida personalizado
python length.py /ruta/a/videos -o mi_analisis.txt

# Salida en JSON
python length.py -f json -o analisis.json

# Extensiones personalizadas y subdirectorios a excluir
python length.py -e .mp4 .mkv .avi -x Subtitles Extras

# Logging detallado
python length.py -l DEBUG
```

**Argumentos:**

| Flag | Por defecto | Descripcion |
|------|-------------|-------------|
| `directory` | `.` | Directorio a analizar |
| `-o` | `video_duration_analysis.txt` | Ruta del archivo de salida |
| `-f` | `txt` | Formato de salida: `txt` o `json` |
| `-e` | `.mp4 .avi .mkv .mov .wmv .flv .webm` | Extensiones a incluir |
| `-x` | `Sub Subs Subtitles Featurettes Extras` | Nombres de subcarpetas a omitir |
| `-l` | `INFO` | Nivel de log: `DEBUG INFO WARNING ERROR` |

Archivo de salida: `video_duration_analysis.txt` (o la ruta indicada con `-o`).
Archivo de log: `video_analyzer.log`.

### pageCounter.py

Cuenta las paginas de documentos en un directorio (no recursivo).

```bash
python pageCounter.py [directorio] [-o salida.txt] [-f json]
```

**Formatos soportados:** PDF, EPUB, DOCX.

El conteo de paginas DOCX es una estimacion: se cuentan los saltos de pagina expliciticos y, si no hay ninguno, se divide la cantidad de caracteres entre 2000.

Archivo de salida: `document_page_counts.txt`.

### steamSorter.py

Obtiene la biblioteca de Steam y consulta los tiempos de completado de la historia principal en HowLongToBeat.

**Configuracion — establecer variables de entorno antes de ejecutar:**

```bash
# Linux/macOS
export STEAM_API_KEY="tu_clave_api_aqui"
export STEAM_IDS="76561197960287930,76561197960287931"

# Windows (PowerShell)
$env:STEAM_API_KEY = "tu_clave_api_aqui"
$env:STEAM_IDS    = "76561197960287930,76561197960287931"
```

```bash
python steamSorter.py
```

`STEAM_IDS` acepta uno o mas IDs de usuario de Steam de 64 bits separados por comas. Los juegos duplicados entre bibliotecas se eliminan. Las consultas a HLTB estan limitadas a una por segundo.

Los resultados de HLTB se guardan en una cache de disco por 90 dias (`%LOCALAPPDATA%\organizers\hltb_cache.json` en Windows, `~/.cache/organizers/hltb_cache.json` en otros sistemas), por lo que las ejecuciones repetidas terminan en segundos y una ejecucion interrumpida se reanuda donde quedo.

Archivo de salida: `steam_games_completion_times.txt`.

### seriesLength.py

Calcula la duracion total de video para cada subcarpeta, tratando cada subcarpeta como una serie de TV diferente.

```bash
python seriesLength.py [directorio] [-o salida.txt] [-f json]
```

Archivo de salida: `series_durations.txt`.

### comanga.py

Cuenta paginas en archivos y directorios de comics/manga.

```bash
# Directorio actual
python comanga.py

# Directorio especifico
python comanga.py /ruta/a/comics
```

**Formatos soportados:** CBZ, CBR, EPUB, PDF.

Cada hijo inmediato del directorio objetivo es analizado: los archivos se cuentan directamente, los subdirectorios se escanean de forma recursiva (tratados como series). El procesamiento esta paralelizado con un pool de hilos (hasta 8 workers).

Archivo de salida: `comanga_page_counts.txt`.

### doc2docx.py

Convierte todos los archivos `.doc` de un directorio a `.docx`.

```bash
python doc2docx.py [directorio] [-o directorio_salida] [--no-skip-existing]
```

Los archivos convertidos se guardan en `<directorio>/output/` (o el directorio indicado con `-o`). Los archivos `.doc` originales no se modifican. Los archivos cuyo `.docx` ya existe en el directorio de salida se omiten; usar `--no-skip-existing` para reconvertirlos.

**Backends de conversion (en orden de preferencia):**
1. Microsoft Word via COM (Windows, requiere pywin32 + Word instalado)
2. LibreOffice en modo headless (todas las plataformas)

## Estructura del Proyecto

```
organizers/
├── pyproject.toml           # empaquetado, config de ruff y pytest; entry point `organizers`
├── requirements.txt
├── shared_utils.py          # shim OBSOLETO — emite DeprecationWarning, se elimina en la proxima version menor
├── organizers_cli.py        # comando `organizers`: un subcomando por herramienta
├── length.py                # CLI: analizador de duracion de video
├── seriesLength.py          # CLI: analizador de duracion de series TV
├── pageCounter.py           # CLI: contador de paginas de documentos
├── comanga.py               # CLI: contador de paginas de comics/manga
├── doc2docx.py              # CLI: convertidor DOC a DOCX
├── steamSorter.py           # CLI: analizador de completado de juegos Steam
├── core/
│   ├── cli.py               # contrato argparse compartido (directorio, -o, -f, -l)
│   ├── formatters.py        # helpers de formato puro (duracion, tamanio de archivo)
│   ├── fs.py                # busqueda de archivos en streaming y verificacion de acceso
│   ├── loaders.py           # registro de imports diferidos para dependencias opcionales
│   ├── log.py               # configuracion de logging (diagnosticos a stderr)
│   └── output.py            # ProgressReporter y serializador de resultados txt/json
├── readers/
│   ├── pages.py             # lectores puros de conteo de paginas (PDF, EPUB, CBZ, CBR, DOCX)
│   └── video.py             # lector puro de duracion de video (ffprobe, moviepy de respaldo)
├── analyzers/
│   ├── comics.py            # escaner de directorio de comics con pool de hilos
│   ├── documents.py         # escaner de directorio de documentos
│   ├── steam.py             # SteamClient, HltbClient, HltbCache, analyze_libraries()
│   └── video.py             # analyze_flat() y analyze_series(), pool de hilos
├── converters/
│   └── doc2docx.py          # backends de conversion DOC-a-DOCX y orquestacion
├── tests/                   # suite pytest (sin red, sin codecs)
├── organizers_c/            # ports C nativos de Windows CONGELADOS (MSVC) — solo referencia
└── organizers_posix/        # ports C de Linux/macOS CONGELADOS (gcc/clang) — solo referencia
```

### Responsabilidades por capa

| Capa | Responsabilidad |
|------|----------------|
| Puntos de entrada CLI | Parseo de argumentos, configuracion de logging, visualizacion, escritura de archivos de salida |
| `analyzers/` | Escaneo de directorios, orquestacion, reporte de progreso |
| `readers/` | Lectura de un archivo individual y retorno de datos crudos |
| `core/` | Formato, utilidades de sistema de archivos, loaders diferidos, logging, helpers de salida |

`shared_utils.py` es un shim de compatibilidad retroactiva obsoleto que re-exporta simbolos de `core/` y `readers/`. Emite un `DeprecationWarning` al importarse y se eliminara en la proxima version menor — importar directamente desde esos paquetes.

### Ports en C (congelados)

`organizers_c/` (Windows, compilado con Visual Studio Build Tools / MSVC `cl`) y `organizers_posix/` (Linux/macOS, gcc/clang) contienen reescrituras en C de las seis herramientas. **Ambos arboles estan congelados y sin mantenimiento**: se conservan como implementaciones de referencia, no reciben nuevas funcionalidades y pueden divergir del comportamiento de las versiones Python. La CI compila ambos arboles en cada push para que no se degraden en silencio. Ver el README de cada carpeta para las instrucciones de compilacion; los binarios compilados no se incluyen en el repositorio.

## Configuracion

### Variables de Entorno

| Variable | Usada por | Descripcion |
|----------|-----------|-------------|
| `STEAM_API_KEY` | `steamSorter.py` | Clave de la Steam Web API (requerida) |
| `STEAM_IDS` | `steamSorter.py` | IDs de usuario Steam de 64 bits separados por comas (requeridos) |

### Logging

Todos los scripts usan el logger `organizers` y aceptan `-l/--log-level` (`DEBUG INFO WARNING ERROR`, por defecto `INFO`). Los diagnosticos van a stderr; los resultados se imprimen en stdout, por lo que la salida sigue siendo compatible con pipes en cualquier nivel de log. `length.py` ademas escribe en `video_analyzer.log`.

## Solucion de Problemas

### Errores de importacion

```bash
# Dependencias principales
pip install pypdf python-docx ebooklib moviepy rarfile

# Herramientas de API
pip install requests howlongtobeatpy

# Conversion DOC en Windows
pip install pywin32
```

### Procesamiento de video

- Los archivos menores a 100 KB se omiten (trailers, miniaturas).
- Los archivos corruptos o con codecs no soportados se registran en el log y se omiten.

### Procesamiento de documentos

- Los PDF cifrados no pueden leerse y se omiten.
- El conteo de paginas DOCX es una estimacion; los resultados pueden diferir del conteo de Word.
- Los archivos CBR requieren `unrar` o `unrar-free` instalado en el sistema.

### Steam

- `STEAM_API_KEY` y `STEAM_IDS` deben estar configurados antes de ejecutar.
- Pueden faltar datos de HLTB para juegos de nicho o muy recientes.
- El script esta limitado a 1 solicitud HLTB por segundo para evitar bloqueos.

### Obtener ayuda

1. Revisar la salida del log para mensajes de error por archivo.
2. Ejecutar `length.py` con `-l DEBUG` para logs detallados de procesamiento de video.
3. Verificar que todas las dependencias esten instaladas con `pip list`.

## Contribucion

1. Hacer fork del repositorio
2. Crear una nueva rama
3. Realizar los cambios
4. Enviar un pull request

Reportar problemas en la seccion de Issues.

## Licencia

Este proyecto esta licenciado bajo la Licencia MIT — ver [LICENSE](LICENSE).

## Creditos

- @based-on-what — Desarrollador principal
