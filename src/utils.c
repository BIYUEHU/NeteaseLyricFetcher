#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_file(const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if (f == NULL)
    return NULL;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  rewind(f);

  char *buffer = malloc(size + 1);
  if (buffer == NULL)
  {
    fclose(f);
    return NULL;
  }

  fread(buffer, 1, size, f);
  buffer[size] = '\0';

  fclose(f);
  return buffer;
}

void write_file(const char *filename, const char *content)
{
  FILE *f = fopen(filename, "wb");
  if (f)
  {
    fwrite(content, 1, strlen(content), f);
    fclose(f);
  }
}

int delete_file(const char *filename)
{
  return remove(filename) == 0;
}

void write_log(const char *label, const char *content)
{
  FILE *f = fopen("debug.log", "a");
  if (!f)
  {
    fprintf(f, "[%s]\n%s\n\n", label, content);
    fclose(f);
  }
}
