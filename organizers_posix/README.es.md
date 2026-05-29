# organizers_posix — Traducciones C Nativas para Linux y macOS

Esta carpeta contiene reescrituras en C (C11) compatibles con POSIX de todos
los scripts Python del repositorio `organizers/`.

Estan disenadas para ejecutarse de forma **nativa en Linux y macOS**. No usan
ninguna API especifica de Windows y producen binarios ELF/Mach-O estandar.

---

## Compatibilidad con sistemas operativos

| Plataforma | Compila | Ejecuta | Notas |
| --- | --- | --- | --- |
| **Linux** (x86-64, ARM64) | Si — gcc / clang | Si | Soporte completo |
| **macOS** (Intel, Apple Silicon) | Si — clang / gcc | Si | Soporte completo |
| **Windows** | No | No | Usar `organizers_c/` en su lugar |
| **BSD / otro POSIX** | Probable | Probable | No probado |

---

## Por que una carpeta separada de `organizers_c/`?

La version Windows usa APIs que no existen en Linux/macOS:

| Funcionalidad | Windows (`organizers_c/`) | POSIX (`organizers_posix/`) |
| --- | --- | --- |
| Recorrido de directorios | `FindFirstFileA` / `FindNextFileA` | `opendir` / `readdir` — `<dirent.h>` |
| E/S de archivos | `CreateFileA` + `ReadFile` | `fopen` + `fread` — C estandar |
| Metadatos de archivos | `GetFileAttributesExA` | `stat()` — `<sys/stat.h>` |
| Timestamps | `GetLocalTime` | `time()` + `localtime()` — `<time.h>` |
| Duracion de video | Windows Media Foundation | **libavformat** (FFmpeg) |
| HTTP / HTTPS | WinHTTP | **libcurl** |
| Conversion DOC | Word COM (`IDispatch`) | LibreOffice CLI (`soffice --headless`) |
| Separador de rutas | `\` | `/` |
| Macro de ruta maxima | `MAX_PATH` (260) | `PATH_MAX` (4096) — `<limits.h>` |
| Utilidades de cadenas | `_stricmp`, `_strdup` | `strcasecmp`, `strdup` — estandar POSIX |

El parser binario ZIP, el escaner de bytes PDF y toda la logica de salida y
ordenamiento son **identicos** en ambas versiones — operan sobre buffers de
bytes sin ninguna dependencia del sistema operativo.

---

## Dependencias externas

| Herramienta / Biblioteca | Usada por | Instalacion |
| --- | --- | --- |
| **libavformat** (FFmpeg) | `length`, `seriesLength` | Ver abajo |
| **libcurl** | `steamSorter` | Ver abajo |
| **LibreOffice** (en tiempo de ejecucion) | `doc2docx` | Ver abajo |
| `unrar` (en tiempo de ejecucion, opcional) | `comanga` — solo archivos CBR | Ver abajo |

### Instalar en Linux (Debian / Ubuntu)

```bash
sudo apt update
sudo apt install libavformat-dev libavutil-dev   # FFmpeg
sudo apt install libcurl4-openssl-dev            # cURL
sudo apt install libreoffice                     # doc2docx
sudo apt install unrar                           # comanga CBR (opcional)
```

### Instalar en Linux (Fedora / RHEL)

```bash
sudo dnf install ffmpeg-devel                   # FFmpeg
sudo dnf install libcurl-devel                  # cURL
sudo dnf install libreoffice                    # doc2docx
sudo dnf install unrar                          # comanga CBR (opcional)
```

### Instalar en macOS (Homebrew)

```bash
brew install ffmpeg                             # FFmpeg
brew install curl                               # cURL (generalmente preinstalado)
brew install --cask libreoffice                 # doc2docx
brew install unar                               # comanga CBR (opcional, unar lee RAR)
```

> En macOS, `clang` es el compilador C por defecto e incluye las Xcode Command
> Line Tools (`xcode-select --install`). Tambien se puede usar `gcc` via Homebrew.

---

## Compilacion

### Todas las herramientas (Linux y macOS)

```bash
# comanga — sin bibliotecas externas
gcc -std=c11 -Wall -O2 comanga.c shared_utils.c -o comanga

# doc2docx — sin bibliotecas externas (LibreOffice es solo dependencia en tiempo de ejecucion)
gcc -std=c11 -Wall -O2 doc2docx.c shared_utils.c -o doc2docx

# pageCounter — sin bibliotecas externas
gcc -std=c11 -Wall -O2 pageCounter.c shared_utils.c -o pageCounter

