# organizers_c — Traducciones C Nativas para Windows

> **Estado: CONGELADO / sin mantenimiento.** Estos ports se conservan como
> referencia y recurso de aprendizaje. Las nuevas funcionalidades y
> correcciones se hacen solo en las herramientas Python; este codigo no
> recibe actualizaciones funcionales. La CI compila este arbol en cada push
> (MSVC, ver `.github/workflows/ci.yml`) para que no se degrade en silencio,
> pero el comportamiento puede divergir de las versiones Python.
> Compilado con **Visual Studio Build Tools (MSVC `cl`)** — ver las
> instrucciones de compilacion mas abajo.
> Los binarios compilados (`.exe`/`.obj`) ya no se incluyen en el repositorio.

Esta carpeta contiene reescrituras en C (C99/C11) de todos los scripts Python
del repositorio `organizers/`, diseñadas para ejecutarse de forma **nativa en Windows**.

Todo el codigo usa exclusivamente APIs de Win32/Windows nativas. Sin llamadas
POSIX, sin dependencias de Cygwin, WSL ni bibliotecas de terceros en tiempo de
ejecucion (excepto los componentes del Windows SDK que se incluyen con toda
instalacion de Windows compatible).

---

## Compatibilidad con sistemas operativos — Resumen

| Plataforma | Puede ejecutar el `.exe`? | Puede compilar el `.c`? | Notas |
| --- | --- | --- | --- |
| **Windows 10** | Si | Si (con Build Tools) | Soporte completo |
| **Windows 11** | Si | Si (con Build Tools) | Soporte completo |
| **Windows 7 / 8.1** | Parcial | Si | Media Foundation limitado en Win7; COM funciona |
| **macOS** | No | No | La API Win32 no existe en macOS |
| **Linux** | No | No | La API Win32 no existe en Linux |
| **Linux + Wine** | Parcial | No | Ver nota de Wine mas abajo |
| **WSL (Windows Subsystem for Linux)** | No | No | WSL es Linux; Win32 no esta disponible dentro de el |

---

## Por que el `.exe` solo funciona en Windows

### Formato binario: PE vs ELF vs Mach-O

El `.exe` compilado es un archivo **PE (Portable Executable)** — el formato
binario nativo de Windows. macOS usa **Mach-O** y Linux usa **ELF**.
Estos formatos son fundamentalmente incompatibles: el cargador del sistema
operativo en macOS y Linux no puede analizar ni ejecutar un archivo PE.

```
Windows  →  .exe / .dll   (formato PE)
Linux    →  sin extension  (formato ELF)
macOS    →  sin extension  (formato Mach-O)
```

No se puede copiar un `.exe` a una Mac o maquina Linux y ejecutarlo,
independientemente del hardware o arquitectura de CPU.

### Dependencias de API: solo Win32

Incluso si el formato binario no fuera un problema, el codigo llama a funciones
que simplemente no existen fuera de Windows:

| API usada en este codigo | DLL de Windows | Equivalente en macOS / Linux |
| --- | --- | --- |
| `FindFirstFileA` / `FindNextFileA` | `kernel32.dll` | `opendir` / `readdir` (POSIX) |
| `CreateFileA`, `ReadFile`, `WriteFile` | `kernel32.dll` | `open`, `read`, `write` (POSIX) |
| `GetLocalTime` | `kernel32.dll` | `localtime` (POSIX) |
| `CoCreateInstance` / `IDispatch` (COM) | `ole32.dll` | Sin equivalente (macOS tiene scripting Cocoa; Linux no tiene nada) |
| `MFCreateSourceReaderFromURL` (Media Foundation) | `mfplat.dll` | `libavformat` / ffmpeg (POSIX) |
| `WinHttpOpen` / `WinHttpSendRequest` | `winhttp.dll` | `libcurl` (POSIX) |
| `GetFileAttributesExA` | `kernel32.dll` | `stat()` (POSIX) |

Estas DLLs de Windows no estan presentes en macOS ni Linux. El enlazador en
esas plataformas rechazaria compilar este codigo aunque se intentara.

