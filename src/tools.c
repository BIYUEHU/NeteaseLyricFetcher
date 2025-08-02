#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#pragma comment(lib, "shell32.lib")

typedef enum {
  LYRIC_ORIGINAL = 1,
  LYRIC_TRANSLATED = 2,
  LYRIC_ROMAJI = 4
} LyricType;

typedef enum {
  COMBINE_SEPARATE = 0,
  COMBINE_INTERLEAVE = 1,
  COMBINE_MERGE = 2
} CombineType;

typedef struct {
  char time_tag[32];
  char *content;
  int time_ms;
  int type; // LYRIC_ORIGINAL, LYRIC_TRANSLATED, LYRIC_ROMAJI
} LyricLine;

// 时间组合结构，用于存储同一时间的多种歌词
typedef struct {
  int time_ms;
  char time_tag[32];
  char *original;
  char *translated;
  char *romaji;
} TimeGroup;

BOOL SelectFolder(HWND hwnd, wchar_t *folderPath, int maxPath) {
  BROWSEINFOW bi = {0};
  bi.hwndOwner = hwnd;
  bi.lpszTitle = L"选择文件夹";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
  if (pidl != NULL) {
    if (SHGetPathFromIDListW(pidl, folderPath)) {
      CoTaskMemFree(pidl);
      return TRUE;
    }
    CoTaskMemFree(pidl);
  }
  return FALSE;
}

int parse_time_to_ms(const char *time_str) {
  int min = 0, sec = 0, ms = 0;
  if (sscanf(time_str, "[%d:%d.%d]", &min, &sec, &ms) == 3) {
    return min * 60000 + sec * 1000 + ms * 10;
  }
  return 0;
}

