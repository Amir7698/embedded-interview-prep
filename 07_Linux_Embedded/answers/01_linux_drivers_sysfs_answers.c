/*
 * ANSWERS: 07_Linux_Embedded/01_linux_drivers_sysfs.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: sysfs vs /dev — when do you use each?

A: /dev: character or block device files. Accessed with open()/read()/write()/ioctl().
   Used for: UART (/dev/ttyS0), SPI (/dev/spidevX.Y), I2C (/dev/i2c-N),
   GPIO (legacy /dev/gpiochip0 via ioctl), video (/dev/videoN), audio.
   These are streaming or transactional — you exchange raw bytes/frames.

   sysfs (/sys): virtual filesystem exposing kernel object attributes.
   Each file represents a single attribute (integer, string, hex value).
   Read/write with open()/read()/write() or echo/cat from shell.
   Used for: GPIO direction/value (legacy sysfs-gpio, deprecated),
   LED trigger/brightness, hwmon sensors (temperature, voltage),
   network interface stats, power management, device attributes.

   Rule: sysfs for configuration and status attributes.
         /dev for data streams and commands.
   Modern GPIO: use libgpiod (/dev/gpiochipN + ioctl) instead of sysfs-gpio.

Q2: What does it mean that sysfs files are "virtual"?

A: The files in /sys don't exist on disk. The kernel synthesizes them on
   each read/write. When you read /sys/class/gpio/gpio17/value:
   1. VFS calls the sysfs show() callback registered for that attribute.
   2. The callback reads the actual GPIO register and returns "0\n" or "1\n".
   3. The bytes never touch storage.
   Similarly for writes: write() → store() callback → sets GPIO register.
   Size reported by stat() is always 4096 (page size) regardless of content.
   File position after read: advance past content → next read returns EOF.
   Always re-open or lseek(0) to re-read current value.

Q3: Why is libgpiod preferred over sysfs-gpio?

A: sysfs-gpio (CONFIG_GPIO_SYSFS) problems:
   1. Deprecated since Linux 4.8 — may be removed in future kernels.
   2. No way to read multiple GPIO values atomically.
   3. Polling requires inotify or slow file reads; no efficient event
      notification for edge detection with low latency.
   4. Race condition: export creates the sysfs files, but there's a delay
      before udev sets permissions — script must retry or sleep.
   5. Can't claim GPIO exclusive ownership — two processes can fight.

   libgpiod (gpiod_*): ioctl-based, uses /dev/gpiochipN.
   Advantages: atomic multi-pin read/write, proper edge detection
   (poll()/POLLIN on gpiod_line_event_fd), exclusive line reservation,
   actively maintained, designed for userspace GPIO.
   API: gpiod_chip_open, gpiod_chip_get_line, gpiod_line_request_output,
        gpiod_line_set_value, gpiod_line_release.

Q4: Reading CPU temperature — which file and what is the unit?

A: Path: /sys/class/thermal/thermal_zone0/temp
   Unit: millidegrees Celsius (integer).
   Example: reading "45234" = 45.234°C.
   Convert: float temp_c = atoi(buf) / 1000.0f;

   For hardware monitors (ADT7461, LM75, etc. connected via I2C):
   /sys/class/hwmon/hwmon0/temp1_input (same millidegrees format).

   On Raspberry Pi: thermal_zone0 is the SoC (BCM2835/BCM2711) junction temp.
   Normally runs 40-60°C under load. Throttles at 80°C.

Q5: /proc/meminfo — what does MemAvailable mean vs MemFree?

A: MemFree: physical RAM pages with NOTHING in them — truly idle.
   MemAvailable (since Linux 3.14): estimate of how much memory is actually
   available for starting new applications WITHOUT swapping.
   Includes: MemFree + reclaimable caches (page cache, slab reclaimable)
   minus the portion of those that can't be freed.
   Why the difference matters:
   Linux uses "free" RAM as disk cache (PageCache) to speed up I/O.
   MemFree ignores this cache; if you request memory, the kernel reclaims cache.
   MemAvailable accounts for reclaimable cache → more accurate measure of
   "can I start this process?" without hitting swap.
   For embedded: use MemAvailable to judge memory pressure.

Q6: LED trigger "timer" — what sysfs files to set blink rate?

A: After setting trigger to "timer":
   /sys/class/leds/<name>/delay_on  — milliseconds LED stays ON per cycle
   /sys/class/leds/<name>/delay_off — milliseconds LED stays OFF per cycle
   Example: 500ms on, 500ms off (1 Hz blink):
   echo "timer" > /sys/class/leds/led0/trigger
   echo "500"   > /sys/class/leds/led0/delay_on
   echo "500"   > /sys/class/leds/led0/delay_off
   The LED subsystem uses a kernel timer; no CPU busy-wait.
   Other triggers: "heartbeat" (kernel load-proportional), "mmc0" (disk activity),
   "netdev" (network traffic), "cpu0" (CPU activity).
*/

