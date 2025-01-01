/**
 * @file minesweeper.c
 * @brief A minimal Minesweeper-like game with command-line interaction.
 *
 * This program creates a grid of cells, places mines randomly,
 * and allows the user to open or flag cells through textual input.
 *
 * The user enters moves in a simple format:
 *  - '#xNN;yNN' opens a cell (NN is the 1-based row/col).
 *  - '?xNN;yNN' flags or unflags a cell.
 *
 * Compile on a Unix-like system (with BSD or GNU-like libraries).
 * For usage instructions, run the program with `-h`.
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __OpenBSD__
/* On OpenBSD, srandom_deterministic can be used for deterministic seeding. */
#define srandom srandom_deterministic
#endif

/*----------------------------------*
 *          USAGE MESSAGES         *
 *----------------------------------*/

#define USAGE_SMALL \
    "usage: %s [-" \
    "h" \
    "]" \
    " [-s seed]" \
    " [-m mines]" \
    " [width height]" \
    "\n"

#define USAGE_DESCRIPTION \
    "  -h            Show this help menu.\n" \
    "  -s seed       Set a user-defined seed for mine generation.\n" \
    "  -m mines      Number of mines to place. Default is (width*height)/10.\n" \
    "  width height  Size of the field, default is 10 by 10.\n"

/**
 * @brief Print usage information, then exit.
 *
 * @param name The program name (usually argv[0]).
 * @param full If non-zero, print full usage. Otherwise, print a short usage.
 */
static void usage(const char *name, int full)
{
    fprintf(
        full ? stdout : stderr,
        full ? USAGE_SMALL USAGE_DESCRIPTION : USAGE_SMALL,
        name
    );
    exit(!full);
}

/* Undefine macros to avoid clutter after usage. */
#undef USAGE_SMALL
#undef USAGE_DESCRIPTION

/*----------------------------------*
 *          DATA STRUCTURES         *
 *----------------------------------*/

/**
 * @enum Field_Cell_Status
 * @brief Possible states of a cell in the field.
 */
enum Field_Cell_Status {
    Field_Cell_Status_HIDDEN = 0,  /**< Cell is hidden (not opened, not flagged). */
    Field_Cell_Status_OPENED,      /**< Cell is opened. */
    Field_Cell_Status_FLAGGED,     /**< Cell is flagged by the user. */
};

/**
 * @struct Field_Cell
 * @brief Represents a single cell in the minefield.
 *
 * @var status    The cell's current status (hidden/opened/flagged).
 * @var is_mine   Whether or not this cell contains a mine (boolean).
 * @var mines_near The number of mines in adjacent (including diagonal) cells.
 */
struct Field_Cell {
    unsigned char status    : 2;  /**< one of Field_Cell_Status */
    unsigned char is_mine   : 1;  /**< 1 => mine, 0 => no mine */
    unsigned char mines_near: 4;  /**< 0..8, neighbors with mines */
};

/**
 * @struct Field
 * @brief Represents the entire minefield grid.
 *
 * @var width   Width of the field (columns).
 * @var height  Height of the field (rows).
 * @var field   Pointer to an array of Field_Cell of size width*height.
 */
struct Field {
    unsigned width, height;
    struct Field_Cell *field;
};

/*----------------------------------*
 *           FIELD HANDLING         *
 *----------------------------------*/

/**
 * @brief Generate mines randomly on the field, updating adjacency counts.
 *
 * @param field  Pointer to an existing Field struct.
 * @param mines  Number of mines to place randomly.
 */
static void Field_generate(struct Field *field, unsigned mines)
{
    while (mines > 0) {
        unsigned x = random() % field->width;
        unsigned y = random() % field->height;

        struct Field_Cell *cur = &field->field[x + y * field->width];
        if (cur->is_mine) {
            /* Already a mine here, skip. */
            continue;
        }
        /* Mark cell as mine */
        cur->is_mine = 1;

        /* Increment adjacency count around (x, y) */
        for (int j = -1; j <= 1; ++j) {
            for (int i = -1; i <= 1; ++i) {
                int xr = (int)x + i;
                int yr = (int)y + j;

                /* Ensure within bounds */
                if (xr < 0 || yr < 0 || (unsigned)xr >= field->width || (unsigned)yr >= field->height) {
                    continue;
                }
                struct Field_Cell *neighbor = &field->field[xr + yr * field->width];
                neighbor->mines_near++;
            }
        }
        --mines;
    }
}

/**
 * @brief Open a cell at (x,y), recursively opening neighbors if zero adjacency.
 *
 * @param field Pointer to a Field structure.
 * @param x     Column index (0-based).
 * @param y     Row index (0-based).
 */
