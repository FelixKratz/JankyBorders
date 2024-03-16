#pragma once
#include "extern.h"
#include "sys/stat.h"
#include "ApplicationServices/ApplicationServices.h"

#define DELAY_ASYNC_EXEC_ON_MAIN_THREAD(delay, code) {\
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{\
    usleep(delay);\
    dispatch_async(dispatch_get_main_queue(), ^{\
      code\
    });\
  });\
}

static inline void debug(const char* message, ...) {
#ifdef DEBUG
  va_list va;
  va_start(va, message);
  vprintf(message, va);
  va_end(va);
#endif
}

static inline void error(const char* message, ...) {
  va_list va;
  va_start(va, message);
  vfprintf(stderr, message, va);
  va_end(va);
  exit(EXIT_FAILURE);
}

static inline bool file_exists(const char* filename) {
  struct stat buffer;
  if (stat(filename, &buffer) != 0) return false;
  if (buffer.st_mode & S_IFDIR) return false;
  return true;
}

static inline bool file_setx(const char* filename) {
  struct stat buffer;
  if (stat(filename, &buffer) != 0) return false;
  bool is_executable = buffer.st_mode & S_IXUSR;
  if (!is_executable && chmod(filename, S_IXUSR | buffer.st_mode) != 0) {
    return false;
  }
  return true;
}

static inline void execute_config_file(const char* name, const char* filename) {
  char *home = getenv("HOME");
  if (!home) return;

  uint32_t size = strlen(home) + strlen(name) + strlen(filename) + 256;
  char path[size];
  snprintf(path, size, "%s/.config/%s/%s", home, name, filename);
  if (!file_exists(path)) {
    snprintf(path, size, "%s/.%s", home, filename);
    if (!file_exists(path)) {
      debug("No config file found...\n");
      return;
    };
  }

  if (!file_setx(path)) {
    printf("[!] Failed to make config at '%s' executable...\n", path);
    return;
  }

  signal(SIGCHLD, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);

  int pid = fork();
  if (pid !=  0) return;

  alarm(60);
  char* exec[] = { "/usr/bin/env", "sh", "-c", path, NULL };
  exit(execvp(exec[0], exec));
}

static inline  CFArrayRef cfarray_of_cfnumbers(void* values, size_t size, int count, CFNumberType type) {
  CFNumberRef temp[count];

  for (int i = 0; i < count; ++i) {
    temp[i] = CFNumberCreate(NULL, type, ((char *)values) + (size * i));
  }

  CFArrayRef result = CFArrayCreate(NULL,
                                    (const void **)temp,
                                    count,
                                    &kCFTypeArrayCallBacks);

  for (int i = 0; i < count; ++i) CFRelease(temp[i]);

  return result;
}
