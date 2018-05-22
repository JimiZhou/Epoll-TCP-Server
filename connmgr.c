//
// Created by yujiezhou on 15/05/18.
//

// Referenced tcpsock.c from Toledo

#include <sys/socket.h>
#include <netinet/in.h>
#include <memory.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <time.h>
#include <stdio.h>
#include <zconf.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include "connmgr.h"
#include "config.h"
#include "dplist.h"


#define MAGIC_COOKIE    (long)(0xA2E1CF37D35)    // used to check if a socket is bounded
#define TIME_OUT 5
#ifdef DEBUG
#define TCP_DEBUG_PRINTF(condition,...)									\
        do {												\
           if((condition)) 										\
           {												\
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	\
            fprintf(stderr,__VA_ARGS__);								\
           }												\
        } while(0)
#else
#define TCP_DEBUG_PRINTF(...) (void)0
#endif


#define TCP_ERR_HANDLER(condition, ...)    \
    do {                        \
        if ((condition))            \
        {                    \
          TCP_DEBUG_PRINTF(1,"error condition \"" #condition "\" is true\n");    \
          __VA_ARGS__;                \
        }                    \
    } while(0)

sensor_data_t data;

int epollfd = 0;
int result = 0;
int server_sock, client_sock;
struct epoll_event event, *my_events;

typedef struct timer timer;
struct timer {
    int fd;
    time_t time;
};

dplist_t *list_time = NULL;

void timer_free(void **);

void *timer_copy(void *);

int timer_compare(void *, void *);

void remove_by_fd(int fd);

/*
 * The real definition of struct node
 */

struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};

/*
 * The real definition of struct list
 */

struct dplist {
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};

