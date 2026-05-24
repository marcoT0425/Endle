#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

#define PATTERN_SPACE 243
#define MAX_BOARDS 2315
#define NUM_TARGETS 2315
#define NUM_DICT 12972

typedef struct { char str[6]; } Word;
typedef struct { Word* data; size_t size; size_t capacity; } WordVector;

WordVector targets;
WordVector dictionary;

/* --- MEMORY CACHE PATTERN MATRIX --- */
unsigned char* pattern_cache = NULL;

int compute_pattern_int(const char* secret, const char* guess) {
    int res[5] = {0, 0, 0, 0, 0};
    char s_tmp[6], g_tmp[6];
    memcpy(s_tmp, secret, 6); memcpy(g_tmp, guess, 6);

    for (int i = 0; i < 5; i++) {
        if (g_tmp[i] == s_tmp[i]) { res[i] = 2; s_tmp[i] = g_tmp[i] = '*'; }
    }
    for (int i = 0; i < 5; i++) {
        if (g_tmp[i] != '*') {
            for (int j = 0; j < 5; j++) {
                if (s_tmp[j] == g_tmp[i]) { res[i] = 1; s_tmp[j] = '*'; break; }
            }
        }
    }
    return res[0] + 3*res[1] + 9*res[2] + 27*res[3] + 81*res[4];
}

void init_pattern_cache(void) {
    pattern_cache = (unsigned char*)malloc((size_t)NUM_TARGETS * NUM_DICT * sizeof(unsigned char));
    if (!pattern_cache) {
        fprintf(stderr, "Allocation Error: Pattern cache matrix memory exhausted.\n");
        exit(1);
    }
    for (size_t t = 0; t < NUM_TARGETS; t++) {
        for (size_t d = 0; d < NUM_DICT; d++) {
            pattern_cache[t * NUM_DICT + d] = (unsigned char)compute_pattern_int(targets.data[t].str, dictionary.data[d].str);
        }
    }
}

int get_pattern_fast(int target_idx, int dict_idx) {
    if (target_idx >= 0 && target_idx < NUM_TARGETS && dict_idx >= 0 && dict_idx < NUM_DICT) {
        return pattern_cache[target_idx * NUM_DICT + dict_idx];
    }
    return 0;
}

int find_dict_index(const char* word) {
    for (size_t i = 0; i < dictionary.size; i++) {
        if (strcmp(dictionary.data[i].str, word) == 0) return (int)i;
    }
    return -1;
}

void vec_init(WordVector* v, size_t cap) {
    v->size = 0; v->capacity = cap;
    v->data = (Word*)malloc(v->capacity * sizeof(Word));
}

void vec_push(WordVector* v, const char* s) {
    if (v->size >= v->capacity) return;
    for (int i = 0; i < 5; i++) v->data[v->size].str[i] = (char)tolower(s[i]);
    v->data[v->size].str[5] = '\0';
    v->size++;
}

double get_entropy_fast(int dict_idx, const int* active_indices, size_t pool_size) {
    if (pool_size <= 1) return 0.0;
    int counts[PATTERN_SPACE] = {0};
    for (size_t i = 0; i < pool_size; i++) {
        counts[get_pattern_fast(active_indices[i], dict_idx)]++;
    }
    double entropy = 0;
    for (int i = 0; i < PATTERN_SPACE; i++) {
        if (counts[i] > 0) {
            double p = (double)counts[i] / (double)pool_size;
            entropy -= p * log2(p);
        }
    }
    return entropy;
}

/* --- TRUECOLOR TERMINAL TILE RENDERER --- */
void print_dark_theme_tile(const char* guess, int pattern_int) {
    int p[5];
    int temp = pattern_int;
    for (int i = 0; i < 5; i++) { p[i] = temp % 3; temp /= 3; }

    for (int i = 0; i < 5; i++) {
        if (p[i] == 2) {
            printf("\033[38;2;221;221;221;48;2;83;141;78m%c\033[0m", toupper(guess[i]));
        } else if (p[i] == 1) {
            printf("\033[38;2;221;221;221;48;2;181;159;59m%c\033[0m", toupper(guess[i]));
        } else {
            printf("\033[38;2;221;221;221;48;2;58;58;60m%c\033[0m", toupper(guess[i]));
        }
    }
}

