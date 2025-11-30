#ifndef PHYSICS_H
#define PHYSICS_H

#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

// csv.c

#define MAX_DATUMS 10000

struct desc {
	int run;
	const char *field;
};

void assert_desc_valid(struct desc d);

struct datum {
	double timestamp;
	double value;
};

void csv_initialize(char *path);
struct datum *csv_iterate(struct desc d);
void csv_stopiter(void);
void csv_finalize(void);

// math.c

typedef double (*uctyf)(double value);

struct result {
	double value;
	double ucty;
};

struct result math_intdt(struct desc d, double lb, double ub, uctyf ucty);
double math_dintdt_bestcond(struct desc d, double lb, double ub);
struct datum math_dintdt_min(struct desc d, double lb, double ub);

// phy.c

#define FORCEPLATE_UCTY_N 5
#define MASS_KG 62.5
#define MASS_UCTY_KG 0.5

#define COM_M 1
#define COM_UCTY_M 0.25

#define W_UCTY_RADSPERSEC 1

struct result phy_vimpulse(int run);
struct result phy_himpulse(int run);

struct result phy_rawheight(int run);
struct result phy_impheight(int run);

// struct datum phy_comdrop(int run);

struct result phy_i(int run);

#endif // PHYSICS_H
