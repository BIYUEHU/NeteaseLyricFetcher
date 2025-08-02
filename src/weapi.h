#ifndef __WEAPI_H__
#define __WEAPI_H__

/* The weapi function takes a string input, replaces a placeholder in machine
 * code with the input, writes the modified code to a temporary file, executes
 * it using Node.js, captures the output, and returns it as a dynamically
 * allocated string. It handles memory allocation, resizing, and cleanup but
 * lacks proper error handling for certain edge cases like returning error codes
 * instead of strings. */
char *weapi(const char *input);

#endif