char *extract_single_lyric_type(const char *json_response,
                                int lyric_type_flag) {
  const char *lyric_key;

  switch (lyric_type_flag) {
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
    return NULL;
  }

  char *start = strstr(json_response, lyric_key);
  if (!start)
    return NULL;

  char *lyric_start = strstr(start, "\"lyric\":\"");
  if (!lyric_start)
    return NULL;

  lyric_start += 9;
  char *lyric_end = strstr(lyric_start, "\"}");
  if (!lyric_end)
    return NULL;

  int lyric_len = lyric_end - lyric_start;
  if (lyric_len <= 0)
    return NULL;

  char *temp_lyric = (char *)malloc(lyric_len + 1);
  strncpy(temp_lyric, lyric_start, lyric_len);
  temp_lyric[lyric_len] = '\0';

  char *processed_lyric = (char *)malloc(lyric_len * 2 + 1);
  char *src = temp_lyric;
  char *dst = processed_lyric;

  while (*src) {
    if (*src == '\\' && *(src + 1) == 'n') {
      *dst++ = '\r';
      *dst++ = '\n';
      src += 2;
    } else if (*src == '\\' && *(src + 1) == 't') {
      *dst++ = '\t';
      src += 2;
    } else if (*src == '\\' && *(src + 1) == '"') {
      *dst++ = '"';
      src += 2;
    } else if (*src == '\\' && *(src + 1) == '\\') {
      *dst++ = '\\';
      src += 2;
    } else if (*src == '\\' && *(src + 1) == 'u') {
      if (src[2] && src[3] && src[4] && src[5]) {
        char unicode_str[5] = {src[2], src[3], src[4], src[5], '\0'};
        int unicode_val = strtol(unicode_str, NULL, 16);

        if (unicode_val > 0) {
          if (unicode_val < 0x80) {
            *dst++ = (char)unicode_val;
          } else if (unicode_val < 0x800) {
            *dst++ = 0xC0 | (unicode_val >> 6);
            *dst++ = 0x80 | (unicode_val & 0x3F);
          } else {
            *dst++ = 0xE0 | (unicode_val >> 12);
            *dst++ = 0x80 | ((unicode_val >> 6) & 0x3F);
            *dst++ = 0x80 | (unicode_val & 0x3F);
          }
        }
        src += 6;
      } else {
        *dst++ = *src++;
      }
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';

  free(temp_lyric);
  return processed_lyric;
}

void extract_song_id_from_url(const char *input, char *song_id) {
  const char *id_param = strstr(input, "?id=");
  if (!id_param) {
    id_param = strstr(input, "&id=");
  }

  if (id_param) {
    id_param += 4;
    int i = 0;
    while (id_param[i] && id_param[i] != '&' && id_param[i] != '#' && i < 31) {
      song_id[i] = id_param[i];
      i++;
    }
    song_id[i] = '\0';
  } else {
    strcpy(song_id, input);
  }
}

LyricLine *parse_lyric_lines(const char *lyric_text, int *line_count,
                             int lyric_type) {
  if (!lyric_text)
    return NULL;

  int count = 0;
  char *temp = strdup(lyric_text);
  char *line = strtok(temp, "\r\n");
  while (line) {
    if (strstr(line, "[") && strstr(line, "]")) {
      count++;
    }
    line = strtok(NULL, "\r\n");
  }
  free(temp);

  if (count == 0)
    return NULL;

  LyricLine *lines = (LyricLine *)calloc(count, sizeof(LyricLine));
  *line_count = 0;

  temp = strdup(lyric_text);
  line = strtok(temp, "\r\n");
  while (line && *line_count < count) {
    char *bracket_end = strchr(line, ']');
    if (bracket_end) {
      int tag_len = bracket_end - line + 1;
      if (tag_len < 32) {
        strncpy(lines[*line_count].time_tag, line, tag_len);
        lines[*line_count].time_tag[tag_len] = '\0';
        lines[*line_count].time_ms = parse_time_to_ms(line);
        lines[*line_count].type = lyric_type;

        char *content = bracket_end + 1;
        if (strlen(content) > 0) {
          lines[*line_count].content = strdup(content);
          (*line_count)++;
        }
      }
    }
    line = strtok(NULL, "\r\n");
  }
  free(temp);

  return lines;
}

// 比较函数，用于按时间排序
int compare_time_groups(const void *a, const void *b) {
  TimeGroup *group_a = (TimeGroup *)a;
  TimeGroup *group_b = (TimeGroup *)b;
  return group_a->time_ms - group_b->time_ms;
}

// 创建时间组合数组
TimeGroup *create_time_groups(LyricLine *orig_lines, int orig_count,
                              LyricLine *trans_lines, int trans_count,
                              LyricLine *roma_lines, int roma_count,
                              int *group_count) {
  // 收集所有不同的时间戳
  int *all_times =
      (int *)malloc((orig_count + trans_count + roma_count) * sizeof(int));
  int time_count = 0;

  // 添加原文时间戳
  for (int i = 0; i < orig_count; i++) {
    int time_ms = orig_lines[i].time_ms;
    int found = 0;
    for (int j = 0; j < time_count; j++) {
      if (all_times[j] == time_ms) {
        found = 1;
        break;
      }
    }
    if (!found) {
      all_times[time_count++] = time_ms;
    }
  }

  // 添加翻译时间戳
  for (int i = 0; i < trans_count; i++) {
    int time_ms = trans_lines[i].time_ms;
    int found = 0;
    for (int j = 0; j < time_count; j++) {
      if (all_times[j] == time_ms) {
        found = 1;
        break;
      }
    }
    if (!found) {
      all_times[time_count++] = time_ms;
    }
  }

  // 添加罗马音时间戳
  for (int i = 0; i < roma_count; i++) {
    int time_ms = roma_lines[i].time_ms;
    int found = 0;
    for (int j = 0; j < time_count; j++) {
      if (all_times[j] == time_ms) {
        found = 1;
        break;
      }
    }
    if (!found) {
      all_times[time_count++] = time_ms;
    }
  }

  // 创建时间组合数组
  TimeGroup *groups = (TimeGroup *)calloc(time_count, sizeof(TimeGroup));
  *group_count = time_count;

  // 初始化每个时间组
  for (int i = 0; i < time_count; i++) {
    groups[i].time_ms = all_times[i];
    groups[i].original = NULL;
    groups[i].translated = NULL;
    groups[i].romaji = NULL;

    // 找到对应的时间标签（使用第一个匹配的）
    for (int j = 0; j < orig_count; j++) {
      if (orig_lines[j].time_ms == all_times[i]) {
        strcpy(groups[i].time_tag, orig_lines[j].time_tag);
        break;
      }
    }
    if (strlen(groups[i].time_tag) == 0) {
      for (int j = 0; j < trans_count; j++) {
        if (trans_lines[j].time_ms == all_times[i]) {
          strcpy(groups[i].time_tag, trans_lines[j].time_tag);
          break;
        }
      }
    }
    if (strlen(groups[i].time_tag) == 0) {
      for (int j = 0; j < roma_count; j++) {
        if (roma_lines[j].time_ms == all_times[i]) {
          strcpy(groups[i].time_tag, roma_lines[j].time_tag);
          break;
        }
      }
    }
  }

  // 填充每个时间组的内容
  for (int i = 0; i < time_count; i++) {
    int time_ms = groups[i].time_ms;

    // 查找原文
    for (int j = 0; j < orig_count; j++) {
      if (orig_lines[j].time_ms == time_ms) {
        groups[i].original = strdup(orig_lines[j].content);
        break;
      }
    }

    // 查找翻译
    for (int j = 0; j < trans_count; j++) {
      if (trans_lines[j].time_ms == time_ms) {
        groups[i].translated = strdup(trans_lines[j].content);
        break;
      }
    }

    // 查找罗马音
    for (int j = 0; j < roma_count; j++) {
      if (roma_lines[j].time_ms == time_ms) {
        groups[i].romaji = strdup(roma_lines[j].content);
        break;
      }
    }
  }

  // 按时间排序
  qsort(groups, time_count, sizeof(TimeGroup), compare_time_groups);

  free(all_times);
  return groups;
}

// 检查歌词内容是否为空或只包含空白字符
int is_content_empty(const char *content) {
  if (!content)
    return 1;

  // 跳过空白字符检查是否有实际内容
  while (*content && (*content == ' ' || *content == '\t' || *content == '\r' ||
                      *content == '\n')) {
    content++;
  }
  return *content == '\0';
}

void combine_lyrics(const char *json_response, int lyric_types,
                    CombineType combine_type, const char *separator,
                    wchar_t *output) {
  char *original = NULL, *translated = NULL, *romaji = NULL;
  LyricLine *orig_lines = NULL, *trans_lines = NULL, *roma_lines = NULL;
  int orig_count = 0, trans_count = 0, roma_count = 0;

  if (lyric_types & LYRIC_ORIGINAL) {
    original = extract_single_lyric_type(json_response, LYRIC_ORIGINAL);
    if (original) {
      orig_lines = parse_lyric_lines(original, &orig_count, LYRIC_ORIGINAL);
    }
  }

  if (lyric_types & LYRIC_TRANSLATED) {
    translated = extract_single_lyric_type(json_response, LYRIC_TRANSLATED);
    if (translated) {
      trans_lines =
          parse_lyric_lines(translated, &trans_count, LYRIC_TRANSLATED);
    }
  }

  if (lyric_types & LYRIC_ROMAJI) {
    romaji = extract_single_lyric_type(json_response, LYRIC_ROMAJI);
    if (romaji) {
      roma_lines = parse_lyric_lines(romaji, &roma_count, LYRIC_ROMAJI);
    }
  }

  char *result = (char *)malloc(65536);
  result[0] = '\0';

  if (combine_type == COMBINE_SEPARATE) {
    // 各排各的，过滤空内容
    if (orig_lines) {
      for (int i = 0; i < orig_count; i++) {
        if (!is_content_empty(orig_lines[i].content)) {
          strcat(result, orig_lines[i].time_tag);
          strcat(result, orig_lines[i].content);
          strcat(result, "\r\n");
        }
      }
    }
    if (trans_lines) {
      for (int i = 0; i < trans_count; i++) {
        if (!is_content_empty(trans_lines[i].content)) {
          strcat(result, trans_lines[i].time_tag);
          strcat(result, trans_lines[i].content);
          strcat(result, "\r\n");
        }
      }
    }
    if (roma_lines) {
      for (int i = 0; i < roma_count; i++) {
        if (!is_content_empty(roma_lines[i].content)) {
          strcat(result, roma_lines[i].time_tag);
          strcat(result, roma_lines[i].content);
          strcat(result, "\r\n");
        }
      }
    }
  } else if (combine_type == COMBINE_INTERLEAVE ||
             combine_type == COMBINE_MERGE) {
    // 创建时间组合
    int group_count = 0;
    TimeGroup *groups =
        create_time_groups(orig_lines, orig_count, trans_lines, trans_count,
                           roma_lines, roma_count, &group_count);

    if (groups) {
      for (int i = 0; i < group_count; i++) {
        if (combine_type == COMBINE_INTERLEAVE) {
          // 交叉排列：按原译音顺序分别输出，过滤空内容
          if ((lyric_types & LYRIC_ORIGINAL) && groups[i].original &&
              !is_content_empty(groups[i].original)) {
            strcat(result, groups[i].time_tag);
            strcat(result, groups[i].original);
            strcat(result, "\r\n");
          }
          if ((lyric_types & LYRIC_TRANSLATED) && groups[i].translated &&
              !is_content_empty(groups[i].translated)) {
            strcat(result, groups[i].time_tag);
            strcat(result, groups[i].translated);
            strcat(result, "\r\n");
          }
          if ((lyric_types & LYRIC_ROMAJI) && groups[i].romaji &&
              !is_content_empty(groups[i].romaji)) {
            strcat(result, groups[i].time_tag);
            strcat(result, groups[i].romaji);
            strcat(result, "\r\n");
          }
        } else if (combine_type == COMBINE_MERGE) {
          // 合并同类项：同一时间标签，用分隔符连接内容，过滤空内容
          int has_valid_content = 0;
          if ((lyric_types & LYRIC_ORIGINAL) && groups[i].original &&
              !is_content_empty(groups[i].original))
            has_valid_content = 1;
          if ((lyric_types & LYRIC_TRANSLATED) && groups[i].translated &&
              !is_content_empty(groups[i].translated))
            has_valid_content = 1;
          if ((lyric_types & LYRIC_ROMAJI) && groups[i].romaji &&
              !is_content_empty(groups[i].romaji))
            has_valid_content = 1;

          if (has_valid_content) {
            strcat(result, groups[i].time_tag);

            int first = 1;
            if ((lyric_types & LYRIC_ORIGINAL) && groups[i].original &&
                !is_content_empty(groups[i].original)) {
              strcat(result, groups[i].original);
              first = 0;
            }
            if ((lyric_types & LYRIC_TRANSLATED) && groups[i].translated &&
                !is_content_empty(groups[i].translated)) {
              if (!first)
                strcat(result, separator);
              strcat(result, groups[i].translated);
              first = 0;
            }
            if ((lyric_types & LYRIC_ROMAJI) && groups[i].romaji &&
                !is_content_empty(groups[i].romaji)) {
              if (!first)
                strcat(result, separator);
              strcat(result, groups[i].romaji);
            }

            strcat(result, "\r\n");
          }
        }
      }

      // 清理时间组合数组
      for (int i = 0; i < group_count; i++) {
        if (groups[i].original)
          free(groups[i].original);
        if (groups[i].translated)
          free(groups[i].translated);
        if (groups[i].romaji)
          free(groups[i].romaji);
      }
      free(groups);
    }
  }

  // 转换为宽字符
  int result_len = MultiByteToWideChar(CP_UTF8, 0, result, -1, NULL, 0);
  wchar_t *wresult = (wchar_t *)malloc(result_len * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, result, -1, wresult, result_len);

  wcsncpy(output, wresult, 4095);
  output[4095] = L'\0';

  // 清理内存
  free(result);
  free(wresult);
  if (original)
    free(original);
  if (translated)
    free(translated);
  if (romaji)
    free(romaji);

  if (orig_lines) {
    for (int i = 0; i < orig_count; i++) {
      if (orig_lines[i].content)
        free(orig_lines[i].content);
    }
    free(orig_lines);
  }
  if (trans_lines) {
    for (int i = 0; i < trans_count; i++) {
      if (trans_lines[i].content)
        free(trans_lines[i].content);
    }
    free(trans_lines);
  }
  if (roma_lines) {
    for (int i = 0; i < roma_count; i++) {
      if (roma_lines[i].content)
        free(roma_lines[i].content);
    }
    free(roma_lines);
  }
}
