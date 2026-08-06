/* This code provides following functionalities:
 * Check PSI pressure for memory and CPU
 * 	! Add "all" into logic for better analysis (only "some" rn from PSI)
 * 	! Add other mertrics when needed (loadavg, CPU usage)
 * 	! Add safe shutdown during spikes
 * 	    + Confirm if [!CORRECT!]  process ended?
 *	    + Add force kill condition
 * Alert via discord webhook
 * Sleep using epoll, spending less clock cycles
 * Checks SHA256sum to verify TORRC file's integrity
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/timerfd.h>
#include <curl/curl.h>
#include <openssl/evp.h>

#define MAX_EVENTS 5
#define CHECK_INTERVAL_SEC 5


/* STRUCTS
 * *******************************************************
 * PSIStats: 	Stores /proc/pressure/<<psi_file>> data
 * ProcessInfo: Stores system process information
 */

/* FUNCTIONS
 * *******************************************************
 * get_sha256(const char *, char *)
 * 	Calculate hex of the file at the path provided.
 * find_max_mem_usage_proc(ProcessInfo *)
 * 	Find the process using the most memory.
 * remediate_process(int, const char *)
 * read_psi_pressure(const char *, PSIStats *)
 * send_discord_alert(const char *, const char *, const char *, int)
 */

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

// Get SHA256SUM of file
int get_sha256(const char *filename, char *output_hex_buffer) {
    FILE *file = fopen(filename, "rb");
    if (!file) return -1;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fclose(file);
        return -1;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return -1;
    }

    unsigned char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(ctx, buffer, bytes_read) != 1) {
            EVP_MD_CTX_free(ctx);
            fclose(file);
            return -1;
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return -1;
    }

    // Convert raw binary hash to hex string
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(output_hex_buffer + (i * 2), "%02x", hash[i]);
    }
    output_hex_buffer[hash_len * 2] = '\0';

    EVP_MD_CTX_free(ctx);
    fclose(file);
    return 0;
}

// Find Process using the most memory
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

// Close the process
int remediate_process(int pid, const char *proc_name) {
    // Safety check: Never kill critical system pids (like systemd, kernel threads, init)
    if (pid <= 100) {
        fprintf(stderr, "Refusing to kill critical system PID: %d\n", pid);
        return -1;
    }

    printf("~_~ High PSI detected! Highest RAM consumer is %s (PID: %d)\n", 
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

// Read PSI file
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


// Send alert via Discord Webhook
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
    char torrc_hash[65];
    const char *api_key = getenv("basilisk_webhook");
    const char *torrc_path = "/etc/tor/torrc";
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
        perror("epoll_create1");
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

    // 4.5. Initialize inotify to watch torrc configuration changes
    int inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    // Watch the specific file for modifications (or watch the parent directory if files are replaced atomically)
    int watch_descriptor = inotify_add_watch(inotify_fd, torrc_path, IN_MODIFY);
    if (watch_descriptor == -1) {
        perror("inotify_add_watch");
        // Non-fatal if file doesn't exist yet, but handle accordingly
    }

    // Register the inotify fd with epoll
    struct epoll_event ev_inotify;
    ev_inotify.events = EPOLLIN;
    ev_inotify.data.fd = inotify_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &ev_inotify) == -1) {
        perror("epoll_ctl (inotify)");
        exit(EXIT_FAILURE);
    }

    printf("PSI Monitor Daemon started using epoll. Waiting for events...\n");

    struct epoll_event events[MAX_EVENTS];
    
    send_discord_alert(webhook_url, "Starting Tor-Watcher.c Service v1.0b", "Watcher now monitoring tor_lxc resources", code_green);
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

            } else if (events[i].data.fd == inotify_fd) {
                // Read the inotify event buffer (required to clear the readiness state)
                char buf[4096]
                __attribute__ ((aligned(__alignof__(struct inotify_event))));
                ssize_t len = read(inotify_fd, buf, sizeof(buf));
                
                if (len > 0) {
                    // File was modified! Recalculate hash and trigger action
                    char new_hash[65];
                    if (get_sha256(torrc_path, new_hash) == 0) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "New SHA256sum: `%s`", new_hash);
                        send_discord_alert(webhook_url, "TORRC MODIFIED!", msg, code_red);
                    }
                }
            }
            
            // You can easily scale this to handle other file descriptors 
            // (e.g., listening sockets, signal fd, inotify fd) in the same loop!
        }
    }
    close(inotify_fd);
    close(tfd);
    close(epoll_fd);
    curl_global_cleanup();
    return 0;
}
