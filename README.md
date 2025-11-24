# SQS_CRAM_MULTI_C — CRAM File Quality Summary Generator ✨

SQS_CRAM_MULTI_C is a lightweight C utility that scans one or more CRAM files and produces a compact summary of basic sequencing quality metrics. Built on top of htslib, it is suitable for quick post-run checks, pipeline smoke tests, or generating per-batch QC snapshots. 📊🧬

---

## ✨ Features

- ✅ Accepts a single CRAM file, a directory containing CRAMs, or a filename prefix
- 🔎 Automatically discovers matching `*.cram` files (excluding names containing `unmatched`) and processes them in alphabetical order
- ➕ Aggregates statistics across all discovered CRAM files into a single summary
- 📈 Reports:
  - 🔢 Total bases and total reads
  - 📏 Average read length
  - 🧪 Base composition (A/T/G/C/N counts)
  - 🧬 GC content (%)
  - ❓ N content (%)
  - 🎯 Q20 and Q30 base counts and percentages
- 🧾 Emits a simple, parseable `.sqs` text report and prints total wall time to stdout ⏱️

---

## 🧭 How it works (high level)

1. 🗂️ Input resolution
   - If the input is:
     - 📄 A regular file: it is used directly if it ends with `.cram`.
     - 📁 A directory: all `*.cram` files under the directory are enumerated (excluding those containing `unmatched`).
     - 🔤 A path-like prefix: the parent directory is scanned and files whose names start with the given prefix and end with `.cram` are selected.
   - Selected files are sorted alphabetically (lexicographic order) 🔤.

2. 📦 CRAM parsing
   - Each CRAM is opened with `sam_open(..., "rc")` and iterated record-by-record via `sam_read1`.
   - For each read:
     - The read length contributes to read and base totals.
     - The per-base sequence and quality arrays are traversed.
     - Base counts (A/T/G/C/N) are incremented from `seq_nt16_str[bam_seqi(seq, i)]`.
     - Q20/Q30 counters are incremented if the raw Phred quality at position `i` satisfies $Q \ge 20$ or $Q \ge 30$.
   - Note: by default, the code does not filter out unmapped/secondary/supplementary reads (the filter is present but commented out).

3. 📤 Aggregation and output
   - Metrics from all files are aggregated in-memory.
   - A single `<input_basename>.sqs` file is written in the current working directory.
   - The program prints total execution time to stdout.

---

## 🧮 Calculations and formulas

Let:
- $B$ be total bases across all processed reads.
- $R$ be total reads.
- $L_r$ be the length of read $r$.
- $A, T, G, C, N$ be base counts across all positions.
- $B_{Q \ge 20}$ and $B_{Q \ge 30}$ be base counts with Phred quality thresholds.
- Phred quality $Q$ at each base is taken directly from `bam_get_qual(b)` (already numeric, not ASCII).

The program computes:

- Total bases  
  - $B = \sum_{r=1}^{R} L_r$
- Average read length  
  - $\overline{L} = \begin{cases} \dfrac{\sum_{r=1}^{R} L_r}{R}, & R > 0 \\ 0, & R = 0 \end{cases}$
- Base counts  
  - Direct tallies over decoded bases: $A, T, G, C, N$
- GC percentage  
  - $\mathrm{GC\%} = 100 \times \dfrac{G + C}{B}$
- N percentage  
  - $\mathrm{N\%} = 100 \times \dfrac{N}{B}$
- Q20 percentage  
  - $\mathrm{Q20\%} = 100 \times \dfrac{B_{Q \ge 20}}{B}$
- Q30 percentage  
  - $\mathrm{Q30\%} = 100 \times \dfrac{B_{Q \ge 30}}{B}$

Notes:
- Phred quality definition: $Q = -10 \log_{10}(p_\text{error})$. This tool does not compute $Q$; it relies on the Phred scores already present in the CRAM stream.
- Threshold counts are inclusive, i.e., $Q \ge 20$ and $Q \ge 30$.

---

## 🛠️ Build

Prerequisites:
- A recent htslib (development headers and library)
- A C11-capable compiler (e.g., `gcc`, `clang`)
- Typical compression backends used by htslib (zlib, bzip2, xz, etc.)

Using pkg-config (recommended):

```bash
gcc -O3 -std=c11 -Wall -Wextra -o sqs_cram_multi sqs_cram_multi.c $(pkg-config --cflags --libs htslib)
```

Without pkg-config (example paths may vary):

```bash
# Adjust include/library paths and the list of libs for your system
gcc -O3 -std=c11 -Wall -Wextra   -I/usr/include/htslib   -L/usr/lib   -o sqs_cram_multi sqs_cram_multi.c   -lhts -lz -lbz2 -llzma -lcurl -lcrypto -lm
```

macOS (Homebrew):

```bash
brew install htslib
gcc -O3 -std=c11 -Wall -Wextra -o sqs_cram_multi sqs_cram_multi.c $(pkg-config --cflags --libs htslib)
```

