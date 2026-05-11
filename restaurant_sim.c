#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <signal.h>

#define MAX_QUEUE       50
#define MAX_ORDERS      500
#define MAX_CHEFS       8
#define MIN_CHEFS       2
#define MAX_LOG_LINES   200
#define STATE_FILE      "sim_state.json"
#define CANCEL_FILE     "cancel_cmd.txt"
#define SPEED_FILE      "speed_cmd.txt"

typedef enum {
    STATE_RECEIVED    = 0,
    STATE_IN_QUEUE    = 1,
    STATE_PREPARING   = 2,
    STATE_COMPLETED   = 3,
    STATE_CANCELLED   = 4
} OrderState;

typedef struct {
    int   id;
    int   priority;
    char  item[64];
    int   prep_time;
    OrderState state;
    time_t created_at;
    time_t completed_at;
    int   chef_id;
    int   cancelled;
} Order;

typedef struct {
    Order *data[MAX_QUEUE];
    int   size;
} PriorityQueue;

typedef struct {
    int       id;
    int       active;
    int       orders_done;
    Order    *current_order;
    pthread_t thread;
} Chef;

static PriorityQueue  queue;
static Order          all_orders[MAX_ORDERS];
static int            order_count    = 0;
static int            next_order_id  = 1;
static int            running        = 1;
static int            active_chefs   = MIN_CHEFS;
static double         sim_speed      = 1.0;

static pthread_mutex_t queue_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t state_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_mutex     = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  queue_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  queue_not_full  = PTHREAD_COND_INITIALIZER;
static sem_t           kitchen_sem;

static Chef   chefs[MAX_CHEFS];
static char   log_lines[MAX_LOG_LINES][256];
static int    log_head = 0, log_count = 0;

static const char *menu_items[] = {
    "Margherita Pizza", "Grilled Salmon", "Caesar Salad",
    "Beef Burger",      "Pasta Carbonara","Chicken Tikka",
    "Sushi Platter",    "Lamb Chops",     "Vegetable Stir-fry",
    "Chocolate Lava Cake"
};

static const int menu_times[] = { 12, 15, 5, 8, 10, 14, 18, 20, 7, 9 };

#define MENU_SIZE 10

static void log_event(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char entry[256];

    snprintf(entry, sizeof(entry), "[%02d:%02d:%02d] %s",
             t->tm_hour, t->tm_min, t->tm_sec, buf);

    pthread_mutex_lock(&log_mutex);

    int idx = (log_head + log_count) % MAX_LOG_LINES;

    strncpy(log_lines[idx], entry, 255);

    if (log_count < MAX_LOG_LINES)
        log_count++;
    else
        log_head = (log_head + 1) % MAX_LOG_LINES;

    pthread_mutex_unlock(&log_mutex);
}

static int pq_priority(const Order *o) {
    return o->priority * 1000000 + (int)(time(NULL) - o->created_at);
}

static void pq_push(PriorityQueue *pq, Order *o) {
    if (pq->size >= MAX_QUEUE) return;

    int i = pq->size++;
    pq->data[i] = o;

    while (i > 0) {
        int parent = (i - 1) / 2;

        if (pq_priority(pq->data[i]) > pq_priority(pq->data[parent])) {
            Order *tmp = pq->data[i];
            pq->data[i] = pq->data[parent];
            pq->data[parent] = tmp;
            i = parent;
        } else {
            break;
        }
    }
}

static Order *pq_pop(PriorityQueue *pq) {
    if (pq->size == 0) return NULL;

    Order *top = pq->data[0];
    pq->data[0] = pq->data[--pq->size];

    int i = 0;

    while (1) {
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        int best = i;

        if (l < pq->size &&
            pq_priority(pq->data[l]) > pq_priority(pq->data[best]))
            best = l;

        if (r < pq->size &&
            pq_priority(pq->data[r]) > pq_priority(pq->data[best]))
            best = r;

        if (best == i) break;

        Order *tmp = pq->data[i];
        pq->data[i] = pq->data[best];
        pq->data[best] = tmp;

        i = best;
    }

    return top;
}

