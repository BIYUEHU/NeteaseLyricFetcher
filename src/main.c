#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include "utils.h"
#include "weapi.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define ID_INPUT 1001
#define ID_BUTTON 1002
#define ID_OUTPUT 1003
#define ID_RADIO_ORIGIN 1004
#define ID_RADIO_TRANS 1005
#define ID_RADIO_ROMAJI 1006
#define ID_SELECT_ALL 1007
#define IDI_APP_ICON 1

typedef enum
{
  LYRIC_ORIGINAL = 0,
  LYRIC_TRANSLATED = 1,
  LYRIC_ROMAJI = 2
} LyricType;

HWND hWnd, hInput, hOutput, hButton;
HWND hRadioOrigin, hRadioTrans, hRadioRomaji;

static BOOL g_debug_mode = FALSE;
static HBRUSH g_bg_brush = NULL;
static HFONT g_font = NULL;
static const int WINDOW_MIN_WIDTH = 600;
static const int WINDOW_MIN_HEIGHT = 550;

static void debug(const char *label, const char *content)
{
  if (g_debug_mode)
    write_log(label, content);
}

char *send_http_request(const char *url, const char *method, const char *headers, const char *body)
{
  HINTERNET hInternet, hConnect, hRequest;
  char *response = NULL;
  DWORD responseSize = 0;

  hInternet = InternetOpenA("WinAPI HTTP Client", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
  if (!hInternet)
  {
    MessageBoxW(hWnd, L"无法初始化网络连接", L"错误", MB_OK | MB_ICONERROR);
    return NULL;
  }

  char hostname[256] = {0};
  char path[1024] = {0};
  int port = 80;
  BOOL isHttps = FALSE;

  if (strncmp(url, "https://", 8) == 0)
  {
    isHttps = TRUE;
    port = 443;
    strcpy(hostname, url + 8);
  }
  else if (strncmp(url, "http://", 7) == 0)
  {
    strcpy(hostname, url + 7);
  }
  else
  {
    strcpy(hostname, url);
  }

  char *pathStart = strchr(hostname, '/');
  if (pathStart)
  {
    strcpy(path, pathStart);
    *pathStart = '\0';
  }
  else
  {
    strcpy(path, "/");
  }

  char *portStart = strchr(hostname, ':');
  if (portStart)
  {
    port = atoi(portStart + 1);
    *portStart = '\0';
  }

  hConnect = InternetConnectA(hInternet, hostname, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
  if (!hConnect)
  {
    MessageBoxW(hWnd, L"无法连接到服务器", L"错误", MB_OK | MB_ICONERROR);
    InternetCloseHandle(hInternet);
    return NULL;
  }

  DWORD flags = INTERNET_FLAG_RELOAD;
  if (isHttps)
  {
    flags |= INTERNET_FLAG_SECURE;
  }

  hRequest = HttpOpenRequestA(hConnect, method, path, NULL, NULL, NULL, flags, 0);
  if (!hRequest)
  {
    MessageBoxW(hWnd, L"无法创建HTTP请求", L"错误", MB_OK | MB_ICONERROR);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return NULL;
  }

  BOOL result;
  if (strcmp(method, "POST") == 0 && body && strlen(body) > 0)
  {
    result = HttpSendRequestA(hRequest, headers, headers ? strlen(headers) : 0, (LPVOID)body, strlen(body));
  }
  else
  {
    result = HttpSendRequestA(hRequest, headers, headers ? strlen(headers) : 0, NULL, 0);
  }

  if (!result)
  {
    MessageBoxW(hWnd, L"发送HTTP请求失败", L"错误", MB_OK | MB_ICONERROR);
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

  while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
  {
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

void extract_lyric(const char *json_response, LyricType lyric_type, wchar_t *lyric_output)
{
  if (json_response == NULL || lyric_output == NULL)
  {
    MessageBoxW(hWnd, L"参数错误", L"错误", MB_OK | MB_ICONERROR);
    return;
  }

  const char *lyric_key;

  switch (lyric_type)
  {
  case LYRIC_ORIGINAL:
    lyric_key = "\"lrc\":{\"version\":";
    break;
  case LYRIC_TRANSLATED:
    lyric_key = "\"tlyric\":{\"version\":";
    break;
  case LYRIC_ROMAJI:
    lyric_key = "\"romalrc\":{\"version\":";
    break;
  default:
    lyric_key = "\"lrc\":{\"version\":";
    break;
  }

  char *start = strstr(json_response, lyric_key);
  if (!start)
  {
    MessageBoxW(hWnd, L"未找到歌词", L"提示", MB_OK | MB_ICONWARNING);
    return;
  }

  char *lyric_start = strstr(start, "\"lyric\":\"");
  if (!lyric_start)
  {
    MessageBoxW(hWnd, L"歌词格式错误", L"提示", MB_OK | MB_ICONERROR);
    return;
  }

  lyric_start += 9;
  char *lyric_end = strstr(lyric_start, "\"}");
  if (!lyric_end)
  {
    MessageBoxW(hWnd, L"歌词解析错误", L"提示", MB_OK | MB_ICONERROR);
    return;
  }

  int lyric_len = lyric_end - lyric_start;
  if (lyric_len > 8192)
    lyric_len = 8192;

  char *temp_lyric = (char *)malloc(lyric_len + 1);
  strncpy(temp_lyric, lyric_start, lyric_len);
  temp_lyric[lyric_len] = '\0';

  char *processed_lyric = (char *)malloc(lyric_len * 2 + 1);
  char *src = temp_lyric;
  char *dst = processed_lyric;

  while (*src)
  {
    if (*src == '\\' && *(src + 1) == 'n')
    {
      *dst++ = '\r';
      *dst++ = '\n';
      src += 2;
    }
    else if (*src == '\\' && *(src + 1) == 't')
    {
      *dst++ = '\t';
      src += 2;
    }
    else if (*src == '\\' && *(src + 1) == '"')
    {
      *dst++ = '"';
      src += 2;
    }
    else if (*src == '\\' && *(src + 1) == '\\')
    {
      *dst++ = '\\';
      src += 2;
    }
    else if (*src == '\\' && *(src + 1) == 'u')
    {
      if (src[2] && src[3] && src[4] && src[5])
      {
        char unicode_str[5] = {src[2], src[3], src[4], src[5], '\0'};
        int unicode_val = strtol(unicode_str, NULL, 16);

        if (unicode_val > 0)
        {
          if (unicode_val < 0x80)
          {
            *dst++ = (char)unicode_val;
          }
          else if (unicode_val < 0x800)
          {
            *dst++ = 0xC0 | (unicode_val >> 6);
            *dst++ = 0x80 | (unicode_val & 0x3F);
          }
          else
          {
            *dst++ = 0xE0 | (unicode_val >> 12);
            *dst++ = 0x80 | ((unicode_val >> 6) & 0x3F);
            *dst++ = 0x80 | (unicode_val & 0x3F);
          }
        }
        src += 6;
      }
      else
      {
        *dst++ = *src++;
      }
    }
    else
    {
      *dst++ = *src++;
    }
  }
  *dst = '\0';

  int processed_lyric_len = MultiByteToWideChar(CP_UTF8, 0, processed_lyric, -1, NULL, 0);
  wchar_t *wlyric = (wchar_t *)malloc(processed_lyric_len * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, processed_lyric, -1, wlyric, processed_lyric_len);

  if (wlyric)
  {
    wcsncpy(lyric_output, wlyric, 4095);
    SetWindowTextW(hOutput, lyric_output);
    lyric_output[4095] = L'\0';
    free(wlyric);
  }
  else
  {
    MessageBoxW(hWnd, L"编码转换失败", L"错误", MB_OK | MB_ICONERROR);
  }

  free(temp_lyric);
  free(processed_lyric);
}

void get_lyrics()
{
  wchar_t song_id_w[32];
  char song_id[32];
  wchar_t lyric_output[4096];
  LyricType lyric_type = LYRIC_ORIGINAL;

  GetWindowTextW(hInput, song_id_w, sizeof(song_id_w) / sizeof(wchar_t));
  if (wcslen(song_id_w) == 0)
  {
    MessageBoxW(hWnd, L"请输入歌曲ID", L"提示", MB_OK | MB_ICONWARNING);
    return;
  }

  WideCharToMultiByte(CP_UTF8, 0, song_id_w, -1, song_id, sizeof(song_id), NULL, NULL);

  if (SendMessage(hRadioTrans, BM_GETCHECK, 0, 0) == BST_CHECKED)
  {
    lyric_type = LYRIC_TRANSLATED;
  }
  else if (SendMessage(hRadioRomaji, BM_GETCHECK, 0, 0) == BST_CHECKED)
  {
    lyric_type = LYRIC_ROMAJI;
  }

  char *body = weapi(song_id);
  if (!body || (strlen(body) <= 15 && isspace(body[0])))
  {
    MessageBoxW(hWnd, L"请求参数加密失败，检查是否安装 Node.js 环境", L"提示", MB_OK | MB_ICONWARNING);
    return;
  }

  debug("Request", body);

  char *response = send_http_request("https://music.163.com/weapi/song/lyric", "POST", "Content-Type: application/x-www-form-urlencoded\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\nReferer: https://music.163.com/\r\n", body);
  free(body);

  if (!response)
    return;

  debug("Response", response);

  extract_lyric(response, lyric_type, lyric_output);
}

BOOL SelectFolder(HWND hwnd, wchar_t *folderPath, int maxPath)
{
  BROWSEINFOW bi = {0};
  bi.hwndOwner = hwnd;
  bi.lpszTitle = L"选择文件夹";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
  if (pidl != NULL)
  {
    if (SHGetPathFromIDListW(pidl, folderPath))
    {
      CoTaskMemFree(pidl);
      return TRUE;
    }
    CoTaskMemFree(pidl);
  }
  return FALSE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_CREATE:
  {
    const int MARGIN = 10;
    const int LABEL_WIDTH = 60;
    const int INPUT_WIDTH = 200;
    const int BUTTON_WIDTH = 80;
    const int CONTROL_HEIGHT = 25;
    const int RADIO_HEIGHT = 20;

    g_font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    g_bg_brush = CreateSolidBrush(RGB(255, 255, 255));

    CreateWindowW(L"STATIC", L"歌曲ID:", WS_VISIBLE | WS_CHILD, MARGIN, MARGIN, LABEL_WIDTH, CONTROL_HEIGHT, hwnd, NULL, NULL, NULL);
    hInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_MULTILINE, MARGIN + LABEL_WIDTH + 10, MARGIN, INPUT_WIDTH, CONTROL_HEIGHT, hwnd, (HMENU)ID_INPUT, NULL, NULL);

    hRadioOrigin = CreateWindowW(L"BUTTON", L"原文", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, MARGIN, MARGIN * 2 + CONTROL_HEIGHT, LABEL_WIDTH, RADIO_HEIGHT, hwnd, (HMENU)ID_RADIO_ORIGIN, NULL, NULL);
    hRadioTrans = CreateWindowW(L"BUTTON", L"翻译", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, MARGIN + LABEL_WIDTH + 10, MARGIN * 2 + CONTROL_HEIGHT, LABEL_WIDTH, RADIO_HEIGHT, hwnd, (HMENU)ID_RADIO_TRANS, NULL, NULL);
    hRadioRomaji = CreateWindowW(L"BUTTON", L"罗马音", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, MARGIN + LABEL_WIDTH * 2 + 20, MARGIN * 2 + CONTROL_HEIGHT, LABEL_WIDTH + 10, RADIO_HEIGHT, hwnd, (HMENU)ID_RADIO_ROMAJI, NULL, NULL);

    hButton = CreateWindowW(L"BUTTON", L"获取歌词", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, MARGIN + LABEL_WIDTH + INPUT_WIDTH + 20, MARGIN, BUTTON_WIDTH, CONTROL_HEIGHT, hwnd, (HMENU)ID_BUTTON, NULL, NULL);

    hOutput = CreateWindowExW(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL, MARGIN, MARGIN * 3 + CONTROL_HEIGHT + RADIO_HEIGHT, WINDOW_MIN_WIDTH - MARGIN * 2, (WINDOW_MIN_HEIGHT - (MARGIN * 4 + CONTROL_HEIGHT + RADIO_HEIGHT)) * 0.95, hwnd, (HMENU)ID_OUTPUT, NULL, NULL);

    SendMessage(hOutput, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hInput, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hButton, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioOrigin, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioTrans, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(hRadioRomaji, WM_SETFONT, (WPARAM)g_font, TRUE);

    SendMessage(hRadioOrigin, BM_SETCHECK, BST_CHECKED, 0);

    ACCEL accel[] = {
        {FCONTROL, 'A', ID_SELECT_ALL}};
    HACCEL hAccel = CreateAcceleratorTable(accel, 1);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)hAccel);
    break;
  }

  case WM_CTLCOLOREDIT:
  {
    HDC hdc = (HDC)wParam;
    HWND hControl = (HWND)lParam;

    if (hControl == hOutput)
    {
      SetBkMode(hdc, OPAQUE);
      SetBkColor(hdc, RGB(255, 255, 255));
      SetTextColor(hdc, RGB(0, 0, 0));
      return (LRESULT)g_bg_brush;
      break;
    }
    if (hControl == hInput)
    {
      SetBkMode(hdc, OPAQUE);
      SetBkColor(hdc, RGB(255, 255, 255));
      SetTextColor(hdc, RGB(0, 0, 0));
      return (LRESULT)g_bg_brush;
    }
    break;
  }

  case WM_CTLCOLORSTATIC:
  {
    HDC hdc = (HDC)wParam;
    return (LRESULT)g_bg_brush;
  }

  case WM_COMMAND:
    if (LOWORD(wParam) == ID_BUTTON && HIWORD(wParam) == BN_CLICKED)
    {
      get_lyrics();
    }
    else if (LOWORD(wParam) == ID_SELECT_ALL)
    {
      HWND hFocused = GetFocus();
      if (hFocused == hInput || hFocused == hOutput)
      {
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
  default:
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  const wchar_t CLASS_NAME[] = L"NetEaseLyricWindow";

  WNDCLASSW wc = {0};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_APP_ICON));

  if (!RegisterClassW(&wc))
    return 1;

  hWnd = CreateWindowExW(0, CLASS_NAME, L"网易云音乐歌词获取器", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT, NULL, NULL, hInstance, NULL);

  if (hWnd == NULL)
    return 1;

  g_debug_mode = (strstr(lpCmdLine, "debug") != NULL);

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  MSG msg = {0};
  HACCEL hAccel = (HACCEL)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  while (GetMessage(&msg, NULL, 0, 0))
  {
    if (!TranslateAccelerator(hWnd, hAccel, &msg))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return 0;
}