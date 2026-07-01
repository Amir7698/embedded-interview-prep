/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * Topic : Linux Embedded — sysfs, Processes, Sockets
 * File  : 07_Linux_Embedded/01_linux_drivers_sysfs.c
 * ============================================================
 *
 * Linux embedded (Yocto, Buildroot, Raspberry Pi, i.MX)
 * is a separate skill from bare-metal.
 * Know userspace, kernel interfaces, and debugging tools.
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

/* ============================================================
 * THEORY — Linux Embedded Architecture
 * ============================================================
 *
 * Userspace ↔ Kernel boundary:
 *
 *   Application (C/Python)
 *       ↕ file I/O (open/read/write/ioctl)
 *   Virtual filesystem (VFS)
 *       ↕
 *   ┌─────────────────────────────────┐
 *   │  sysfs  (/sys/class/gpio/...)   │ ← Hardware control via files
 *   │  devfs  (/dev/ttyS0, /dev/i2c) │ ← Character devices
 *   │  procfs (/proc/meminfo, ...)    │ ← Kernel runtime info
 *   └─────────────────────────────────┘
 *       ↕ kernel driver
 *   Hardware (GPIO, SPI, I2C, UART, ...)
 *
 * Key interfaces:
 *   /sys/class/gpio/    — GPIO control via sysfs
 *   /sys/class/leds/    — LED control (brightness, trigger)
 *   /sys/class/hwmon/   — Hardware monitoring (temp, voltage)
 *   /sys/bus/i2c/       — I2C device tree
 *   /dev/i2c-N          — I2C userspace access (I2C_RDWR ioctl)
 *   /dev/spidevN.N      — SPI userspace access (SPI_IOC_MESSAGE ioctl)
 *   /proc/meminfo       — Memory statistics
 *   /proc/net/dev       — Network interface statistics
 * ============================================================ */

/* ============================================================
 * TASK 1 — GPIO control via sysfs
 *
 * Legacy interface (still widely used in embedded Linux).
 * New projects use libgpiod / /dev/gpiochipN instead.
 * ============================================================ */

#define SYSFS_GPIO_PATH  "/sys/class/gpio"

int sysfs_gpio_export(uint32_t pin)
{
    /* TODO: open "/sys/class/gpio/export" for write
     * write the pin number as a string (e.g., "17\n")
     * close and return 0, or -1 on error */
    char path[64];
    snprintf(path, sizeof(path), "%s/export", SYSFS_GPIO_PATH);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;   /* needs root or gpio group */
    char buf[8];
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)pin);
    int ret = (write(fd, buf, (size_t)n) == n) ? 0 : -1;
    close(fd);
    return ret;
}

int sysfs_gpio_set_direction(uint32_t pin, const char *direction)
{
    /* TODO: open "/sys/class/gpio/gpioN/direction" for write
     * write "in" or "out"
     * return 0 on success, -1 on error */
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%u/direction", SYSFS_GPIO_PATH, (unsigned)pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t written = write(fd, direction, strlen(direction));
    close(fd);
    return (written == (ssize_t)strlen(direction)) ? 0 : -1;
}

int sysfs_gpio_write(uint32_t pin, uint8_t value)
{
    /* TODO: open "/sys/class/gpio/gpioN/value" for write
     * write "1" or "0" */
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%u/value", SYSFS_GPIO_PATH, (unsigned)pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    char c = value ? '1' : '0';
    int ret = (write(fd, &c, 1) == 1) ? 0 : -1;
    close(fd);
    return ret;
}

int sysfs_gpio_read(uint32_t pin)
{
    /* TODO: open for read, read 1 char, return 0 or 1 */
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%u/value", SYSFS_GPIO_PATH, (unsigned)pin);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char c = '0';
    ssize_t r = read(fd, &c, 1);
    close(fd);
    return (r == 1) ? (c == '1' ? 1 : 0) : -1;
}

