//
//  main.c
//  physics
//
//  Created by Jay Lang on 11/30/25.
//

#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "physics.h"

static void output_result(const char *n, const char *units, struct result r) {
	printf("  %-35s %12.6f ± %-12.6f %s\n", n, r.value, r.ucty, units);
}

static void usage(void) {
	fprintf(stderr, "usage: backflip -c file [-j run] [-f run]\n");
	fprintf(stderr, "  -c file    CSV data file (required)\n");
	fprintf(stderr, "  -j run     Jump run number (optional)\n");
	fprintf(stderr, "  -f run     Flip run number (optional)\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	const char *csv_file = NULL;
	int jump_run = -1, flip_run = -1;
	int ch;

	while ((ch = getopt(argc, argv, "c:j:f:")) != -1) {
		switch (ch) {
		case 'c':
			csv_file = optarg;
			break;
		case 'j':
			jump_run = atoi(optarg);
			if (jump_run <= 0) {
				errx(1, "jump run must be positive");
			}
			break;
		case 'f':
			flip_run = atoi(optarg);
			if (flip_run <= 0) {
				errx(1, "flip run must be positive");
			}
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (csv_file == NULL) {
		usage();
	}

	printf("=== Backflip Analyzer ===\n\n");

	csv_initialize((char *)csv_file);

	if (jump_run > 0) {
		printf("JUMP RUN #%d\n", jump_run);
		output_result("Vertical Impulse", "N s", phy_vimpulse(jump_run));
		output_result("Horizontal Impulse", "N s", phy_himpulse(jump_run));
		output_result("True height achieved", "m", phy_rawheight(jump_run));
		output_result("Height via impulse (at feet)", "m", phy_impheight(jump_run));
		printf("\n");
	}

	if (flip_run > 0) {
		printf("FLIP RUN #%d\n", flip_run);
		output_result("Vertical Impulse", "N s", phy_vimpulse(flip_run));
		output_result("Horizontal Impulse", "N s", phy_himpulse(flip_run));
		output_result("True height achieved", "m", phy_rawheight(flip_run));
		output_result("Height via impulse (at feet)", "m", phy_impheight(flip_run));
		output_result("Moment of inertia", "kg m^2", phy_i(flip_run));
		printf("\n");
	}

	if (jump_run <= 0 && flip_run <= 0) {
		errx(2, "No runs specified. Use -j for jump run or -F for flip run\n");
	}

	csv_finalize();
	return 0;
}
