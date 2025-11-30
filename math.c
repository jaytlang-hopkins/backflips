#include <sys/types.h>

#include <err.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>

#include "physics.h"

// MARK: Utilities

static double intdt_ucty_term(struct datum t1, struct datum t2, double ucty) {
	return sqrt(2) * ucty / 2 * (t2.timestamp - t1.timestamp);
}

// MARK: Integrator

struct integrator;
typedef struct datum *(*nf)(struct integrator *in);

struct integrator {
	double bounds[2];
	double icond;
	struct datum window[2];

	void *ctx;
	nf next;
	uctyf ucty;
};

static void assert_integrator_valid(struct integrator *in) {
	assert(in->bounds[0] >= 0 && in->bounds[1] > 0);
	assert(in->bounds[1] > in->bounds[0]);
	assert(in->next != NULL);
	assert(in->ctx != NULL);
}

static struct datum find_lb(struct integrator *in, double lb) {
	struct datum *cur = NULL;

	for (int i = 0; i < MAX_DATUMS; i++) {
		cur = in->next(in);
		if (cur == NULL) {
			errx(1, "oob lb %f", lb);
		} else if (cur->timestamp >= lb) {
			break;
		}
	}

	if (cur == NULL) {
		errx(1, "huge csv");
	}

	return *cur;
}


static void integrator_init(struct integrator *in, double lb, double ub, double icond, void *ctx, nf next, uctyf ucty) {
	in->bounds[0] = lb;
	in->bounds[1] = ub;
	in->ucty = ucty;
	in->icond = icond;
	bzero(in->window, sizeof(in->window));

	in->next = next;
	in->ctx = ctx;

	assert_integrator_valid(in);

	in->window[1] = find_lb(in, in->bounds[0]);
}

static struct result *integrator_next(struct integrator *in, double *ts) {
	static struct result rout = { 0 };
	struct datum *cur = NULL;

	assert_integrator_valid(in);

	// Apply the initial condition
	rout.value = in->icond;

	// Slide current value over to backup slot,
	// then pull a new value from our file
	in->window[0] = in->window[1];
	cur = in->next(in);

	if (cur == NULL || cur->timestamp > in->bounds[1]) {
		csv_stopiter();
		return NULL;
	}

	// Integrate
	{
		in->window[1] = *cur;
		double average = (in->window[0].value + in->window[1].value) / 2;
		rout.value += (in->window[1].timestamp - in->window[0].timestamp) * average;

		if (in->ucty != NULL) {
			rout.ucty = intdt_ucty_term(in->window[0], in->window[1], in->ucty(average));
		}
	}

	// ...and polish off uncertainty.
	if (ts != NULL) {
		*ts = (in->window[1].timestamp + in->window[0].timestamp) / 2;
	}
	return &rout;
}

static struct result do_integration(struct integrator *in) {
	struct result r = { 0 };
	double sq_ucty_rsum = 0;

	assert_integrator_valid(in);

	for (int i = 0; i < MAX_DATUMS; i++) {
		struct result *int_r = integrator_next(in, NULL);

		if (int_r == NULL) {
			r.ucty = sqrt(sq_ucty_rsum);
			return r;
		}

		r.value += int_r->value;
		sq_ucty_rsum += pow(int_r->ucty, 2);
	}

	// Should not be reached
	errx(1, "huge csv");
}

// MARK: Functions

static struct datum *intdt_next(struct integrator *in) {
	struct desc d = { 0 };
	assert_integrator_valid(in);

	d = *(struct desc *)in->ctx;
	assert_desc_valid(d);
	return csv_iterate(d);
}

struct result math_intdt(struct desc d, double lb, double ub, uctyf ucty) {
	struct integrator in = { 0 };

	assert_desc_valid(d);
	integrator_init(&in, lb, ub, 0, &d, &intdt_next, ucty);
	assert_integrator_valid(&in);

	return do_integration(&in);
}

// MARK: Double integration

#define MAX_INITIAL_CONDITION 100
#define MAX_ITERATIONS 100
#define EPSILON 0.000001

static struct datum *dintdt_next(struct integrator *in) {
	static struct datum dout = { 0 };

	struct integrator *nested = NULL;
	struct result *nres = NULL;
	double nts = 0;

	assert_integrator_valid(in);
	nested = (struct integrator *)in->ctx;
	assert_integrator_valid(nested);

	nres = integrator_next(nested, &nts);
	if (nres == NULL) {
		return NULL;
	} else {
		dout.timestamp = nts;
		dout.value = nres->value;
		return &dout;
	}
}

static double math_dintdt(struct desc d, double lb, double ub, double icond) {
	struct integrator outer = { 0 }, inner = { 0 };

	assert_desc_valid(d);
	integrator_init(&inner, lb, ub, icond, &d, &intdt_next, NULL);
	assert_integrator_valid(&inner);

	integrator_init(&outer, lb, ub, 0, &inner, &dintdt_next, NULL);
	assert_integrator_valid(&outer);

	return do_integration(&outer).value;
}

static void debug_log(int debug, const char *msg, ...) {
	va_list ap;

	if (debug == 0) {
		return;
	}

	va_start(ap, msg);
	vwarnx(msg, ap);
	va_end(ap);
}

double math_dintdt_bestcond(struct desc d, double lb, double ub) {
	// Find the best initial condition we can find
	double min = -MAX_INITIAL_CONDITION, max = MAX_INITIAL_CONDITION;
	int dbg = getenv("PHYSICS_DEBUG_DINTDT") != NULL;

	assert_desc_valid(d);
	assert(lb >= 0 && ub > 0);
	assert(ub > lb);

	debug_log(dbg, "start (%f - %f)", lb, ub);
	for (int i = 0; i < 100; i++) {
		double icond = (max + min) / 2;
		double result = math_dintdt(d, lb, ub, icond);

		if (fabs(result) < EPSILON) {
			debug_log(dbg, "DONE: %f err %f took %d iterations", icond, result, i);
			return icond;

		} else if (result < 0) {
			min = icond;
			debug_log(dbg, "%f too low", icond);

		} else {
			max = icond;
			debug_log(dbg, "%f too high", icond);
		}
	}

	errx(1, "search failed; best bounds %f-%f", min, max);
}

struct datum math_dintdt_min(struct desc d, double lb, double ub) {
	struct datum finding = { 0 };
	struct integrator outer = { 0 }, inner = { 0 };

	assert_desc_valid(d);
	assert(lb >= 0 && ub > 0);
	assert(ub > lb);

	// 1. Find the optimal initial condition
	{
		double bestcond = 0;

		bestcond = math_dintdt_bestcond(d, lb, ub);

		integrator_init(&inner, lb, ub, bestcond, &d, &intdt_next, NULL);
		assert_integrator_valid(&inner);

		integrator_init(&outer, lb, ub, 0, &inner, &dintdt_next, NULL);
		assert_integrator_valid(&outer);
	}

	// 2. Look for the minimum...
	finding.value = HUGE_VAL;
	finding.timestamp = -1;

	for (int i = 0; i < MAX_DATUMS; i++) {
		double ts = 0;
		struct result *int_r = integrator_next(&outer, &ts);

		if (int_r == NULL) {
			assert(finding.timestamp > -1);
			return finding;
		}

		if (int_r->value < finding.value) {
			finding.timestamp = ts;
			finding.value = int_r->value;
		}
	}

	// Should not be reached
	errx(1, "huge csv");
}

