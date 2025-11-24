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
#include <fcntl.h>

#define GRID_SIZE 10
#define MAX_STRING_LEN 64
#define MSG_SIZE 512

// Timestamped printf that prefixes each line with HH:MM:SS.mmm
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

// Message types
typedef enum {
    MSG_READ,
    MSG_WRITE,
    MSG_READ_RESPONSE,
    MSG_WRITE_RESPONSE,
    MSG_PRINT_DOC_READ,
    MSG_SHUTDOWN
} MessageType;

// Message structure
typedef struct {
    MessageType type;
    int client_id;
    int line;
    int word_pos;
    char word[MAX_STRING_LEN];
    int duration_ms;
    int success;
} Message;

mqd_t server_mq;
mqd_t client_mq;
int client_id_global;
volatile int shutdown_flag = 0;
pthread_mutex_t mq_mutex = PTHREAD_MUTEX_INITIALIZER;

void msleep(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void print_doc(const char* doc) {
    char filename[64];
    snprintf(filename, sizeof(filename), "output_client%d.txt", client_id_global);

    FILE* fp = fopen(filename, "w");
    if (fp) {
        fprintf(fp, "%s", doc);
        fclose(fp);
    }
}

void fetch_and_print_document() {
    char doc_buf[GRID_SIZE * GRID_SIZE * (MAX_STRING_LEN + 2)] = {0};
    char document_cache[GRID_SIZE][GRID_SIZE][MAX_STRING_LEN];
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            memset(document_cache[i][j], 0, MAX_STRING_LEN);
        }
    }
    
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            if (shutdown_flag) return;
            
            Message req;
            req.type = MSG_PRINT_DOC_READ;
            req.client_id = client_id_global;
            req.line = i;
            req.word_pos = j;
            
            pthread_mutex_lock(&mq_mutex);
            
            struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 2;
            
            if (mq_timedsend(server_mq, (char*)&req, sizeof(Message), 0, &timeout) < 0) {
                pthread_mutex_unlock(&mq_mutex);
                return;
            }
            
            Message resp;
            if (mq_timedreceive(client_mq, (char*)&resp, sizeof(Message), NULL, &timeout) < 0) {
                pthread_mutex_unlock(&mq_mutex);
                return;
            }
            pthread_mutex_unlock(&mq_mutex);
            
            strcpy(document_cache[i][j], resp.word);
        }
    }
    
    for (int i = 0; i < GRID_SIZE; i++) {
        int has_content = 0;
        char line_buf[GRID_SIZE * (MAX_STRING_LEN + 1)] = {0};
        
        for (int j = 0; j < GRID_SIZE; j++) {
            if (strlen(document_cache[i][j]) > 0) {
                if (has_content) {
                    strcat(line_buf, " ");
                }
                strcat(line_buf, document_cache[i][j]);
                has_content = 1;
            }
        }
        
        if (has_content) {
            strcat(doc_buf, line_buf);
            strcat(doc_buf, "\n");
        }
    }
    
    print_doc(doc_buf);
}

void* print_doc_thread(void* arg) {
    while (!shutdown_flag) {
        msleep(2000);
        if (!shutdown_flag) {
            fetch_and_print_document();
        }
    }
    return NULL;
}

void send_read(int line, int word_pos) {
    Message req;
    req.type = MSG_READ;
    req.client_id = client_id_global;
    req.line = line;
    req.word_pos = word_pos;
    
    printf("Client %d: Requesting READ lock for (%d,%d)\n", client_id_global, line, word_pos);
    
    pthread_mutex_lock(&mq_mutex);
    
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 5;
    
    mq_timedsend(server_mq, (char*)&req, sizeof(Message), 0, &timeout);
    
    Message resp;
    mq_timedreceive(client_mq, (char*)&resp, sizeof(Message), NULL, &timeout);
    pthread_mutex_unlock(&mq_mutex);
    
    if (resp.success) {
        printf("Client %d: READ(%d,%d) SUCCESS - Value: '%s'\n", 
               client_id_global, line, word_pos, resp.word);
    } else {
        printf("Client %d: READ(%d,%d) DROPPED\n", 
               client_id_global, line, word_pos);
    }
}