static void *chef_thread(void *arg) {
    Chef *chef = (Chef *)arg;

    log_event("Chef %d entered the kitchen", chef->id);

    while (running) {
        struct timespec ts;

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        if (sem_timedwait(&kitchen_sem, &ts) != 0)
            continue;

        pthread_mutex_lock(&queue_mutex);

        while (queue.size == 0 && running) {
            struct timespec tw;

            clock_gettime(CLOCK_REALTIME, &tw);
            tw.tv_sec += 1;

            pthread_cond_timedwait(
                &queue_not_empty,
                &queue_mutex,
                &tw
            );
        }

        if (!running) {
            pthread_mutex_unlock(&queue_mutex);
            sem_post(&kitchen_sem);
            break;
        }

        if (chef->id > active_chefs) {
            pthread_mutex_unlock(&queue_mutex);
            sem_post(&kitchen_sem);
            usleep(200000);
            continue;
        }

        Order *order = pq_pop(&queue);

        if (!order) {
            pthread_mutex_unlock(&queue_mutex);
            sem_post(&kitchen_sem);
            continue;
        }

        if (order->cancelled) {
            pthread_cond_signal(&queue_not_full);
            pthread_mutex_unlock(&queue_mutex);
            sem_post(&kitchen_sem);
            continue;
        }

        order->state = STATE_PREPARING;
        order->chef_id = chef->id;
        chef->current_order = order;

        pthread_cond_signal(&queue_not_full);

        pthread_mutex_unlock(&queue_mutex);

        log_event(
            "Chef %d starts \"%s\" (Order #%d, %s)",
            chef->id,
            order->item,
            order->id,
            order->priority ? "VIP" : "Normal"
        );

        int ticks = (int)(order->prep_time * 10 / sim_speed);

        for (int t = 0; t < ticks &&
             !order->cancelled &&
             running; t++) {
            usleep(100000);
        }

        sem_post(&kitchen_sem);

        pthread_mutex_lock(&state_mutex);

        if (!order->cancelled) {
            order->state = STATE_COMPLETED;
            order->completed_at = time(NULL);

            chef->orders_done++;

            log_event(
                "Chef %d completed \"%s\" (Order #%d)",
                chef->id,
                order->item,
                order->id
            );
        }

        chef->current_order = NULL;

        pthread_mutex_unlock(&state_mutex);
    }

    log_event("Chef %d left the kitchen", chef->id);

    return NULL;
}

static void *waiter_thread(void *arg) {
    (void)arg;

    srand((unsigned)time(NULL) ^ (unsigned)pthread_self());

    while (running && order_count < MAX_ORDERS) {
        double delay =
            (1.0 + (rand() % 30) / 10.0) / sim_speed;

        usleep((useconds_t)(delay * 1000000));

        if (!running) break;

        pthread_mutex_lock(&queue_mutex);

        while (queue.size >= MAX_QUEUE && running) {
            log_event("Waiter: queue full, waiting…");

            pthread_cond_wait(
                &queue_not_full,
                &queue_mutex
            );
        }

        if (!running) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        pthread_mutex_lock(&state_mutex);

        if (order_count >= MAX_ORDERS) {
            pthread_mutex_unlock(&state_mutex);
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        int idx = order_count;

        Order *o = &all_orders[idx];

        memset(o, 0, sizeof(Order));

        int menu_idx = rand() % MENU_SIZE;

        o->id = next_order_id++;
        o->priority = (rand() % 5 == 0) ? 1 : 0;

        strncpy(o->item, menu_items[menu_idx], 63);

        o->prep_time = menu_times[menu_idx];
        o->state = STATE_IN_QUEUE;
        o->created_at = time(NULL);
        o->chef_id = -1;

        order_count++;

        pthread_mutex_unlock(&state_mutex);

        pq_push(&queue, o);

        pthread_cond_signal(&queue_not_empty);

        pthread_mutex_unlock(&queue_mutex);

        log_event(
            "Waiter added Order #%d \"%s\" (%s)",
            o->id,
            o->item,
            o->priority ? "VIP" : "Normal"
        );
    }

    log_event("Waiter finished for the evening.");

    return NULL;
}
