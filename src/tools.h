#ifndef LYRICS_H
#define LYRICS_H

#include <wchar.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 歌词类型枚举，支持原文、翻译和罗马音。
 */
typedef enum {
  LYRIC_ORIGINAL = 1,   ///< 原文歌词
  LYRIC_TRANSLATED = 2, ///< 翻译歌词
  LYRIC_ROMAJI = 4      ///< 罗马音歌词
} LyricType;

/**
 * @brief 歌词合并方式。
 */
typedef enum {
  COMBINE_SEPARATE = 0,   ///< 每种歌词分段展示
  COMBINE_INTERLEAVE = 1, ///< 不同歌词类型交错显示
  COMBINE_MERGE = 2       ///< 多种歌词合并为一行
} CombineType;

/**
 * @brief 歌词行结构。
 */
typedef struct {
  char time_tag[32]; ///< 时间标签（如 [01:23.45]）
  char *content;     ///< 歌词文本内容
  int time_ms;       ///< 时间（以毫秒为单位）
} LyricLine;

BOOL SelectFolder(HWND hwnd, wchar_t *folderPath, int maxPath);

int parse_time_to_ms(const char *time_str);

/**
 * @brief 从输入 URL 中提取歌曲 ID。
 *
 * @param input 输入的字符串，可能包含 id 参数
 * @param song_id 输出的歌曲 ID，缓冲区至少应为 32 字节
 */
void extract_song_id_from_url(const char *input, char *song_id);

/**
 * @brief 解析歌词文本为歌词行数组。
 *
 * @param lyric_text 歌词文本（如 LRC 内容）
 * @param line_count 返回解析出的行数
 * @return 动态分配的歌词行数组，调用者需负责释放内存
 */
LyricLine *parse_lyric_lines(const char *lyric_text, int *line_count);

/**
 * @brief 合并不同类型的歌词并输出为宽字符字符串。
 *
 * @param json_response 包含歌词数据的 JSON 字符串
 * @param lyric_types 要包含的歌词类型位掩码（如 LYRIC_ORIGINAL |
 * LYRIC_TRANSLATED）
 * @param combine_type 合并策略（分开、交错或合并）
 * @param separator 合并时不同歌词之间的分隔符（如 " / "）
 * @param output 输出缓冲区（wchar_t[4096]）
 */
void combine_lyrics(const char *json_response, int lyric_types,
                    CombineType combine_type, const char *separator,
                    wchar_t *output);

/**
 * @brief 从 JSON 中提取指定类型歌词（你需自行实现或链接）。
 *
 * @param json JSON 字符串
 * @param type 歌词类型
 * @return 动态分配的字符串，调用者需负责释放内存
 */
char *extract_single_lyric_type(const char *json, LyricType type);

/**
 * @brief 将时间标签（如 [01:23.45]）转换为毫秒。
 *
 * @param tag 时间标签字符串
 * @return 毫秒数
 */
int parse_time_to_ms(const char *tag);

#ifdef __cplusplus
}
#endif

#endif // LYRICS_H
