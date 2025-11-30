#include <sys/types.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "physics.h"

// "Never write your own CSV parser".
// I know understand why. But hey! It was fun!
// TODO:
// => `fread_but_better` (see README)
// => Various dialects (quoted fields) not supported but the files
//    we're reading don't require them. So it's fine.

#define TIME_FIELD "Time(s)"

#define NUM_HEADERS 500

struct csv_context {
	FILE *fp;

	char cur_field[BUFSIZ];
	int cur_run;

	int ts_col;
	int data_col;
};

static struct csv_context csv = { 0 };

// MARK: Utilities

void assert_desc_valid(struct desc d) {
	size_t flen = 0;

	assert(d.run > 0);
	assert(d.field != NULL);

	flen = strnlen(d.field, BUFSIZ);
	assert(flen > 0);
}

// MARK: Setup/Teardown

static void assert_context_inactive(void) {
	assert(csv.fp == NULL);
}

static void assert_context_valid(int check_cols) {
	assert(csv.fp != NULL);

	assert(csv.cur_run >= 0);
	if (csv.cur_run > 0) {
		size_t l = strnlen(csv.cur_field, BUFSIZ);
		assert(l > 0 && l < BUFSIZ);
	}

	assert(csv.ts_col < NUM_HEADERS);
	assert(csv.data_col < NUM_HEADERS);

	if (check_cols) {
		assert(csv.ts_col < csv.data_col);
	}
}

void csv_initialize(char *path) {
	assert_context_inactive();
	assert(path != NULL);

	if (NULL == (csv.fp = fopen(path, "r"))) {
		err(1, "csv_open %s", path);
	}
}

void csv_finalize(void) {
	assert_context_valid(0);
	fclose(csv.fp);
	bzero(&csv, sizeof(struct csv_context));
}

// MARK: Reading machinery

// Return next CSV field or NULL if EOF
static char *advance(int *newline) {
	static char readbuf[BUFSIZ] = { 0 };
	size_t cursor = 0;

	assert(newline != NULL);
	assert_context_valid(0);

	for (; cursor < sizeof(readbuf); cursor++) {
		size_t ret = 0;

		// 1. Pull a character out of buffer
		// Listen. I know what you're thinking. HEY, 
		// THIS IS TERRIBLE FOR PERFORMANCE. Well I'll
		// have you know that `fread` implements a buffer underneath
		// us so I'm not planning to reinvent the wheel; otherwise I
		// would have used open(2).
		// This SHOULD BE FINE. And if the implementation is truly brain
		// dead then go get coffee idc.
		ret = fread(readbuf + cursor, 1, 1, csv.fp);

		// 2. Check for EOF
		if (ret == 0) {
			if (ferror(csv.fp) != 0) {
				err(1, "fread");
			} else if (feof(csv.fp) == 0) {
				errx(1, "zero bytes read but not at eof");
			}

			// EOF.
			break;

		} else if (readbuf[cursor] == ',') {
			break;
		}
	}

	// At the end of this loop, cursor is parked on
	// either BUFSIZ, EOF, or a final comma.
	// Rule out the error case first.
	if (cursor == sizeof(readbuf)) {
		errx(1, "big field (offset %ld)", ftell(csv.fp));
	}

	// Valid data will always end on a comma.
	// If we're at EOF, I can imagine situations where
	// we have trailing garbage.
	if (feof(csv.fp) != 0) {
		assert(cursor < 2); // 1 if final newline or 0
		return NULL;

	} else if (readbuf[cursor] != ',') {
		errx(1, "no comma (offset %ld)", ftell(csv.fp));
	}

	readbuf[cursor] = '\0';

	// Strip out leading newline PRN
	if (readbuf[0] == '\r') {
		*newline = 1;
		return readbuf + 2;
	} else {
		*newline = 0;
		return readbuf;
	}
}

static char *advance_multiple(int n) {
	char *v = NULL;

	assert_context_valid(0);
	assert(n > 0 && n < NUM_HEADERS);

	for (int i = 0; i < n; i++) {
		int newline = 0;
		v = advance(&newline);

		if (newline != 0) {
			errx(1, "bad advance_multiple offset");
		} else if (v == NULL) {
			errx(1, "unexpected eof");
		}
	}

	return v;
}

static char *advance_to_next_newline(void) {
	char *outptr = NULL;
	assert_context_valid(0);

	for (int i = 0; i < NUM_HEADERS; i++) {
		int newline = 0;
		outptr = advance(&newline);

		if (outptr == NULL) {
			return NULL;
		} else if (newline != 0) {
			return outptr;
		}
	}

	errx(1, "too many columns in csv");
	// never reached
}

// MARK: Finding columns

static char *name_for_column(struct desc d) {
	static char b[BUFSIZ] = { 0 };
	int ret = 0;

	assert_desc_valid(d);

	ret = snprintf(b, sizeof(b), "Data Set %d:%s", d.run, d.field);

	if (ret < 0) {
		err(1, "snprintf name for %d/%s", d.run, d.field);
	} else if ((size_t)ret >= sizeof(b)) {
		errx(1, "snprintf %d/%s too long", d.run, d.field);
	}

	return b;
}

