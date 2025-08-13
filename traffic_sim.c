#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <windows.h>
#define SLEEP_1S() Sleep(1000)

#define NUM_VEHICLES     20
#define NUM_LIGHTS        4
#define NUM_ITERATIONS    8

#define RESET       "\033[0m"
#define ROJO        "\033[31m"
#define VERDE       "\033[32m"
#define AMARILLO    "\033[33m"

typedef enum { RED = 0, GREEN = 1, YELLOW = 2 } LightState;

typedef struct {
    int id;
    LightState state;
    int timer;         
    int greenTime;     
    int yellowTime;    
    int redTime;       
} TrafficLight;

typedef struct {
    int id;
    int position;      // posicion en cola
    int speed;         
    int light_id;      // indice del semaforo
} Vehicle;

// offsets por semáforo (cuántos ya pasaron)
typedef struct {
    int count[NUM_LIGHTS];
} Offsets;


// --------------------------- Utils --------------------------
static inline int rand_between(int a, int b) {
    return a + rand() % (b - a + 1);
}

int validate_sequence(const TrafficLight *prev, const TrafficLight *cur) {
    int ok_same = (cur->state == prev->state) && (cur->timer == prev->timer + 1);
    int ok_trans = 0;
    if (prev->state == GREEN  && cur->state == YELLOW && cur->timer == 0) ok_trans = 1;
    if (prev->state == YELLOW && cur->state == RED    && cur->timer == 0) ok_trans = 1;
    if (prev->state == RED    && cur->state == GREEN  && cur->timer == 0) ok_trans = 1;
    return ok_same || ok_trans;
}

// generador de numeros random
static inline unsigned int genRand(unsigned int *state) {
    unsigned int x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// ------------------------ Inicializacion de datos ------------------------
void init_lights(TrafficLight *lights, int n) {
    for (int i = 0; i < n; i++) {
        lights[i].id = i;

        lights[i].state = (LightState)(i % 3); // 0,1,2,0,...
        lights[i].timer = 0;
        lights[i].greenTime  = 3;  // 3 s
        lights[i].yellowTime = 1;  // 1 s
        lights[i].redTime    = 3;  // 3 s
    }
}

void init_vehicles(Vehicle *v, int n, int num_lights) {
    int cola_count[NUM_LIGHTS] = {0};

    for (int i = 0; i < n; i++) {
        v[i].id = i;
        v[i].light_id = i % num_lights;
        v[i].position = cola_count[v[i].light_id]++;
        v[i].speed = 80;
    }
}

// ---------------------- semaforos ----------------

void update_traffic_lights(TrafficLight *lights, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        TrafficLight *L = &lights[i];
        L->timer++;

        int limit = (L->state == GREEN) ? L->greenTime :
                    (L->state == YELLOW) ? L->yellowTime : L->redTime;

        if (L->timer >= limit) {
            if (L->state == GREEN)      L->state = YELLOW;
            else if (L->state == YELLOW) L->state = RED;
            else                         L->state = GREEN;
            L->timer = 0;
        }
    }
}

// ---------------------- carros ----------------
void move_cars(const TrafficLight *lights, int num_lights, Offsets *offs) {
    #pragma omp parallel for
    for (int L = 0; L < num_lights; L++) {
        if (lights[L].state == GREEN) {

            offs->count[L] += 1; 

        } else if (lights[L].state == YELLOW) {

            unsigned int seed = (unsigned)(1469598103u ^ (L * 16777619u) ^ clock());
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            if ((seed & 1u) == 0u) offs->count[L] += 1;
        }
        if (offs->count[L] < 0) offs->count[L] = 0;
    }
}

// Funcion para imprimir posicion en cola
int queue_pos(const Vehicle *v, const Offsets *offs) {
    return v->position - offs->count[v->light_id];
}



// ---------------------- Simulacion ----------------
void simulate_traffic(int num_iterations, Vehicle *vehicles, int num_vehicles, TrafficLight *lights, int num_lights) {
    Offsets offs = {0};

    for (int it = 1; it <= num_iterations; it++) {

        update_traffic_lights(lights, num_lights);

        move_cars(lights, num_lights, &offs);

        printf("\nIteracion %d\n", it);

        for (int i = 0; i < num_vehicles; i++) {
            int qp = queue_pos(&vehicles[i], &offs);
            if (qp >= 0) {
                printf("Vehiculo %2d - Posicion: %d -> Semaforo %d\n", vehicles[i].id, qp, vehicles[i].light_id);
            }
        }

        for (int j = 0; j < num_lights; j++) {
            LightState s = lights[j].state;
            if (s == GREEN) {
                printf(VERDE    "Semaforo %d - Estado: VERDE\n"     RESET, lights[j].id);
            } else if (s == YELLOW) {
                printf(AMARILLO "Semaforo %d - Estado: AMARILLO\n" RESET, lights[j].id);
            } else {
                printf(ROJO     "Semaforo %d - Estado: ROJO\n"      RESET, lights[j].id);
            }
        }

        SLEEP_1S();
    }
}


// --------- Hacerlo Dinamico  -------
void simulate_traffic_dynamic(int num_iterations, Vehicle *vehicles, int num_vehicles, TrafficLight *lights, int num_lights) {
    omp_set_dynamic(1);
    Offsets offs = {0};

    for (int it = 1; it <= num_iterations; it++) {
        int num_threads = num_vehicles / 10 + 1;
        omp_set_num_threads(num_threads);

        update_traffic_lights(lights, num_lights);

        move_cars(lights, num_lights, &offs);

        printf("\n[Dinamico] Iteracion %d (hilos=%d)\n", it, num_threads);

        for (int i = 0; i < num_vehicles; i++) {
            int qp = queue_pos(&vehicles[i], &offs);
            if (qp >= 0) {
                printf("Vehiculo %2d - Posicion: %d -> Semaforo %d\n", vehicles[i].id, qp, vehicles[i].light_id);
            }
        }

        for (int j = 0; j < num_lights; j++) {
            LightState s = lights[j].state;
            if (s == GREEN) {
                printf(VERDE    "Semaforo %d - Estado: VERDE\n"     RESET, lights[j].id);
            } else if (s == YELLOW) {
                printf(AMARILLO "Semaforo %d - Estado: AMARILLO\n" RESET, lights[j].id);
            } else {
                printf(ROJO     "Semaforo %d - Estado: ROJO\n"      RESET, lights[j].id);
            }
        }

        SLEEP_1S();
    }
}


// --------------------------------- main -----------------------------------
int main(void) {
    srand((unsigned int)time(NULL));

    TrafficLight lights[NUM_LIGHTS];
    Vehicle vehicles[NUM_VEHICLES];

    // No dinamica

    init_lights(lights, NUM_LIGHTS);
    init_vehicles(vehicles, NUM_VEHICLES, NUM_LIGHTS);

    printf("=== Simulacion no dinamica ===\n");
    simulate_traffic(NUM_ITERATIONS, vehicles, NUM_VEHICLES, lights, NUM_LIGHTS);

    // Dinamica

    init_lights(lights, NUM_LIGHTS);
    init_vehicles(vehicles, NUM_VEHICLES, NUM_LIGHTS);

    printf("\n=== Simulacion dinamica ===\n");
    simulate_traffic_dynamic(NUM_ITERATIONS, vehicles, NUM_VEHICLES, lights, NUM_LIGHTS);

    return 0;
}
