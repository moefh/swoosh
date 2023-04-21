#ifndef UTIL_H_FILE
#define UTIL_H_FILE

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

void DebugLog(const char *fmt, ...);
void DebugDumpBytes(const void *data, size_t len);
char *WCharArrayToCharArray(char *str, size_t str_size, wchar_t *wstr);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_H_FILE */