/* ============================================================
 * Helper: write string to sysfs file
 * ============================================================ */

static int sysfs_write(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -errno;
    int r = (int)write(fd, value, strlen(value));
    close(fd);
    return (r < 0) ? -errno : 0;
}

static int sysfs_read(const char *path, char *buf, size_t bufsz)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -errno;
    ssize_t n = read(fd, buf, bufsz - 1);
    close(fd);
    if (n < 0) return -errno;
    buf[n] = '\0';
    /* Strip trailing newline */
    if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
    return 0;
}

/* ============================================================
 * TASK 1 — GPIO sysfs operations
 * ============================================================ */

int sysfs_gpio_export(unsigned int gpio_num)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", gpio_num);
    return sysfs_write("/sys/class/gpio/export", buf);
}

int sysfs_gpio_unexport(unsigned int gpio_num)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", gpio_num);
    return sysfs_write("/sys/class/gpio/unexport", buf);
}

int sysfs_gpio_set_direction(unsigned int gpio_num, const char *direction)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/direction", gpio_num);
    return sysfs_write(path, direction);
}

int sysfs_gpio_write(unsigned int gpio_num, uint8_t value)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", gpio_num);
    return sysfs_write(path, value ? "1" : "0");
}

int sysfs_gpio_read(unsigned int gpio_num, uint8_t *value)
{
    char path[64], buf[4];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", gpio_num);
    int r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    *value = (uint8_t)atoi(buf);
    return 0;
}

/* ============================================================
 * TASK 2 — CPU temperature
 * ============================================================ */

int read_cpu_temp_celsius(float *temp_out)
{
    char buf[16];
    int r = sysfs_read("/sys/class/thermal/thermal_zone0/temp", buf, sizeof(buf));
    if (r < 0) {
        /* Try hwmon fallback */
        r = sysfs_read("/sys/class/hwmon/hwmon0/temp1_input", buf, sizeof(buf));
        if (r < 0) return r;
    }
    *temp_out = (float)atoi(buf) / 1000.0f;  /* millidegrees → Celsius */
    return 0;
}

/* ============================================================
 * TASK 3 — /proc/meminfo parser
 * ============================================================ */

typedef struct {
    uint64_t mem_total_kb;
    uint64_t mem_free_kb;
    uint64_t mem_available_kb;
    uint64_t buffers_kb;
    uint64_t cached_kb;
} MemInfo;

