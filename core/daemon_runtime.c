#include "daemon.h"
#include "http.h"
#include "container.h"
#include "image.h"
#include <linux/prctl.h>
#include <sys/prctl.h>

static int run_daemon_foreground(void);

int daemon_main() {
    return run_daemon_foreground();
}


// custom signal handler for Ctrl + c or any other signals
// graceful shutdown
void daemon_signal_handler(int sig) {
    fprintf(stderr, "Daemon received signal %d, shutting down...\n", sig);

    cleanup_image_system();
    cleanup_container_system();

    exit(EXIT_SUCCESS);
}

static int run_daemon_foreground(void) {
    if (prctl(PR_SET_NAME, DAEMON_NAME, 0, 0, 0) < 0) {
        perror("prctl");
    }

    // custom signal handler called
    signal(SIGTERM, daemon_signal_handler);
    signal(SIGINT, daemon_signal_handler);

    if (init_image_system() != 0) {
        fprintf(stderr, "Failed to initialize image system\n");
        return -1;
    }

    if (init_container_system() != 0) {
        fprintf(stderr, "Failed to initialize container system\n");
        cleanup_image_system();
        return -1;
    }

    int result = start_http_server(DAEMON_PORT);
    printf("Docker daemon listening on port %d\n", DAEMON_PORT);

    cleanup_image_system();
    cleanup_container_system();

    return result;
}
