/*
 * doc2docx.c
 * Translated from: doc2docx.py
 *
 * Purpose: Converts legacy .doc files to modern .docx format.
 *   Primary method: Microsoft Word COM automation (Word.Application).
 *   Fallback method: Invokes LibreOffice via CreateProcess if Word
 *                    is not installed.
 *
 * Windows-native APIs used:
 *   - COM/OLE2: CoInitialize, CoCreateInstance, IDispatch, Invoke
 *   - FindFirstFile / FindNextFile  (enumerate .doc files)
 *   - CreateDirectoryA              (create output\ subdirectory)
 *   - CreateProcess                 (LibreOffice fallback)
 *   - WaitForSingleObject           (wait for child process)
 *
 * Limitations:
 *   - Requires either Microsoft Word or LibreOffice to be installed.
 *   - Word COM automation: only primary interop calls are used;
 *     complex macros/embedded objects may not convert perfectly.
 *   - Only processes .doc files in the immediate directory (non-recursive),
 *     matching the behaviour of the original Python script.
 *
 * Compilation (MSVC):
 *   cl /W3 /std:c11 doc2docx.c shared_utils.c /Fe:doc2docx.exe ^
 *      ole32.lib oleaut32.lib
 * Compilation (MinGW):
 *   gcc -std=c11 -Wall doc2docx.c shared_utils.c -o doc2docx.exe ^
 *       -lole32 -loleaut32
 */

#define UNICODE
#define _UNICODE
#include "shared_utils.h"
#include <objbase.h>
#include <oleauto.h>
#include <unknwn.h>

/* ------------------------------------------------------------------ */
/*  COM / IDispatch helper: invoke a method / get / set property       */
/* ------------------------------------------------------------------ */

/* Get a DISPID for a member name on an IDispatch object. */
static HRESULT get_dispid(IDispatch *pDisp, const wchar_t *name, DISPID *pdid)
{
    OLECHAR *wname = (OLECHAR *)name;
    return pDisp->lpVtbl->GetIDsOfNames(pDisp, &IID_NULL,
                                         &wname, 1,
                                         LOCALE_USER_DEFAULT, pdid);
}

/*
 * Call a method on IDispatch with up to 4 VARIANT arguments
 * (arguments supplied in reverse order, as required by COM).
 * Returns the return VARIANT in *pvResult (caller VariantClear's it).
 */
static HRESULT disp_invoke(IDispatch *pDisp, DISPID did,
                            WORD call_type,
                            VARIANT *pvResult,
                            int argc, VARIANT *argv /* reversed */)
{
    DISPPARAMS dp;
    dp.rgvarg            = argv;
    dp.cArgs             = (UINT)argc;
    dp.rgdispidNamedArgs = NULL;
    dp.cNamedArgs        = 0;

    DISPID propput_id = DISPID_PROPERTYPUT;
    if (call_type == DISPATCH_PROPERTYPUT) {
        dp.rgdispidNamedArgs = &propput_id;
        dp.cNamedArgs        = 1;
    }

    return pDisp->lpVtbl->Invoke(pDisp, did, &IID_NULL,
                                  LOCALE_USER_DEFAULT,
                                  call_type, &dp,
                                  pvResult, NULL, NULL);
}

/* Convenience: set a boolean property by name. */
static void set_bool_prop(IDispatch *pDisp, const wchar_t *name, BOOL val)
{
    DISPID did;
    if (FAILED(get_dispid(pDisp, name, &did))) return;
    VARIANT v;
    VariantInit(&v);
    v.vt      = VT_BOOL;
    v.boolVal = val ? VARIANT_TRUE : VARIANT_FALSE;
    disp_invoke(pDisp, did, DISPATCH_PROPERTYPUT, NULL, 1, &v);
    VariantClear(&v);
}

/* Convenience: call a method with one BSTR argument; return VT_DISPATCH or empty. */
static HRESULT call_with_bstr(IDispatch *pDisp, const wchar_t *method,
                               const wchar_t *arg1, IDispatch **ppOut)
{
    DISPID did;
    HRESULT hr = get_dispid(pDisp, method, &did);
    if (FAILED(hr)) return hr;

    VARIANT varg;
    VariantInit(&varg);
    varg.vt      = VT_BSTR;
    varg.bstrVal = SysAllocString(arg1);

    VARIANT vResult;
    VariantInit(&vResult);
    hr = disp_invoke(pDisp, did, DISPATCH_METHOD, &vResult, 1, &varg);

    VariantClear(&varg);

    if (SUCCEEDED(hr) && ppOut) {
        if (vResult.vt == VT_DISPATCH)
            *ppOut = vResult.pdispVal;
        else
            VariantClear(&vResult);
    }
    return hr;
}