int sysfs_gpio_unexport(uint32_t pin)
{
    /* TODO: write pin number to /sys/class/gpio/unexport */
    char path[64], buf[8];
    snprintf(path, sizeof(path), "%s/unexport", SYSFS_GPIO_PATH);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)pin);
    int ret = (write(fd, buf, (size_t)n) == n) ? 0 : -1;
    close(fd);
    return ret;
}

/* ============================================================
 * TASK 2 — Read CPU temperature via hwmon sysfs
 * ============================================================ */

float read_cpu_temp_celsius(void)
{
    /* Try thermal_zone0 first (most common on SBCs) */
    const char *paths[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/hwmon/hwmon0/temp1_input",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        int fd = open(paths[i], O_RDONLY);
        if (fd < 0) continue;
        char buf[16] = {0};
        ssize_t r = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (r > 0) {
            long millideg = strtol(buf, NULL, 10);
            return (float)millideg / 1000.0f;
        }
    }
    return -1.0f;   /* not found */
}

/* ============================================================
 * TASK 3 — Parse /proc/meminfo
 * ============================================================ */

typedef struct {
    uint64_t total_kb;
    uint64_t free_kb;
    uint64_t available_kb;
    uint64_t buffers_kb;
    uint64_t cached_kb;
} MemInfo;

int parse_meminfo(MemInfo *info)
{
    /* TODO: open /proc/meminfo
     * Read line by line, parse "Key: value kB" format.
     * Fill in total, free, available, buffers, cached.
     * Return 0 on success, -1 on error. */
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    memset(info, 0, sizeof(*info));
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        uint64_t val = 0;
        if      (sscanf(line, "MemTotal:     %llu kB", (unsigned long long*)&val) == 1) info->total_kb     = val;
        else if (sscanf(line, "MemFree:      %llu kB", (unsigned long long*)&val) == 1) info->free_kb      = val;
        else if (sscanf(line, "MemAvailable: %llu kB", (unsigned long long*)&val) == 1) info->available_kb = val;
        else if (sscanf(line, "Buffers:      %llu kB", (unsigned long long*)&val) == 1) info->buffers_kb   = val;
        else if (sscanf(line, "Cached:       %llu kB", (unsigned long long*)&val) == 1) info->cached_kb    = val;
    }
    fclose(f);
    return 0;
}

/* ============================================================
 * TASK 4 — LED trigger control (heartbeat, timer, etc.)
 *
 * /sys/class/leds/<name>/trigger  — set trigger type
 * /sys/class/leds/<name>/brightness — set 0 or max_brightness
 * Timer trigger:
 *   /sys/class/leds/<name>/delay_on  — ms on
 *   /sys/class/leds/<name>/delay_off — ms off
 * ============================================================ */

int led_set_trigger(const char *led_name, const char *trigger)
{
    /* TODO: write trigger to /sys/class/leds/led_name/trigger */
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/trigger", led_name);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t r = write(fd, trigger, strlen(trigger));
    close(fd);
    return (r == (ssize_t)strlen(trigger)) ? 0 : -1;
}

int led_set_brightness(const char *led_name, uint32_t brightness)
{
    char path[128], buf[16];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/brightness", led_name);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)brightness);
    ssize_t r = write(fd, buf, (size_t)n);
    close(fd);
    return (r == n) ? 0 : -1;
}

int led_blink_timer(const char *led_name, uint32_t on_ms, uint32_t off_ms)
{
    /* TODO: 1. set trigger to "timer"
     * 2. write on_ms to delay_on
     * 3. write off_ms to delay_off */
    if (led_set_trigger(led_name, "timer") < 0) return -1;

    char path[128], buf[16];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_on", led_name);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)on_ms);
    write(fd, buf, (size_t)n);
    close(fd);

    snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_off", led_name);
    fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    n = snprintf(buf, sizeof(buf), "%u", (unsigned)off_ms);
    write(fd, buf, (size_t)n);
    close(fd);
    return 0;
}

