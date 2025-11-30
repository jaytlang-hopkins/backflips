#include <sys/types.h>

#include <err.h>
#include <math.h>

#include "physics.h"

#define LITTLE_G 9.81
#define UCTY_LITTLE_G 0.01

// MARK: Utilities
static double takeoff_time(int run) {
	struct datum *takeoff = NULL;
	struct desc d = {
		.run = run,
		.field = "Hang Time(s)",
	};

	assert_desc_valid(d);
	takeoff = csv_iterate(d);
	csv_stopiter();

	return takeoff->timestamp;
}

static struct datum *landing_datum(int run) {
	struct datum *ht = 0;
	struct desc d = {
		.run = run,
		.field = "Hang Time(s)",
	};

	assert_desc_valid(d);

	for (int i = 0; i < 2; i++) {
		ht = csv_iterate(d);
		if (ht == NULL) {
			errx(1, "no ht for run %d", run);
		}
	}

	csv_stopiter();
	assert(ht->value > 0);
	return ht;
}

static double landing_time(int run) {
	assert(run > 0);
	return landing_datum(run)->timestamp;
}

static double hang_time(int run) {
	assert(run > 0);
	return landing_datum(run)->value;
}

// MARK: Statistics

static double phy_impulse_ucty(double unused) {
	return FORCEPLATE_UCTY_N;
#pragma unused(unused)
}

struct result phy_vimpulse(int run) {
	double ub = -1;
	struct desc d = {
		.run = run,
		.field = "Force(N)",
	};

	assert_desc_valid(d);
	ub = takeoff_time(run);
	return math_intdt(d, 0, ub, phy_impulse_ucty);
}

struct result phy_himpulse(int run) {
	double ub = -1;
	struct desc d = {
		.run = run,
		.field = "Lateral Force(N)",
	};

	assert_desc_valid(d);
	ub = takeoff_time(run);
	return math_intdt(d, 0, ub, phy_impulse_ucty);
}

struct result phy_rawheight(int run) {
	struct result r = { 0 };
	double airtime = 0;

	assert(run > 0);
	airtime = hang_time(run);

	r.value = LITTLE_G * pow(airtime, 2) / 8;
	r.ucty = pow(airtime, 2) * UCTY_LITTLE_G / 8;
	return r;
}

static struct result jump_velocity(int run) {
	struct result r = { 0 }, vi = { 0 };

	assert(run > 0);
	vi = phy_vimpulse(run);

	r.value = vi.value / MASS_KG;
	{
		double ucty_p = (1 / MASS_KG) * vi.ucty;
		double ucty_m = (vi.value / pow(MASS_KG, 2)) * MASS_UCTY_KG;

		r.ucty = sqrt(pow(ucty_p, 2) + pow(ucty_m, 2));
	}

	return r;
}

struct result phy_impheight(int run) {
	struct result r = { 0 }, vel = { 0 };

	// 1. Figure out initial velocity
	assert(run > 0);
	vel = jump_velocity(run);

	// 2. Do the maths
	r.value = pow(vel.value, 2) / (2 * LITTLE_G);
	{
		double ucty_v = vel.value * vel.ucty / LITTLE_G;
		double ucty_g = pow(vel.value, 2) * UCTY_LITTLE_G / (2 * pow(LITTLE_G, 2));

		r.ucty = sqrt(pow(ucty_v, 2) + pow(ucty_g, 2));
	}

	return r;
}


struct datum phy_comdrop(int run) {
	double takeoff = 0;
	struct desc d = {
		.run = run,
		.field = "Z-axis acceleration(m/s2)",
	};

	assert_desc_valid(d);
	takeoff = takeoff_time(run);
	return math_dintdt_min(d, 0, takeoff);
}

// MARK: The great push for I

struct result phy_maxw(int run) {
	double cutoff = 0;
	struct result rout = {
		.value = -HUGE_VAL,
		.ucty = W_UCTY_RADSPERSEC
	};

	struct desc d = {
		.run = run,
		.field = "Z-angular velocity(rad/s)",
	};

	assert_desc_valid(d);
	cutoff = landing_time(run);

	for (int i = 0; i < MAX_DATUMS; i++) {
		struct datum *w = csv_iterate(d);
		if (w->timestamp > cutoff) {
			break;
		} else if (w->value > rout.value) {
			rout.value = w->value;
		}
	}

	return rout;
}

static double phy_torque_ucty(double t) {
	return sqrt(pow(t * COM_UCTY_M, 2) + pow(COM_M * FORCEPLATE_UCTY_N, 2));
}

struct result phy_i(int run) {
	struct result momentum = { 0 }, maxw = { 0 }, rout = { 0 };
	struct desc d = {
		.run = run,
		.field = "Lateral Force(N)",
	};

	assert_desc_valid(d);
	assert(COM_M == 1); // TODO: FIXME!

	// 1. Compute maximum angular velocity
	maxw = phy_maxw(run);

	// 2. Integrate over lateral forces AKA lateral torques at launch
	momentum = math_intdt(d, 0, takeoff_time(run), phy_torque_ucty);

	// 3. Moment of inertia!
	rout.value = momentum.value / maxw.value;
	{
		double ucty_p = momentum.ucty / maxw.value;
		double ucty_w = momentum.value * maxw.ucty / pow(maxw.value, 2);

		rout.ucty = sqrt(pow(ucty_p, 2) + pow(ucty_w, 2));
	}

	return rout;
}
