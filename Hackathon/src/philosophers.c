#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "oslab.h"

#define MIN_PHILOSOPHERS 2
#define MAX_PHILOSOPHERS 20

typedef enum { STRATEGY_WAITER, STRATEGY_ORDERED } Strategy;
typedef enum { THINKING, HUNGRY, EATING, FINISHED } State;

typedef struct {
    int id;
    int meals_target;
    int meals_done;
    int left_fork;
    int right_fork;
    unsigned long wait_millis;
} Philosopher;

static int philosopher_count = 5;
static int meals_target = 3;
static int quiet_mode = 0;
static int run_forever = 0;
static Strategy strategy = STRATEGY_WAITER;
static char log_path[512] = "dining_log.txt";
static FILE *log_file = NULL;
static pthread_t *threads = NULL;
static Philosopher *philosophers = NULL;
static pthread_mutex_t *forks = NULL;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t state_changed = PTHREAD_COND_INITIALIZER;
static State *states = NULL;
static unsigned long ticket_next = 0;
static unsigned long ticket_serving = 0;
static unsigned long *tickets = NULL;
static int *waiting = NULL;
static volatile sig_atomic_t stop_requested = 0;
static unsigned long total_meals = 0;
static unsigned long total_wait_ms = 0;
static unsigned long max_wait_ms = 0;
static unsigned long *meals_by_philosopher = NULL;

static unsigned long long now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)ts.tv_nsec / 1000000ULL;
}

static void on_signal(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

static void event_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (!quiet_mode) {
        vprintf(fmt, args);
        fflush(stdout);
    }
    va_end(args);
    if (log_file) {
        va_start(args, fmt);
        vfprintf(log_file, fmt, args);
        fflush(log_file);
        va_end(args);
    }
}

static void sleep_ms(long milliseconds) {
    struct timespec req = { milliseconds / 1000, (milliseconds % 1000) * 1000000L };
    while (nanosleep(&req, &req) == -1 && errno == EINTR && !stop_requested) { }
}

/* FIFO waiter: one ticket per meal. Only the ticket at the head may attempt
 * to take forks; it waits until both adjacent forks are free, then claims both
 * atomically while holding state_lock. This breaks circular wait and prevents
 * a philosopher from repeatedly bypassing an older waiter. */
static int waiter_acquire(int id) {
    unsigned long long started = now_ms();
    pthread_mutex_lock(&state_lock);
    unsigned long my_ticket = ticket_next++;
    tickets[id] = my_ticket;
    waiting[id] = 1;
    states[id] = HUNGRY;
    pthread_cond_broadcast(&state_changed);
    while (!stop_requested) {
        int left = id;
        int right = (id + 1) % philosopher_count;
        int is_head = (my_ticket == ticket_serving);
        if (is_head) {
            int forks_free = (pthread_mutex_trylock(&forks[left]) == 0);
            if (forks_free) {
                if (pthread_mutex_trylock(&forks[right]) == 0) {
                    waiting[id] = 0;
                    ticket_serving++;
                    states[id] = EATING;
                    pthread_cond_broadcast(&state_changed);
                    pthread_mutex_unlock(&state_lock);
                    philosophers[id].wait_millis += (unsigned long)(now_ms() - started);
                    return 1; /* both fork mutexes are held by this thread */
                }
                pthread_mutex_unlock(&forks[left]);
            }
        }
        /* Timed waits also let Ctrl+C shut down a run-forever simulation safely. */
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_nsec += 100000000L;
        if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec++; deadline.tv_nsec -= 1000000000L; }
        (void)pthread_cond_timedwait(&state_changed, &state_lock, &deadline);
    }
    waiting[id] = 0;
    pthread_cond_broadcast(&state_changed);
    pthread_mutex_unlock(&state_lock);
    return 0;
}

static void waiter_release(int id) {
    int left = id;
    int right = (id + 1) % philosopher_count;
    pthread_mutex_unlock(&forks[right]);
    pthread_mutex_unlock(&forks[left]);
    pthread_mutex_lock(&state_lock);
    states[id] = THINKING;
    pthread_cond_broadcast(&state_changed);
    pthread_mutex_unlock(&state_lock);
}

/* Resource ordering: acquire the lower-numbered fork first, then the higher.
 * A strict global ordering prevents circular wait, though it does not promise FIFO fairness. */