/* ============================================================
 * TASK 5 — Read network interface statistics
 * ============================================================ */

typedef struct {
    uint64_t rx_bytes;
    uint64_t rx_packets;
    uint64_t rx_errors;
    uint64_t tx_bytes;
    uint64_t tx_packets;
    uint64_t tx_errors;
} NetIfStats;

int read_netif_stats(const char *ifname, NetIfStats *stats)
{
    /* /sys/class/net/<ifname>/statistics/rx_bytes etc. */
    struct { const char *file; uint64_t *field; } fields[] = {
        {"rx_bytes",   &stats->rx_bytes},
        {"rx_packets", &stats->rx_packets},
        {"rx_errors",  &stats->rx_errors},
        {"tx_bytes",   &stats->tx_bytes},
        {"tx_packets", &stats->tx_packets},
        {"tx_errors",  &stats->tx_errors},
        {NULL, NULL}
    };
    memset(stats, 0, sizeof(*stats));
    for (int i = 0; fields[i].file; i++) {
        char path[128], buf[32] = {0};
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/%s", ifname, fields[i].file);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        read(fd, buf, sizeof(buf)-1);
        close(fd);
        *fields[i].field = (uint64_t)strtoull(buf, NULL, 10);
    }
    return 0;
}

/* ============================================================
 * TASK 6 — BUG HUNT: sysfs GPIO usage bugs
 *
 * The function below exports a GPIO and blinks it.
 * It has 3 bugs.
 * ============================================================ */

void gpio_blink_BUGGY(uint32_t pin)
{
    char path[64], buf[8];

    /* Bug 1: export path is wrong */
    snprintf(path, sizeof(path), "/sys/class/gpio/%u/export", pin);  /* should be /sys/class/gpio/export */
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        snprintf(buf, sizeof(buf), "%u", (unsigned)pin);
        write(fd, buf, strlen(buf));
        close(fd);
    }

    /* Bug 2: no sleep/delay after export — direction file may not exist yet
     * Kernel needs time to create the sysfs entries after export */
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/direction", pin);
    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, "out", 3);
        close(fd);
    }

    /* Bug 3: value file not closed before next write — fd leak in loop */
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", pin);
        fd = open(path, O_WRONLY);
        write(fd, "1", 1);
        /* Missing: close(fd); */
        usleep(500000);
        /* fd = open(path, O_WRONLY); — but fd was never closed above! */
        write(fd, "0", 1);
        close(fd);
        usleep(500000);
    }
}

/* ============================================================
 * MAIN — prints system info if running on Linux
 * ============================================================ */

int main(void)
{
    printf("Linux Embedded sysfs demo\n");

    float temp = read_cpu_temp_celsius();
    if (temp > 0)
        printf("CPU temperature: %.1f °C\n", (double)temp);
    else
        printf("Temperature not available (run on Linux target)\n");

    MemInfo mem = {0};
    if (parse_meminfo(&mem) == 0) {
        printf("Memory: total=%llu kB, available=%llu kB\n",
               (unsigned long long)mem.total_kb,
               (unsigned long long)mem.available_kb);
    }

    printf("sysfs demo complete.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: What is the difference between /sys/class/gpio (sysfs)
 *     and /dev/gpiochipN (libgpiod)? Which is preferred now?
 *     Answer: TODO
 *
 * Q2: You write to /sys/class/gpio/export and get EBUSY.
 *     What does this mean and how do you fix it?
 *     Answer: TODO
 *
 * Q3: How do you read a hardware sensor from userspace?
 *     Name the kernel interface and the typical file path.
 *     Answer: TODO
 *
 * Q4: What is a kernel module? How do you load/unload one?
 *     When would you write a kernel driver vs a userspace driver?
 *     Answer: TODO
 *
 * Q5: Explain the difference between character devices (/dev/ttyS0)
 *     and sysfs attributes (/sys/...). When is each used?
 *     Answer: TODO
 */
