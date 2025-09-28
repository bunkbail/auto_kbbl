#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>      // For PATH_MAX
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <getopt.h>      // For getopt_long

// Use PATH_MAX for buffer sizes, with a fallback
#ifdef PATH_MAX
  #define KBBL_BUFFER_SIZE PATH_MAX
#else
  #define KBBL_BUFFER_SIZE 4096 // A common fallback for PATH_MAX
#endif

// Default values (can be overridden by flags)
#define DEFAULT_BACKLIGHT_TIMEOUT 2 // seconds
#define DEFAULT_BRIGHTNESS_ON -1    // -1 means use max_brightness
#define DEFAULT_INITIAL_OFF 0       // 0 means start ON

// Helper macros for EVIOCGBIT bit manipulation
#ifndef BITS_PER_LONG
#define BITS_PER_LONG (sizeof(long) * 8)
#endif
#ifndef NLONGS
#define NLONGS(x) (((x) + BITS_PER_LONG - 1) / BITS_PER_LONG)
#endif
#ifndef test_bit
#define test_bit(nr, addr) (((1UL << ((nr) % BITS_PER_LONG)) & ((unsigned long *)(addr))[(nr) / BITS_PER_LONG]) != 0)
#endif

// Global variables for configuration
int verbose_mode = 0;
int backlight_timeout = DEFAULT_BACKLIGHT_TIMEOUT;
int desired_brightness_on = DEFAULT_BRIGHTNESS_ON;
int initial_off_mode = DEFAULT_INITIAL_OFF;
char input_dev_path[KBBL_BUFFER_SIZE] = ""; // User-specified or auto-detected
char led_brightness_path[KBBL_BUFFER_SIZE];
char led_max_brightness_path[KBBL_BUFFER_SIZE];
char user_led_name_fragment[KBBL_BUFFER_SIZE] = ""; // User-specified fragment for LED name

// Function to print usage information
void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [options]\n\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help            Display this help message and exit.\n");
    fprintf(stderr, "  -v, --verbose         Enable verbose output.\n");
    fprintf(stderr, "  -t, --timeout SECS    Set inactivity timeout in seconds (default: %d).\n", DEFAULT_BACKLIGHT_TIMEOUT);
    fprintf(stderr, "  -b, --brightness VAL  Set 'on' brightness level. Can be absolute (0-max) or percentage (e.g., '50%%').\n");
    fprintf(stderr, "  -d, --device PATH     Manually specify keyboard event device path (e.g., /dev/input/event3).\n");
    fprintf(stderr, "  -l, --led NAME_FRAG   Manually specify LED device name fragment (e.g., 'kbd_backlight').\n");
    fprintf(stderr, "  -i, --initial-off     Start with backlight off, turn on only after first keypress.\n");
    fprintf(stderr, "\n");
}