/*
 * This method holds the core functionality of your connmgr.
 * It starts listening on the given port and when when a
 * sensor node connects it writes the data to a sensor_data_recv
 * file. This file must have the same format as the sensor_data
 * file in assignment 6 and 7.
*/
void connmgr_listen(int port_number) {
    // Check if port number is valid
    TCP_ERR_HANDLER(((port_number < MIN_PORT) || (port_number > MAX_PORT)),
                    fprintf(stderr, "ERROR: %d", TCP_ADDRESS_ERROR));
    // Create server socket
    server_sock = socket(PROTOCOLFAMILY, TYPE, PROTOCOL);
    TCP_DEBUG_PRINTF(server_sock < 0, "Socket() failed with errno = %d [%s]", errno, strerror(errno));
    TCP_ERR_HANDLER(server_sock < 0, fprintf(stderr, "ERROR: %d", TCP_SOCKET_ERROR));
    // Construct the server address structure
    struct sockaddr_in server_address;
    // Set all bytes to zero
    memset(&server_address, 0, sizeof(server_address));
    // Use IPv4 address
    server_address.sin_family = PROTOCOLFAMILY;
    // IP address, use localhost loopback to test
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(port_number);
    // Bind struct with socket
    result = bind(server_sock, (struct sockaddr *) &server_address, sizeof(server_address));
    TCP_DEBUG_PRINTF(result == -1, "Bind() failed with errno = %d [%s]", errno, strerror(errno));
    TCP_ERR_HANDLER(result != 0, fprintf(stderr, "ERROR: %d", TCP_SOCKET_ERROR));
    // Start listening for clients
    result = listen(server_sock, MAX_PENDING);
    TCP_DEBUG_PRINTF(result == -1, "Listen() failed with errno = %d [%s]", errno, strerror(errno));
    TCP_ERR_HANDLER(result != 0, fprintf(stderr, "ERROR: %d", TCP_SOCKET_ERROR));
    //Create epoll
    epollfd = epoll_create(MAX_EPOLL);
    TCP_ERR_HANDLER(epollfd < 0, close(epollfd);
            close(server_sock);
            fprintf(stderr, "ERROR: %d", TCP_EPOLL_CREATE_ERROR));


    //Add server sock to epoll
    event.events = EPOLLIN;
    event.data.fd = server_sock;
    result = epoll_ctl(epollfd, EPOLL_CTL_ADD, server_sock, &event);
    TCP_ERR_HANDLER(result < 0, close(epollfd);
            close(server_sock);
            fprintf(stderr, "ERROR: %d", TCP_EPOLL_CTL_ADD_ERROR));

    my_events = malloc(sizeof(struct epoll_event) * MAX_EPOLL);
    list_time = dpl_create(&timer_copy, &timer_free, &timer_compare);

    while (1) {
        // Start epoll wait
        int active_fds = 0;
        int timeout = 0;
        if (dpl_size(list_time) == 0) {
            if (epoll_wait(epollfd, my_events, MAX_EPOLL, TIME_OUT * 1000) == 0) {
                printf("No active connection in %d seconds!\n", TIME_OUT);
                printf("Shutting down...\n");
                connmgr_free();
                close(server_sock);
                printf("Server is closed!\n");
                exit(1);
            }
        } else timeout = (*(timer *) list_time->head->element).time - time(NULL);
        active_fds = epoll_wait(epollfd, my_events, MAX_EPOLL, timeout);

        for (int i = 0; i < active_fds; i++) {
            // if the current fd equals server socket
            if (my_events[i].data.fd == server_sock) {
                // Accept this client and add to the epoll events
                int client_size = sizeof(client_sock);
                client_sock = accept(server_sock, (struct sockaddr *) &(client_sock), (socklen_t *) &client_size);
                TCP_ERR_HANDLER(client_sock < 0, fprintf(stderr, "ERROR: %d", TCP_ACCEPT_ERROR));
                // Enable edge trigger
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = client_sock;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, client_sock, &event);

                struct timer temp;
                temp.fd = client_sock;
                temp.time = time(NULL) + TIME_OUT;
                printf("Client %d added at time: %ld\n", client_sock, time(NULL));
                timer *t = malloc(sizeof(timer));
                *t = temp;
                list_time = dpl_insert_at_index(list_time, t, dpl_size(list_time) + 1, true);
                free(t);
            } else if (my_events[i].events & EPOLLIN) {
                // Create client socket
                client_sock = my_events[i].data.fd;
                // Update the last-modified timer
                for (dplist_node_t *dummy = list_time->head; dummy != NULL; dummy = dummy->next) {
                    if ((*(timer *) dummy->element).fd == client_sock) {
                        (*(timer *) dummy->element).time = time(NULL) + TIME_OUT;
                    }
                }
                // Read the ID data out of client socket
                int n = read(client_sock, (void *) &data.id, sizeof(data.id));
                // Check if read is successful
                TCP_ERR_HANDLER(n < 0, fprintf(stderr, "ERROR: %d", TCP_READ_ERROR));
                // Handle client exit
                if (n == 0) {
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, client_sock, &event);
                    close(client_sock);
                    //remove_by_fd(client_sock);
                    break;
                }
                    // Print out the ID received
                else {
                    printf("Client fd: %d\n", client_sock);
                    printf("[Sensor ID]: %"PRIu16"\n", data.id);
                }

                // Read the temperature data out of client socket
                n = read(client_sock, (void *) &data.value, sizeof(data.value));
                // Check if read is successful
                TCP_ERR_HANDLER(n < 0, fprintf(stderr, "ERROR: %d", TCP_READ_ERROR));
                // Handle client exit
                if (n == 0) {
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, client_sock, &event);
                    close(client_sock);
                    //remove_by_fd(client_sock);
                    break;
                }
                    // Print out the ID received
                else {
                    printf("[Temperature]: %g\n", data.value);
                }

                // Read the timestamp data out of client socket
                n = read(client_sock, (void *) &data.ts, sizeof(data.ts));
                // Check if read is successful
                TCP_ERR_HANDLER(n < 0, fprintf(stderr, "ERROR: %d", TCP_READ_ERROR));
                // Handle client exit
                if (n == 0) {

                    epoll_ctl(epollfd, EPOLL_CTL_DEL, client_sock, &event);
                    close(client_sock);
                    //remove_by_fd(client_sock);
                    break;
                }
                    // Print out the timestamp received
                else {
                    printf("[Timestamp]: %ld\n", data.ts);
                }
            }
            break;
        }

        dplist_node_t *dummy = NULL;

        for (dummy = list_time->head; dummy != NULL; dummy = dummy->next) {
            if ((*(timer *) dummy->element).time < time(NULL)) {
                client_sock = (*(timer *) dummy->element).fd;
                printf("Client %d timeout!\n", client_sock);
                printf("Client time: %ld\n", (long int) (*(timer *) dummy->element).time);
                printf("Disconnecting from this timeout client...\n");
                list_time = dpl_remove_at_reference(list_time, dummy, true);
                printf("Client sock: %d\n", client_sock);
                epoll_ctl(epollfd, EPOLL_CTL_DEL, client_sock, &event);
                close(client_sock);
                printf("Disconnectted!\n");
            }
            if (dpl_size(list_time) == 0)break;
        }
    }
}

/*
 * This method should be called to clean up the connmgr, and
 * to free all used memory. After this no new connections
 * will be accepted
*/
void connmgr_free() {
    free(my_events);
    dpl_free(&list_time, true);
    my_events = NULL;
}

void remove_by_fd(int fd) {
    for (dplist_node_t *dummy = list_time->head; dummy != NULL; dummy = dummy->next) {
        if ((*(timer *) dummy->element).fd == fd) {
            dpl_remove_at_reference(list_time, dummy, true);
            free(dummy);
        }
    }
}

void timer_free(void **e) {
    timer **p = (timer **) e;
    free(*p);
}

void *timer_copy(void *e) {
    timer *p = (timer *) malloc(sizeof(timer));
    assert(p != NULL);
    *p = *(timer *) e;
    return (void *) p;
}

int timer_compare(void *x, void *y) {
    timer *p, *q;
    p = (timer *) x;
    q = (timer *) y;
    int *a, *b;
    a = (int *) &(p->time);
    b = (int *) &(q->time);
    if (*a > *b) {
        return 1;
    } else if (*a < *b) {
        return -1;
    } else {
        return 0;
    }
}