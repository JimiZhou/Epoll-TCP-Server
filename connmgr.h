//
// Created by yujiezhou on 15/05/18.
//

#ifndef CONNMGR_H
#define CONNMGR_H

#define MIN_PORT    1024
#define MAX_PORT    65536
#define MAX_PENDING 10
#define CHAR_IP_ADDR_LENGTH 16        // 4 numbers of 3 digits, 3 dots and \0
#define    PROTOCOLFAMILY    AF_INET        // internet protocol suite
#define    TYPE        SOCK_STREAM    // streaming protool type
#define    PROTOCOL    IPPROTO_TCP    // TCP protocol
#define MAX_EPOLL 3
#define BUFFER_MAX_LEN  4096


#define    TCP_NO_ERROR        0
#define    TCP_SOCKET_ERROR    1  // invalid socket
#define    TCP_ADDRESS_ERROR    2  // invalid port and/or IP address
#define    TCP_SOCKOP_ERROR    3  // socket operator (socket, listen, bind, accept,...) error
#define TCP_CONNECTION_CLOSED    4  // send/receive indicate connection is closed
#define    TCP_MEMORY_ERROR    5  // mem alloc error
#define TCP_ACCEPT_ERROR 6
#define TCP_READ_ERROR 7
#define TCP_EPOLL_CREATE_ERROR 8
#define TCP_EPOLL_CTL_ADD_ERROR 9


void connmgr_listen(int port_number);

/*
 * This method holds the core functionality of your connmgr.
 * It starts listening on the given port and when when a
 * sensor node connects it writes the data to a sensor_data_recv
 * file. This file must have the same format as the sensor_data
 * file in assignment 6 and 7.
*/

void connmgr_free();
/*
 * This method should be called to clean up the connmgr, and
 * to free all used memory. After this no new connections
 * will be accepted
*/

#endif //CONNMGR_H
