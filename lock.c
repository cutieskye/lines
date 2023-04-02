#include <gpiod.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CONSUMER "Consumer"
#define LENGTH 4

#define LED_COUNT 4
#define BLUE_LED 24
#define GREEN_LED 22
#define YELLOW_LED 23
#define RED_LED 27

#define BUTTON_COUNT 4
#define LEFT_BUTTON 25
#define MIDDLE_BUTTON 10
#define RIGHT_BUTTON 17
#define OK_BUTTON 18

int init_led_lines(struct gpiod_chip *chip, struct gpiod_line *led_lines[], const int leds[])
{
    for (int i = 0; i < LED_COUNT; ++i)
    {
        led_lines[i] = gpiod_chip_get_line(chip, leds[i]);
        if (!led_lines[i])
        {
            perror("Get lines failed\n");
            return -1;
        }
    }
    for (int i = 0; i < LED_COUNT; ++i)
    {
        int result = gpiod_line_request_output(led_lines[i], CONSUMER, 0);
        if (result < 0)
        {
            perror("Request output failed\n");
            return -1;
        }
    }
    return 0;
}

int init_button_lines(struct gpiod_chip *chip, struct gpiod_line *button_lines[], const int buttons[])
{
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        button_lines[i] = gpiod_chip_get_line(chip, buttons[i]);
        if (!button_lines[i])
        {
            perror("Get lines failed\n");
            return -1;
        }
    }
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        int result = gpiod_line_request_falling_edge_events(button_lines[i], CONSUMER);
        if (result < 0)
        {
            perror("Request both edges events failed\n");
            return -1;
        }
    }
    return 0;
}

int *get_random_code(int length)
{
    int *code = malloc(length * sizeof(*code));
    for (int i = 0; i < length; ++i)
    {
        code[i] = rand() % 3;
    }
    return code;
}

void print(int arr[], int length)
{
    printf("\n");
    for (int i = 0; i < length; ++i)
    {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

int convert_button_to_idx(int button)
{
    switch (button)
    {
    case LEFT_BUTTON:
        return 0;
    case MIDDLE_BUTTON:
        return 1;
    case RIGHT_BUTTON:
        return 2;
    case OK_BUTTON:
        return 3;
    default:
        return -1;
    }
}

int show_value(struct gpiod_line *led_lines[], int value, struct timespec ts)
{
    for (int j = 1; j >= 0; --j)
    {
        if (gpiod_line_set_value(led_lines[convert_button_to_idx(value)], j) < 0)
        {
            perror("Set value failed\n");
            return -1;
        }
        nanosleep(&ts, NULL);
    }
    return 0;
}

void release_lines(struct gpiod_line *led_lines[], struct gpiod_line *button_lines[])
{
    for (int i = 0; i < LED_COUNT; i++)
    {
        if (led_lines[i] != NULL)
        {
            gpiod_line_release(led_lines[i]);
        }
    }

    for (int i = 0; i < BUTTON_COUNT; i++)
    {
        if (button_lines[i] != NULL)
        {
            gpiod_line_release(button_lines[i]);
        }
    }
}

int blink_all(struct gpiod_line *led_lines[], int value, struct timespec ts)
{
    for (int j = 0; j < LED_COUNT; ++j)
    {
        if (gpiod_line_set_value(led_lines[j], value) < 0)
        {
            perror("Set value failed\n");
            return -1;
        }
        nanosleep(&ts, NULL);
    }
    return 0;
}

int main(int argc, char **argv)
{
    srand(time(NULL));

    struct timespec ts = {0, 100000000};

    int ret = EXIT_SUCCESS;

    const int leds[] = {BLUE_LED, GREEN_LED, YELLOW_LED, RED_LED};
    struct gpiod_line *led_lines[LED_COUNT];
    for (int i = 0; i < LED_COUNT; ++i)
    {
        led_lines[i] = NULL;
    }

    const int buttons[] = {LEFT_BUTTON, MIDDLE_BUTTON, RIGHT_BUTTON, OK_BUTTON};
    struct gpiod_line *button_lines[BUTTON_COUNT];
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        button_lines[i] = NULL;
    }

    const char *chipname = "/dev/gpiochip0";
    struct gpiod_chip *chip = gpiod_chip_open(chipname);
    if (!chip)
    {
        perror("Open chip failed\n");
        ret = EXIT_FAILURE;
        goto end;
    }

    if (init_led_lines(chip, led_lines, leds) < 0 ||
        init_button_lines(chip, button_lines, buttons) < 0)
    {
        goto close_chip;
    }

    struct gpiod_line_bulk bulk = GPIOD_LINE_BULK_INITIALIZER;
    gpiod_line_bulk_init(&bulk);
    for (int i = 0; i < BUTTON_COUNT; i++)
    {
        gpiod_line_bulk_add(&bulk, button_lines[i]);
    }

    struct gpiod_line_bulk event_bulk;
    event_bulk.num_lines = 0;
    memset(event_bulk.lines, 0, GPIOD_LINE_BULK_MAX_LINES * sizeof(struct gpiod_line *));

    struct timespec round_time;
    round_time.tv_sec = 20;
    round_time.tv_nsec = 0;

    int *code = get_random_code(LENGTH);
    print(code, LENGTH);
    int sequence[LENGTH];
    for (int i = 0; i < LENGTH; ++i)
    {
        sequence[i] = -1;
    }
    int idx = 0;

    blink_all(led_lines, 0, ts);

    bool correct = false;
    int attempt = 0;
    while (!correct && attempt < 3)
    {
        ret = gpiod_line_event_wait_bulk(&bulk, &round_time, &event_bulk);
        if (ret == 1)
        {
            const int num_lines = gpiod_line_bulk_num_lines(&event_bulk);
            struct gpiod_line_event events[16];

            struct gpiod_line *line, **lineptr;
            gpiod_line_bulk_foreach_line(&event_bulk, line, lineptr)
            {
                while (gpiod_line_event_read_multiple(line, events, 16) == 16)
                    ;
                int button = gpiod_line_offset(line);
                int value = convert_button_to_idx(button);
                printf("%d\n", value);
                if (idx < LENGTH && button != OK_BUTTON)
                {
                    sequence[idx++] = value;
                }
                if (button == OK_BUTTON)
                {
                    correct = true;
                    ++attempt;
                    for (int i = 0; i < LENGTH; ++i)
                    {
                        if (code[i] != sequence[i])
                        {
                            correct = false;
                            idx = 0;
                            for (int i = 0; i < 3; ++i)
                            {
                                for (int j = 1; j >= 0; --j)
                                {
                                    blink_all(led_lines, j, ts);
                                }
                            }
                            break;
                        }
                    }
                }
                else
                {
                    show_value(led_lines, button, ts);
                }
            }
        }
    }

    if (correct)
    {
        if (gpiod_line_set_value(led_lines[0], 1) < 0)
        {
            goto release_lines;
        }
    }
    else
    {
        if (gpiod_line_set_value(led_lines[3], 1) < 0)
        {
            goto release_lines;
        }
    }

    sleep(10);

release_lines:
    release_lines(led_lines, button_lines);
close_chip:
    gpiod_chip_close(chip);
end:
    free(code);
    return ret;
}