#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>
#include <pthread.h>
#include <mqueue.h>
#include <signal.h>
#include <fcntl.h>

#define GRID_SIZE 10
#define MAX_STRING_LEN 64
#define MAX_CLIENTS 10

static void ts_printf(const char *fmt, ...) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_info;
    localtime_r(&tv.tv_sec, &tm_info);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm_info);

    fprintf(stdout, "%s.%03ld ", tbuf, tv.tv_usec / 1000);

    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);

    fflush(stdout);
}

#define printf(...) ts_printf(__VA_ARGS__)

typedef enum {
    MSG_READ,
    MSG_WRITE,
    MSG_READ_RESPONSE,
    MSG_WRITE_RESPONSE,
    MSG_PRINT_DOC_READ,
    MSG_SHUTDOWN
} MessageType;

typedef struct {
    MessageType type;
    int client_id;
    int line;
    int word_pos;
    char word[MAX_STRING_LEN];
    int duration_ms;
    int success;
} Message;

typedef struct {
    char word[MAX_STRING_LEN];
    int is_locked;
    int locked_by;
    pthread_mutex_t mutex;
} WordState;

WordState grid[GRID_SIZE][GRID_SIZE];
mqd_t server_mq;
volatile sig_atomic_t shutdown_flag = 0;

void msleep(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void init_grid() {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            memset(grid[i][j].word, 0, MAX_STRING_LEN);
            grid[i][j].is_locked = 0;
            grid[i][j].locked_by = -1;
            pthread_mutex_init(&grid[i][j].mutex, NULL);
        }
    }
}

void write_output() {
    FILE* fp = fopen("output.txt", "w");
    if (!fp) return;

    for (int i = 0; i < GRID_SIZE; i++) {
        int has_content = 0;
        char line_buf[GRID_SIZE * (MAX_STRING_LEN + 1)] = {0};
        
        for (int j = 0; j < GRID_SIZE; j++) {
            if (strlen(grid[i][j].word) > 0) {
                if (has_content) {
                    strcat(line_buf, " ");
                }
                strcat(line_buf, grid[i][j].word);
                has_content = 1;
            }
        }
        
        if (has_content) {
            fprintf(fp, "%s\n", line_buf);
        }
    }
    
    fclose(fp);
}

void* unlock_word_timer(void* arg) {
    Message* msg = (Message*)arg;
    msleep(msg->duration_ms);
    
    pthread_mutex_lock(&grid[msg->line][msg->word_pos].mutex);
    grid[msg->line][msg->word_pos].is_locked = 0;
    grid[msg->line][msg->word_pos].locked_by = -1;
    pthread_mutex_unlock(&grid[msg->line][msg->word_pos].mutex);
    
    printf("Server: Client %d UNLOCK(%d,%d)\n", msg->client_id, msg->line, msg->word_pos);
    
    free(msg);
    return NULL;
}

mqd_t open_client_queue(int client_id) {
    char qname[64];
    snprintf(qname, sizeof(qname), "/osdoc_client%d", client_id);
    
    mqd_t mq = (mqd_t)-1;
    for (int i = 0; i < 20; i++) {
        mq = mq_open(qname, O_WRONLY | O_NONBLOCK);
        if (mq != (mqd_t)-1) break;
        msleep(50);
    }
    
    return mq;
}

void handle_request(Message* msg) {
    mqd_t client_mq = open_client_queue(msg->client_id);
    if (client_mq == (mqd_t)-1) {
        printf("Server: Failed to open client %d queue\n", msg->client_id);
        return;
    }
    
    Message response;
    response.type = (msg->type == MSG_WRITE) ? MSG_WRITE_RESPONSE : MSG_READ_RESPONSE;
    response.client_id = msg->client_id;
    response.line = msg->line;
    response.word_pos = msg->word_pos;
    
    pthread_mutex_lock(&grid[msg->line][msg->word_pos].mutex);
    
    if (msg->type == MSG_READ || msg->type == MSG_PRINT_DOC_READ) {
        if (grid[msg->line][msg->word_pos].is_locked) {
            response.success = 0;
            if (msg->type == MSG_PRINT_DOC_READ) {
                strcpy(response.word, "???");
            } else {
                strcpy(response.word, "");
            }
            pthread_mutex_unlock(&grid[msg->line][msg->word_pos].mutex);
            
            if (msg->type == MSG_PRINT_DOC_READ) {
                printf("Server: Client %d PRINT_DOC READ(%d,%d) DROPPED\n", 
                       msg->client_id, msg->line, msg->word_pos);
            } else {
                printf("Server: Client %d READ LOCK(%d,%d) DENIED\n", 
                       msg->client_id, msg->line, msg->word_pos);
            }
        } else {
            response.success = 1;
            strcpy(response.word, grid[msg->line][msg->word_pos].word);
            pthread_mutex_unlock(&grid[msg->line][msg->word_pos].mutex);
            
            if (msg->type != MSG_PRINT_DOC_READ) {
                printf("Server: Client %d READ LOCK(%d,%d) GRANTED\n", 
                       msg->client_id, msg->line, msg->word_pos);
            }
        }
    } else if (msg->type == MSG_WRITE) {
        if (grid[msg->line][msg->word_pos].is_locked) {
            response.success = 0;
            pthread_mutex_unlock(&grid[msg->line][msg->word_pos].mutex);
            printf("Server: Client %d WRITE LOCK(%d,%d) DENIED\n", 
                   msg->client_id, msg->line, msg->word_pos);
        } else {
            grid[msg->line][msg->word_pos].is_locked = 1;
            grid[msg->line][msg->word_pos].locked_by = msg->client_id;
            strcpy(grid[msg->line][msg->word_pos].word, msg->word);
            response.success = 1;
            pthread_mutex_unlock(&grid[msg->line][msg->word_pos].mutex);
            
            printf("Server: Client %d WRITE LOCK(%d,%d) GRANTED\n", 
                   msg->client_id, msg->line, msg->word_pos);
            
            pthread_t timer_thread;
            Message* timer_msg = malloc(sizeof(Message));
            memcpy(timer_msg, msg, sizeof(Message));
            pthread_create(&timer_thread, NULL, unlock_word_timer, timer_msg);
            pthread_detach(timer_thread);
        }
    }
    
    mq_send(client_mq, (char*)&response, sizeof(Message), 0);
    mq_close(client_mq);
}

void signal_handler(int sig) {
    (void)sig;
    shutdown_flag = 1;
}

int main() {
    printf("Server: Started. Grid initialized.\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    init_grid();
    
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(Message);
    attr.mq_curmsgs = 0;
    
    mq_unlink("/osdoc_server");
    server_mq = mq_open("/osdoc_server", O_CREAT | O_RDONLY, 0644, &attr);
    if (server_mq == (mqd_t)-1) {
        perror("mq_open server");
        return 1;
    }
    
    while (!shutdown_flag) {
        Message msg;
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1;
        
        ssize_t bytes = mq_timedreceive(server_mq, (char*)&msg, sizeof(Message), NULL, &timeout);
        
        if (bytes > 0) {
            if (msg.type == MSG_SHUTDOWN) {
                shutdown_flag = 1;
            } else {
                handle_request(&msg);
            }
        }
    }
    
    write_output();
    mq_close(server_mq);
    mq_unlink("/osdoc_server");
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            pthread_mutex_destroy(&grid[i][j].mutex);
        }
    }
    
    printf("Server: Shutdown complete\n");
    return 0;
}