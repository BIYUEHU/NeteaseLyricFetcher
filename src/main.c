#include "constant.h"
#include "resource.h"
#include "tools.h"
#include "utils.h"
#include "weapi.h"
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

HINSTANCE hInstance;
HWND hWnd, hInput, hOutput, hButton;
HWND hRadioOrigin, hRadioTrans, hRadioRomaji;
HWND hRadioOriginTrans, hRadioOriginRomaji, hRadioTransRomaji, hRadioAllThree;
HWND hRadioSeparate, hRadioInterleave, hRadioMerge;
HWND hFolderButton, hSaveButton, hFolderPath, hSeparatorInput;

static BOOL g_debug_mode = FALSE;
static HBRUSH g_bg_brush = NULL;
static HFONT g_font = NULL;
static const int WINDOW_MIN_WIDTH = 580;
static const int WINDOW_MIN_HEIGHT = 680;

static void debug(const char *label, const char *content) {
  if (g_debug_mode)
    write_log(label, content);
}

char *send_http_request(const char *url, const char *method,
                        const char *headers, const char *body) {
  HINTERNET hInternet, hConnect, hRequest;
  char *response = NULL;
  DWORD responseSize = 0;

  hInternet = InternetOpenW(STR_WINAPI_CLIENT, INTERNET_OPEN_TYPE_DIRECT, NULL,
                            NULL, 0);
  if (!hInternet) {
    MessageBoxW(hWnd, STR_SERVER_CONNECT_ERROR, STR_MSG_TITLE_ERROR,
                MB_OK | MB_ICONERROR);
    return NULL;
  }

  char hostname[256] = {0};
  char path[1024] = {0};
  int port = 80;
  BOOL isHttps = FALSE;

  if (strncmp(url, "https://", 8) == 0) {
    isHttps = TRUE;
    port = 443;
    strcpy(hostname, url + 8);
  } else if (strncmp(url, "http://", 7) == 0) {
    strcpy(hostname, url + 7);
  } else {
    strcpy(hostname, url);
  }

  char *pathStart = strchr(hostname, '/');
  if (pathStart) {
    strcpy(path, pathStart);
    *pathStart = '\0';
  } else {
    strcpy(path, "/");
  }

  char *portStart = strchr(hostname, ':');
  if (portStart) {
    port = atoi(portStart + 1);
    *portStart = '\0';
  }

  hConnect = InternetConnectA(hInternet, hostname, port, NULL, NULL,
                              INTERNET_SERVICE_HTTP, 0, 0);
  if (!hConnect) {
    MessageBoxW(hWnd, STR_SERVER_CONNECT_ERROR, STR_MSG_TITLE_ERROR,
                MB_OK | MB_ICONERROR);
    InternetCloseHandle(hInternet);
    return NULL;
  }

  DWORD flags = INTERNET_FLAG_RELOAD;
  if (isHttps) {
    flags |= INTERNET_FLAG_SECURE;
  }

  hRequest =
      HttpOpenRequestA(hConnect, method, path, NULL, NULL, NULL, flags, 0);
  if (!hRequest) {
    MessageBoxW(hWnd, STR_HTTP_REQUEST_ERROR, STR_MSG_TITLE_ERROR,
                MB_OK | MB_ICONERROR);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return NULL;
  }

  BOOL result;
  if (strcmp(method, "POST") == 0 && body && strlen(body) > 0) {
    result = HttpSendRequestA(hRequest, headers, headers ? strlen(headers) : 0,
                              (LPVOID)body, strlen(body));
  } else {
    result = HttpSendRequestA(hRequest, headers, headers ? strlen(headers) : 0,
                              NULL, 0);
  }

  if (!result) {
    MessageBoxW(hWnd, STR_SEND_HTTP_REQUEST_ERROR, STR_MSG_TITLE_ERROR,
                MB_OK | MB_ICONERROR);
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return NULL;
  }

  char buffer[4096];
  DWORD bytesRead;
  response = (char *)malloc(1);
  response[0] = '\0';
  responseSize = 1;

  while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) &&
         bytesRead > 0) {
    buffer[bytesRead] = '\0';
    response = (char *)realloc(response, responseSize + bytesRead);
    strcat(response, buffer);
    responseSize += bytesRead;
  }

  InternetCloseHandle(hRequest);
  InternetCloseHandle(hConnect);
  InternetCloseHandle(hInternet);

  return response;
}