static int ordered_acquire(int id) {
    int left = id;
    int right = (id + 1) % philosopher_count;
    int first = left < right ? left : right;
    int second = left < right ? right : left;
    unsigned long long started = now_ms();
    pthread_mutex_lock(&state_lock);
    states[id] = HUNGRY;
    pthread_mutex_unlock(&state_lock);
    while (!stop_requested) {
        if (pthread_mutex_trylock(&forks[first]) == 0) {
            if (pthread_mutex_trylock(&forks[second]) == 0) {
                philosophers[id].wait_millis += (unsigned long)(now_ms() - started);
                pthread_mutex_lock(&state_lock);
                states[id] = EATING;
                pthread_mutex_unlock(&state_lock);
                return 1;
            }
            pthread_mutex_unlock(&forks[first]);
        }
        sleep_ms(2);
    }
    return 0;
}

static void ordered_release(int id) {
    int left = id;
    int right = (id + 1) % philosopher_count;
    int first = left < right ? left : right;
    int second = left < right ? right : left;
    pthread_mutex_unlock(&forks[second]);
    pthread_mutex_unlock(&forks[first]);
    pthread_mutex_lock(&state_lock);
    states[id] = THINKING;
    pthread_cond_broadcast(&state_changed);
    pthread_mutex_unlock(&state_lock);
}

static void *philosopher_main(void *arg) {
    Philosopher *p = (Philosopher *)arg;
    while (!stop_requested && (run_forever || p->meals_done < p->meals_target)) {
        pthread_mutex_lock(&state_lock);
        states[p->id] = THINKING;
        pthread_mutex_unlock(&state_lock);
        if (!quiet_mode) event_log("Philosopher %d is thinking.\n", p->id + 1);
        /* Staggered, short thinking time keeps the demo readable and reproducible. */
        sleep_ms(15 + (p->id % 4) * 7);
        if (stop_requested) break;
        event_log("Philosopher %d is hungry and waiting for forks.\n", p->id + 1);
        int acquired = strategy == STRATEGY_WAITER ? waiter_acquire(p->id) : ordered_acquire(p->id);
        if (!acquired) break;
        event_log("Philosopher %d picked up forks %d and %d; EATING.\n", p->id + 1, p->left_fork + 1, p->right_fork + 1);
        sleep_ms(20 + (p->id % 3) * 10);
        p->meals_done++;
        meals_by_philosopher[p->id]++;
        pthread_mutex_lock(&state_lock);
        total_meals++;
        total_wait_ms += p->wait_millis;
        if (p->wait_millis > max_wait_ms) max_wait_ms = p->wait_millis;
        pthread_mutex_unlock(&state_lock);
        event_log("Philosopher %d finished meal %d%s.\n", p->id + 1, p->meals_done,
                  (!run_forever && p->meals_done >= p->meals_target) ? " (target reached)" : "");
        if (strategy == STRATEGY_WAITER) waiter_release(p->id);
        else ordered_release(p->id);
        /* reset per-meal wait measure after adding it to the cumulative total */
        p->wait_millis = 0;
    }
    pthread_mutex_lock(&state_lock);
    states[p->id] = FINISHED;
    pthread_cond_broadcast(&state_changed);
    pthread_mutex_unlock(&state_lock);
    return NULL;
}

static int parse_positive(const char *s, int min, int max, int *out) {
    char *end = NULL;
    long n = strtol(s, &end, 10);
    if (!s[0] || !end || *end != '\0' || n < min || n > max) return 0;
    *out = (int)n;
    return 1;
}

