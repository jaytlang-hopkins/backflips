# Backflips!

https://github.com/user-attachments/assets/4e7641f9-0a66-4f79-8067-3fcfbe814f49

A physics analysis tool for measuring and analyzing backflip biomechanics on force plate sensor data. Calculates impulses, jump heights, and rotational inertia from acceleration and force measurements.

## BEFORE YOU BEGIN

This code isn't threadsafe. The naming is terrible (looking at you, uctyf function pointers). If for some reason you are actually trying to maintain this, don't. Talk to Jay first.

But for a physics project... it's QUITE sufficient!

### Things for you to do if you actually want to maintain this thing

* Online the COM finder with real data
* Unit tests, unit tests, unit tests. The rest of this code is JPL-spec in terms of the conventions it follows (even down to static buffers, see below); but any branch-line coverage at all would be reassuring.
* This code is not threadsafe. Seriously. There are static buffers sprayed EVERYWHERE because I know we're going to be single-threaded, and doing this also trades off not needing to worry about correct `malloc(3)` usage given the time constraint. Under typical circumstances we'd allocate new buffers, but these circumstances are not typical.
* Name things rationally (looking at you, `uctyf`).
* I understand that `fread`, especially on a minimal BSD, is not optimizing for my one-byte-at-a-time reads. But this would be easily remediable by the implementation of `fread_but_better`. This is left as an exercise for the reader if you made it this far.

## Overview

This tool processes CSV data from force plate and IMU sensors to compute physics-based metrics for backflip analysis:

- **Vertical and horizontal impulses** - Total force integrated over time during takeoff
- **Jump height** - Computed both from air time and from initial velocity (via impulse)
- **Moment of inertia** - Rotational inertia during the flip

## Building

```bash
# This was originally set up on an OpenBSD box. Accordingly, on this OS,
# the system makefiles will build you just fine.
make

# On macOS, Xcode should be able to build the project.
# Otherwise, compile manually:
cc -O2 -Wall -Wextra -Werror -o backflip main.c phy.c math.c csv.c -lm
```

## Usage

```bash
./backflip -c file [-j run] [-f run]
```

### Options

- `-c file` - Path to CSV data file (required)
- `-j run` - Analyze jump run (run number, e.g., `-j 3`)
- `-f run` - Analyze flip run (run number, e.g., `-f 9`)

### Examples

```bash
# Analyze jump run 3 and flip run 9
./backflip -c data.csv -j 3 -f 9

# Analyze only flip run 5
./backflip -c data.csv -f 5

# Analyze jump run 2
./backflip -c data.csv -j 2
```

## CSV Format

The input CSV file should contain columns in the format:
- `Data Set {run}:Time(s)` - Timestamp
- `Data Set {run}:Force(N)` - Vertical force
- `Data Set {run}:Lateral Force(N)` - Horizontal force
- `Data Set {run}:Hang Time(s)` - Detected hang time during flight
- `Data Set {run}:Z-angular velocity(rad/s)` - Rotational velocity
- `Data Set {run}:Z-axis acceleration(m/s2)` - Vertical acceleration

## Output

Results are formatted as:
```
Metric Name                            value ± uncertainty unit
```

Example:
```
=== Backflip Analyzer ===

JUMP RUN #3
  Vertical Impulse               100.234567 ± 5.123456  N s
  Horizontal Impulse               2.345678 ± 0.234568  N s
  True height achieved             0.456789 ± 0.023456  m
  Height via impulse (at feet)     0.434567 ± 0.021234  m
```

## License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details. Actually forking the project per the terms is allowed, of course, but ill-advised.

## Author

Created by Jay Lang in a borderline nightmarish sprint on 2025-11-30