void extract_lyric(const char *json_response, int lyric_types,
                   CombineType combine_type, const char *separator,
                   wchar_t *lyric_output) {
  if (json_response == NULL || lyric_output == NULL) {
    MessageBoxW(hWnd, STR_PARAM_ERROR, STR_MSG_TITLE_ERROR,
                MB_OK | MB_ICONERROR);
    return;
  }

  int type_count = 0;
  if (lyric_types & LYRIC_ORIGINAL)
    type_count++;
  if (lyric_types & LYRIC_TRANSLATED)
    type_count++;
  if (lyric_types & LYRIC_ROMAJI)
    type_count++;

  if (type_count > 1) {
    combine_lyrics(json_response, lyric_types, combine_type, separator,
                   lyric_output);
  } else {
    int single_type = LYRIC_ORIGINAL;
    if (lyric_types & LYRIC_TRANSLATED)
      single_type = LYRIC_TRANSLATED;
    else if (lyric_types & LYRIC_ROMAJI)
      single_type = LYRIC_ROMAJI;

    char *lyric_text = extract_single_lyric_type(json_response, single_type);
    if (lyric_text) {
      int lyric_len = MultiByteToWideChar(CP_UTF8, 0, lyric_text, -1, NULL, 0);
      wchar_t *wlyric = (wchar_t *)malloc(lyric_len * sizeof(wchar_t));
      MultiByteToWideChar(CP_UTF8, 0, lyric_text, -1, wlyric, lyric_len);

      wcsncpy(lyric_output, wlyric, 4095);
      lyric_output[4095] = L'\0';

      free(wlyric);
      free(lyric_text);
    } else {
      MessageBoxW(hWnd, STR_LYRIC_NOT_FOUND, STR_MSG_TITLE_TIPS,
                  MB_OK | MB_ICONWARNING);
    }
  }

  SetWindowTextW(hOutput, lyric_output);
}

