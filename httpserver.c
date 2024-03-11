#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "asgn2_helper_funcs.h"
#include <stdbool.h>
#include <ctype.h>
#include <regex.h>
#include <pthread.h>
#include <semaphore.h>
#include "queue.h"

#define BUF_SIZE 1000000
#define DEFAULT_THREAD_COUNT 4;
#define OPTIONS "t:"
queue_t *q;
pthread_mutex_t mutex;
pthread_mutex_t mutex2;
pthread_mutex_t mutex3;
pthread_mutex_t wrt;
sem_t workers;
int readCount;
char** fileLog;
int logId;
int threadCount;

void fatal_error(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

typedef struct Request {
    int sock;
    char meth[20];
    char uri[100];
    char vers[20];
    char hdr[100];
    unsigned int val;
    unsigned int id;
    bool found;
    int threadId;
    bool logLock;
} Request;

void getOutput(int connn, char *b, size_t len) {
    size_t num_written = 0;

    write_n_bytes(connn, b + num_written, len);
}

void processPUT(Request *req, char *bufm, int conny, int signal) {
    pthread_mutex_lock(&mutex);
    pthread_mutex_lock(&wrt);
    //pthread_mutex_lock(&mutex);

    char response[150] = { 0 };
    if ((strcmp(req->vers, "HTTP/1.1") != 0) || signal == 1) {
        sprintf(response, "HTTP/1.1 400 Bad Request\r\nContent-Length: %d\r\n\r\nBad Request\n", req->val);
        
        if(req->found == false){
            fprintf(stderr, "GET,/%s,400,%d\n", req->uri, 0);
        }
        else{
            fprintf(stderr, "PUT,/%s,400,%d\n", req->uri, req->id);
        }

        getOutput(conny, response, strlen(response));
        pthread_mutex_unlock(&wrt);
        pthread_mutex_unlock(&mutex);
        return;
    }

    int fd = open(req->uri, O_WRONLY | O_TRUNC, 0666);
    if (fd == -1) { //FILE DONT EXIST
        fd = open(req->uri, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        if (fd == -1) {
            printf("creating error");
        }
        sprintf(response, "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n");
        if(req->found == false){
            fprintf(stderr, "GET,/%s,201,%d\n", req->uri, 0);
        }
        else{
            fprintf(stderr, "PUT,/%s,201,%d\n", req->uri, req->id);
        }
        getOutput(conny, response, strlen(response));
    } else { //YOURE NOT WRITING
        sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n");
        if(req->found == false){
            fprintf(stderr, "GET,/%s,200,%d\n", req->uri, 0);
        }
        else{
            fprintf(stderr, "PUT,/%s,200,%d\n", req->uri, req->id);
        }
        getOutput(conny, response, strlen(response));
    }
    //just need to figure out flags and make this PUT shit work

    //WRITE WHAT WAS IN THE BUFFER INITIALLY
    //fprintf(stderr, "%c", bufm[0]);
    //fprintf(stderr, "%d", req->val);
    size_t num_written = 0;
    write_n_bytes(fd, bufm + num_written, (size_t) req->val);

    //NOW CONTINUE READING AND WRITING VIA SOCKET
    
    /*
    size_t bytez = 0;
    size_t num_read = 0;
    size_t free_space = sizeof(bufm);

    for (;;) {
        bytez = read(conny, bufm + num_read, free_space);
        if (bytez < 0)
            printf("bytez error");
        if (bytez == 0){
            break;
        }
        num_read += bytez;
        free_space -= bytez;
            
        if (free_space == 0) {
                //getOutput(conny, bufm + 0, num_read);
            write_n_bytes(fd, bufm + 0, (size_t) req->val);
            free_space = sizeof(bufm);
            num_read = 0;
        } else {
            getOutput(fd, bufm + 0, num_read);
        }
            
    }
    */
    pthread_mutex_unlock(&mutex);
    pthread_mutex_unlock(&wrt);
    close(fd);
    return;
}

void processGet(Request *req, int conny, int signal) {
    if(readCount >= 1){
        for(int i = 0; i < logId; i++){
            if((req->uri != fileLog[i]) && (req->threadId != i))
                req->logLock = true;
            else{
                req->logLock = false;
                break;
            }
        }
    }
    else 
        req->logLock = false;

    if(req->logLock == false)
        pthread_mutex_lock(&mutex);
    pthread_mutex_lock(&mutex2);
    readCount++;
    if(readCount == 1){
        pthread_mutex_lock(&wrt);
    }
    pthread_mutex_unlock(&mutex2);
    //if getting diff files then allow 
        //else only allow when leaving 
    
    char response[100] = { 0 };
    if ((strcmp(req->vers, "HTTP/1.1") != 0) || signal == 1) {
        sprintf(response, "HTTP/1.1 505 Version Not Supported\r\nContent-Length: 22\r\n\r\nVersion "
                          "Not Supported\n");
        if(req->found == false){
            fprintf(stderr, "GET,/%s,505,%d\n", req->uri, 0);
        }
        else{
            fprintf(stderr, "GET,/%s,505,%d\n", req->uri, req->id);
        }
        getOutput(conny, response, strlen(response));
        pthread_mutex_lock(&mutex2);
        readCount--;
        if(readCount == 0){
            pthread_mutex_unlock(&wrt);
        }
        pthread_mutex_unlock(&mutex2);
        pthread_mutex_unlock(&mutex);
        return;
    }

    if (access(req->uri, X_OK) == 0) {
        sprintf(response, "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n");
        if(req->found == false){
            fprintf(stderr, "GET,/%s,403,%d\n", req->uri, 0);
        }
        else{
            fprintf(stderr, "GET,/%s,403,%d\n", req->uri, req->id);
        }
        getOutput(conny, response, strlen(response));
        pthread_mutex_lock(&mutex2);
        readCount--;
        if(readCount == 0){
            pthread_mutex_unlock(&wrt);
        }
        pthread_mutex_unlock(&mutex2);
        pthread_mutex_unlock(&mutex);
        return;
    }

    int fd = open(req->uri, O_RDONLY, 0666);
    if (fd == -1) {
        sprintf(response, "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n");
        if(req->found == false)
            fprintf(stderr, "GET,/%s,404,%d\n", req->uri, 0);
        else
            fprintf(stderr, "GET,/%s,404,%d\n", req->uri, req->id);
        getOutput(conny, response, strlen(response));
        close(fd);
        pthread_mutex_lock(&mutex2);
        readCount--;
        if(readCount == 0){
            pthread_mutex_unlock(&wrt);
        }
        pthread_mutex_unlock(&mutex2);
        pthread_mutex_unlock(&mutex);
        return;
    }

    char buf[BUF_SIZE] = { 0 };
    size_t free_space = sizeof(buf);
    size_t num_read = 0;

    struct stat finfo;
    fstat(fd, &finfo);
    off_t fileSize = finfo.st_size;

    //printf("\n%s%zu\n", "davis", (size_t)fileSize);

    sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", (size_t) fileSize);
    if(req->found == false){
        fprintf(stderr, "GET,/%s,200,%d\n", req->uri, 0);
    }
    else{
        fprintf(stderr, "GET,/%s,200,%d\n", req->uri, req->id);
    }
    getOutput(conny, response, strlen(response));

    size_t bytez = 0;

    for (;;) {
        bytez = read(fd, buf + num_read, free_space);
        if (bytez < 0)
            printf("bytez error");
        if (bytez == 0)
            break;
        num_read += bytez;
        free_space -= bytez;
        if (free_space == 0) {
            getOutput(conny, buf + 0, num_read);
            free_space = sizeof(buf);
            num_read = 0;
        } else {
            getOutput(conny, buf + 0, num_read);
        }
    }

    //num_read = read_until(fd, buf + num_read, free_space, "");
    //sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", num_read);
    //getOutput(conny, response, strlen(response));
    //getOutput(conny, buf + 0, num_read);

    close(fd);
    pthread_mutex_lock(&mutex2);
    readCount--;
    if(readCount == 0){
        pthread_mutex_unlock(&wrt);
    }
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex);
    return;
}

#define REQLINE "^([A-Za-z]{1,8}) /([A-Za-z0-9_.-]{2,64}) (HTTP/[0-9]\\.[0-9])\r\n\0$"
#define HEAD    "^([A-Za-z0-9_.-]{1,28}:) ([ -~]{1,128})\r\n$"
#define HEAD2   "^(([A-Za-z0-9_.-]{1,28}:) ([ -~]{1,128})\r\n)*\0$"

void processReq(Request *req, char *bufa, int con) {
    pthread_mutex_lock(&mutex3);
    regex_t preg = { 0 };
    regmatch_t p[4] = { 0 };
    char response[100];
    if ((regcomp(&preg, REQLINE, REG_EXTENDED)) != 0) {
        sprintf(response, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
        getOutput(con, response, strlen(response));
        pthread_mutex_unlock(&mutex3);
        return;
    }

    if ((regexec(&preg, bufa, (size_t) 4, p, 0)) != 0) {
        sprintf(response, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
        getOutput(con, response, strlen(response));
        pthread_mutex_unlock(&mutex3);
        return;
    } else { //METHOD URI VERSION
        //METHOD
        memcpy(req->meth, &bufa[p[1].rm_so], p[1].rm_eo);
        req->meth[p[1].rm_eo] = '\0';
        //URI
        int d = p[2].rm_eo - p[2].rm_so;
        memcpy(req->uri, &bufa[p[2].rm_so], d);
        req->uri[d] = '\0';

        //pthread_mutex_lock(&mutex3);
        fileLog[logId] = req->uri;
        req->threadId = logId;
        logId++;
        //pthread_mutex_unlock(&mutex3);
        
        //VERSION
        int k = p[3].rm_eo - p[3].rm_so;
        memcpy(req->vers, &bufa[p[3].rm_so], k);
        req->vers[k] = '\0';
    }

    bufa += p[0].rm_eo; //point after \r\n
    if (bufa[3] == '\0') {
        req->found = false;
        if (strcmp(req->meth, "GET") == 0) { //GET /..txt HTTP/#.#\r\n\r\n\0
            processGet(req, con, 0);
            pthread_mutex_unlock(&mutex3);
            return;
        } else if (strcmp(req->meth, "PUT") == 0) { //PUT /..txt HTTP/#.#\r\n\r\n\0
            processPUT(req, bufa, con, 1);
            pthread_mutex_unlock(&mutex3);
            return;
        } else {
            //sprintf(response,"HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n");
            getOutput(con, response, strlen(response));
            pthread_mutex_unlock(&mutex3);
            return;
        }
    }

    //HEADER
    regex_t preg2 = { 0 };
    regmatch_t p2[4] = { 0 };
    if ((regcomp(&preg2, HEAD2, REG_EXTENDED)) != 0)
        fatal_error("regcomp error");
    if ((regexec(&preg2, bufa, (size_t) 4, p2, 0)) != 0) {
        processGet(req, con, 0);
    }

    memcpy(req->hdr, &bufa[p2[0].rm_so], p2[0].rm_eo);
    //printf("\nstart:%d\n", p2[0].rm_so);
    //printf("\n%c\n",bufa[p2[0].rm_so+1]);
    //printf("\nend:%d\n", p2[0].rm_eo);
    req->hdr[p2[0].rm_eo] = '\0';
    req->val = atol(&bufa[p2[3].rm_so]);
    req->id = atol(&bufa[12]);
    //printf("\nPROVIDER:%d\n", req->val);

    //printf("\nhere-1:%d\n", p2[2].rm_so);
    //printf("\nhere0:%d\n", p2[2].rm_eo);
    //printf("\nhere1:%d\n", p2[3].rm_so);
    //printf("\nhere2:%d\n", p2[3].rm_eo);
    //printf("\nhere3:%d\n", p2[3].rm_so - p2[3].rm_eo);

    bufa += p2[0].rm_eo; //Now point before \r\n then msgBody
    //1. if bufa[3] == null then check if get or put to determine course of action
    if (bufa[3] == '\0') {
        req->found = true;
        if (strcmp(req->meth, "GET") == 0) { //GET /...txt HTTP/#.#\r\nContent-Length: #\r\n\r\n\0
            processGet(req, con, 0);
            pthread_mutex_unlock(&mutex3);
            return;
        } else if (strcmp(req->meth, "PUT")
                   == 0) { //PUT /...txt HTTP/#.#\r\nContent-Length: #\r\n\r\n\0
            processPUT(req, bufa, con, 1);
            pthread_mutex_unlock(&mutex3);
            return;
        } else {
            pthread_mutex_unlock(&mutex3);
            return; //FIX THIS
        }
    } else {
         req->found = true;
        if (strcmp(req->meth, "GET")
            == 0) { //GET /...txt HTTP/#.#\r\nContent-Length: #\r\n\r\nMESSAGE
            processGet(req, con, 1);
            pthread_mutex_unlock(&mutex3);
            return;
        } else if (strcmp(req->meth, "PUT")
                   == 0) { //PUT /...txt HTTP/#.#\r\nContent-Length: #\r\n\r\nMESSAGE
            bufa += 2;
            processPUT(req, bufa, con, 0);
            pthread_mutex_unlock(&mutex3);
            return;
        } else {
            pthread_mutex_unlock(&mutex3);
            return; //FIX THIS
        }
    }
}

void handle_connection(int connfd) {
    char buffer[1000000] = { '\0' };
    //memset(buffer, 0, 1000);
    ssize_t bytes = 0;
    Request req = { 0 };
    
    while ((bytes = read_until(connfd, buffer, 1000000, NULL)) > 0) {
        processReq(&req, buffer, connfd);
    }
    
    /*
    while ((bytes = read_n_bytes(connfd, buffer, 1000000)) > 0) {
        processReq(&req, buffer, connfd);
    }
    */
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        char response[70];
        //sprintf(response, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
        sprintf(response, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
        getOutput(connfd, response, strlen(response));
    }

    (void) connfd;
    close(connfd);
    return;
}

static void usage(char *exec){
    fprintf(stderr, "usage: %s [-t threads] <port>\n", exec);
}

static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

void *worker_thread(){
        //uintptr_t connfd3;
        //int conny;
        //sem_wait(&workers); and post whenever a worker is done
        //I believe the for loop wont be needed 
        for(;;){
            uintptr_t connfd3 = -1;
            //queue_pop(q, (void**)&connfd3); //then pop it to display it to make sure working 
            queue_pop(q, (void**)&connfd3);
            //conny = (int)connfd3;
            //handle_connection(conny);
            handle_connection(connfd3);
            //close(conny);
            close(connfd3);
        }
        return NULL;
}


int main(int argc, char *argv[]) {
    int opt = 0;
    int threads = DEFAULT_THREAD_COUNT;
    

    while((opt = getopt(argc, argv, OPTIONS)) != -1){
        switch(opt){
            case 't':
                //printf("\n%s%s\n", "optarg:", optarg);
                threads = strtol(optarg, NULL, 10);
                break; 
            return EXIT_FAILURE;
        }
    } 

    if(optind >= argc){
        warnx("wrong number of args");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    int port = strtouint16(argv[optind]);
    if(port == 0){
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }
    //printf("\n%s%d\n", "port:", port);

    signal(SIGPIPE, SIG_IGN);

    q = queue_new(threads); //to hold enough connfds

    threadCount = threads;

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&mutex2, NULL);
    pthread_mutex_init(&mutex3, NULL);
    pthread_mutex_init(&wrt, NULL);
    sem_init(&workers, 0, 0);

    //number of clients?
    fileLog = malloc(100 * sizeof(char *));
    logId = 0;
    for(int i = 0; i < threads; i++){
        fileLog[i] = (char *)malloc(64+1); //stringsize+1
    }

    pthread_t thread_pool[threads];
    for(int i = 0; i < threads; i++){
        pthread_create(&(thread_pool[i]), NULL, worker_thread, NULL);
    }
    
    Listener_Socket listenfd;

    if ((listener_init(&listenfd, port)) == -1)
        errx(EXIT_FAILURE, "not listening");

    //signal(SIGPIPE, SIG_IGN);

    errno = 0;
    while (1) { //just place connections in queue 
        int connfd = listener_accept(&listenfd);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        //connfd = 444;
        int64_t con = connfd;
        queue_push(q, (void *)(intptr_t)con); //should push this connfd 
        //queue_pop(q, (void**)&num); //then pop it to display it to make sure working 
        //printf("\n\n%s%lu\n\n", "is it:", num);
        //sem_post(&workers);
    }
    free(fileLog);
    free(q);
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&mutex2);
    pthread_mutex_destroy(&mutex3);
    pthread_mutex_destroy(&wrt);
    sem_destroy(&workers);
    return EXIT_SUCCESS;
    
}