static void Field_open(struct Field *field, unsigned x, unsigned y)
{
    if (x >= field->width || y >= field->height) {
        return; /* Out of bounds. */
    }

    struct Field_Cell *cell = &field->field[x + y * field->width];
    if (cell->status == Field_Cell_Status_OPENED) {
        return; /* Already opened. */
    }

    cell->status = Field_Cell_Status_OPENED;

    /* If cell has adjacency > 0, no further recursion needed. */
    if (cell->mines_near != 0) {
        return;
    }

    /* If adjacency == 0, open all neighbors. */
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            int nx = (int)x + i;
            int ny = (int)y + j;
            /* Only open valid neighbors in grid. */
            if (nx >= 0 && ny >= 0 &&
                (unsigned)nx < field->width &&
                (unsigned)ny < field->height) {
                Field_open(field, nx, ny);
            }
        }
    }
}

/**
 * @brief Check if game is in a win or lose state.
 *
 * @param field Pointer to the Field structure.
 *
 * @return -1 if a mined cell was opened (lose),
 *         +1 if all non-mined cells are opened (win),
 *          0 otherwise (continue).
 */
static int Field_isWin(const struct Field *field)
{
    int closed = 0, mines = 0;
    unsigned total_cells = field->width * field->height;

    for (unsigned i = 0; i < total_cells; ++i) {
        const struct Field_Cell *cell = &field->field[i];
        if (cell->is_mine && cell->status == Field_Cell_Status_OPENED) {
            return -1; /* Opened a mine => lose. */
        }
        if (cell->status != Field_Cell_Status_OPENED) {
            closed++;
        }
        if (cell->is_mine) {
            mines++;
        }
    }
    /* Win if all closed cells are exactly the mines. */
    return (closed == mines) ? 1 : 0;
}

/**
 * @brief Print the current field state to stdout.
 *
 * @param field Pointer to Field structure to display.
 */
static void Field_print(const struct Field *field)
{
    for (unsigned y = 0; y < field->height; ++y) {
        for (unsigned x = 0; x < field->width; ++x) {
            const struct Field_Cell c = field->field[x + y * field->width];
            switch (c.status) {
            case Field_Cell_Status_HIDDEN:
                fputs("[]", stdout);
                break;
            case Field_Cell_Status_FLAGGED:
                fputs("??", stdout);
                break;
            case Field_Cell_Status_OPENED:
                if (c.is_mine) {
                    fputs("##", stdout);
                } else {
                    /* Print adjacency count with leading space if single digit. */
                    printf(" %d", c.mines_near);
                }
                break;
            }
        }
        putchar('\n');
    }
}

/*----------------------------------*
 *     PLAYER ACTION DEFINITIONS    *
 *----------------------------------*/

/**
 * @enum Player_Move_Action
 * @brief Describes whether the player wants to open or flag a cell.
 */
enum Player_Move_Action {
    Player_Move_Action_FLAG = 0,  /**< Flag/unflag a cell */
    Player_Move_Action_OPEN,      /**< Open a cell */
};

/**
 * @struct Player_Move
 * @brief Represents a single user command (x,y plus action).
 *
 * @var x       0-based column index
 * @var y       0-based row index
 * @var action  Either Player_Move_Action_OPEN or FLAG
 */
struct Player_Move {
    int x, y;                        /**< Coordinates in grid */
    enum Player_Move_Action action;  /**< Desired action */
};

/**
 * @brief Read a move from standard input, returning the parsed Player_Move.
 *
 * Expected input format is something like:
 *   #x4;y1
 * or
 *   ?x2;y5
 * '#' means OPEN, '?' means FLAG.
 *
 * \return A Player_Move struct with x,y,action. Exits on EOF or read error.
 */
static struct Player_Move Player_process(void)
{
    enum Player_Move_Action action;
    int x = 0, y = 0;
    int ch;

    /* Wait until we see a '#' or '?' to decide action. */
    while ((ch = getchar()) != EOF && ch != '#' && ch != '?')
        /* skip */ ;

    if (ch == EOF) {
        if (errno) {
            warn("cannot read input");
        }
        exit(errno ? errno : 1);
    }

    action = (ch == '#') ? Player_Move_Action_OPEN : Player_Move_Action_FLAG;

    /* Read until we find 'x' or EOF */
    while ((ch = getchar()) != EOF && ch != 'x') {
        if (isdigit(ch)) {
            x = x * 10 + (ch - '0');
        }
    }
    x -= 1; /* Convert 1-based to 0-based index. */

    /* Read until we find ';' or EOF. */
    while ((ch = getchar()) != EOF && ch != ';') {
        if (isdigit(ch)) {
            y = y * 10 + (ch - '0');
        }
    }
    y -= 1; /* Convert 1-based to 0-based index. */

