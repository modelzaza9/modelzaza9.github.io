#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <mpc.h>
#include <mpfr.h>
#include <time.h>

#define THREAD_COUNT 2

// Thread data structure
typedef struct {
    mpc_t result;
    mpfr_t angle;
    int sign; // +1 or -1
    int prec;
} ThreadData;

// Exponential calculation thread
void* compute_exponential(void* arg) {
    ThreadData* data = (ThreadData*)arg;

    mpc_t j;
    mpc_init2(j, data->prec);
    mpc_set_ui_ui(j, 0, 1, MPC_RNDNN); // j = i

    mpfr_t temp_angle;
    mpfr_init2(temp_angle, data->prec);

    if (data->sign == -1)
        mpfr_neg(temp_angle, data->angle, MPFR_RNDN);
    else
        mpfr_set(temp_angle, data->angle, MPFR_RNDN);

    mpc_pow_fr(data->result, j, temp_angle, MPC_RNDNN);

    mpc_clear(j);
    mpfr_clear(temp_angle);

    pthread_exit(NULL);
}

int main() {
    // High-resolution timer start
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // === Step 1: Get digit input from user ===
    int digits;
    printf("Enter number of digits you want: ");
    scanf("%d", &digits);

    // === Step 2: Compute m from digits using custom formula ===
    int k = (int)(log10(digits * 3.2)) + 1;
    int n = digits + k;
    int m = (5 * n + 1 + 2) / 3;  // ceiling((5n+1)/3)

    printf("Using d = %d, k = %d, n = %d ⇒ m = %d\n", digits, k, n, m);

    // === Step 3: Compute required precision ===
    double bits_estimate = ceil(digits * log2(10));
    int prec_bits = (int)bits_estimate;
    mpfr_set_default_prec(prec_bits);

    // === Step 4: Setup angles and values ===
    mpfr_t angle, two_pow_m_minus_1, real_part, final_result;
    mpfr_inits2(prec_bits, angle, two_pow_m_minus_1, real_part, final_result, NULL);

    mpfr_ui_pow_ui(angle, 2, m - 1, MPFR_RNDN);         // angle = 2^(m-1)
    mpfr_ui_div(angle, 1, angle, MPFR_RNDN);            // angle = 1 / 2^(m-1)
    mpfr_ui_pow_ui(two_pow_m_minus_1, 2, m - 1, MPFR_RNDN); // 2^(m-1)

    // === Step 5: Launch threads to compute exp(±i * angle) ===
    pthread_t threads[THREAD_COUNT];
    ThreadData data[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        mpc_init2(data[i].result, prec_bits);
        mpfr_init2(data[i].angle, prec_bits);
        mpfr_set(data[i].angle, angle, MPFR_RNDN);
        data[i].sign = (i == 0) ? +1 : -1;
        data[i].prec = prec_bits;

        pthread_create(&threads[i], NULL, compute_exponential, &data[i]);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    // === Step 6: Combine results to get final pi approximation ===
    mpc_t numerator, result, j;
    mpc_init2(numerator, prec_bits);
    mpc_init2(result, prec_bits);
    mpc_init2(j, prec_bits);

    mpc_sub(numerator, data[0].result, data[1].result, MPC_RNDNN);  // exp(iθ) - exp(-iθ)
    mpc_set_ui_ui(j, 0, 1, MPC_RNDNN);                               // j = i
    mpc_div(result, numerator, j, MPC_RNDNN);                        // (exp(iθ) - exp(-iθ)) / i
    mpc_real(real_part, result, MPFR_RNDN);                          // Real part of result
    mpfr_mul(final_result, real_part, two_pow_m_minus_1, MPFR_RNDN);// × 2^(m - 1)

    // === Step 7: Output to file ===
    FILE* f = fopen("pi_result.txt", "w");
    if (f) {
        mpfr_out_str(f, 10, 0, final_result, MPFR_RNDN);
        fprintf(f, "\n");

        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) +
                         (end.tv_nsec - start.tv_nsec) / 1e9;
        fprintf(f, "Execution Time: %.6f seconds\n", elapsed);

        fclose(f);
    }

    // === Step 8: Cleanup ===
    for (int i = 0; i < THREAD_COUNT; i++) {
        mpc_clear(data[i].result);
        mpfr_clear(data[i].angle);
    }
    mpc_clear(j);
    mpc_clear(numerator);
    mpc_clear(result);
    mpfr_clears(angle, two_pow_m_minus_1, real_part, final_result, NULL);

    return 0;
}
