#ifndef __UTILS_H__
#define __UTILS_H__

/* File I/O Utilities */

/* Read a file and return its contents as a string */
char *read_file(const char *filename);

/* Write a string to a file */
void write_file(const char *filename, const char *content);

/* Delete a file */
int delete_file(const char *filename);

/* Write a log entry with a label and content */
void write_log(const char *label, const char *content);

/* 通用错误码 */
enum ResultCode
{
  RESULT_SUCCESS = 0,
  RESULT_FILE_ERROR = -1,
  RESULT_MEMORY_ERROR = -2
};

#endif