    if (ch == EOF) {
        if (errno) {
            warn("cannot read input");
        }
        exit(errno ? errno : 2);
    }

    struct Player_Move pm = { .x = x, .y = y, .action = action };
    return pm;
}

/*----------------------------------*
 *              MAIN                *
 *----------------------------------*/

/**
 * @brief Entry point.
 */
int main(int argc, char **argv)
{
    const char *name = argv[0];

    /* Default field size 10x10 and no pointer yet. */
    struct Field field = { .width = 10, .height = 10, .field = NULL };
    unsigned seed = (unsigned)time(NULL);
    unsigned mines = 0;
    int is_mines_set = 0;

    int ch;
    while ((ch = getopt(argc, argv, "hm:s:")) != -1) {
        switch (ch) {
        case 's': {
            const char *errstr;
            extern long long strtonum(const char*, long long, long long, const char**);
            seed = (unsigned)strtonum(optarg, 0, UINT_MAX, &errstr);
            if (errstr) {
                warnx("Invalid seed '%s': %s", optarg, errstr);
                usage(name, 0);
            }
        } break;
        case 'm': {
            const char *errstr;
            extern long long strtonum(const char*, long long, long long, const char**);
            mines = (unsigned)strtonum(optarg, 0, UINT_MAX, &errstr);
            if (errstr) {
                warnx("Invalid mines '%s': %s", optarg, errstr);
                usage(name, 0);
            }
            is_mines_set = 1;
        } break;
        case 'h':
            usage(name, 1);
            break;
        default:
            usage(name, 0);
            break;
        }
    }

    argc -= optind;
    argv += optind;

    /* Possibly read width/height from positional args. */
    if (argc == 2) {
        const char *dimensions[] = { "width", "height" };
        extern long long strtonum(const char*, long long, long long, const char**);

        for (int i = 0; i < 2; ++i) {
            const char *errstr;
            unsigned val = (unsigned)strtonum(argv[i], 1, UINT_MAX, &errstr);
            if (errstr) {
                warnx("%s is %s: %s", dimensions[i], errstr, argv[i]);
                usage(name, 0);
            }
            if (i == 0) {
                field.width = val;
            } else {
                field.height = val;
            }
        }
    } else if (argc != 0) {
        warnx("Please pass exactly 0 or 2 positional arguments (width height).");
        usage(name, 0);
    }

    /* If mines count not explicitly set, default to (w*h)/10. */
    if (!is_mines_set) {
        mines = field.width * field.height / 10;
    } else if (mines > (field.width * field.height)) {
        warnx("Mines too large: %u (exceeds width*height).", mines);
        usage(name, 0);
    }

    /* Seed the PRNG. */
    srandom(seed);

    /* Allocate memory on the stack for field cells. */
    struct Field_Cell field_buffer[field.width * field.height];
    memset(field_buffer, 0, sizeof(field_buffer));
    field.field = field_buffer;

    /* Randomly generate mines and adjacency info. */
    Field_generate(&field, mines);

    /* MAIN LOOP */
    for (;;) {
        Field_print(&field);

        int status = Field_isWin(&field);
        if (status > 0) {
            printf("You won!\n");
            break;
        } else if (status < 0) {
            printf("You lost :(\n");
            break;
        }

        /* Wait for player input (#xNN;yNN or ?xNN;yNN). */
        struct Player_Move move = Player_process();

        /* Validate user coords. */
        if (move.x < 0) {
            warnx("X coordinate invalid: %d (1-based => %d)", move.x, move.x + 1);
            continue;
        } else if (move.y < 0) {
            warnx("Y coordinate invalid: %d (1-based => %d)", move.y, move.y + 1);
            continue;
        } else if ((unsigned)move.x >= field.width) {
            warnx("X coordinate too large: %d", move.x + 1);
            continue;
        } else if ((unsigned)move.y >= field.height) {
            warnx("Y coordinate too large: %d", move.y + 1);
            continue;
        }

        /* Perform requested action. */
        struct Field_Cell *cell = &field.field[move.x + move.y * field.width];
        switch (move.action) {
        case Player_Move_Action_OPEN:
            Field_open(&field, (unsigned)move.x, (unsigned)move.y);
            break;

        case Player_Move_Action_FLAG:
            /* Toggle between FLAGGED and HIDDEN. */
            if (cell->status == Field_Cell_Status_HIDDEN) {
                cell->status = Field_Cell_Status_FLAGGED;
            } else if (cell->status == Field_Cell_Status_FLAGGED) {
                cell->status = Field_Cell_Status_HIDDEN;
            }
            break;
        }
    }
    return 0;
}