/* Convenience: call SaveAs(path, FileFormat=16). */
static HRESULT doc_save_as(IDispatch *pDoc, const wchar_t *out_path)
{
    DISPID did;
    HRESULT hr = get_dispid(pDoc, L"SaveAs", &did);
    if (FAILED(hr)) return hr;

    VARIANT vargs[2];
    VariantInit(&vargs[0]); VariantInit(&vargs[1]);
    /* COM: args in reverse order */
    vargs[0].vt   = VT_I4;  vargs[0].lVal    = 16; /* wdFormatXMLDocument */
    vargs[1].vt   = VT_BSTR; vargs[1].bstrVal = SysAllocString(out_path);

    VARIANT vResult;
    VariantInit(&vResult);
    hr = disp_invoke(pDoc, did, DISPATCH_METHOD, &vResult, 2, vargs);
    VariantClear(&vargs[0]);
    VariantClear(&vargs[1]);
    return hr;
}

/* Convenience: call a no-arg method. */
static void call_no_arg(IDispatch *pDisp, const wchar_t *method)
{
    DISPID did;
    if (FAILED(get_dispid(pDisp, method, &did))) return;
    DISPPARAMS dp = {NULL, NULL, 0, 0};
    pDisp->lpVtbl->Invoke(pDisp, did, &IID_NULL, LOCALE_USER_DEFAULT,
                           DISPATCH_METHOD, &dp, NULL, NULL, NULL);
}

