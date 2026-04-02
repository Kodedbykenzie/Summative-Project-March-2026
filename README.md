# Summative Project — Build, Run, and Expected Output

This guide is written for **macOS on a MacBook** (Apple **Silicon** M1/M2/M3 or **Intel**). The same source files can be built on **Linux** for lab VMs or ELF-specific coursework; those cases are called out explicitly.

All commands assume you are in the project directory:

```bash
cd /path/to/Summative-Project-March-2026
```

---

## MacBook setup (do this once)

| Need | What to install |
|------|------------------|
| **C compiler, `make`, headers** | **Xcode Command Line Tools:** `xcode-select --install` |
| **Homebrew** (recommended) | See [brew.sh](https://brew.sh) — used below for NASM and optional tools |
| **Python 3** | Preinstalled on many Macs; or `brew install python` — you need headers for the C extension (CLT usually enough) |
| **Docker Desktop** (optional, **strongly recommended** for §2) | [docker.com](https://www.docker.com/products/docker-desktop/) — builds and runs the **Linux x86-64 ELF** assembly binary the way the assignment expects |

**Shell:** Examples use **zsh** (default on macOS). **Terminal.app** or **iTerm2** both work.

**Important distinction**

- **`gcc` on macOS** (really **clang** front-end) produces **Mach-O** binaries, not Linux **ELF**.
- For **Question 1** report tools (`readelf`, ELF `objdump`, `strace`, Linux `gdb` on ELF), you still need a **Linux** environment (lab PC, VM, or remote) for the *submission* binary and analysis — your Mac is fine for **editing and testing** the C program.

---

## 1. ELF demo — `program.c`

### On your MacBook (recommended for day-to-day)

Build **without** `strip` first (stripping sometimes triggers security tools). If the name `program` causes **`zsh: killed`** (exit **137**), use a clearer name:

```bash
gcc -Wall -O0 -fno-inline -o q1_demo program.c
./q1_demo
```

Expected:

```text
Result: 8
```

If `./q1_demo` is blocked, try: `xattr -cr .` then `codesign -s - -f ./q1_demo` then run again (see **Troubleshooting**).

### Assignment hand-in (Linux ELF + `strip`)

Where the brief requires **ELF** and `strip`, run **on Linux** (lab / VM):

```bash
gcc -Wall -O0 -fno-inline -o program program.c
strip program
```

On macOS, the same commands produce a **Mach-O** `program`, not ELF — use Linux for the actual **ELF** file and for `readelf` / ELF-focused analysis.

### Input

None.

### Behaviour

Allocates with `malloc`, fills the array, sums positive elements, adds global `global_counter` (5), prints the result.

---

## 2. Assembly temperature counter — `temperature.asm`

This source targets **64-bit Linux syscalls** and **ELF** (`nasm -f elf64`). On a MacBook:

1. **Assemble** on the Mac (optional check that NASM accepts the file):

   ```bash
   brew install nasm
   nasm -f elf64 temperature.asm -o temperature.o
   ```

2. **Link and run** — Apple’s system **`ld` is not the Linux linker**; you will see errors such as **`ld: Missing -arch option`** if you run `ld temperature.o -o temperature` on macOS. Use one of:

   - **Docker** (simplest on a Mac if Docker Desktop is installed):

     ```bash
     docker run --rm -v "$PWD":/work -w /work ubuntu:22.04 bash -lc \
       "apt-get update -qq && apt-get install -y -qq nasm binutils && nasm -f elf64 temperature.asm -o temperature.o && ld temperature.o -o temperature && ./temperature"
     ```

   - **Linux VM / lab machine** — run `nasm` + `ld` there and execute `./temperature` in that environment.

**Apple Silicon:** The binary is **x86-64** Linux code. Docker on Apple Silicon often uses **QEMU** under the hood for **linux/amd64** images; the one-liner above still works for building and running inside the container. Running the ELF **natively** on the bare Mac is not the target platform.

### Input files

- Place **`temperature_data.txt`** next to the binary (same relative path as in the assignment).
- Bundled sample file: **6** total lines, **4** valid (non-empty) lines.

### Expected output (bundled file)

```text
Total readings: 6
Valid readings: 4
```

### Error case

Cannot open file → message on **stderr**, exit code **1**.

---

## 3. Python C extension — `vibration.c` + `setup.py` + `test.py`

Works **natively on macOS**.

```bash
python3 setup.py build_ext --inplace
python3 test.py
```

This creates a module like `vibration.cpython-312-darwin.so` (exact name depends on Python version and arch).

### Input / notes

- Lists/tuples must contain **Python `float`** objects only (not bare `int`s).

### Expected output (representative)

```text
=== Normal sample ===
peak_to_peak: 3.0
rms: 2.7386127875258306
...
=== Invalid input (expect TypeError) ===
Caught: all elements must be float
...
```

---

## 4. Baggage simulation — `baggage.c`

Works **natively on macOS** (POSIX threads).

```bash
gcc -Wall -Wextra -O2 -o baggage baggage.c -pthread
./baggage
```

Runs until **20** items complete (on the order of **minutes** due to sleeps). Ends with `Simulation complete (20 items).`

---

## 5. Library server and client — `server.c` / `client.c`

Works **natively on macOS**. Default port **8080**.

```bash
gcc -Wall -Wextra -O2 -o server server.c -pthread
gcc -Wall -Wextra -O2 -o client client.c
```

**Terminal 1**

```bash
./server
```

**Terminal 2**

```bash
./client
```

Valid IDs: `LIB101`, `LIB102`, `LIB103`, `LIB104`. Reserve using exact book titles (e.g. `Signals-Systems`).

**Free the port if needed**

```bash
lsof -i :8080
kill <PID>
```

**Quick automated test** (after server is up)

```bash
./server &
sleep 1
printf 'LIB101\nSignals-Systems\n' | ./client
kill %1
```

---

## Quick checklist (MacBook)

```bash
# Q1 — local Mach-O (rename if ./program is killed)
gcc -Wall -O0 -fno-inline -o q1_demo program.c && ./q1_demo

# Q3
python3 setup.py build_ext --inplace && python3 test.py

# Q4
gcc -Wall -Wextra -O2 -o baggage baggage.c -pthread

# Q5
gcc -Wall -Wextra -O2 -o server server.c -pthread
gcc -Wall -Wextra -O2 -o client client.c

# Q2 — use Docker one-liner in §2, or Linux lab
```

For **ELF analysis** and the **stripped Linux `program`**, repeat the Q1 compile/`strip` on **Linux** and keep `program.stripped` from:

```bash
gcc -Wall -O0 -fno-inline -o program program.c
strip -o program.stripped program
```

---

## Troubleshooting (MacBook)

| Issue | What to try |
|--------|-------------|
| **`xcode-select: note: no developer tools`** | Run `xcode-select --install`. |
| **`zsh: killed` / exit **137** on `./program` or `./q1_demo`** | SIGKILL — skip `strip`; rename binary; `xattr -cr .`; `codesign -s - -f ./q1_demo`. |
| **`nasm: command not found`** | `brew install nasm`. |
| **`ld: Missing -arch option`** | Do not use Apple `ld` for ELF — use **Docker** or **Linux** to link (§2). |
| **`docker: command not found`** | Install **Docker Desktop** for Mac, or do assembly on a Linux lab PC. |
| **`python3 setup.py` fails** | Ensure **Xcode CLT** is installed; use `python3` that matches your environment (`which python3`). |
| **Port **8080** in use** | `lsof -i :8080` then `kill <PID>`, or change `PORT` in both `.c` files and recompile. |
| **Client auth fails** | Same machine: only one server; use IDs `LIB101`–`LIB104` exactly. |

---

## Expected outputs (summary)

| Piece | Expected |
|--------|----------|
| **Q1** | `Result: 8` |
| **Q2** (bundled `temperature_data.txt`) | `Total readings: 6` / `Valid readings: 4` |
| **Q3** | `test.py` sections print statistics and catch `TypeError` on bad types |
| **Q4** | Loader / Aircraft / Monitor lines; ends with `Simulation complete (20 items).` |
| **Q5** | `Authentication: OK`, book list, `Reserved successfully.` or `TAKEN`, 
