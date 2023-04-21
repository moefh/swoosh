
#include "util.h"

#ifdef _WIN32
#include "targetver.h"
#include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>

char *WCharArrayToCharArray(char *str, size_t str_size, wchar_t *wstr)
{
  if (str_size == 0) return NULL;
  char *p = str;
  for (size_t i = 0; wstr[i]; i++) {
    if (i == str_size - 1) break;
    *p++ = (char) (unsigned char) wstr[i];
  }
  *p = '\0';
  return str;
}

void DebugLog(const char *fmt, ...)
{
    va_list ap;

#ifdef _WIN32
    char str[1024];

    va_start(ap, fmt);
    vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    OutputDebugStringA(str);
#else
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
#endif
}

void DebugDumpBytes(const void *data, size_t len)
{
  const unsigned char *p = data;
  while (len > 0) {
    char line_bytes[80];
    char line_text[80];
    size_t line_len = (len > 16) ? 16 : len;
    char *p_bytes = line_bytes;
    char *p_text = line_text;
    for (size_t i = 0; i < line_len; i++) {
      p_bytes += snprintf(p_bytes, sizeof(line_bytes)-(p_bytes-line_bytes), "%02x ", p[i]);
      p_text  += snprintf(p_text, sizeof(line_bytes)-(p_bytes-line_bytes), "%c", (p[i] >= 32 && p[i] < 127) ? p[i] : '.');
    }
    for (size_t i = line_len; i < 16; i++) {
      p_bytes += snprintf(p_bytes, sizeof(line_bytes)-(p_bytes-line_bytes), "   ");
      p_text  += snprintf(p_text, sizeof(line_bytes)-(p_bytes-line_bytes), " ");
    }
    DebugLog("%s | %s |\n", line_bytes, line_text);
    len -= line_len;
    p += line_len;
  }
}