void get_lyrics() {
  wchar_t input_text_w[512];
  char input_text[512];
  char song_id[32];
  wchar_t lyric_output[4096];
  int lyric_types = 0;
  CombineType combine_type = COMBINE_SEPARATE;
  char separator[16];

  GetWindowTextW(hInput, input_text_w, sizeof(input_text_w) / sizeof(wchar_t));
  if (wcslen(input_text_w) == 0) {
    MessageBoxW(hWnd, STR_NO_SONG_ID, STR_MSG_TITLE_TIPS,
                MB_OK | MB_ICONWARNING);
    return;
  }

  WideCharToMultiByte(CP_UTF8, 0, input_text_w, -1, input_text,
                      sizeof(input_text), NULL, NULL);
  extract_song_id_from_url(input_text, song_id);

  if (SendMessage(hRadioOrigin, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    lyric_types = LYRIC_ORIGINAL;
  } else if (SendMessage(hRadioTrans, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    lyric_types = LYRIC_TRANSLATED;
  } else if (SendMessage(hRadioRomaji, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    lyric_types = LYRIC_ROMAJI;
  } else if (SendMessage(hRadioOriginTrans, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    lyric_types = LYRIC_ORIGINAL | LYRIC_TRANSLATED;
  } else if (SendMessage(hRadioOriginRomaji, BM_GETCHECK, 0, 0) ==
             BST_CHECKED) {
    lyric_types = LYRIC_ORIGINAL | LYRIC_ROMAJI;
  } else if (SendMessage(hRadioTransRomaji, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    lyric_types = LYRIC_TRANSLATED | LYRIC_ROMAJI;
  } else if (SendMessage(hRadioAllThree, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    lyric_types = LYRIC_ORIGINAL | LYRIC_TRANSLATED | LYRIC_ROMAJI;
  }

  if (SendMessage(hRadioInterleave, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    combine_type = COMBINE_INTERLEAVE;
  } else if (SendMessage(hRadioMerge, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    combine_type = COMBINE_MERGE;
  }

  wchar_t sep_w[16];
  GetWindowTextW(hSeparatorInput, sep_w, sizeof(sep_w) / sizeof(wchar_t));
  WideCharToMultiByte(CP_UTF8, 0, sep_w, -1, separator, sizeof(separator), NULL,
                      NULL);
  if (strlen(separator) == 0) {
    strcpy(separator, "\n");
  }

  char *body = weapi(song_id);
  if (!body || (strlen(body) <= 15 && isspace(body[0]))) {
    MessageBoxW(hWnd, STR_NODEJS_ERROR, STR_MSG_TITLE_TIPS,
                MB_OK | MB_ICONWARNING);
    return;
  }

  debug("Request", body);

  char *response = send_http_request(
      "https://music.163.com/weapi/song/lyric", "POST",
      "Content-Type: application/x-www-form-urlencoded\r\nUser-Agent: "
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36\r\nReferer: https://music.163.com/\r\n",
      body);
  free(body);

  if (!response)
    return;

  debug("Response", response);

  extract_lyric(response, lyric_types, combine_type, separator, lyric_output);
  free(response);
}

void save_lyrics() {
  wchar_t folder_path[MAX_PATH];
  wchar_t song_id_w[128];
  wchar_t lyric_text[4096];

  GetWindowTextW(hInput, song_id_w, sizeof(song_id_w) / sizeof(wchar_t));
  GetWindowTextW(hOutput, lyric_text, sizeof(lyric_text) / sizeof(wchar_t));

  if (wcslen(song_id_w) == 0 || wcslen(lyric_text) == 0) {
    MessageBoxW(hWnd, STR_SAVE_ERROR, STR_MSG_TITLE_TIPS,
                MB_OK | MB_ICONWARNING);
    return;
  }

  GetWindowTextW(hFolderPath, folder_path,
                 sizeof(folder_path) / sizeof(wchar_t));
  if (wcslen(folder_path) == 0) {
    MessageBoxW(hWnd, STR_NO_FOLDER, STR_MSG_TITLE_TIPS,
                MB_OK | MB_ICONWARNING);
    return;
  }

  char song_id[128];
  char song_id_clean[128];
  wchar_t song_id_clean_w[128];
  wchar_t filename[MAX_PATH];
  wcscpy(song_id_clean_w, song_id_w);
  WideCharToMultiByte(CP_UTF8, 0, song_id_clean_w, -1, song_id_clean,
                      sizeof(song_id_clean), NULL, NULL);
  extract_song_id_from_url(song_id_clean, song_id);
  MultiByteToWideChar(CP_UTF8, 0, song_id, -1, song_id_w, MAX_PATH);
  swprintf(filename, MAX_PATH, L"%s\\%s.lrc", folder_path, song_id_w);

  HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    MessageBoxW(hWnd, STR_FILE_CREATE_ERROR, STR_MSG_TITLE_ERROR,
                MB_OK | MB_ICONERROR);
    debug("FileCreateErrorName", song_id);
    return;
  }

  int utf8_len =
      WideCharToMultiByte(CP_UTF8, 0, lyric_text, -1, NULL, 0, NULL, NULL);
  char *utf8_text = (char *)malloc(utf8_len);
  WideCharToMultiByte(CP_UTF8, 0, lyric_text, -1, utf8_text, utf8_len, NULL,
                      NULL);

  unsigned char bom[] = {0xEF, 0xBB, 0xBF};
  DWORD written;
  WriteFile(hFile, bom, 3, &written, NULL);

  WriteFile(hFile, utf8_text, utf8_len - 1, &written, NULL);

  CloseHandle(hFile);
  free(utf8_text);

  MessageBoxW(hWnd, STR_SAVE_SUCCESS, STR_MSG_TITLE_TIPS,
              MB_OK | MB_ICONINFORMATION);
}

void set_default_folder_path() {
  wchar_t default_path[MAX_PATH];

  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT,
                                 default_path))) {
    SetWindowTextW(hFolderPath, default_path);
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE: {
    const int MARGIN = 10;
    const int LABEL_WIDTH = 100;
    const int INPUT_WIDTH = 310;
    const int BUTTON_WIDTH = 100;
    const int CONTROL_HEIGHT = 25;
    const int RADIO_HEIGHT = 20;

    g_font =
        CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    g_bg_brush = CreateSolidBrush(RGB(255, 255, 255));

    int y_pos = MARGIN;

    CreateWindowW(L"STATIC", STR_SONG_ID_LABEL, WS_VISIBLE | WS_CHILD, MARGIN,
                  y_pos, LABEL_WIDTH, CONTROL_HEIGHT, hwnd, NULL, NULL, NULL);
    hInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                             WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
                             MARGIN + LABEL_WIDTH + 10, y_pos, INPUT_WIDTH,
                             CONTROL_HEIGHT, hwnd, (HMENU)ID_INPUT, NULL, NULL);

    hButton = CreateWindowW(
        L"BUTTON", STR_GET_LYRICS_BTN, WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        MARGIN + LABEL_WIDTH + INPUT_WIDTH + 20, y_pos, BUTTON_WIDTH,
        CONTROL_HEIGHT, hwnd, (HMENU)ID_BUTTON, NULL, NULL);

    y_pos += CONTROL_HEIGHT + MARGIN;

    CreateWindowW(L"STATIC", STR_LYRIC_TYPE_LABEL, WS_VISIBLE | WS_CHILD,
                  MARGIN, y_pos, LABEL_WIDTH, RADIO_HEIGHT, hwnd, NULL, NULL,
                  NULL);

    hRadioOrigin =
        CreateWindowW(L"BUTTON", STR_ORIGINAL_RADIO,
                      WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                      MARGIN + LABEL_WIDTH + 10, y_pos, 50, RADIO_HEIGHT, hwnd,
                      (HMENU)ID_RADIO_ORIGIN, NULL, NULL);

    hRadioTrans = CreateWindowW(
        L"BUTTON", STR_TRANSLATED_RADIO,
        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, MARGIN + LABEL_WIDTH + 70,
        y_pos, 50, RADIO_HEIGHT, hwnd, (HMENU)ID_RADIO_TRANS, NULL, NULL);

    hRadioRomaji = CreateWindowW(
        L"BUTTON", STR_ROMAJI_RADIO, WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        MARGIN + LABEL_WIDTH + 130, y_pos, 50, RADIO_HEIGHT, hwnd,
        (HMENU)ID_RADIO_ROMAJI, NULL, NULL);

    y_pos += RADIO_HEIGHT + 5;

    hRadioOriginTrans =
        CreateWindowW(L"BUTTON", STR_ORIGIN_TRANS_RADIO,
                      WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                      MARGIN + LABEL_WIDTH + 10, y_pos, 50, RADIO_HEIGHT, hwnd,
                      (HMENU)ID_RADIO_ORIGIN_TRANS, NULL, NULL);

    hRadioOriginRomaji =
        CreateWindowW(L"BUTTON", STR_ORIGIN_ROMAJI_RADIO,
                      WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                      MARGIN + LABEL_WIDTH + 70, y_pos, 50, RADIO_HEIGHT, hwnd,
                      (HMENU)ID_RADIO_ORIGIN_ROMAJI, NULL, NULL);

    hRadioTransRomaji =
        CreateWindowW(L"BUTTON", STR_TRANS_ROMAJI_RADIO,
                      WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
                      MARGIN + LABEL_WIDTH + 130, y_pos, 50, RADIO_HEIGHT, hwnd,
                      (HMENU)ID_RADIO_TRANS_ROMAJI, NULL, NULL);

    hRadioAllThree = CreateWindowW(
        L"BUTTON", STR_ALL_THREE_RADIO,
        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, MARGIN + LABEL_WIDTH + 190,
        y_pos, 70, RADIO_HEIGHT, hwnd, (HMENU)ID_RADIO_ALL_THREE, NULL, NULL);

    y_pos += RADIO_HEIGHT + MARGIN;

    CreateWindowW(L"STATIC", STR_COMBINE_METHOD_LABEL, WS_VISIBLE | WS_CHILD,
                  MARGIN, y_pos, LABEL_WIDTH, RADIO_HEIGHT, hwnd, NULL, NULL,
                  NULL);

    hRadioSeparate =
        CreateWindowW(L"BUTTON", STR_SEPARATE_RADIO,
                      WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
                      MARGIN + LABEL_WIDTH + 10, y_pos, 80, RADIO_HEIGHT, hwnd,
                      (HMENU)ID_RADIO_SEPARATE, NULL, NULL);

    hRadioInterleave = CreateWindowW(
        L"BUTTON", STR_INTERLEAVE_RADIO,
        WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, MARGIN + LABEL_WIDTH + 100,
        y_pos, 80, RADIO_HEIGHT, hwnd, (HMENU)ID_RADIO_INTERLEAVE, NULL, NULL);

    hRadioMerge = CreateWindowW(
        L"BUTTON", STR_MERGE_RADIO, WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        MARGIN + LABEL_WIDTH + 190, y_pos, 90, RADIO_HEIGHT, hwnd,
        (HMENU)ID_RADIO_MERGE, NULL, NULL);

    CreateWindowW(L"STATIC", STR_SEPARATOR_LABEL, WS_VISIBLE | WS_CHILD,
                  MARGIN + LABEL_WIDTH + 280, y_pos, 50, RADIO_HEIGHT, hwnd,
                  NULL, NULL, NULL);

    hSeparatorInput = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", STR_DEFAULT_SEPARATOR,
        WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, MARGIN + LABEL_WIDTH + 340,
        y_pos, 90, RADIO_HEIGHT, hwnd, (HMENU)ID_SEPARATOR_INPUT, NULL, NULL);
    y_pos += CONTROL_HEIGHT + MARGIN;

    hOutput = CreateWindowExW(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE, L"EDIT", L"",
                              WS_VISIBLE | WS_CHILD | ES_MULTILINE |
                                  ES_READONLY | WS_VSCROLL,
                              MARGIN, y_pos, WINDOW_MIN_WIDTH - MARGIN * 2, 450,
                              hwnd, (HMENU)ID_OUTPUT, NULL, NULL);

    y_pos += 450 + MARGIN;

    CreateWindowW(L"STATIC", STR_SAVE_PATH_LABEL, WS_VISIBLE | WS_CHILD, MARGIN,
                  y_pos, LABEL_WIDTH, CONTROL_HEIGHT, hwnd, NULL, NULL, NULL);
    hFolderPath = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_READONLY,
        MARGIN + LABEL_WIDTH + 10, y_pos, INPUT_WIDTH - 90, CONTROL_HEIGHT,
        hwnd, (HMENU)ID_FOLDER_PATH, NULL, NULL);
    hFolderButton = CreateWindowW(
        L"BUTTON", STR_BROWSE_BTN, WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        MARGIN + LABEL_WIDTH + INPUT_WIDTH - 70, y_pos, 80, CONTROL_HEIGHT,
        hwnd, (HMENU)ID_FOLDER_BUTTON, NULL, NULL);
    hSaveButton = CreateWindowW(
        L"BUTTON", STR_SAVE_LYRICS_BTN, WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        MARGIN + LABEL_WIDTH + INPUT_WIDTH + 20, y_pos, BUTTON_WIDTH,
        CONTROL_HEIGHT, hwnd, (HMENU)ID_SAVE_BUTTON, NULL, NULL);

    SendMessage(hOutput, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hInput, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hButton, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioOrigin, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioTrans, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioRomaji, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioOriginTrans, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioOriginRomaji, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioTransRomaji, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioAllThree, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioSeparate, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioInterleave, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioMerge, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hFolderPath, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hFolderButton, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hSaveButton, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hSeparatorInput, WM_SETFONT, (WPARAM)g_font, TRUE);

    SendMessage(hRadioOrigin, BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(hRadioSeparate, BM_SETCHECK, BST_CHECKED, 0);

    set_default_folder_path();

    ACCEL accel[] = {{FCONTROL, 'A', ID_SELECT_ALL}};
    HACCEL hAccel = CreateAcceleratorTable(accel, 1);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)hAccel);
    break;
  }

  case WM_CTLCOLOREDIT: {
    HDC hdc = (HDC)wParam;
    HWND hControl = (HWND)lParam;

    if (hControl == hOutput || hControl == hInput || hControl == hFolderPath ||
        hControl == hSeparatorInput) {
      SetBkMode(hdc, OPAQUE);
      SetBkColor(hdc, RGB(255, 255, 255));
      SetTextColor(hdc, RGB(0, 0, 0));
      return (LRESULT)g_bg_brush;
    }
    break;
  }

  case WM_CTLCOLORSTATIC: {
    HDC hdc = (HDC)wParam;
    return (LRESULT)g_bg_brush;
  }

  case WM_COMMAND:
    if (LOWORD(wParam) == ID_BUTTON && HIWORD(wParam) == BN_CLICKED) {
      get_lyrics();
    } else if (LOWORD(wParam) == ID_FOLDER_BUTTON &&
               HIWORD(wParam) == BN_CLICKED) {
      wchar_t folderPath[MAX_PATH];
      if (SelectFolder(hwnd, folderPath, MAX_PATH)) {
        SetWindowTextW(hFolderPath, folderPath);
      }
    } else if (LOWORD(wParam) == ID_SAVE_BUTTON &&
               HIWORD(wParam) == BN_CLICKED) {
      save_lyrics();
    } else if (LOWORD(wParam) == ID_SELECT_ALL) {
      HWND hFocused = GetFocus();
      if (hFocused == hInput || hFocused == hOutput) {
        SendMessage(hFocused, EM_SETSEL, 0, -1);
      }
    }
    break;

  case WM_DESTROY:
    if (g_font != NULL)
      DeleteObject(g_font);
    if (g_bg_brush != NULL)
      DeleteObject(g_bg_brush);
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
                   int nCmdShow) {
  hInstance = h;
  WNDCLASSW wc = {0};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = STR_WINDOW_CLASS_NAME;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);

  wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_APP_ICON));
  if (!wc.hIcon)
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

  if (!RegisterClassW(&wc))
    return 1;

  hWnd =
      CreateWindowExW(0, STR_WINDOW_CLASS_NAME, STR_WINDOW_TITLE,
                      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                      CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_MIN_WIDTH,
                      WINDOW_MIN_HEIGHT, NULL, NULL, hInstance, NULL);
  if (hWnd == NULL)
    return 1;

  g_debug_mode = (strstr(lpCmdLine, "debug") != NULL);

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  MSG msg = {0};
  HACCEL hAccel = (HACCEL)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (!TranslateAccelerator(hWnd, hAccel, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return 0;
}