// Find a suitable keyboard input device
int find_keyboard_device(char *path_out) {
    DIR *dir;
    struct dirent *entry;
    char test_path[KBBL_BUFFER_SIZE];

    // If user provided a path, use it directly
    if (input_dev_path[0] != '\0') {
        if (access(input_dev_path, R_OK) == 0) {
            strncpy(path_out, input_dev_path, KBBL_BUFFER_SIZE -1);
            path_out[KBBL_BUFFER_SIZE-1] = '\0';
            if (verbose_mode) fprintf(stdout, "Using user-specified keyboard device: %s\n", path_out);
            return 0;
        } else {
            fprintf(stderr, "ERROR: User-specified keyboard device '%s' is not accessible.\n", input_dev_path);
            return -1;
        }
    }

    // 1. Try /dev/input/by-path/*-event-kbd (more specific)
    dir = opendir("/dev/input/by-path");
    if (dir) {
        if (verbose_mode) fprintf(stdout, "Searching for keyboard in /dev/input/by-path...\n");
        while ((entry = readdir(dir))) {
            if (strstr(entry->d_name, "-event-kbd")) {
                snprintf(test_path, sizeof(test_path), "/dev/input/by-path/%s", entry->d_name);
                int fd = open(test_path, O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    unsigned long evbit = 0;
                    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit) != -1 && (evbit & (1 << EV_KEY))) {
                        unsigned long keybit[NLONGS(KEY_MAX)];
                        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) != -1) {
                            if (test_bit(KEY_Q, keybit) || test_bit(KEY_SPACE, keybit) || test_bit(KEY_ENTER, keybit)) {
                                strncpy(path_out, test_path, KBBL_BUFFER_SIZE -1);
                                path_out[KBBL_BUFFER_SIZE-1] = '\0';
                                close(fd);
                                closedir(dir);
                                if (verbose_mode) fprintf(stdout, "Found keyboard (by-path): %s\n", path_out);
                                return 0;
                            }
                        }
                    }
                    close(fd);
                }
            }
        }
        closedir(dir);
    }

    // 2. Fallback: scan /dev/input/event*
    dir = opendir("/dev/input");
    if (!dir) {
        perror("find_keyboard_device: opendir /dev/input");
        return -1;
    }
    if (verbose_mode) fprintf(stdout, "Searching for keyboard in /dev/input/event*...\n");
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            snprintf(test_path, sizeof(test_path), "/dev/input/%s", entry->d_name);
            int fd = open(test_path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;

            unsigned long evbit = 0;
            if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit) == -1 || !(evbit & (1 << EV_KEY))) {
                close(fd);
                continue; 
            }

            unsigned long keybit[NLONGS(KEY_MAX)]; 
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) != -1) {
                if (test_bit(KEY_Q, keybit) || test_bit(KEY_A, keybit) || test_bit(KEY_SPACE, keybit)) {
                    strncpy(path_out, test_path, KBBL_BUFFER_SIZE -1);
                    path_out[KBBL_BUFFER_SIZE-1] = '\0';
                    close(fd);
                    closedir(dir);
                    if (verbose_mode) fprintf(stdout, "Found keyboard (event* scan): %s\n", path_out);
                    return 0;
                }
            }
            close(fd);
        }
    }
    closedir(dir);
    fprintf(stderr, "ERROR: No suitable keyboard event device found.\n");
    return -1;
}


// Find the keyboard backlight brightness control path
int find_led_brightness_path(char *led_b_path, char *led_mb_path) {
    DIR *dir = opendir("/sys/class/leds");
    if (!dir) {
        perror("find_led_brightness_path: opendir /sys/class/leds");
        return -1;
    }

    struct dirent *entry;
    char test_b_path[KBBL_BUFFER_SIZE];
    char test_mb_path[KBBL_BUFFER_SIZE];
    const char *search_fragment = (user_led_name_fragment[0] != '\0') ? user_led_name_fragment : "kbd";

    if (verbose_mode) fprintf(stdout, "Searching for LED control matching '%s' in /sys/class/leds...\n", search_fragment);

    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, search_fragment)) { 
            snprintf(test_b_path, sizeof(test_b_path), "/sys/class/leds/%s/brightness", entry->d_name);
            snprintf(test_mb_path, sizeof(test_mb_path), "/sys/class/leds/%s/max_brightness", entry->d_name);

            if (access(test_b_path, W_OK) == 0 && access(test_mb_path, R_OK) == 0) {
                strncpy(led_b_path, test_b_path, KBBL_BUFFER_SIZE - 1);
                led_b_path[KBBL_BUFFER_SIZE - 1] = '\0';
                strncpy(led_mb_path, test_mb_path, KBBL_BUFFER_SIZE - 1);
                led_mb_path[KBBL_BUFFER_SIZE - 1] = '\0';
                closedir(dir);
                if (verbose_mode) fprintf(stdout, "Found LED control: /sys/class/leds/%s\n", entry->d_name);
                return 0;
            }
        }
    }
    closedir(dir);
    fprintf(stderr, "ERROR: No kbd backlight LED found in /sys/class/leds matching '%s'.\n", search_fragment);
    return -1;
}

