#include "resource.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

char *weapi(char *input) {
  HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(ID_MACHINE_CODE), RT_RCDATA);
  HGLOBAL hData = LoadResource(NULL, hRes);
  DWORD size = SizeofResource(NULL, hRes);
  char *data = (char *)LockResource(hData);

  write_file("temp", data);

  char cmd[2048];
  snprintf(cmd, sizeof(cmd), "node temp \"%s\"", input);

  SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

  HANDLE hRead, hWrite;
  if (!CreatePipe(&hRead, &hWrite, &saAttr, 0)) {
    perror("CreatePipe failed");
    return NULL;
  }

  if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0)) {
    perror("SetHandleInformation failed");
    CloseHandle(hRead);
    CloseHandle(hWrite);
    return NULL;
  }

  PROCESS_INFORMATION pi;
  STARTUPINFOA si;
  ZeroMemory(&pi, sizeof(pi));
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.hStdOutput = hWrite;
  si.hStdError = hWrite;
  si.hStdInput = NULL;
  si.wShowWindow = SW_HIDE;

  BOOL success = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                                NULL, NULL, &si, &pi);

  if (!success) {
    perror("CreateProcess failed");
    CloseHandle(hRead);
    CloseHandle(hWrite);
    return NULL;
  }

  CloseHandle(hWrite);

  char buffer[1024];
  size_t capacity = 4096, length = 0;
  char *output = malloc(capacity);
  if (!output) {
    perror("malloc failed");
    CloseHandle(hRead);
    return NULL;
  }
  output[0] = '\0';

  DWORD bytesRead;
  while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) &&
         bytesRead > 0) {
    buffer[bytesRead] = '\0';
    size_t buf_len = strlen(buffer);

    if (length + buf_len + 1 > capacity) {
      capacity *= 2;
      char *tmp = realloc(output, capacity);
      if (!tmp) {
        free(output);
        CloseHandle(hRead);
        return NULL;
      }
      output = tmp;
    }

    strcpy(output + length, buffer);
    length += buf_len;
  }

  CloseHandle(hRead);
  WaitForSingleObject(pi.hProcess, INFINITE);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  // delete_file("temp");

  return output;
}
