#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <err.h>
#include <getopt.h>
#include <unistd.h>


#ifdef __OpenBSD__
#define srandom srandom_deterministic
#endif

#define USAGE_SMALL \
    "usage: %s [-" \
    "h" \
    "S" \
    "]" \
    " [-s seed]" \
    " [-m mines]" \
    " [width height]" \
    "\n"
#define USAGE_DESCRIPTION \
    "  -h            show this help menu\n" \
    "  -S            show used seed\n" \
    "  -s seed       set user-defined seed for mines generation\n" \
    "  -m mines      amount of mines to place, default is width*height/10\n" \
    "  width height  size of field, default is 10 by 10\n" \

void usage(int full)
{
    fprintf(
        full ? stdout : stderr,
        full
            ? USAGE_SMALL USAGE_DESCRIPTION
            : USAGE_SMALL,
       getprogname()
    );
    exit(!full);
}

#undef USAGE_SMALL
#undef USAGE_DESCRIPTION

enum Field_Cell_Status
{
    Field_Cell_Status_HIDDEN,
    Field_Cell_Status_OPENED,
    Field_Cell_Status_FLAGGED,
};

struct Field_Cell
{
    unsigned char status : 2;
    unsigned char is_mine : 1;
    unsigned char is_selected : 1;
    unsigned char mines_near : 4;
};

struct Field
{
    unsigned width, height;
    struct Field_Cell *field;
};

void Field_generate(struct Field *field, unsigned mines)
{
    unsigned x, y, xr, yr;
    struct Field_Cell *cur;

    while (mines > 0)
    {
        x = random() % field->width;
        y = random() % field->height;

        cur = field->field + x + y * field->width;
        if (cur->is_mine)
            continue;
        cur->is_mine = 1;

        for (int j = -1; j <= 1; ++j)
        {
            for (int i = -1; i <= 1; ++i)
            {
                xr = x + i;
                yr = y + j;

                cur = field->field + xr + yr * field->width;
                if (xr < 0 || yr < 0 || xr >= field->width || yr >= field->height)
                    continue;
                ++cur->mines_near;
            }
        }
        --mines;
    }
}

void Field_open(struct Field *field, unsigned x, unsigned y)
{
    if (x < 0 || y < 0 || x >= field->width || y >= field->height)
        return;

    if (field->field[x + y * field->width].status != Field_Cell_Status_OPENED)
        field->field[x + y * field->width].status = Field_Cell_Status_OPENED;
    else
        return;

    if (field->field[x + y * field->width].mines_near != 0)
        return;

    for (int j = -1; j <= 1; ++j)
        for (int i = -1; i <= 1; ++i)
            Field_open(field, x + i, y + j);
}

int Field_isWin(struct Field *field)
{
    int closed = 0, mines = 0;
    for (unsigned i = 0; i < field->width * field->height; ++i)
    {
        if (field->field[i].is_mine && field->field[i].status == Field_Cell_Status_OPENED)
            return -1;
        closed += field->field[i].status != Field_Cell_Status_OPENED;
        mines += field->field[i].is_mine;
    }
    return closed == mines;
}

void Field_print(struct Field *field)
{
    for (unsigned y = 0; y < field->height; ++y)
    {
        for (unsigned x = 0; x < field->width; ++x)
        {
            struct Field_Cell c = field->field[x + y * field->width];
            switch (c.status)
            {
            case Field_Cell_Status_HIDDEN:
            {
                printf(c.is_selected ? "X]" : "[]");
            } break;
            case Field_Cell_Status_FLAGGED:
            {
                printf(c.is_selected ? "X?" : "??");
            } break;
            case Field_Cell_Status_OPENED:
            {
                if (c.is_mine)
                    printf(c.is_selected ? "X#" : "##");
                else
                    printf(c.is_selected ? "X%d" : " %d", c.mines_near);
            } break;
            }
        }
        putchar('\n');
    }
}

