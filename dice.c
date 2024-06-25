#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/syscall.h>

unsigned char THREADS = 25;

constexpr unsigned char OFFSET = 1;
constexpr unsigned char SIDES = 6;
unsigned int ROLLS = 100;
unsigned long long DICES = 100000000; //100000000 max (10 Mrd. Rolls)

struct thread_data
{
    unsigned char **matrix;
    unsigned long long abs_frq[SIDES];
    unsigned long long total;
    unsigned long long min;
    unsigned long long max;
    bool locked;
};
typedef struct thread_data ThreadData;

struct statistic
{
    unsigned long long absolute_frequency[SIDES];
    double relative_frequency[SIDES];
    unsigned long long total_rolls;
    double total_frequency;
};
typedef struct statistic Statistic;

unsigned char roll(void)
{
    unsigned char random;
    syscall(SYS_getrandom, &random, sizeof(unsigned char), 0);
    return random % SIDES;
}

void thread(ThreadData *data)
{
    for (int i = 0; i < data->max; i++)
    {
        for (int j = 0; j < ROLLS; j++)
        {
            const unsigned char rnd = roll();
            data->matrix[data->min+i][rnd] += 1;
            data->abs_frq[rnd] += 1;
            data->total += 1;
        }
    }
}

Statistic calculate(unsigned char **matrix)
{
    unsigned long long absolute_frequency[SIDES] = {0};
    double relative_frequency[SIDES] = {0};
    unsigned long long total_rolls = 0;
    double total_frequency = 0;

    pthread_t threads[THREADS];
    ThreadData data[THREADS];

    unsigned long long min = 0;
    for (int i = 0; i < THREADS; i++)
    {
        data[i].min = min;
        data[i].max = DICES / THREADS;
        data[i].total = 0;
        memset(data[i].abs_frq, 0, SIDES * sizeof(unsigned long long));
        data[i].matrix = matrix;
        data[i].locked = false;

        min += data[i].max;
        pthread_create(&threads[i], NULL, (void *)thread, &data[i]);
    }

    for (int i = 0; i < THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < THREADS; i++)
    {
        total_rolls += data[i].total;
        for (int j = 0; j < SIDES; j++)
        {
            absolute_frequency[j] += data[i].abs_frq[j];
        }
    }

    for (int i = 0; i < SIDES; i++)
    {
        relative_frequency[i] = (double)absolute_frequency[i] / (double)total_rolls;
        total_frequency += relative_frequency[i];
    }

    Statistic statistic;
    memcpy(statistic.absolute_frequency, absolute_frequency, sizeof statistic.absolute_frequency);
    memcpy(statistic.relative_frequency, relative_frequency, sizeof statistic.relative_frequency);
    statistic.total_rolls = total_rolls;
    statistic.total_frequency = total_frequency;

    return statistic;
}

void save_statistic(const Statistic *stats)
{
    FILE *fptr = fopen("dice_statistic.csv", "w");

    fprintf(fptr, "Häufigkeit");
    for (int i = 0; i < SIDES; i++)
    {
        fprintf(fptr, ",%d", i + OFFSET);
    }
    fprintf(fptr, ",Gesamt\n");

    fprintf(fptr, "Absolut");
    for (int i = 0; i < SIDES; i++)
    {
        fprintf(fptr, ",%llu", stats->absolute_frequency[i]);
    }
    fprintf(fptr, ",%llu\n", stats->total_rolls);

    fprintf(fptr, "Relativ");
    for (int i = 0; i < SIDES; i++)
    {
        fprintf(fptr, ",%.16g", stats->relative_frequency[i]);
    }
    fprintf(fptr, ",%f", stats->total_frequency);

    fclose(fptr);
}

void save_matrix(unsigned char **matrix)
{
    FILE *fptr = fopen("dice.csv", "w");

    fprintf(fptr, "Würfel");
    for (int i = 0; i < SIDES; i++)
    {
        fprintf(fptr, ",%d", i + OFFSET);
    }
    fprintf(fptr, "\n");

    for (int i = 0; i < DICES; i++)
    {
        fprintf(fptr, "%d,", i + 1);
        for (int j = 0; j < SIDES; j++)
        {
            if (j != SIDES-1)
            {
                fprintf(fptr, "%u,", matrix[i][j]);
            } else
            {
                fprintf(fptr, "%u", matrix[i][j]);
            }

        }
        fprintf(fptr, "\n");
    }

    fclose(fptr);
}

int main(int argc, char **argv)
{
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);

    DICES = atoll(argv[1]);
    ROLLS = atoll(argv[2]);
    THREADS = atoll(argv[3]);
    printf("Dices: %llu, Rolls: %u, Threads: %u\n\n", DICES, ROLLS, THREADS);

    unsigned char **total = malloc(DICES * sizeof(unsigned char *));
    for (int i = 0; i < DICES; i++)
    {
        total[i] = malloc(ROLLS * sizeof(unsigned char));
    }
    printf("Allocated memory\n\n");

    printf("Beginn calculation\n");
    const Statistic stats = calculate(total);
    printf("Ended calculation\n\n");

    printf("Writing matrix\n");
    save_matrix(total);
    printf("Ended writing matrix\n\n");

    printf("Finishing programm...\n");
    save_statistic(&stats);

    for (int i = 0; i < DICES; i++)
        free(total[i]);
    free(total);

    gettimeofday(&t1, NULL);
    printf("Finished in %.2g seconds\n", t1.tv_sec - t0.tv_sec + 1E-6 * (t1.tv_usec - t0.tv_usec));

    return 0;
}
