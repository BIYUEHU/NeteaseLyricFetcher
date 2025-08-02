#ifndef STRINGS_H
#define STRINGS_H

#include <windows.h>

static const wchar_t STR_WINDOW_CLASS_NAME[] = L"NetEaseLyricWindow";
static const wchar_t *STR_WINDOW_TITLE = L"网易云音乐歌词获取器";
static const wchar_t *STR_WINAPI_CLIENT = L"WinAPI HTTP Client";

// UI Labels
static const wchar_t *STR_SONG_ID_LABEL = L"歌曲ID/链接:";
static const wchar_t *STR_LYRIC_TYPE_LABEL = L"歌词类型:";
static const wchar_t *STR_COMBINE_METHOD_LABEL = L"组合方式:";
static const wchar_t *STR_SEPARATOR_LABEL = L"分隔符:";
static const wchar_t *STR_SAVE_PATH_LABEL = L"保存路径:";

// Button Texts
static const wchar_t *STR_GET_LYRICS_BTN = L"获取歌词";
static const wchar_t *STR_BROWSE_BTN = L"浏览";
static const wchar_t *STR_SAVE_LYRICS_BTN = L"保存歌词";

// Radio Button Texts
static const wchar_t *STR_ORIGINAL_RADIO = L"原文";
static const wchar_t *STR_TRANSLATED_RADIO = L"译文";
static const wchar_t *STR_ROMAJI_RADIO = L"音译";
static const wchar_t *STR_ORIGIN_TRANS_RADIO = L"原译";
static const wchar_t *STR_ORIGIN_ROMAJI_RADIO = L"原音";
static const wchar_t *STR_TRANS_ROMAJI_RADIO = L"译音";
static const wchar_t *STR_ALL_THREE_RADIO = L"原译音";
static const wchar_t *STR_SEPARATE_RADIO = L"各排各的";
static const wchar_t *STR_INTERLEAVE_RADIO = L"交叉排列";
static const wchar_t *STR_MERGE_RADIO = L"合并同类";

// Error Messages
static const wchar_t *STR_NETWORK_INIT_ERROR = L"无法初始化网络连接";
static const wchar_t *STR_SERVER_CONNECT_ERROR = L"无法连接到服务器";
static const wchar_t *STR_HTTP_REQUEST_ERROR = L"无法创建HTTP请求";
static const wchar_t *STR_SEND_HTTP_REQUEST_ERROR = L"发送HTTP请求失败";
static const wchar_t *STR_PARAM_ERROR = L"参数错误";
static const wchar_t *STR_LYRIC_NOT_FOUND = L"未找到歌词";
static const wchar_t *STR_NO_SONG_ID = L"请输入歌曲ID或链接";
static const wchar_t *STR_NO_FOLDER = L"请选择保存文件夹";
static const wchar_t *STR_FILE_CREATE_ERROR = L"无法创建文件";
static const wchar_t *STR_NODEJS_ERROR =
    L"请求参数加密失败，检查是否安装 Node.js 环境";
static const wchar_t *STR_SAVE_ERROR = L"输入框或输出框为空，无法保存";
static const wchar_t *STR_SAVE_SUCCESS = L"歌词保存成功";

// Misc
static const wchar_t *STR_MSG_TITLE_TIPS = L"提示";
static const wchar_t *STR_MSG_TITLE_ERROR = L"错误";
static const wchar_t *STR_DEFAULT_SEPARATOR = L" ";

#endif // STRINGS_H
