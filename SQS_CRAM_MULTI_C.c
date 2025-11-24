#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <htslib/sam.h>
#include <time.h>

#define BASE_A 0
#define BASE_T 1
#define BASE_G 2
#define BASE_C 3
#define BASE_N 4

int base_index(char base) {
    switch (base) {
        case 'A': return BASE_A;
        case 'T': return BASE_T;
        case 'G': return BASE_G;
        case 'C': return BASE_C;
        case 'N': return BASE_N;
        default: return BASE_N;
    }
}

// cram_files: 파일 경로 목록, max_files: 최대 개수
// path: 파일/디렉토리/경로+prefix
int find_cram_files(const char* path, char cram_files[][1024], int max_files) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISREG(st.st_mode)) {
            // 파일이면 그대로 추가
            strncpy(cram_files[0], path, 1023);
            cram_files[0][1023] = 0;
            return 1;
        } else if (S_ISDIR(st.st_mode)) {
            // 디렉토리면, 그 안에서 *.cram 모두 추가
            DIR *dir = opendir(path);
            struct dirent *entry;
            int count = 0;
            while ((entry = readdir(dir)) != NULL) {
                if (strstr(entry->d_name, ".cram") && strstr(entry->d_name, "unmatched") == NULL) {
                    snprintf(cram_files[count], 1023, "%s/%s", path, entry->d_name);
                    cram_files[count][1023] = 0;
                    if (++count >= max_files) break;
                }
            }
            closedir(dir);
            return count;
        }
    }
    // 경로+prefix (ex: /path/to/14-1671)
    char dirbuf[1024], prefix[512];
    strcpy(dirbuf, path);
    char *slash = strrchr(dirbuf, '/');
    if (slash) {
        *slash = 0;
        strcpy(prefix, slash + 1);
    } else {
        strcpy(dirbuf, ".");
        strcpy(prefix, path);
    }
    DIR *dir = opendir(dirbuf);
    struct dirent *entry;
    int count = 0;
    size_t prefix_len = strlen(prefix);
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefix_len) == 0 &&
            strstr(entry->d_name, ".cram") &&
            strstr(entry->d_name, "unmatched") == NULL) {
            snprintf(cram_files[count], 1023, "%s/%s", dirbuf, entry->d_name);
            cram_files[count][1023] = 0;
            if (++count >= max_files) break;
        }
    }
    closedir(dir);
    return count;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr,
            "\n"
            "============================================================\n"
            "  SQS_CRAM_MULTI_C - CRAM File Quality Summary Generator\n"
            "============================================================\n"
            "Usage:\n"
            "  %s <CRAM file | CRAM prefix | directory>\n"
            "\n"
            "Description:\n"
            "  This tool generates a summary of sequencing quality statistics\n"
            "  from one or more CRAM files. You can specify:\n"
            "    - An absolute or relative path to a single CRAM file\n"
            "    - A CRAM file name in the current directory\n"
            "    - A file name prefix to process all matching CRAM files in the directory\n"
            "    - A directory to process all CRAM files within\n"
            "\n"
            "Examples:\n"
            "  # Absolute path to a single CRAM file\n"
            "    %s /path/to/14-1671.cram\n"
            "\n"
            "  # File name in the current directory\n"
            "    %s 14-1671.cram\n"
            "\n"
            "  # Prefix for multiple CRAM files (e.g., 420071-S1_QSR-1*.cram)\n"
            "    %s 420071-S1_QSR-1\n"
            "\n"
            "  # All CRAM files in a directory\n"
            "    %s /path/to/dir\n"
            "\n"
            "------------------------------------------------------------\n"
            "Output:\n"
            "  For each run, a summary file <sample>.sqs will be generated\n"
            "  in the current directory.\n"
            "============================================================\n"
            "\n", argv[0], argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }

    char cram_files[100][1024];
    int cram_count = find_cram_files(argv[1], cram_files, 100);

    if (cram_count == 0) {
        fprintf(stderr, "ERROR: no .cram files for '%s'\n", argv[1]);
        return 1;
    }

    long long total_bases = 0, total_reads = 0, total_length = 0;
    long long base_counts[5] = {0, 0, 0, 0, 0}; // A, T, G, C, N
    long long q20_bases = 0, q30_bases = 0;

    clock_t start = clock();

    // 알파벳순 정렬
    for (int i = 0; i < cram_count - 1; i++) {
        for (int j = i + 1; j < cram_count; j++) {
            if (strcmp(cram_files[i], cram_files[j]) > 0) {
                char tmp[1024];
                strcpy(tmp, cram_files[i]);
                strcpy(cram_files[i], cram_files[j]);
                strcpy(cram_files[j], tmp);
            }
        }
    }

    for (int f = 0; f < cram_count; f++) {
        samFile *in = sam_open(cram_files[f], "rc");
        if (in == NULL) {
            fprintf(stderr, "Error opening CRAM file: %s\n", cram_files[f]);
            continue;
        }
        // 필요시 참조 fasta 지정: hts_set_fai_filename(in->fp.cram, "/path/to/ref.fa");
        bam1_t *b = bam_init1();
        bam_hdr_t *header = sam_hdr_read(in);

        while (sam_read1(in, header, b) >= 0) {
            // unmapped, secondary, supplementary read는 건너뜀
            //if (b->core.flag & (BAM_FUNMAP | BAM_FSECONDARY | BAM_FSUPPLEMENTARY)) continue;

            uint8_t *seq = bam_get_seq(b);
            uint8_t *qual = bam_get_qual(b);
            int read_length = b->core.l_qseq;

            total_reads++;
            total_length += read_length;
            total_bases += read_length;

            for (int i = 0; i < read_length; i++) {
                char base = seq_nt16_str[bam_seqi(seq, i)];
                int idx = base_index(base);
                base_counts[idx]++;
                int q = qual[i];
                if (q >= 20) q20_bases++;
                if (q >= 30) q30_bases++;
            }
        }
        bam_destroy1(b);
        bam_hdr_destroy(header);
        sam_close(in);
    }

    double avg_read_length = (total_reads > 0) ? (double)total_length / total_reads : 0.0;
    double q20p = (total_bases > 0) ? ((double)q20_bases * 100.0 / total_bases) : 0.0;
    double q30p = (total_bases > 0) ? ((double)q30_bases * 100.0 / total_bases) : 0.0;
    double nPct = (total_bases > 0) ? ((double)base_counts[BASE_N] * 100.0 / total_bases) : 0.0;
    double gcPct = (total_bases > 0) ? ((double)(base_counts[BASE_G] + base_counts[BASE_C]) * 100.0 / total_bases) : 0.0;

    char output_file[1024];
    // 출력 파일명: prefix만 추출
    const char *slash = strrchr(argv[1], '/');
    const char *basename = slash ? slash + 1 : argv[1];
    char sample_name[512];
    strcpy(sample_name, basename);
    char *dot = strrchr(sample_name, '.');
    if (dot) *dot = 0;
    snprintf(output_file, sizeof(output_file), "%s.sqs", sample_name);

    FILE *f = fopen(output_file, "w");
    if (!f) {
        fprintf(stderr, "Error opening output file\n");
        return 1;
    }
    fprintf(f, "Sample Name: %s\n", sample_name);
    fprintf(f, "Total Bases: %lld\n", total_bases);
    fprintf(f, "Total Reads: %lld\n", total_reads);
    fprintf(f, "N Percentage: %.2f%%\n", nPct);
    fprintf(f, "GC Content: %.2f%%\n", gcPct);
    fprintf(f, "Q20 Percentage: %.2f%%\n", q20p);
    fprintf(f, "Q30 Percentage: %.2f%%\n", q30p);
    fprintf(f, "A base count: %lld\n", base_counts[BASE_A]);
    fprintf(f, "T base count: %lld\n", base_counts[BASE_T]);
    fprintf(f, "G base count: %lld\n", base_counts[BASE_G]);
    fprintf(f, "C base count: %lld\n", base_counts[BASE_C]);
    fprintf(f, "N base count: %lld\n", base_counts[BASE_N]);
    fprintf(f, "Q20 Bases: %lld\n", q20_bases);
    fprintf(f, "Q30 Bases: %lld\n", q30_bases);
    fprintf(f, "Average Read Length: %.2f\n", avg_read_length);
    fprintf(f, "------------------------------------------------------\n");
    fclose(f);

    clock_t end = clock();
    double execution_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Execution Time: %.2f seconds\n", execution_time);

    return 0;
}