const char *const Player_Move_Action_CHARS =
    "@"
    "!"
    "j"
    "?"
    "h"
    "#"
    "l"
    "k"
;

enum Player_Move_Action
{
    Player_Move_Action_CLICK_OPEN,
    Player_Move_Action_CLICK_FLAG,
    Player_Move_Action_DOWN,
    Player_Move_Action_FLAG,
    Player_Move_Action_LEFT,
    Player_Move_Action_OPEN,
    Player_Move_Action_RIGHT,
    Player_Move_Action_UP,
};

struct Player_Move
{
    int x, y;
    enum Player_Move_Action action;
};

struct Player_Move Player_process(void)
{
    enum Player_Move_Action action;
    int x = 0, y = 0, ch;
    char *actionp;

    while ((ch = getchar()) >= 0 && !(actionp = strchr(Player_Move_Action_CHARS, ch)));
    if (ch < 0)
        goto eof;
    action = actionp - Player_Move_Action_CHARS;

    switch (action)
    case Player_Move_Action_FLAG:
    case Player_Move_Action_OPEN:
    {
        {
        while ((ch = getchar()) >= 0 && ch != 'x')
        {
            if (!isdigit(ch))
                continue;
            x *= 10;
            x += ch - '0';
        }
        x -= 1;

        while ((ch = getchar()) >= 0 && ch != ';')
        {
            if (!isdigit(ch))
                continue;
            y *= 10;
            y += ch - '0';
        }
        y -= 1;
    } break;
    case Player_Move_Action_UP:
    case Player_Move_Action_DOWN:
    {
        while ((ch = getchar()) >= 0 && ch != ';')
        {
            if (!isdigit(ch))
                continue;
            y *= 10;
            y += ch - '0';
        }
    } break;
    case Player_Move_Action_LEFT:
    case Player_Move_Action_RIGHT:
    {
        while ((ch = getchar()) >= 0 && ch != ';')
        {
            if (!isdigit(ch))
                continue;
            x *= 10;
            x += ch - '0';
        }
    } break;
    }

    if (ch < 0)
    {
eof:
        if (errno)
            warn("cannot read input");
        exit(errno);
    }

    return (struct Player_Move){.x=x, .y=y, .action=action};
}