/* --- LIVE INTERACTIVE SINGLE-GAME AND MULTI-BOARD CORE ENGINE --- */
int play_visual_game(const char* starter, char targets_list[MAX_BOARDS][6], int num_boards, bool cumulative_mode, int* lines_printed) {
    int* pool_indices[MAX_BOARDS];
    size_t pool_sizes[MAX_BOARDS];
    bool board_solved[MAX_BOARDS];
    int solved_on_turn[MAX_BOARDS];
    *lines_printed = 0;

    int actual_target_global_idx[MAX_BOARDS];
    for (int b = 0; b < num_boards; b++) {
        actual_target_global_idx[b] = -1;
        for (size_t i = 0; i < targets.size; i++) {
            if (strcmp(targets.data[i].str, targets_list[b]) == 0) {
                actual_target_global_idx[b] = (int)i;
                break;
            }
        }
    }

    for (int b = 0; b < num_boards; b++) {
        pool_indices[b] = (int*)malloc(targets.size * sizeof(int));
        pool_sizes[b] = targets.size;
        for (size_t i = 0; i < targets.size; i++) pool_indices[b][i] = (int)i;
        board_solved[b] = false;
        solved_on_turn[b] = 0;
    }

    char guess[6];
    strcpy(guess, starter);
    int guess_dict_idx = find_dict_index(guess);

    int max_simulation_turns = (num_boards > 1) ? (5 + num_boards) : 6;
    if (max_simulation_turns > 24) max_simulation_turns = 24;

    for (int turn = 1; turn <= max_simulation_turns; turn++) {
        if (cumulative_mode) {
            printf("Turn %d: Guessing \"", turn);
            for (int b = 0; b < num_boards; b++) {
                if (board_solved[b] && turn > solved_on_turn[b]) {
                    printf("\033[32m     \033[0m");
                } else {
                    int p = compute_pattern_int(targets_list[b], guess);
                    print_dark_theme_tile(guess, p);
                }
                if (b < num_boards - 1) printf(" ");
            }
            printf("\" (Remaining options -> ");
            for (int b = 0; b < num_boards; b++) {
                if (board_solved[b]) printf("B%d: Solved | ", b + 1);
                else printf("B%d: %zu left | ", b + 1, pool_sizes[b]);
            }
            printf("\n");
            (*lines_printed)++;
        } else {
            if (num_boards == 1) {
                printf("Turn %d: Guessing \"", turn);
                int p = compute_pattern_int(targets_list[0], guess);
                print_dark_theme_tile(guess, p);
                printf("\" (Remaining possibilities: %zu)\n", pool_sizes[0]);
                (*lines_printed)++;
            } else {
                printf("Turn %d: Guessing \"", turn);
                for (int b = 0; b < num_boards; b++) {
                    if (board_solved[b] && turn > solved_on_turn[b]) {
                        printf("\033[32m     \033[0m");
                    } else {
                        int p = compute_pattern_int(targets_list[b], guess);
                        print_dark_theme_tile(guess, p);
                    }
                    if (b < num_boards - 1) printf(" ");
                }
                printf("\"\n");
                (*lines_printed)++;

                printf("    Remaining options -> ");
                for (int b = 0; b < num_boards; b++) {
                    if (board_solved[b]) {
                        printf("B%d: Solved (%d) | ", b + 1, solved_on_turn[b]);
                    } else {
                        printf("B%d: %zu left | ", b + 1, pool_sizes[b]);
                    }
                }
                printf("\n");
                (*lines_printed)++;
            }
        }

        bool all_solved = true;
        for (int b = 0; b < num_boards; b++) {
            if (board_solved[b]) continue;

            int p = (actual_target_global_idx[b] != -1 && guess_dict_idx != -1) ?
                    get_pattern_fast(actual_target_global_idx[b], guess_dict_idx) :
                    compute_pattern_int(targets_list[b], guess);

            if (p == 242) {
                board_solved[b] = true;
                solved_on_turn[b] = turn;
                pool_sizes[b] = 1;
                continue;
            }

            all_solved = false;
            size_t next_idx = 0;
            for (size_t i = 0; i < pool_sizes[b]; i++) {
                int target_id = pool_indices[b][i];
                if (guess_dict_idx != -1) {
                    if (get_pattern_fast(target_id, guess_dict_idx) == p) {
                        pool_indices[b][next_idx++] = target_id;
                    }
                } else {
                    if (compute_pattern_int(targets.data[target_id].str, guess) == p) {
                        pool_indices[b][next_idx++] = target_id;
                    }
                }
            }
            pool_sizes[b] = next_idx;
        }

        if (all_solved) {
            for (int b = 0; b < num_boards; b++) free(pool_indices[b]);
            int final_max = 0;
            for (int b = 0; b < num_boards; b++) {
                if (solved_on_turn[b] > final_max) final_max = solved_on_turn[b];
            }
            return final_max;
        }

        if (turn == max_simulation_turns) {
            for (int b = 0; b < num_boards; b++) free(pool_indices[b]);
            return max_simulation_turns + 1;
        }

        double best_combined_entropy = -1.0;
        int best_dict_idx = -1;

        for (int b = 0; b < num_boards; b++) {
            if (!board_solved[b] && pool_sizes[b] == 1) {
                best_dict_idx = find_dict_index(targets.data[pool_indices[b][0]].str);
                best_combined_entropy = 999.0;
                break;
            }
        }

        if (best_combined_entropy < 900.0) {
            for (size_t i = 0; i < dictionary.size; i++) {
                double combined_entropy = 0.0;
                for (int b = 0; b < num_boards; b++) {
                    if (board_solved[b]) continue;
                    combined_entropy += get_entropy_fast((int)i, pool_indices[b], pool_sizes[b]);
                }
                for (int b = 0; b < num_boards; b++) {
                    if (!board_solved[b]) {
                        for (size_t k = 0; k < pool_sizes[b]; k++) {
                            if (strcmp(dictionary.data[i].str, targets.data[pool_indices[b][k]].str) == 0) {
                                combined_entropy += 0.05;
                                break;
                            }
                        }
                    }
                }
                if (combined_entropy > best_combined_entropy) {
                    best_combined_entropy = combined_entropy;
                    best_dict_idx = (int)i;
                }
            }
        }

        if (best_dict_idx == -1) {
            for (int b = 0; b < num_boards; b++) {
                if (!board_solved[b]) {
                    strcpy(guess, targets.data[pool_indices[b][0]].str);
                    guess_dict_idx = find_dict_index(guess);
                    break;
                }
            }
        } else {
            strcpy(guess, dictionary.data[best_dict_idx].str);
            guess_dict_idx = best_dict_idx;
        }
    }

    for (int b = 0; b < num_boards; b++) free(pool_indices[b]);
    return max_simulation_turns + 1;
}