void send_write(int line, int word_pos, const char* word, int duration_ms) {
    Message req;
    req.type = MSG_WRITE;
    req.client_id = client_id_global;
    req.line = line;
    req.word_pos = word_pos;
    strcpy(req.word, word);
    req.duration_ms = duration_ms;
    
    printf("Client %d: Requesting WRITE lock for (%d,%d)\n", 
           client_id_global, line, word_pos);
    
    pthread_mutex_lock(&mq_mutex);
    
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 5;
    
    mq_timedsend(server_mq, (char*)&req, sizeof(Message), 0, &timeout);
    
    Message resp;
    mq_timedreceive(client_mq, (char*)&resp, sizeof(Message), NULL, &timeout);
    pthread_mutex_unlock(&mq_mutex);
    
    if (resp.success) {
        printf("Client %d: WRITE(%d,%d) = '%s', sleeping for %dms\n", 
               client_id_global, line, word_pos, word, duration_ms);
        msleep(duration_ms);
        printf("Client %d: WRITE(%d,%d) COMPLETED\n", 
               client_id_global, line, word_pos);
    } else {
        printf("Client %d: WRITE(%d,%d) DROPPED\n", 
               client_id_global, line, word_pos);
    }
}

void process_commands() {
    FILE* fp = fopen("input.txt", "r");
    if (!fp) {
        printf("Client %d: Cannot open input.txt\n", client_id_global);
        return;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (shutdown_flag) break;
        
        int cid;
        char cmd[32];
        
        if (sscanf(line, "C%d %s", &cid, cmd) < 2) {
            continue;
        }
        
        if (cid != client_id_global) {
            continue;
        }
        
        if (strcmp(cmd, "READ") == 0) {
            int l, w;
            if (sscanf(line, "C%d READ %d %d", &cid, &l, &w) == 3) {
                send_read(l, w);
            }
        } else if (strcmp(cmd, "WRITE") == 0) {
            int l, w, dur;
            char word[MAX_STRING_LEN];
            if (sscanf(line, "C%d WRITE %d %d %s %d", &cid, &l, &w, word, &dur) == 5) {
                send_write(l, w, word, dur);
            }
        } else if (strcmp(cmd, "SLEEP") == 0) {
            int dur;
            if (sscanf(line, "C%d SLEEP %d", &cid, &dur) == 2) {
                printf("Client %d: Sleeping for %dms\n", client_id_global, dur);
                msleep(dur);
            }
        }
    }
    
    fclose(fp);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <client_id>\n", argv[0]);
        return 1;
    }

    int client_id = atoi(argv[1]);
    client_id_global = client_id;
    
    char qname[64];
    snprintf(qname, sizeof(qname), "/osdoc_client%d", client_id);
    
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(Message);
    attr.mq_curmsgs = 0;
    
    mq_unlink(qname);
    client_mq = mq_open(qname, O_CREAT | O_RDONLY, 0644, &attr);
    if (client_mq == (mqd_t)-1) {
        perror("mq_open client");
        return 1;
    }
    
    msleep(200);
    
    server_mq = (mqd_t)-1;
    for (int i = 0; i < 50; i++) {
        server_mq = mq_open("/osdoc_server", O_WRONLY);
        if (server_mq != (mqd_t)-1) break;
        msleep(100);
    }
    
    if (server_mq == (mqd_t)-1) {
        perror("mq_open server");
        mq_close(client_mq);
        mq_unlink(qname);
        return 1;
    }
    
    printf("Client %d: Starting\n", client_id);
    
    pthread_t print_thread;
    pthread_create(&print_thread, NULL, print_doc_thread, NULL);
    
    process_commands();
    
    // Wait for all operations to complete and print_doc to stabilize
    msleep(1000);
    
    // Do one final document fetch to ensure we have the complete state
    if (!shutdown_flag) {
        fetch_and_print_document();
    }
    
    shutdown_flag = 1;
    pthread_join(print_thread, NULL);
    
    if (client_id == 0) {
        msleep(1000);
        Message shutdown_msg;
        shutdown_msg.type = MSG_SHUTDOWN;
        shutdown_msg.client_id = client_id;
        mq_send(server_mq, (char*)&shutdown_msg, sizeof(Message), 0);
    }
    
    mq_close(client_mq);
    mq_close(server_mq);
    mq_unlink(qname);
    
    printf("Client %d: Exiting\n", client_id);
    return 0;
}