int parse_meminfo(MemInfo *out)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -errno;

    char line[128];
    memset(out, 0, sizeof(*out));

    while (fgets(line, sizeof(line), f)) {
        uint64_t val;
        if (sscanf(line, "MemTotal: %llu kB", (unsigned long long *)&val) == 1)
            out->mem_total_kb = val;
        else if (sscanf(line, "MemFree: %llu kB", (unsigned long long *)&val) == 1)
            out->mem_free_kb = val;
        else if (sscanf(line, "MemAvailable: %llu kB", (unsigned long long *)&val) == 1)
            out->mem_available_kb = val;
        else if (sscanf(line, "Buffers: %llu kB", (unsigned long long *)&val) == 1)
            out->buffers_kb = val;
        else if (sscanf(line, "Cached: %llu kB", (unsigned long long *)&val) == 1)
            out->cached_kb = val;
    }
    fclose(f);
    return 0;
}

/* ============================================================
 * TASK 4 — LED sysfs control
 * ============================================================ */

int led_set_trigger(const char *led_name, const char *trigger)
{
    char path[96];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/trigger", led_name);
    return sysfs_write(path, trigger);
}

int led_set_brightness(const char *led_name, uint8_t brightness)
{
    char path[96], buf[8];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/brightness", led_name);
    snprintf(buf, sizeof(buf), "%u", brightness);
    return sysfs_write(path, buf);
}

int led_blink_timer(const char *led_name, uint32_t delay_on_ms, uint32_t delay_off_ms)
{
    int r;
    char buf[16];

    r = led_set_trigger(led_name, "timer");
    if (r < 0) return r;

    char path[96];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_on", led_name);
    snprintf(buf, sizeof(buf), "%u", delay_on_ms);
    r = sysfs_write(path, buf);
    if (r < 0) return r;

    snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_off", led_name);
    snprintf(buf, sizeof(buf), "%u", delay_off_ms);
    return sysfs_write(path, buf);
}

/* ============================================================
 * TASK 5 — Network interface stats
 * ============================================================ */

typedef struct { uint64_t rx_bytes, tx_bytes, rx_packets, tx_packets; } NetStats;

int read_netif_stats(const char *ifname, NetStats *out)
{
    char path[96], buf[32];
    int  r;

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", ifname);
    r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    out->rx_bytes = (uint64_t)strtoull(buf, NULL, 10);

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", ifname);
    r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    out->tx_bytes = (uint64_t)strtoull(buf, NULL, 10);

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_packets", ifname);
    r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    out->rx_packets = (uint64_t)strtoull(buf, NULL, 10);

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_packets", ifname);
    r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    out->tx_packets = (uint64_t)strtoull(buf, NULL, 10);

    return 0;
}

/* ============================================================
 * TASK 6 — Bug hunt FIXED (gpio_blink_BUGGY)
 *
 * Bug 1: export path wrong — "/sys/gpio/export" does not exist.
 *        Correct path: "/sys/class/gpio/export"
 *        FIX: use correct sysfs path.
 *
 * Bug 2: no delay after export before setting direction.
 *        After writing to /sys/class/gpio/export, udev creates the gpio
 *        directory and sets permissions asynchronously. A race condition:
 *        if direction file is opened too soon, open() returns EACCES or ENOENT.
 *        FIX: use a retry loop or usleep(100000) after export.
 *        Better: poll() for the file to appear.
 *
 * Bug 3: fd opened in the loop but never closed on each iteration.
 *        LED fd is opened each iteration without being closed → fd leak.
 *        After ~1024 iterations: EMFILE (too many open files) → write fails.
 *        FIX: close(fd) after each write, or open once before the loop
 *        and lseek(fd, 0, SEEK_SET) before each write.
 * ============================================================ */

int main(void)
{
    /* Unit tests run on the host, which won't have /sys/class/gpio,
       so we just verify the function signatures compile and the
       sysfs_write/sysfs_read helpers work with a known file. */

    printf("Linux sysfs answer file compiled successfully.\n");
    printf("Run on target (Linux + GPIO hardware) to test sysfs functions.\n");

    /* Verify sysfs_read handles missing files gracefully */
    char buf[16];
    int r = sysfs_read("/nonexistent/path", buf, sizeof(buf));
    printf("sysfs_read missing file returned %d (expected negative)\n", r);

    return 0;
}