---

## Ejecutar en macOS o Linux — opciones

### Opcion 1: Usar los scripts Python originales (recomendado)

Los scripts Python en la carpeta padre `organizers/` son multiplataforma.
Ya funcionan en macOS y Linux siempre que esten instalados los paquetes Python
requeridos. Ese es el camino previsto para usuarios que no usan Windows.

### Opcion 2: Wine (solo Linux, parcial)

[Wine](https://www.winehq.org/) implementa un subconjunto de la API Win32 en
Linux y macOS, permitiendo que algunos programas Windows se ejecuten.

- `comanga.exe`, `pageCounter.exe` — probablemente funcionan con Wine (usan solo
  E/S de archivos Win32 basica y parseo ZIP).
- `length.exe`, `seriesLength.exe` — poco probable que funcionen; el soporte de
  Media Foundation en Wine esta incompleto a partir de 2025.
- `doc2docx.exe` — requiere automatizacion COM de Word, que no funciona con Wine
  a menos que Word este instalado dentro del prefijo de Wine.
- `steamSorter.exe` — WinHTTP bajo Wine puede funcionar para HTTP simple; HTTPS
  (HLTB) es variable segun la version de Wine.

Wine **no** es una plataforma soportada para estas herramientas; se menciona
solo como ultimo recurso.

### Opcion 3: Usar las compilaciones C POSIX (Linux / macOS)

La carpeta `organizers_posix/` en este repositorio contiene traducciones C de
los mismos scripts para Linux y macOS de forma nativa. Reemplazan cada llamada
Win32 con su equivalente POSIX o especifico de plataforma (FFmpeg en lugar de
Media Foundation, libcurl en lugar de WinHTTP, etc.) y se compilan con `gcc` o
`clang` sin ningun Windows SDK.

Ver `organizers_posix/README.es.md` para instrucciones de compilacion e
instalacion de dependencias.

---

## Mapeo de archivos

| Archivo Python original | Traduccion C | Notas |
| --- | --- | --- |
| `shared_utils.py` | `shared_utils.h` + `shared_utils.c` | Utilidades compartidas (logging, lector ZIP, buscador de archivos...) |
| `comanga.py` | `comanga.c` | Contador de paginas de comics/manga |
| `doc2docx.py` | `doc2docx.c` | DOC a DOCX via COM (Word) o LibreOffice |
| `length.py` | `length.c` | Analizador de duracion de video |
| `pageCounter.py` | `pageCounter.c` | Contador de paginas de PDF / EPUB / DOCX |
| `seriesLength.py` | `seriesLength.c` | Analizador de duracion de series de TV |
| `steamSorter.py` | `steamSorter.c` | Analizador Steam + HowLongToBeat |

---

## Sustitucion de APIs nativas de Windows

| Biblioteca Python / llamada | Equivalente C en Windows |
| --- | --- |
| `pathlib.Path` | Arrays `char[]` + `WIN32_FIND_DATAA` |
| `os.walk` / `Path.iterdir` | `FindFirstFileA` / `FindNextFileA` |
| `open(path, 'rb')` | `CreateFileA` + `ReadFile` |
| `os.stat().st_size` | `GetFileAttributesExA` + `LARGE_INTEGER` |
| `zipfile.ZipFile` | Parser manual del directorio central ZIP |
| `logging` (con timestamps) | `log_msg()` personalizado usando `GetLocalTime` |
| `win32com.client` (Word COM) | `CoCreateInstance` + `IDispatch::Invoke` |
| `subprocess.run` (LibreOffice) | `CreateProcessA` + `WaitForSingleObject` |
| `moviepy.editor.VideoFileClip` | Windows Media Foundation (`MFCreateSourceReaderFromURL` + `MF_PD_DURATION`) |
| `requests.get` / `requests.post` | WinHTTP (`WinHttpOpen`, `WinHttpSendRequest`...) |
| `json.loads` | Escaner de cadenas minimalista en linea |
| `howlongtobeatpy.HowLongToBeat` | POST WinHTTP a `howlongtobeat.com/api/search` |

---

## Limitaciones y funcionalidad no traducible

| Funcionalidad | Limitacion |
| --- | --- |
| **Conteo de paginas CBR (RAR)** (`comanga.c`) | Requiere `unrar.exe` en el `PATH`. Sin el, los archivos CBR se reportan con 0 paginas. No existe API nativa de Windows para descompresion RAR; se necesitaria el SDK de DLL de UnRAR (propietario) para una solucion completamente integrada. |
| **Conteo de paginas PDF** (tanto `comanga.c` como `pageCounter.c`) | Usa un escaneo binario de objetos `/Type /Page`. Funciona para la gran mayoria de PDFs estandar, pero puede contar mal PDFs con flujos de referencia cruzada (PDF 1.5+ XRef streams). Se necesitaria un parser PDF completo para precision del 100%. |
| **Conteo de paginas DOCX con deflate** (`pageCounter.c`) | Cuando `word/document.xml` esta comprimido con deflate (metodo ZIP 8), la herramienta recurre a una estimacion por tamano descomprimido. Descomprimir deflate sin zlib requiere enlazar zlib o implementar RFC 1951, fuera del alcance de esta traduccion. |
| **Duracion de video** (`length.c`, `seriesLength.c`) | Windows Media Foundation puede no decodificar todos los codecs (ej. DivX/Xvid antiguos). Instalar el paquete de codecs correspondiente si los archivos no reportan duracion. |
| **Estabilidad de la API HLTB** (`steamSorter.c`) | El endpoint de busqueda de HowLongToBeat es no oficial y puede cambiar. Si las consultas fallan, actualizar `path` y `post_body` en `hltb_get_main_story()`. |
| **Parseo JSON** (`steamSorter.c`) | Usa un escaner de cadenas minimalista — suficiente para respuestas bien formadas de la API de Steam, pero no es un parser JSON completo. Estructuras anidadas con el mismo nombre de clave podrian producir resultados inesperados. |
| **Archivos grandes / rutas > MAX_PATH** | Todas las herramientas usan `char[MAX_PATH]` (260 caracteres). Rutas muy largas requieren prefijos de ruta extendida (`\\?\`) y buffers mas amplios; no implementado para mantener el codigo conciso. |

---

## Instrucciones de compilacion generales (Windows)

### Requisitos previos

- **MSVC (Visual Studio Build Tools)** — recomendado, incluye todos los encabezados del SDK necesarios.
  Instalar la carga de trabajo **"Desarrollo de escritorio con C++"** (incluye `cl.exe` + Windows SDK).
- **MinGW-w64** — alternativa gratuita; instalar via MSYS2 (`pacman -S mingw-w64-ucrt-x86_64-gcc`).
- Windows 10 SDK o posterior (para encabezados de Media Foundation).

---

### Arquitectura de destino — que Simbolo del sistema abrir

La arquitectura del `.exe` compilado depende del **Simbolo del sistema para desarrolladores** que se abra:

| Acceso directo a abrir | `.exe` destino | Funciona en |
| --- | --- | --- |
| **Developer Command Prompt for VS 2022** | x86 (32-bit) | Cualquier Windows 10/11 (x86, x64, ARM64 via emulacion) |
| **x64 Native Tools Command Prompt for VS 2022** | x64 (64-bit) | Windows 10/11 x64 y ARM64 (via emulacion) |
| **ARM64 Native Tools Command Prompt for VS 2022** | ARM64 (nativo) | Solo Windows 10/11 ARM64, sin emulacion |

**Recomendacion:** Usar el simbolo de sistema **x64 Native Tools** para mejor rendimiento en maquinas modernas.
Los binarios de 32-bit (x86) tambien funcionan en todas partes via WOW64, pero con un limite de 2 GB de
memoria por proceso.

---

### Compilar todas las herramientas con MSVC

Abrir el **Simbolo del sistema Native Tools para VS 2022** apropiado y ejecutar desde esta carpeta.

La opcion `/D_CRT_SECURE_NO_WARNINGS` suprime las advertencias de deprecacion de MSVC sobre funciones
C estandar (`_snprintf`, `fopen`, etc.) — el codigo es correcto, estas son solo advertencias cosmeticas.

#### Compilacion rapida — pegar el bloque completo en la consola de desarrollador

```bat
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS comanga.c      shared_utils.c /Fe:comanga.exe
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS doc2docx.c     shared_utils.c /Fe:doc2docx.exe     ole32.lib oleaut32.lib
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS length.c       shared_utils.c /Fe:length.exe        mfplat.lib mfreadwrite.lib mf.lib mfuuid.lib propsys.lib ole32.lib
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS pageCounter.c  shared_utils.c /Fe:pageCounter.exe
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS seriesLength.c shared_utils.c /Fe:seriesLength.exe  mfplat.lib mfreadwrite.lib mf.lib mfuuid.lib propsys.lib ole32.lib
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS steamSorter.c  shared_utils.c /Fe:steamSorter.exe   winhttp.lib
```

> **Nota:** `length.exe` y `seriesLength.exe` requieren `ole32.lib` para `PropVariantClear`
> (usado por Windows Media Foundation). Omitirla causa errores de enlazado `LNK2019`.

### Compilar todas las herramientas con MinGW (gcc)

Abrir un shell MSYS2 / MinGW-w64:

```bash
# comanga
gcc -std=c11 -Wall -O2 comanga.c shared_utils.c -o comanga.exe

# doc2docx
gcc -std=c11 -Wall -O2 doc2docx.c shared_utils.c -o doc2docx.exe \
    -lole32 -loleaut32

# length
gcc -std=c11 -Wall -O2 length.c shared_utils.c -o length.exe \
    -lmfplat -lmfreadwrite -lmf -lmfuuid -lpropsys -lole32

# pageCounter
gcc -std=c11 -Wall -O2 pageCounter.c shared_utils.c -o pageCounter.exe

# seriesLength
gcc -std=c11 -Wall -O2 seriesLength.c shared_utils.c -o seriesLength.exe \
    -lmfplat -lmfreadwrite -lmf -lmfuuid -lpropsys -lole32

# steamSorter
gcc -std=c11 -Wall -O2 steamSorter.c shared_utils.c -o steamSorter.exe \
    -lwinhttp
```

---

## Uso

Todas las herramientas toman por defecto el **directorio de trabajo actual** cuando se ejecutan sin argumentos.

```
comanga.exe [directorio]
    Contar paginas en archivos CBZ/CBR/EPUB/PDF.
    Salida: page_count_results.txt

doc2docx.exe [directorio]
    Convertir todos los archivos .doc a .docx (requiere Word o LibreOffice).
    Salida: .\output\*.docx

length.exe [directorio] [-o salida.txt]
    Analizar duraciones de video de forma recursiva.
    Salida: video_duration_analysis.txt

pageCounter.exe [directorio]
    Contar paginas en archivos PDF/EPUB/DOCX (no recursivo).
    Salida: page_count_results.txt

seriesLength.exe [directorio]
    Sumar duraciones de video por subdirectorio (modo series de TV).
    Salida: series_durations.txt

steamSorter.exe <API_KEY> <STEAM_ID> [STEAM_ID2 ...]
    Obtener biblioteca de Steam y consultar tiempos de completado en HLTB.
    Salida: steam_games_completion_times.txt
```

---

## Nota sobre enlazado estatico

Todas las herramientas enlazan solo contra bibliotecas del sistema que forman parte del
Windows SDK (`ole32.lib`, `mfplat.lib`, `winhttp.lib`, etc.).
Estas se enlazan **estaticamente** en el ejecutable con `/MT` (MSVC) o `-static` (MinGW),
produciendo un `.exe` autocontenido.

Para habilitar enlazado completamente estatico con MSVC, agregar `/MT` al comando `cl`.
Con MinGW, agregar `-static` al comando `gcc`.
