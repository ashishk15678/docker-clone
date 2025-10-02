void *startDaemon();

#ifdef _WIN32

_WIN32 bool is_process_running(const char *process_name) {}

#endif
int is_process_running(const char *process_name);