int main(int argc, char **argv)
{
    struct Field field = {10, 10, 0};
    int selected_x, selected_y;
    unsigned seed, mines;
    int is_mines_set = 0, is_seed_set = 0, ch;
    int show_seed = 0;

#ifdef __OpenBSD__
    pledge("stdio", NULL);
#endif

    while ((ch = getopt(argc, argv, "hSm:s:")) > 0)
    {
        switch (ch)
        {
        case 's':
        {
            const char *e;

            is_seed_set = 1;
            seed = strtonum(optarg, 0, UINT_MAX, &e);
            if (e)
            {
                warnx("%s is %s: %s", "seed", e, optarg);
                usage(0);
            }
        } break;
        case 'm':
        {
            const char *e;

            is_mines_set = 1;
            mines = strtonum(optarg, 0, UINT_MAX, &e);
            if (e)
            {
                warnx("%s is %s: %s", "mines", e, optarg);
                usage(0);
            }
        } break;
        case 'S':
        {
            show_seed = 1;
        } break;
        case 'h':
        {
            usage(1);
        } break;
        default:
        {
            usage(0);
        } break;
        }
    }
    argc -= optind;
    argv += optind;

    switch (argc)
    {
    case 0:
        break;
    case 2:
    {
        char *names[] = {"width", "height"};

        for (int i = 0; i < 2; ++i)
        {
            const char *e;

            ((unsigned *)&field)[i] = strtonum(argv[i], 1, UINT_MAX, &e);
            if (e)
            {
                warnx("%s is %s: %s", names[i], e, argv[i]);
                usage(0);
            }
        }
    } break;
    default:
    {
        warnx("you should pass exactly 2 positional arguments");
        usage(0);
    } break;
    }

    if (!is_seed_set)
        if (getentropy(&seed, sizeof(seed)) < 0)
            err(1, "getentropy()");

    if (!is_mines_set)
    {
        mines = field.width * field.height / 10;
    }
    else if (mines > field.width * field.height)
    {
        warnx("%s is %s: %u", "mines", "too large", mines);
        usage(0);
    }

    if (show_seed)
        warnx("seed is %u", seed);
    srandom(seed);

    struct Field_Cell field_buffer[field.width * field.height];
    field.field = field_buffer;

    memset(field.field, 0, field.width * field.height);
    selected_x = selected_y = 0;
    field.field[0].is_selected = 1;
    Field_generate(&field, mines);

    for (;;)
    {
        Field_print(&field);
        int win = Field_isWin(&field);
        if (win > 0)
            printf("You won! UwU\n");
        else if (win < 0)
            printf("You lost :<\n");
        else
            printf("Your current location is (%d, %d)\n", selected_x + 1, selected_y + 1);

        struct Player_Move move = Player_process();
        switch (move.action)
        {
        case Player_Move_Action_OPEN:
        case Player_Move_Action_FLAG:
        {
            if (move.x < 0)
            {
                warnx("%s is %s: %d", "x", "too small", move.x + 1);
                continue;
            } else if (move.y < 0)
            {
                warnx("%s is %s: %d", "y", "too small", move.y + 1);
                continue;
            } else if (move.x >= field.width)
            {
                warnx("%s is %s: %d", "x", "too large", move.x + 1);
                continue;
            } else if (move.y >= field.height)
            {
                warnx("%s is %s: %d", "y", "too large", move.y + 1);
                continue;
            }
        } break;
        case Player_Move_Action_UP:
        case Player_Move_Action_DOWN:
        case Player_Move_Action_LEFT:
        case Player_Move_Action_RIGHT:
        {
            field.field[selected_x + selected_y * field.width].is_selected = 0;
        } break;
        case Player_Move_Action_CLICK_OPEN:
        {
            move.x = selected_x;
            move.y = selected_y;
            move.action = Player_Move_Action_OPEN;
        } break;
        case Player_Move_Action_CLICK_FLAG:
        {
            move.x = selected_x;
            move.y = selected_y;
            move.action = Player_Move_Action_FLAG;
        } break;
        }

        switch (move.action)
        {
        case Player_Move_Action_OPEN:
        {
            Field_open(&field, move.x, move.y);
        } break;
        case Player_Move_Action_FLAG:
        {
            switch (field.field[move.x + move.y * field.width].status)
            {
            break; case Field_Cell_Status_HIDDEN:
                field.field[move.x + move.y * field.width].status = Field_Cell_Status_FLAGGED;
            break; case Field_Cell_Status_FLAGGED:
                field.field[move.x + move.y * field.width].status = Field_Cell_Status_HIDDEN;
            }
        } break;
        case Player_Move_Action_UP:
        {
            if (selected_y - move.y < 0)
                warnx("invalid location");
            else
                selected_y -= move.y;
        } break;
        case Player_Move_Action_DOWN:
        {
            if (selected_y + move.y >= field.height)
                warnx("invalid location");
            else
                selected_y += move.y;
        } break;
        case Player_Move_Action_LEFT:
        {
            if (selected_x - move.x < 0)
                warnx("invalid location");
            else
                selected_x -= move.x;
        } break;
        case Player_Move_Action_RIGHT:
        {
            if (selected_x + move.x >= field.width)
                warnx("invalid location");
            else
                selected_x += move.x;
        } break;
        }

        switch (move.action)
        {
        case Player_Move_Action_UP:
        case Player_Move_Action_DOWN:
        case Player_Move_Action_LEFT:
        case Player_Move_Action_RIGHT:
            field.field[selected_x + selected_y * field.width].is_selected = 1;
        }
    }
}
