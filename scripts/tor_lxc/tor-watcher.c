/* This code provides following functionalities:
 * Check PSI pressure for memory and CPU
 * 	! Add "all" into logic for better analysis (only "some" rn from PSI)
 * 	! Add other mertrics when needed (loadavg, CPU usage)
 * 	! Add safe shutdown during spikes
 * Alert via discord webhook
 * Sleep using epoll, spending less clock cycles
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <curl/curl.h>

#define MAX_EVENTS 5
#define CHECK_INTERVAL_SEC 5

// Function to read PSI (same as before)
typedef struct {
    float avg10;
    float avg60;
    float avg300;
} PSIStats;

typedef struct {
    int pid;
    char name[256];
    unsigned long rss_kb; // Resident Set Size (RAM)
    unsigned long utime;  // CPU ticks user
    unsigned long stime;  // CPU ticks system
} ProcessInfo;

int find_max_mem_usage_proc(ProcessInfo *result) {
    DIR *dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent *entry;
    unsigned long max_rss = 0;
    result->pid = -1;

    while ((entry = readdir(dir)) != NULL) {
        // Check if directory name is a PID (all digits)
        if (entry->d_type == DT_DIR) {
            int pid = atoi(entry->d_name);
            if (pid <= 0) continue;

            char path[512];
            snprintf(path, sizeof(path), "/proc/%d/status", pid);
            FILE *proc_status = fopen(path, "r");
            if (!proc_status) continue;

            char line[256];
            char name[256] = "unknown";
            unsigned long rss = 0;

            while (fgets(line, sizeof(line), proc_status)) {
                if (strncmp(line, "Name:", 5) == 0) {
                    sscanf(line, "Name:\t%s", name);
                } else if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line, "VmRSS:\t%lu", &rss); // Value in kB
                }
            }
            fclose(proc_status);

            if (rss > max_rss) {
                max_rss = rss;
                result->pid = pid;
                strncpy(result->name, name, sizeof(result->name) - 1);
                result->rss_kb = rss;
            }
        }
    }
    closedir(dir);
    return (result->pid != -1) ? 0 : -1;
}

int remediate_process(int pid, const char *proc_name) {
    // Safety check: Never kill critical system pids (like systemd, kernel threads, init)
    if (pid <= 100) {
        fprintf(stderr, "Refusing to kill critical system PID: %d\n", pid);
        return -1;
    }

    printf("~_~ High PSI detected! Highest RAM consumer is  (PID: %d)\n", 
           proc_name, pid);

    // SIGTERM for graceful shutdown
    if (kill(pid, SIGTERM) == 0) {
        // Optional: wait a moment, then send SIGKILL if it refuses to drop
        usleep(2000000); // 2 seconds
        // kill(pid, SIGKILL); 
        return 0;
    } else {
        perror("Failed to send signal to process");
	return 1;
    }
}

int read_psi_pressure(const char *resource, PSIStats *stats) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/pressure/%s", resource);
    
    FILE *psi_file = fopen(path, "r");
    if (!psi_file) return -1;

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), psi_file)) {
        unsigned long long total;
        sscanf(buffer, "some avg10=%f avg60=%f avg300=%f total=%llu", 
               &stats->avg10, &stats->avg60, &stats->avg300, &total);
    }
    
    fclose(psi_file);
    return 0;
}

void send_discord_alert(const char *webhook_url, const char *title, const char *description, int color) {
    CURL *curl = curl_easy_init();
    if (!curl) return;
    
    char json_payload[1024];
    snprintf(
        json_payload, sizeof(json_payload), 
        "{\"embeds\": [{\"title\": \"%s\", \"description\": \"%s\", \"color\": %d}]}", 
        title, description, color
    );


    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, webhook_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set timeouts to prevent hanging (equivalent to requests timeout=(3.0, 10.0))
    // Connect timeout: max 3 seconds to establish connection
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    // Overall timeout: max 3 seconds for the entire transfer
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Discord alert failed: %s\n", curl_easy_strerror(res));
    }

    // Cleanup
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
}

int main(void) {
    char webhook_url[512];
    const char *api_key = getenv("basilisk_webhook");
    const int code_red = 15158332;
    const int code_orange = 16741120;
    const int code_yellow = 16773151;
    const int code_green = 446839;
    
    if (api_key == NULL) {
	fprintf(stderr, "Environment variable basilisk_webhook is not set\n");
	exit(EXIT_FAILURE);
    }
    
    snprintf(webhook_url, sizeof(webhook_url),
        "https://discordapp.com/api/webhooks/%s",
        api_key);

    // 1. Initialize cURL globally
    curl_global_init(CURL_GLOBAL_ALL);

    // 2. Create an epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    // 3. Create a timer file descriptor (replaces active sleeping)
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd == -1) {
        perror("timerfd_create");
        exit(EXIT_FAILURE);
    }

    // Configure timer to tick every CHECK_INTERVAL_SEC seconds
    struct itimerspec timer_spec;
    timer_spec.it_value.tv_sec = CHECK_INTERVAL_SEC;
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = CHECK_INTERVAL_SEC;
    timer_spec.it_interval.tv_nsec = 0;

    if (timerfd_settime(tfd, 0, &timer_spec, NULL) == -1) {
        perror("timerfd_settime");
        exit(EXIT_FAILURE);
    }

    // 4. Register the timer fd with epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tfd, &ev) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    printf("PSI Monitor Daemon started using epoll. Waiting for events...\n");

    struct epoll_event events[MAX_EVENTS];
    
    send_discord_alert(webhook_url, "Starting Tor-Watcher.c Service", "Watcher now monitoring tor_lxc resources", code_green);
    // 5. Event Loop (analogous to asyncio / selectors loop)
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == tfd) {
                // Clear the timer expiration counter (required for timerfd)
                uint64_t expirations;
                read(tfd, &expirations, sizeof(expirations));

                // Perform our periodic check
                PSIStats psi_stats;
		// For Memory first
                if (read_psi_pressure("memory", &psi_stats) == 0) {
                    // Example threshold: avg10 pressure > 5.0%
                    if (psi_stats.avg10 > 15) {
			// Add this logic after sustained 15% stall 
			ProcessInfo top_proc;
			if (find_max_mem_usage_proc(&top_proc) == 0) {
		            char msg[128];
			    snprintf(msg, sizeof(msg), "MEM_PSI (~10s): %.2f%% Beginning to find the culprit", psi_stats.avg10);
			    send_discord_alert(webhook_url, "!ALERT! Memory Pressure High!", msg, code_red);
			    remediate_process(top_proc.pid, top_proc.name);
			}
		    }
		    else if (psi_stats.avg10 > 5.0) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Kernel reports MEM_PSI (~10s): %.2f%%", psi_stats.avg10);
			send_discord_alert(webhook_url, "Memory Pressure High!", msg, code_red);
                    }
		    else if (psi_stats.avg10 > 1.25) {
			char msg[128];
			snprintf(msg, sizeof(msg), "Kernel reports MEM_PSI (~10s): %.2f%%", psi_stats.avg10);
			send_discord_alert(webhook_url, "Memory Pressure Increasing!", msg, code_orange);
		    }
                }
	    if (read_psi_pressure("cpu", &psi_stats) == 0) {
                    if (psi_stats.avg10 > 20.0) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Kernel reports CPU_PSI (~10s): %.2f%%", psi_stats.avg10);
                        send_discord_alert(webhook_url, "CPU Pressure High!", msg, code_red);
                    }
		    else if (psi_stats.avg10 > 5.0) {
			char msg[128];
			snprintf(msg, sizeof(msg), "Kernel reports CPU_PSI (~10s): %.2f%%", psi_stats.avg10);
			send_discord_alert(webhook_url, "CPU Pressure Increasing!", msg, code_orange);
		    }
                }

            }
            // You can easily scale this to handle other file descriptors 
            // (e.g., listening sockets, signal fd, inotify fd) in the same loop!
        }
    }

    close(tfd);
    close(epoll_fd);
    curl_global_cleanup();
    return 0;
}