long long get_combinations_count(long long n, int k) {
    if (k == 1) return n;
    if (k == 2) return (n * (n - 1)) / 2;
    if (k == 3) return (n * (n - 1) * (n - 2)) / 6;
    if (k == 4) return (n * (n - 1) * (n - 2) * (n - 3)) / 24;
    return 0;
}

void dhms_format(double total_sec, char* buffer) {
    int d = (int)(total_sec / 86400.0); total_sec -= d * 86400.0;
    int h = (int)(total_sec / 3600.0);  total_sec -= h * 3600.0;
    int m = (int)(total_sec / 60.0);    total_sec -= m * 60.0;
    sprintf(buffer, "%02dd:%02dh:%02dm:%06.3fs", d, h, m, total_sec);
}

/* --- REPLACEMENT-BASED ANALYSIS MATRIX RUNNER --- */
void run_cumulative_analysis(const char* starter, int mode_type) {
    printf("\n--------------------------------------------------------------------\n");
    int distribution[16] = {0};
    double running_sum = 0.0;
    long long items_processed = 0;
    int total_failures = 0;

    char game_target_wrapper[MAX_BOARDS][6];
    int lines_to_clear = 0;

    long long total_combinations = get_combinations_count((long long)targets.size, mode_type);
    int limit_check = (mode_type == 1) ? 6 : ((mode_type == 2) ? 9 : ((mode_type == 3) ? 11 : 13));

    // Speedrun baseline clock stopwatch resets ONCE post colour-matrix generation here
    time_t analysis_start_wall_time = time(NULL);

    for (size_t i = 0; i < targets.size; i++) {
        for (size_t j = (mode_type >= 2 ? i + 1 : 0); j < (mode_type >= 2 ? targets.size : 1); j++) {
            for (size_t k = (mode_type >= 3 ? j + 1 : 0); k < (mode_type >= 3 ? targets.size : 1); k++) {
                for (size_t m = (mode_type >= 4 ? k + 1 : 0); m < (mode_type >= 4 ? targets.size : 1); m++) {

                    if (lines_to_clear > 0) {
                        for (int l = 0; l < lines_to_clear; l++) {
                            printf("\033[A\033[2K");
                        }
                        fflush(stdout);
                    }

                    strcpy(game_target_wrapper[0], targets.data[i].str);
                    if (mode_type >= 2) strcpy(game_target_wrapper[1], targets.data[j].str);
                    if (mode_type >= 3) strcpy(game_target_wrapper[2], targets.data[k].str);
                    if (mode_type >= 4) strcpy(game_target_wrapper[3], targets.data[m].str);

                    items_processed++;

                    printf("--- [Game %lld/%lld] Simulating Targets Set: [ ", items_processed, total_combinations);
                    for (int b = 0; b < mode_type; b++) printf("%s ", game_target_wrapper[b]);
                    printf("] ---\n");
                    int local_lines = 1;

                    int visual_lines = 0;
                    int turns = play_visual_game(starter, game_target_wrapper, mode_type, true, &visual_lines);
                    local_lines += visual_lines;

                    if (turns <= limit_check) {
                        distribution[turns]++;
                        running_sum += turns;
                        printf(" 🟩 Result: Solved in %d turns.\n", turns);
                    } else {
                        distribution[15]++;
                        total_failures++;
                        printf(" 🟥 Result: Unsolved (Exceeded Max Simulation Guesses).\n");
                    }
                    local_lines++;

                    double current_avg = (items_processed - total_failures > 0) ? running_sum / (items_processed - total_failures) : 0.0;
                    printf(" 📊 Running Stats Summary -> Avg Turns: %.3f | Total Failures: %d\n", current_avg, total_failures);
                    local_lines++;

                    // RUNNING ENGINE TIMELINE EXTRAPOLATIONS
                    double timer_now = (double)(time(NULL) - analysis_start_wall_time);
                    double games_per_sec = (timer_now > 0.0) ? ((double)items_processed / timer_now) : 0.0;
                    double final_trajectory_est_seconds = (games_per_sec > 0.0) ? ((double)total_combinations / games_per_sec) : 0.0;

                    char timer_now_formatted[128];
                    char trajectory_x_formatted[128];

                    dhms_format(timer_now, timer_now_formatted);
                    dhms_format(final_trajectory_est_seconds, trajectory_x_formatted);

                    // Precise Requested Line: games played / timer now = X (formatted time estimation to finish total profile)
                    printf(" ⏱️  %lld games played / [%s] timer now = [%s]\n",
                           items_processed, timer_now_formatted, trajectory_x_formatted);
                    local_lines++;

                    printf("--------------------------------------------------------------------\n");
                    local_lines++;

                    lines_to_clear = local_lines;
                    fflush(stdout);
                }
            }
        }
    }

    double total_seconds_run = (double)(time(NULL) - analysis_start_wall_time);
    char total_run_dhms[128];
    dhms_format(total_seconds_run, total_run_dhms);

    printf("\n==================== FINAL CUMULATIVE PERFORMANCE PROFILE ====================\n");
    printf(" Evaluating Starter Token   :  \"%s\"\n", starter);
    printf(" Processed Target Count     :  %lld\n", items_processed);
    printf(" Successful Strategy Clears :  %lld/%lld (%.2f%%)\n", items_processed - total_failures, items_processed, ((double)(items_processed - total_failures) / items_processed) * 100.0);
    printf(" Benchmark Turn Average     :  %.3f\n", running_sum / (items_processed - total_failures));
    printf(" Total Simulation Time Run  :  %s\n", total_run_dhms);
    printf("------------------------------------------------------------------------------\n");
    printf(" SOLVE DISTRIBUTION ANALYSIS:\n");
    for (int t = 1; t <= limit_check; t++) {
        printf("   %d Turns: %lld words  (%.2f%%)\n", t, (long long)distribution[t], ((double)distribution[t] / items_processed) * 100.0);
    }
    printf("   Turn X (Failures)  : %lld words  (%.2f%%)\n", (long long)distribution[15], ((double)distribution[15] / items_processed) * 100.0);
    printf("==============================================================================\n");
}