void run_philosophers(int argc, char **argv) {
    /* argc/argv may include command at argv[1], or be called with no command. */
    int start = (argc > 1 && strcmp(argv[1], "simulate") == 0) ? 2 : 1;
    for (int i = start; i < argc; i++) {
        if (strcmp(argv[i], "--philosophers") == 0 && i + 1 < argc) {
            if (!parse_positive(argv[++i], MIN_PHILOSOPHERS, MAX_PHILOSOPHERS, &philosopher_count)) {
                fprintf(stderr, "--philosophers must be between %d and %d.\n", MIN_PHILOSOPHERS, MAX_PHILOSOPHERS); return;
            }
        } else if (strcmp(argv[i], "--meals") == 0 && i + 1 < argc) {
            if (!parse_positive(argv[++i], 0, 1000000, &meals_target)) {
                /* zero is allowed for run-until-interrupted */
                if (strcmp(argv[i], "0") != 0) { fprintf(stderr, "--meals must be 0 or a positive integer.\n"); return; }
                meals_target = 0;
            }
        } else if (strcmp(argv[i], "--strategy") == 0 && i + 1 < argc) {
            const char *s = argv[++i];
            if (strcmp(s, "waiter") == 0) strategy = STRATEGY_WAITER;
            else if (strcmp(s, "ordered") == 0) strategy = STRATEGY_ORDERED;
            else { fprintf(stderr, "Strategy must be waiter or ordered.\n"); return; }
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            if (strlen(argv[++i]) >= sizeof(log_path)) { fprintf(stderr, "Log path is too long.\n"); return; }
            strcpy(log_path, argv[i]);
        } else if (strcmp(argv[i], "--quiet") == 0) quiet_mode = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { print_help(argv[0]); return; }
        else { fprintf(stderr, "Unknown simulation option: %s\n", argv[i]); print_help(argv[0]); return; }
    }
    run_forever = meals_target == 0;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    log_file = fopen(log_path, "a");
    if (!log_file) { perror("Could not open log file; continuing without file logging"); }

    /* Allocate simulation resources for the selected philosopher count. */
    threads = calloc((size_t)philosopher_count, sizeof(*threads));
    philosophers = calloc((size_t)philosopher_count, sizeof(*philosophers));
    forks = calloc((size_t)philosopher_count, sizeof(*forks));
    states = calloc((size_t)philosopher_count, sizeof(*states));
    tickets = calloc((size_t)philosopher_count, sizeof(*tickets));
    waiting = calloc((size_t)philosopher_count, sizeof(*waiting));
    meals_by_philosopher = calloc((size_t)philosopher_count,
                                  sizeof(*meals_by_philosopher));

    if (!threads || !philosophers || !forks || !states ||
        !tickets || !waiting || !meals_by_philosopher) {
        fprintf(stderr, "Unable to allocate simulation memory.\\n");
        free(threads);
        free(philosophers);
        free(forks);
        free(states);
        free(tickets);
        free(waiting);
        free(meals_by_philosopher);
        threads = NULL;
        philosophers = NULL;
        forks = NULL;
        states = NULL;
        tickets = NULL;
        waiting = NULL;
        meals_by_philosopher = NULL;
        if (log_file) {
            fclose(log_file);
            log_file = NULL;
        }
        return;
    }
    event_log("\n=== Dining Philosophers Simulation ===\n");
    event_log("Philosophers: %d | Strategy: %s | Meals each: %s | PID: %ld\n",
              philosopher_count, strategy == STRATEGY_WAITER ? "fair FIFO waiter" : "resource ordering",
              run_forever ? "until Ctrl+C" : "configured target", (long)getpid());
    event_log("Safety design: no circular wait; waiter strategy additionally queues requests FIFO.\n\n");
    for (int i = 0; i < philosopher_count; i++) {
        pthread_mutex_init(&forks[i], NULL);
        philosophers[i] = (Philosopher){ .id=i, .meals_target=meals_target, .meals_done=0,
            .left_fork=i, .right_fork=(i+1)%philosopher_count, .wait_millis=0 };
        states[i] = THINKING;
        waiting[i] = 0;
        meals_by_philosopher[i] = 0;
    }
    int created = 0;
    for (int i = 0; i < philosopher_count; i++) {
        int rc = pthread_create(&threads[i], NULL, philosopher_main, &philosophers[i]);
        if (rc != 0) { fprintf(stderr, "pthread_create failed for philosopher %d: %s\n", i+1, strerror(rc)); stop_requested = 1; break; }
        created++;
    }
    for (int i = 0; i < created; i++) pthread_join(threads[i], NULL);
    event_log("\n=== Final Results ===\n");
    for (int i = 0; i < philosopher_count; i++) event_log("Philosopher %d meals completed: %lu\n", i+1, meals_by_philosopher[i]);
    event_log("Total meals completed: %lu\n", total_meals);
    event_log("Status: %s\n", stop_requested ? "stopped safely by signal/request" : "all configured meal targets completed");
    event_log("Deadlock prevention: %s\n", strategy == STRATEGY_WAITER ? "FIFO waiter grants both forks together" : "global fork ordering prevents circular wait");
    if (strategy == STRATEGY_WAITER) event_log("Starvation mitigation: FIFO ticket order prevents later requests bypassing earlier requests.\n");
    if (log_file) { fclose(log_file); log_file = NULL; }
    for (int i = 0; i < philosopher_count; i++) pthread_mutex_destroy(&forks[i]);

    free(threads);
    free(philosophers);
    free(forks);
    free(states);
    free(tickets);
    free(waiting);
    free(meals_by_philosopher);

    threads = NULL;
    philosophers = NULL;
    forks = NULL;
    states = NULL;
    tickets = NULL;
    waiting = NULL;
    meals_by_philosopher = NULL;

    pthread_mutex_destroy(&state_lock);
    pthread_cond_destroy(&state_changed);
}
