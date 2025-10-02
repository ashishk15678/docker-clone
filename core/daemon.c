
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <tlhelp32.h>
#include <windows.h>

int is_process_running_win(const char *process_name) {
  // ... (Windows API logic using CreateToolhelp32Snapshot) ...
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
    return false;

  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);
  int found = 0;

  if (Process32First(hSnapshot, &pe32)) {
    do {
      if (_stricmp(pe32.szExeFile, process_name) == 0) {
        found = 1;
        break;
      }
    } while (Process32Next(hSnapshot, &pe32));
  }

  CloseHandle(hSnapshot);
  return found;
}

#endif

#ifndef _WIN32
#include <dirent.h>

int is_process_running_linux(const char *process_name) {
  // ... (Linux logic reading /proc) ...
  DIR *dir;
  struct dirent *entry;

  if ((dir = opendir("/proc")) == NULL) {
    perror("opendir /proc failed");
    return 0;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (strspn(entry->d_name, "0123456789") == strlen(entry->d_name)) {
      char path[256];
      FILE *fp;
      char comm_name[256] = {0};

      snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
      if ((fp = fopen(path, "r")) != NULL) {
        if (fgets(comm_name, sizeof(comm_name) - 1, fp) != NULL) {
          comm_name[strcspn(comm_name, "\n")] = 0;
          if (strcmp(comm_name, process_name) == 0) {
            fclose(fp);
            closedir(dir);
            return 1;
          }
        }
        fclose(fp);
      }
    }
  }

  closedir(dir);
  return 0;
}

#endif

int is_process_running(const char *process_name) {
#ifdef _WIN32
  return is_process_running_win(process_name);
#else
  // Otherwise (i.e., we're on Linux, macOS, etc.)
  return is_process_running_linux(process_name);
#endif
}

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// This is the function that launches the detached process
int launch_detached_program(const char *path, const char *arg,
                            const char *file_name) {
  pid_t pid;

  pid = fork();

  if (pid < 0) {
    // Fork failed
    perror("First fork failed");
    return -1;
  }

  if (pid > 0) {
    return 0;
  }
  if (setsid() < 0) {
    perror("setsid failed");
    exit(EXIT_FAILURE);
  }

  pid_t grandchild_pid = fork();

  if (grandchild_pid < 0) {
    perror("Second fork failed");
    exit(EXIT_FAILURE);
  }

  if (grandchild_pid > 0) {
    exit(EXIT_SUCCESS);
  }

  printf("Grandchild (new process) is starting. PID: %d\n", getpid());

  freopen("/dev/null", "r", stdin);
  freopen("/dev/null", "w", stdout);
  freopen("/dev/null", "w", stderr);
  if (execlp(path, file_name, arg, NULL) == -1) {
    perror("execlp failed");
    exit(EXIT_FAILURE);
  }

  return 0;
}