/* Get IDispatch sub-object by property name. */
static IDispatch *get_obj_prop(IDispatch *pDisp, const wchar_t *name)
{
    DISPID did;
    if (FAILED(get_dispid(pDisp, name, &did))) return NULL;
    DISPPARAMS dp = {NULL, NULL, 0, 0};
    VARIANT vResult;
    VariantInit(&vResult);
    HRESULT hr = pDisp->lpVtbl->Invoke(pDisp, did, &IID_NULL,
                                        LOCALE_USER_DEFAULT,
                                        DISPATCH_PROPERTYGET,
                                        &dp, &vResult, NULL, NULL);
    if (SUCCEEDED(hr) && vResult.vt == VT_DISPATCH)
        return vResult.pdispVal;
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Word COM conversion                                                 */
/* ------------------------------------------------------------------ */
static int convert_with_word(IDispatch *pWord,
                              const char *input_path,
                              const char *output_dir,
                              const char *stem)
{
    /* Build wide-string paths */
    wchar_t w_input[MAX_PATH], w_output[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, input_path, -1, w_input, MAX_PATH);

    char out_path[MAX_PATH];
    _snprintf(out_path, sizeof(out_path), "%s\\%s.docx", output_dir, stem);
    MultiByteToWideChar(CP_ACP, 0, out_path, -1, w_output, MAX_PATH);

    /* pWord.Documents.Open(input_path) */
    IDispatch *pDocs = get_obj_prop(pWord, L"Documents");
    if (!pDocs) {
        LOG_E("Cannot get Word.Documents");
        return 0;
    }

    IDispatch *pDoc = NULL;
    HRESULT hr = call_with_bstr(pDocs, L"Open", w_input, &pDoc);
    pDocs->lpVtbl->Release(pDocs);

    if (FAILED(hr) || !pDoc) {
        LOG_E("Word.Documents.Open failed (HRESULT 0x%08X): %s",
              (unsigned)hr, input_path);
        return 0;
    }

    /* pDoc.SaveAs(output_path, FileFormat=16) */
    hr = doc_save_as(pDoc, w_output);
    if (FAILED(hr))
        LOG_E("Word.Document.SaveAs failed (HRESULT 0x%08X)", (unsigned)hr);

    /* pDoc.Close() */
    call_no_arg(pDoc, L"Close");
    pDoc->lpVtbl->Release(pDoc);

    if (SUCCEEDED(hr)) {
        LOG_I("Word conversion OK: %s -> %s", input_path, out_path);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  LibreOffice fallback (CreateProcess)                               */
/* ------------------------------------------------------------------ */
static int convert_with_libreoffice(const char *input_path,
                                     const char *output_dir)
{
    char cmd[MAX_PATH * 3];
    _snprintf(cmd, sizeof(cmd),
              "soffice --headless --convert-to docx --outdir \"%s\" \"%s\"",
              output_dir, input_path);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                         CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        LOG_W("LibreOffice (soffice) not found or failed to start.");
        return 0;
    }

    WaitForSingleObject(pi.hProcess, 60000); /* 60-second timeout */
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code == 0) {
        LOG_I("LibreOffice conversion OK: %s", input_path);
        return 1;
    }
    LOG_E("LibreOffice conversion failed (exit %lu): %s", exit_code, input_path);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Main conversion logic                                               */
/* ------------------------------------------------------------------ */
static void convert_doc_to_docx(const char *input_folder)
{
    /* Create output\ subdirectory */
    char output_dir[MAX_PATH];
    _snprintf(output_dir, sizeof(output_dir), "%s\\output", input_folder);
    CreateDirectoryA(output_dir, NULL); /* OK if already exists */
    LOG_I("Output directory: %s", output_dir);

    /* Enumerate .doc files (non-recursive, matching original script) */
    char pattern[MAX_PATH];
    _snprintf(pattern, sizeof(pattern), "%s\\*.doc", input_folder);

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);

    /* Collect doc files */
    char doc_paths[1024][MAX_PATH];
    char doc_stems[1024][MAX_PATH];
    int  doc_count = 0;

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            /* Exclude .docx (Windows glob *.doc also matches *.docx) */
            if (str_ends_with_ci(ffd.cFileName, ".docx")) continue;
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            _snprintf(doc_paths[doc_count], MAX_PATH, "%s\\%s",
                      input_folder, ffd.cFileName);

            /* Compute stem (filename without extension) */
            strncpy(doc_stems[doc_count], ffd.cFileName, MAX_PATH - 1);
            char *dot = strrchr(doc_stems[doc_count], '.');
            if (dot) *dot = '\0';

            doc_count++;
            if (doc_count >= 1024) break;
        } while (FindNextFileA(hFind, &ffd));
        FindClose(hFind);
    }

    if (doc_count == 0) {
        LOG_I("No .doc files found in: %s", input_folder);
        return;
    }
    LOG_I("Found %d .doc file(s) to convert.", doc_count);

    /* Attempt to initialise Microsoft Word via COM */
    BOOL com_ok = SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));
    IDispatch *pWord = NULL;

    if (com_ok) {
        CLSID clsid;
        HRESULT hr = CLSIDFromProgID(L"Word.Application", &clsid);
        if (SUCCEEDED(hr)) {
            hr = CoCreateInstance(&clsid, NULL, CLSCTX_LOCAL_SERVER,
                                  &IID_IDispatch, (void **)&pWord);
            if (SUCCEEDED(hr) && pWord) {
                set_bool_prop(pWord, L"Visible", FALSE);
                LOG_I("Using Microsoft Word for conversion.");
            } else {
                LOG_W("Could not launch Word.Application (HRESULT 0x%08X). "
                      "Falling back to LibreOffice.", (unsigned)hr);
                pWord = NULL;
            }
        }
    }

    /* Process each .doc file */
    int converted = 0;
    ProgressReporter pr;
    progress_init(&pr, doc_count, "Converting files");

    for (int i = 0; i < doc_count; i++) {
        int ok = 0;

        if (pWord)
            ok = convert_with_word(pWord, doc_paths[i],
                                   output_dir, doc_stems[i]);

        if (!ok)
            ok = convert_with_libreoffice(doc_paths[i], output_dir);

        if (ok) {
            converted++;
        } else {
            LOG_E("Failed to convert: %s", doc_paths[i]);
        }
        progress_update(&pr, 1);
    }

    /* Shutdown Word */
    if (pWord) {
        call_no_arg(pWord, L"Quit");
        pWord->lpVtbl->Release(pWord);
    }
    if (com_ok) CoUninitialize();

    progress_finish(&pr);

    LOG_I("Conversion complete!");
    LOG_I("Successfully converted: %d/%d files", converted, doc_count);
    LOG_I("Converted files saved in: %s", output_dir);

    if (converted < doc_count)
        LOG_W("Failed conversions: %d. "
              "Install Microsoft Word or LibreOffice for best results.",
              doc_count - converted);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    log_init(LOG_INFO, NULL);

    char folder[MAX_PATH];
    if (argc >= 2) {
        _snprintf(folder, sizeof(folder), "%s", argv[1]);
    } else {
        GetCurrentDirectoryA(MAX_PATH, folder);
    }

    LOG_I("doc2docx - DOC to DOCX Converter");
    LOG_I("Input folder: %s", folder);

    convert_doc_to_docx(folder);

    log_close();
    return 0;
}