# length — requiere encabezados FFmpeg
gcc -std=c11 -Wall -O2 length.c shared_utils.c -o length \
    $(pkg-config --cflags --libs libavformat libavutil)

# seriesLength — requiere encabezados FFmpeg
gcc -std=c11 -Wall -O2 seriesLength.c shared_utils.c -o seriesLength \
    $(pkg-config --cflags --libs libavformat libavutil)

# steamSorter — requiere encabezados libcurl
gcc -std=c11 -Wall -O2 steamSorter.c shared_utils.c -o steamSorter \
    $(pkg-config --cflags --libs libcurl)
```

Reemplazar `gcc` con `clang` en macOS si se prefiere — ambos aceptan los mismos flags.

> **pkg-config no encontrado?**
> En macOS puede ser necesario: `brew install pkg-config`
> O reemplazar el `$(pkg-config ...)` con flags explicitos, por ejemplo:
>
> ```bash
> # length en macOS sin pkg-config
> clang -std=c11 -O2 length.c shared_utils.c -o length \
>     -I/opt/homebrew/include -L/opt/homebrew/lib -lavformat -lavutil
> ```

### Enlazado estatico (opcional, binario completamente autocontenido)

```bash
# Ejemplo estatico en Linux (enlaza todas las libs en el binario)
gcc -std=c11 -O2 length.c shared_utils.c -o length \
    -static $(pkg-config --cflags --libs libavformat libavutil) \
    -lpthread -lm -lz -lbz2 -llzma
```

> El enlazado estatico en macOS esta restringido para las bibliotecas del sistema
> — el enlazado dinamico es el enfoque recomendado alli.

---

## Uso

Todas las herramientas toman por defecto el directorio de trabajo actual.

```text
./comanga [directorio]
    Contar paginas en archivos CBZ/CBR/EPUB/PDF.
    Salida: page_count_results.txt

./doc2docx [directorio]
    Convertir archivos .doc a .docx usando LibreOffice.
    Salida: ./output/*.docx

./length [directorio] [-o salida.txt]
    Analizar duraciones de video de forma recursiva.
    Salida: video_duration_analysis.txt

./pageCounter [directorio]
    Contar paginas en archivos PDF/EPUB/DOCX (no recursivo).
    Salida: page_count_results.txt

./seriesLength [directorio]
    Sumar duraciones de video por subdirectorio (modo series de TV).
    Salida: series_durations.txt

./steamSorter <API_KEY> <STEAM_ID> [STEAM_ID2 ...]
    Obtener biblioteca de Steam y consultar tiempos de completado en HLTB.
    Salida: steam_games_completion_times.txt
```

---

## Limitaciones

| Funcionalidad | Limitacion |
| --- | --- |
| **Conversion DOC** | Solo LibreOffice esta disponible; Microsoft Word no se ejecuta de forma nativa en Linux/macOS. La calidad de la conversion depende del soporte DOC de LibreOffice. |
| **CBR (RAR) en comanga** | Requiere `unrar` (Linux) o `unar` (macOS) en el `PATH`. Sin ello, los archivos CBR se omiten. |
| **Conteo de paginas PDF** | Heuristica de escaneo binario (`/Type /Page`). Funciona para PDFs estandar; puede contar mal archivos PDF 1.5+ con flujos de referencia cruzada. |
| **DOCX con deflate** | Si `word/document.xml` esta comprimido con deflate (metodo ZIP 8), se recurre a un conteo estimado de paginas. La descompresion exacta requiere zlib. |
| **API HLTB** | El endpoint de busqueda de HowLongToBeat es no oficial y puede cambiar sin previo aviso. |
| **Soporte de codecs** | `length` y `seriesLength` dependen de FFmpeg para decodificar metadatos. Todos los formatos comunes (MP4, MKV, AVI, MOV...) son compatibles de serie. |

---

## Mapeo de archivos (Python → C POSIX)

| Archivo Python original | Traduccion C POSIX |
| --- | --- |
| `shared_utils.py` | `shared_utils.h` + `shared_utils.c` |
| `comanga.py` | `comanga.c` |
| `doc2docx.py` | `doc2docx.c` |
| `length.py` | `length.c` |
| `pageCounter.py` | `pageCounter.c` |
| `seriesLength.py` | `seriesLength.c` |
| `steamSorter.py` | `steamSorter.c` |

---

## Relacionado

- `organizers_c/` — Compilaciones nativas para Windows con APIs Win32 y binarios `.exe` precompilados.
- `organizers_posix/` — Esta carpeta; compilaciones para Linux y macOS con APIs POSIX, FFmpeg y libcurl.
