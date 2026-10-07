/*
 * hal_gpio_linux -- Linux GPIO character device v2 后端
 * 只用 <linux/gpio.h> 的 UAPI, 不依赖 libgpiod。
 */
#include "hal_gpio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

/* 不同内核头文件里 v2 ioctl 宏命名不一(有的带 _IOCTL 后缀), 统一一下 */
#ifndef GPIO_V2_GET_LINE
#define GPIO_V2_GET_LINE GPIO_V2_GET_LINE_IOCTL
#endif
#ifndef GPIO_V2_LINE_SET_CONFIG
#define GPIO_V2_LINE_SET_CONFIG GPIO_V2_LINE_SET_CONFIG_IOCTL
#endif
#ifndef GPIO_V2_LINE_GET_VALUES
#define GPIO_V2_LINE_GET_VALUES GPIO_V2_LINE_GET_VALUES_IOCTL
#endif
#ifndef GPIO_V2_LINE_SET_VALUES
#define GPIO_V2_LINE_SET_VALUES GPIO_V2_LINE_SET_VALUES_IOCTL
#endif

struct ptx_HalPin {
    int fd;      /* line fd (GPIO_V2_GET_LINE 返回) */
    int chip;    /* chip 号 */
    int line;    /* line offset */
};

/* Raspberry Pi 40 针物理排针号 -> BCM GPIO (适用于 Pi 3/4/Zero) */
static const struct { int pin; int bcm; } k_pin_bcm[] = {
    {3,2},
    {5,3},
    {7,4},
    {8,14},
    {10,15},
    {11,17},
    {12,18},
    {13,27},
    {15,22},
    {16,23},
    {18,24},
    {19,10},
    {21,9},
    {22,25},
    {23,11},
    {24,8},
    {26,7},
    {27,0},
    {28,1},
    {29,5},
    {31,6},
    {32,12},
    {33,13},
    {35,19},
    {36,16},
    {37,26},
    {38,20},
    {40,21},
};

static int board_pin_to_bcm(int pin)
{
    for (size_t i = 0; i < sizeof(k_pin_bcm)/sizeof(k_pin_bcm[0]); i++)
        if (k_pin_bcm[i].pin == pin)
            return k_pin_bcm[i].bcm;
    return -1;
}

int ptx_hal_gpio_init(void) { return 0; }
void ptx_hal_gpio_deinit(void) {}

static int parse_spec(const char* spec, int* chip, int* line)
{
    if (!spec) return -1;
    *chip = 0;

    if (strncmp(spec, "BOARD", 5) == 0) {
        int bcm = board_pin_to_bcm(atoi(spec + 5));
        if (bcm < 0) return -1;
        *line = bcm;
        return 0;
    }
    if (strncmp(spec, "BCM", 3) == 0) {
        *line = atoi(spec + 3);
        return 0;
    }
    if (strncmp(spec, "gpiochip", 8) == 0 || strncmp(spec, "chip", 4) == 0) {
        const char* p = strchr(spec, ':');
        if (!p) return -1;
        const char* c = strpbrk(spec, "0123456789");
        if (!c) return -1;
        *chip = atoi(c);
        *line = atoi(p + 1);
        return 0;
    }
    return -1;   /* 未知串 */
}

ptx_HalPin* ptx_hal_pin_open(const char* spec)
{
    int chip, line;
    if (parse_spec(spec, &chip, &line) != 0)
        return NULL;

    char path[32];
    snprintf(path, sizeof(path), "/dev/gpiochip%d", chip);

    int cfd = open(path, O_RDWR | O_CLOEXEC);
    if (cfd < 0)
        return NULL;

    struct gpio_v2_line_request req;
    memset(&req, 0, sizeof(req));
    req.offsets[0] = line;
    req.num_lines = 1;
    snprintf(req.consumer, sizeof(req.consumer), "pinetrix");

    if (ioctl(cfd, GPIO_V2_GET_LINE, &req) < 0) {
        close(cfd);
        return NULL;
    }
    close(cfd);

    ptx_HalPin* pin = (ptx_HalPin*)calloc(1, sizeof(*pin));
    if (!pin) {
        close(req.fd);
        return NULL;
    }
    pin->fd = req.fd;
    pin->chip = chip;
    pin->line = line;
    return pin;
}

void ptx_hal_pin_close(ptx_HalPin* pin)
{
    if (!pin) return;
    if (pin->fd >= 0) close(pin->fd);
    free(pin);
}

int ptx_hal_pin_config_input(ptx_HalPin* pin, ptx_gpio_pull_t pull)
{
    if (!pin) return -1;
    struct gpio_v2_line_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.flags = GPIO_V2_LINE_FLAG_INPUT;
    if (pull == PTX_GPIO_PULL_UP)
        cfg.flags |= GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
    else if (pull == PTX_GPIO_PULL_DOWN)
        cfg.flags |= GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN;
    else
        cfg.flags |= GPIO_V2_LINE_FLAG_BIAS_DISABLED;
    return ioctl(pin->fd, GPIO_V2_LINE_SET_CONFIG, &cfg) < 0 ? -1 : 0;
}

int ptx_hal_pin_read(ptx_HalPin* pin)
{
    if (!pin) return -1;
    struct gpio_v2_line_values v;
    memset(&v, 0, sizeof(v));
    v.mask = 1;
    if (ioctl(pin->fd, GPIO_V2_LINE_GET_VALUES, &v) < 0)
        return -1;
    return (int)(v.bits & 1);
}

int ptx_hal_pin_config_output(ptx_HalPin* pin, int initial_level)
{
    if (!pin) return -1;
    struct gpio_v2_line_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    if (ioctl(pin->fd, GPIO_V2_LINE_SET_CONFIG, &cfg) < 0)
        return -1;
    return ptx_hal_pin_write(pin, initial_level);
}

int ptx_hal_pin_write(ptx_HalPin* pin, int level)
{
    if (!pin) return -1;
    struct gpio_v2_line_values v;
    memset(&v, 0, sizeof(v));
    v.mask = 1;
    v.bits = level ? 1 : 0;
    return ioctl(pin->fd, GPIO_V2_LINE_SET_VALUES, &v) < 0 ? -1 : 0;
}