> Tip: For additional performance, consider adding architecture-specific flags such as `-march=native` when appropriate. ⚡

---

## 🚀 Usage

```text
SQS_CRAM_MULTI_C - CRAM File Quality Summary Generator

Usage:
  ./sqs_cram_multi <CRAM file | CRAM prefix | directory>

Description:
  This tool generates a summary of sequencing quality statistics
  from one or more CRAM files. You can specify:
    - An absolute or relative path to a single CRAM file
    - A CRAM file name in the current directory
    - A file name prefix to process all matching CRAM files in the directory
    - A directory to process all CRAM files within

Examples:
  # Absolute path to a single CRAM file
    ./sqs_cram_multi /path/to/14-1671.cram

  # File name in the current directory
    ./sqs_cram_multi 14-1671.cram

  # Prefix for multiple CRAM files (e.g., 420071-S1_QSR-1*.cram)
    ./sqs_cram_multi 420071-S1_QSR-1

  # All CRAM files in a directory
    ./sqs_cram_multi /path/to/dir
```

Behavioral details:
- 🚫 File discovery excludes filenames containing the substring `unmatched`.
- 🔢 Up to 100 CRAM files are collected per invocation (see the `max_files` array size in code).
- 🔤 Files are processed in lexicographic order.
- 🧮 A single report is produced by aggregating all statistics across the discovered files.

Output:
- 🧾 The report is written to `<input_basename>.sqs` in the current directory.
  - If you pass a directory, `<input_basename>` is the directory name without path.
  - If you pass a prefix, `<input_basename>` is the prefix string (without trailing extension if present).

---

## 🧾 Example `.sqs` output

```text
Sample Name: 420071-S1_QSR-1
Total Bases: 1234567890
Total Reads: 9876543
N Percentage: 0.45%
GC Content: 41.28%
Q20 Percentage: 97.85%
Q30 Percentage: 92.17%
A base count: 182345678
T base count: 183456789
G base count: 254321098
C base count: 255432109
N base count: 543210
Q20 Bases: 1208723456
Q30 Bases: 1138943210
Average Read Length: 124.99
------------------------------------------------------
```
> Values shown above are illustrative.

---

## 🧬 Reference FASTA for CRAM

Some CRAM files require access to the original reference to decode sequences. You have two options:

1) Configure htslib to locate references at runtime (environment variables such as `REF_PATH` or a local reference cache), or  
2) Specify a reference explicitly in code. The source includes a commented example; you can enable and set the path:
```c
// Example (as in the source comment):
// hts_set_fai_filename(in->fp.cram, "/path/to/ref.fa");
```
Make sure the `.fai` index exists (create with `samtools faidx ref.fa`).

---

## 🎯 Optional read filtering

By default, the tool counts all reads, including unmapped, secondary, and supplementary alignments. To restrict to primary mapped reads only, uncomment the following in the loop:

```c
if (b->core.flag & (BAM_FUNMAP | BAM_FSECONDARY | BAM_FSUPPLEMENTARY)) continue;
```

This will:
- ⛔ Skip unmapped reads (`BAM_FUNMAP`)
- 🔁 Skip secondary alignments (`BAM_FSECONDARY`)
- ✂️ Skip supplementary alignments (`BAM_FSUPPLEMENTARY`)

Impact on metrics:
- $R$ and $B$ will generally decrease.
- Base composition, GC%, and Q20/Q30% may shift depending on dataset characteristics.

---

## ⚡ Performance notes

- I/O bound workloads (networked storage) may dominate runtime. Local SSDs can substantially reduce wall time.
- Using `-O3` and, when reasonable, `-march=native` can improve throughput.
- The current implementation is single-threaded. For very large datasets, consider extending it with htslib threading (e.g., `hts_set_threads`) or parallelizing across files at the application level.

---

## ⚠️ Limits and caveats

- Aggregation: Statistics are merged across all matching CRAMs into one report; per-file breakdowns are not emitted.
- Qualities: Q20/Q30 are simple thresholds on raw Phred values; no recalibration or binning is performed.
- Bases: `N` counts include any non-ACGT decoded as `'N'`.
- Discovery: Only files ending in `.cram` are considered; `.bam` or `.sam` are not processed.
- Cap: Up to 100 CRAM files per run (adjust `max_files` array if needed).
- Reference: CRAM decoding may require the corresponding reference; ensure htslib can find it.

---

## 🧩 Exit codes

- `0`: success
- `1`: invalid arguments, no CRAM files found, or output file cannot be created

---

## ♻️ Reproducibility tips

- Record the exact command and htslib version used.
- Keep the reference FASTA (and its index) consistent when decoding CRAMs.
- Prefer immutable storage paths or checksums for inputs.

---

## 🤝 Contributing

Issues and pull requests are welcome. Please include:
- Command used and environment details (OS, compiler, htslib version)
- A minimal reproducible example or a synthetic test CRAM if feasible

---

## 🙏 Acknowledgments

- Built with htslib. Many thanks to its maintainers and contributors.