// Write brightness value to LED file
void set_led_brightness(const char* path, int value) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("Failed to open brightness file for writing");
        fprintf(stderr, "Attempted path: %s, value: %d\n", path, value);
        return;
    }
    fprintf(f, "%d\n", value);
    fclose(f);
    if (verbose_mode) fprintf(stdout, "Set brightness for '%s' to %d.\n", path, value);
}

// Get max brightness value from LED file
int get_led_max_brightness(const char* path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("Failed to open max_brightness file for reading");
        fprintf(stderr, "Attempted path: %s\n", path);
        return 1; 
    }
    int max_val = 1;
    if (fscanf(f, "%d", &max_val) != 1) {
        fprintf(stderr, "Failed to parse max_brightness from %s. Using default 1.\n", path);
        max_val = 1;
    }
    fclose(f);
    if (max_val <= 0) { 
        if (verbose_mode) fprintf(stderr, "Warning: max_brightness read as %d. Using 1.\n", max_val);
        max_val = 1;
    }
    return max_val;
}

int main(int argc, char *argv[]) {
    // Argument parsing with getopt_long
    int opt;
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"timeout", required_argument, 0, 't'},
        {"brightness", required_argument, 0, 'b'},
        {"device", required_argument, 0, 'd'},
        {"led", required_argument, 0, 'l'},
        {"initial-off", no_argument, 0, 'i'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hvt:b:d:l:i", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                verbose_mode = 1;
                break;
            case 't':
                backlight_timeout = atoi(optarg);
                if (backlight_timeout < 0) {
                    fprintf(stderr, "Warning: Timeout cannot be negative. Using default %d.\n", DEFAULT_BACKLIGHT_TIMEOUT);
                    backlight_timeout = DEFAULT_BACKLIGHT_TIMEOUT;
                }
                break;
            case 'b':
                desired_brightness_on = -2; // Sentinel value to indicate user specified brightness
                if (strchr(optarg, '%')) {
                    int percent = atoi(optarg);
                    if (percent >= 0 && percent <= 100) {
                        desired_brightness_on = percent; // Store as percentage for now
                    } else {
                        fprintf(stderr, "Warning: Brightness percentage out of range (0-100%%). Using default.\n");
                        desired_brightness_on = DEFAULT_BRIGHTNESS_ON;
                    }
                } else {
                    desired_brightness_on = atoi(optarg); // Store as absolute value
                    if (desired_brightness_on < 0) {
                        fprintf(stderr, "Warning: Brightness value cannot be negative. Using default.\n");
                        desired_brightness_on = DEFAULT_BRIGHTNESS_ON;
                    }
                }
                break;
            case 'd':
                strncpy(input_dev_path, optarg, KBBL_BUFFER_SIZE - 1);
                input_dev_path[KBBL_BUFFER_SIZE - 1] = '\0';
                break;
            case 'l':
                strncpy(user_led_name_fragment, optarg, KBBL_BUFFER_SIZE - 1);
                user_led_name_fragment[KBBL_BUFFER_SIZE - 1] = '\0';
                break;
            case 'i':
                initial_off_mode = 1;
                break;
            case '?': // getopt_long prints error for unknown options
                print_usage(argv[0]);
                return 1;
        }
    }

    if (find_keyboard_device(input_dev_path) != 0) {
        return 1;
    }

    if (find_led_brightness_path(led_brightness_path, led_max_brightness_path) != 0) {
        return 1;
    }

    int max_brightness = get_led_max_brightness(led_max_brightness_path);
    int brightness_on = max_brightness; // Default to max

    // Adjust brightness_on based on user input
    if (desired_brightness_on == -2) { // User explicitly set brightness
        if (strchr(argv[optind-1], '%')) { // If it was a percentage
             brightness_on = (max_brightness * atoi(argv[optind-1])) / 100;
        } else { // If it was an absolute value
            brightness_on = atoi(argv[optind-1]);
        }
        // Cap the brightness to max
        if (brightness_on > max_brightness) brightness_on = max_brightness;
        if (brightness_on < 0) brightness_on = 0; // Ensure it's not negative
    }

    int brightness_off = 0;

    if (verbose_mode) {
        fprintf(stdout, "Configuration:\n");
        fprintf(stdout, "  Keyboard Device: %s\n", input_dev_path);
        fprintf(stdout, "  LED Brightness Path: %s (Max: %d)\n", led_brightness_path, max_brightness);
        fprintf(stdout, "  Timeout: %d seconds\n", backlight_timeout);
        fprintf(stdout, "  Brightness ON: %d, OFF: %d\n", brightness_on, brightness_off);
        fprintf(stdout, "  Initial State: %s\n", initial_off_mode ? "OFF" : "ON");
    }


    struct input_event ev;
    time_t last_key_activity_time;

    int input_fd = open(input_dev_path, O_RDONLY);
    if (input_fd < 0) {
        perror("Failed to open input device");
        return 1;
    }

    int current_brightness_is_on;
    if (initial_off_mode) {
        set_led_brightness(led_brightness_path, brightness_off);
        current_brightness_is_on = 0;
    } else {
        set_led_brightness(led_brightness_path, brightness_on);
        current_brightness_is_on = 1;
    }
    last_key_activity_time = time(NULL);


    while (1) {
        struct pollfd fds[1];
        fds[0].fd = input_fd;
        fds[0].events = POLLIN;

        // Poll with a small timeout to allow checking activity time regularly
        int poll_ret = poll(fds, 1, 500); // Poll every 0.5 seconds
        time_t current_time = time(NULL);

        if (poll_ret < 0) {
            if (errno == EINTR) continue; 
            perror("poll() error");
            break;
        }

        if (poll_ret > 0) { 
            if (fds[0].revents & POLLIN) {
                ssize_t bytes_read = read(input_fd, &ev, sizeof(ev));
                if (bytes_read < 0) {
                    if (errno == EINTR) continue;
                    perror("read error from input device");
                    break;
                }
                if (bytes_read == 0) { 
                    if (verbose_mode) fprintf(stderr, "EOF from input device, exiting.\n");
                    break;
                }
                if (bytes_read == sizeof(ev)) {
                    if (ev.type == EV_KEY) {
                        if (verbose_mode > 1) fprintf(stdout, "Key event: type=%d, code=%d, value=%d\n", ev.type, ev.code, ev.value); 

                        if (ev.value == 1 || ev.value == 2) { // Key press or repeat
                            if (!current_brightness_is_on) {
                                if (verbose_mode) fprintf(stdout, "Key press/repeat: Turning backlight ON.\n");
                                set_led_brightness(led_brightness_path, brightness_on);
                                current_brightness_is_on = 1;
                            }
                        }
                        last_key_activity_time = current_time;
                    }
                } else if (bytes_read > 0) {
                     if (verbose_mode) fprintf(stderr, "Partial read from input device (%zd bytes), discarding.\n", bytes_read);
                }
            }
        }

        if (current_brightness_is_on) {
            if (current_time - last_key_activity_time >= backlight_timeout) {
                if (verbose_mode) fprintf(stdout, "Timeout (%lds): Turning backlight OFF.\n", (long)(current_time - last_key_activity_time));
                set_led_brightness(led_brightness_path, brightness_off);
                current_brightness_is_on = 0;
            }
        }
    }

    close(input_fd);
    if (verbose_mode) fprintf(stdout, "Exiting. Turning backlight OFF.\n");
    set_led_brightness(led_brightness_path, brightness_off); 
    return 0;
}