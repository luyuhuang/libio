/**
 * @author:luyuhuang
 * @brief: server
 */

#ifndef _SERVER_H_
#define _SERVER_H_

#include "reactor.h"
#include "reactor_event.h"
#include "session.h"
#include <stdint.h>

#define INIT_LISTERNER_LEN 4

typedef int (*newconnect_cb)(session_t);
typedef int (*receive_cb)(session_t, void*, size_t);
typedef int (*connected_cb)(session_t);

struct server {
    char *addr;
    struct rfile listener;
    
    /*callback functions*/
    newconnect_cb on_newconnect;
    receive_cb on_receive;
    connected_cb on_connected;

    session_manager_t session_mgr;
};

typedef struct server *server_t;

server_t server_create(const char *addr, newconnect_cb, receive_cb, connected_cb);
void server_destroy(server_t *s);

int server_listen(server_t s);

#endif //_SERVER_H_