int main() {
    vec_init(&targets, NUM_TARGETS);
    vec_init(&dictionary, NUM_DICT);

    const char* p_path = "/Users/marco/CLionProjects/Endle/proper word.txt";
    const char* l_path = "/Users/marco/CLionProjects/Endle/word list.txt";

    FILE *f1 = fopen(p_path, "r"), *f2 = fopen(l_path, "r");
    if (!f1 || !f2) { printf("Error: Dictionary files not found.\n"); return 1; }

    char buf[256];
    while (fscanf(f1, "%s", buf) != EOF && targets.size < NUM_TARGETS) {
        if (strlen(buf) == 5) vec_push(&targets, buf);
    }
    while (fscanf(f2, "%s", buf) != EOF && dictionary.size < NUM_DICT) {
        if (strlen(buf) == 5) vec_push(&dictionary, buf);
    }
    fclose(f1); fclose(f2);

    init_pattern_cache();

    char starting_input[256];
    char mode_select[16];

    printf("What is the starting word?: ");
    if (scanf("%255s", starting_input) != 1) return 1;
    for (int i = 0; i < 5; i++) starting_input[i] = (char)tolower(starting_input[i]);
    starting_input[5] = '\0';

    printf("Cumulative Selection? (y / y2 / y3 / y4 / n): ");
    if (scanf("%15s", mode_select) != 1) return 1;

    int mode_flag = 0;
    if (strcmp(mode_select, "y") == 0) mode_flag = 1;
    else if (strcmp(mode_select, "y2") == 0) mode_flag = 2;
    else if (strcmp(mode_select, "y3") == 0) mode_flag = 3;
    else if (strcmp(mode_select, "y4") == 0) mode_flag = 4;

    if (mode_flag > 0) {
        run_cumulative_analysis(starting_input, mode_flag);
    } else {
        char target_input_buffer[1024];
        char separated_targets[MAX_BOARDS][6];
        int board_count = 0;

        printf("Target?: ");
        if (scanf("%1023s", target_input_buffer) != 1) return 1;

        char* token = strtok(target_input_buffer, ",");
        while (token != NULL && board_count < MAX_BOARDS) {
            if (strlen(token) == 5) {
                for (int i = 0; i < 5; i++) separated_targets[board_count][i] = (char)tolower(token[i]);
                separated_targets[board_count][5] = '\0';
                board_count++;
            }
            token = strtok(NULL, ",");
        }

        if (board_count == 0) {
            printf("Error: No valid targets detected.\n");
            free(pattern_cache); free(targets.data); free(dictionary.data);
            return 1;
        }

        int dummy_lines = 0;
        play_visual_game(starting_input, separated_targets, board_count, false, &dummy_lines);
        printf("\n");
    }

    free(pattern_cache);
    free(targets.data);
    free(dictionary.data);
    return 0;
}
