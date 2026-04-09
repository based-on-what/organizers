/*
 * doc2docx.c  (POSIX version)
 * Translated from: doc2docx.py
 *
 * Purpose: Convert legacy .doc files to .docx using LibreOffice.
 *
 * POSIX differences vs Windows version:
 *   - No COM/IDispatch (Word not available on Linux/macOS natively).
 *   - LibreOffice is the only conversion method.
 *   - LibreOffice path differs per OS — the code tries common locations.
 *   - Process launch: popen() / system() instead of CreateProcess.
 *   - Directory enumeration: opendir/readdir instead of FindFirstFile.
 *
 * LibreOffice installation:
 *   Linux (Debian/Ubuntu):  sudo apt install libreoffice
 *   Linux (Fedora):         sudo dnf install libreoffice
 *   macOS (Homebrew):       brew install --cask libreoffice
 *   macOS (direct):         download from https://libreoffice.org
 *
 * Compilation (Linux):
 *   gcc -std=c11 -Wall -O2 doc2docx.c shared_utils.c -o doc2docx
 * Compilation (macOS):
 *   clang -std=c11 -Wall -O2 doc2docx.c shared_utils.c -o doc2docx
 */

#include "shared_utils.h"
#include <sys/wait.h>

/* ------------------------------------------------------------------ */
/*  Find the LibreOffice executable                                     */
/* ------------------------------------------------------------------ */
/*
 * Returns the path to soffice/libreoffice in a static buffer,
 * or NULL if not found.
 */
static const char *find_libreoffice(void)
{
    static const char *candidates[] = {
        /* Linux */
        "libreoffice",
        "soffice",
        "/usr/bin/libreoffice",
        "/usr/bin/soffice",
        "/usr/local/bin/libreoffice",
        /* macOS (typical install paths) */
        "/Applications/LibreOffice.app/Contents/MacOS/soffice",
        "/Applications/LibreOffice.app/Contents/MacOS/libreoffice",
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        /* Quick check: try running --version */
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "\"%s\" --version >/dev/null 2>&1",
                 candidates[i]);
        if (system(cmd) == 0)
            return candidates[i];
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Convert a single .doc file to .docx via LibreOffice                */
/* ------------------------------------------------------------------ */
static int convert_with_libreoffice(const char *soffice,
                                     const char *input_path,
                                     const char *output_dir)
{
    char cmd[PATH_MAX * 3];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" --headless --convert-to docx --outdir \"%s\" \"%s\""
             " >/dev/null 2>&1",
             soffice, output_dir, input_path);

    int ret = system(cmd);
    if (ret == 0) {
        LOG_I("OK  converted: %s", input_path);
        return 1;
    }
    LOG_E("FAIL conversion failed (exit %d): %s", ret, input_path);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Main logic                                                           */
/* ------------------------------------------------------------------ */
static void convert_doc_to_docx(const char *input_folder)
{
    /* Create output/ subdirectory */
    char output_dir[PATH_MAX];
    snprintf(output_dir, sizeof(output_dir), "%s/output", input_folder);
    mkdir(output_dir, 0755); /* OK if already exists */
    LOG_I("Output directory: %s", output_dir);

    /* Find LibreOffice */
    const char *soffice = find_libreoffice();
    if (!soffice) {
        LOG_E("LibreOffice not found. Install it to use doc2docx.");
        LOG_E("  Linux:  sudo apt install libreoffice");
        LOG_E("  macOS:  brew install --cask libreoffice");
        return;
    }
    LOG_I("Using LibreOffice: %s", soffice);

    /* Enumerate .doc files (non-recursive, matching original script) */
    DIR *dir = opendir(input_folder);
    if (!dir) { LOG_E("Cannot open directory: %s", input_folder); return; }

    char  doc_paths[1024][PATH_MAX];
    int   doc_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && doc_count < 1024) {
        if (!str_ends_with_ci(entry->d_name, ".doc")) continue;
        if ( str_ends_with_ci(entry->d_name, ".docx")) continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s",
                 input_folder, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        snprintf(doc_paths[doc_count], PATH_MAX, "%s", full_path);
        doc_count++;
    }
    closedir(dir);

    if (doc_count == 0) {
        LOG_I("No .doc files found in: %s", input_folder);
        return;
    }
    LOG_I("Found %d .doc file(s) to convert.", doc_count);

    ProgressReporter pr;
    progress_init(&pr, doc_count, "Converting files");

    int converted = 0;
    for (int i = 0; i < doc_count; i++) {
        if (convert_with_libreoffice(soffice, doc_paths[i], output_dir))
            converted++;
        progress_update(&pr, 1);
    }
    progress_finish(&pr);

    LOG_I("Conversion complete!");
    LOG_I("Successfully converted: %d/%d files", converted, doc_count);
    LOG_I("Converted files saved in: %s", output_dir);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);

    char folder[PATH_MAX];
    if (argc >= 2) snprintf(folder, sizeof(folder), "%s", argv[1]);
    else           getcwd(folder, sizeof(folder));

    LOG_I("doc2docx - DOC to DOCX Converter (LibreOffice)");
    LOG_I("Input folder: %s", folder);
    convert_doc_to_docx(folder);

    log_close();
    return 0;
}