// Zero indexed column #
static int find_column(struct desc d) {
	char *target = NULL;
	long saved_position = 0;
	int found_col = -1;

	assert_context_valid(0);
	assert_desc_valid(d);

	// 1. Back up our current position in the file
	saved_position = ftell(csv.fp);
	if (saved_position < 0) {
		err(1, "fgetpos");
	}
	rewind(csv.fp);

	// 2. Figure out the name of what we want
	target = name_for_column(d);

	// 3. Find it
	for (int col = 0; col < NUM_HEADERS; col++) {
		int newline = 0;
		char *v = advance(&newline);

		if (v == NULL || newline > 0) {
			break;
		}

		if (strncmp(v, target, BUFSIZ) == 0) {
			found_col = col;
			break;
		}
	}

	if (found_col < 0) {
		errx(1, "can't find column '%s'", target);
	}

	// 4. Restore
	if (fseek(csv.fp, saved_position, SEEK_SET) != 0) {
		err(1, "fseek");
	}

	return found_col;
}

// MARK: Selecting columns

static void clear_cache(void) {
	csv.cur_field[0] = '\0';
	csv.cur_run = 0;
}

static void set_columns_wo_cache(struct desc d) {
	struct desc time_d = {
		.run = d.run,
		.field = TIME_FIELD,
	};

	assert_context_valid(0);
	assert_desc_valid(d);

	csv.ts_col = find_column(time_d);
	csv.data_col = find_column(d);
	assert_context_valid(1);
	clear_cache();
}

// Returns 1 if $ hit, 0 if needed to reset
static int set_columns_with_cache(struct desc d) {
	size_t fsize = sizeof(csv.cur_field);
	assert_context_valid(0);
	assert_desc_valid(d);

	// 1. Hit in $?
	if (strncmp(d.field, csv.cur_field, fsize) == 0 && \
		d.run == csv.cur_run) {
		// No need to do anything
		return 1;
	}

	// 2. Nope. Update.
	set_columns_wo_cache(d);
	strncpy(csv.cur_field, d.field, fsize);
	csv.cur_run = d.run;
	return 0;
}

// MARK: Iterator

// Return -1 if empty row, 0 w/ populated dout otherwise
static int parse_cell(char *cell, double *dout) {
	char *eptr = NULL;
	double d = 0;

	assert(cell != NULL);
	assert(dout != NULL);
	assert(strnlen(cell, BUFSIZ) < BUFSIZ);

	if (strnlen(cell, BUFSIZ) == 0) {
		return -1;
	}

	d = strtod(cell, &eptr);
	if (eptr == cell) {
		errx(1, "bad cell '%s'", cell);
	} else if (errno == ERANGE) {
		char *desc = (d == HUGE_VAL) ? "huge" : "tiny";
		errx(1, "%s cell '%s'", desc, cell);
	}

	*dout = d;
	return 0;
}

#define RNR_OK 0
#define RNR_EMPTY_ROW 1
#define RNR_HIT_EOF 2

// Returns one of the above
static int rnr(struct datum *dout) {
	char *v = NULL;

	assert_context_valid(1);
	bzero(dout, sizeof(struct datum));

	// 1. Jump to a newline
	v = advance_to_next_newline();
	if (v == NULL) {
		return RNR_HIT_EOF;
	}

	// 2. Fast-forward to the timestamp
	if (csv.ts_col > 0) {
		v = advance_multiple(csv.ts_col);
	}

	// 3. Other runs might have valid timestamps
	// at this point, but this run doesn't. Treat
	// as EOF.
	if (parse_cell(v, &dout->timestamp) != 0) {
		return RNR_HIT_EOF;
	}

	// 3. Collect the value
	v = advance_multiple(csv.data_col - csv.ts_col);
	if (parse_cell(v, &dout->value) != 0) {
		return RNR_EMPTY_ROW;
	}

	return RNR_OK;
}

struct datum *csv_iterate(struct desc d) {
	static struct datum dout = { 0 };
	int ret = 0, continuing = 0;

	assert_desc_valid(d);
	assert_context_valid(0);
	bzero(&dout, sizeof(struct datum));

	// 1. Make sure we're in the right place
	continuing = set_columns_with_cache(d);
	if (continuing == 0) {
		rewind(csv.fp);
	}

	// 2. Go!
	for (int i = 0; i < MAX_DATUMS; i++) {
		ret = rnr(&dout);
		switch (ret) {
			case RNR_OK:
				return &dout;
			case RNR_HIT_EOF:
				clear_cache();
				return NULL;
			case RNR_EMPTY_ROW:
				continue;
			default:
				assert(0);
		}
	}

	// never reached
	assert(0);
}

void csv_stopiter(void) {
	assert_context_valid(1);
	clear_cache();
}
