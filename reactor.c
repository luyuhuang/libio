/**
 * @author: luyuhuang
 * @brief:
 */

#include "reactor.h"
#include "reactor_epoll.h"
#include "comm.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>

static uint64_t _reactor_get_nextid(reactor_t r) {
    return r->next_eventid++;
}

static __thread int _pipefd[2];

static void _sighandler(int sig) {
    int save_errno = errno;
    
    int msg = sig;
    int ret = write(_pipefd[1], &msg, sizeof(int));
    assert(ret == 4);

    errno = save_errno;
}

int reactor_asyn_read(reactor_t r, struct rfile *file, int32_t mtime, read_cb callback, void *data)
{
    if (hashmap_is_in(r->file_events, L2BASIC(file->fd)))
        return -1;

    struct revent *event = (struct revent*)calloc(1, sizeof(struct revent));
    event->eventid = _reactor_get_nextid(r);
    event->r = r;
    event->fd = file->fd;
    event->type = REVENT_READ;
    event->mtime = mtime;
    event->callback = (void*)callback;
    event->data = data;
    event->delete_while_done = false;
    event->__next__ = NULL;

    hashmap_add(r->reactor_events, U2BASIC(event->eventid), P2BASIC(event));

    struct _m_file *new_file = (struct _m_file*)calloc(1, sizeof(struct _m_file));
    new_file->eventid = event->eventid;
    new_file->fd = file->fd;

    hashmap_add(r->file_events, L2BASIC(new_file->fd), P2BASIC(new_file));

    if (mtime >= 0) {
        struct _h_timer *new_timer = (struct _h_timer*)calloc(1, sizeof(struct _h_timer));
        new_timer->eventid = event->eventid;
        new_timer->absolute_mtime = get_absolute_time(mtime);
        minheap_add(r->time_heap, P2BASIC(new_timer));
    }

    int ret = repoll_add_read_file(r->epfd, file->fd, true);
    if (ret == 0)
        return REACTER_OK;
    else
        return REACTER_ERR;
}


int reactor_asyn_write(reactor_t r, struct rfile *file, void *buffer, size_t len, int32_t mtime, write_cb callback, void *data)
{
    if (hashmap_is_in(r->file_events, L2BASIC(file->fd)))
        return -1;

    struct revent *event = (struct revent*)calloc(1, sizeof(struct revent));
    event->eventid = _reactor_get_nextid(r);
    event->r = r;
    event->fd = file->fd;
    event->type = REVENT_WRITE;
    event->mtime = mtime;
    event->buffer = buffer;
    event->buffer_len = len;
    event->callback = (void*)callback;
    event->data = data;
    event->delete_while_done = false;
    event->__next__ = NULL;

    hashmap_add(r->reactor_events, U2BASIC(event->eventid), P2BASIC(event));

    struct _m_file *new_file = (struct _m_file*)calloc(1, sizeof(struct _m_file));
    new_file->eventid = event->eventid;
    new_file->fd = file->fd;

    hashmap_add(r->file_events, L2BASIC(new_file->fd), P2BASIC(new_file));

    if (mtime >= 0) {
        struct _h_timer *new_timer = (struct _h_timer*)calloc(1, sizeof(struct _h_timer));
        new_timer->eventid = event->eventid;
        new_timer->absolute_mtime = get_absolute_time(mtime);
        minheap_add(r->time_heap, P2BASIC(new_timer));
    }
    
    int ret = repoll_add_write_file(r->epfd, file->fd, true);
    if (ret == 0)
        return REACTER_OK;
    else
        return REACTER_ERR;
}

int reactor_asyn_accept(reactor_t r, struct rfile *file, int32_t mtime, accept_cb callback, void *data)
{
    if (hashmap_is_in(r->file_events, L2BASIC(file->fd)))
        return -1;

    struct revent *event = (struct revent*)calloc(1, sizeof(struct revent));
    event->eventid = _reactor_get_nextid(r);
    event->r = r;
    event->fd = file->fd;
    event->type = REVENT_ACCEPT;
    event->mtime = mtime;
    event->callback = (void*)callback;
    event->data = data;
    event->delete_while_done = false;
    event->__next__ = NULL;

    hashmap_add(r->reactor_events, U2BASIC(event->eventid), P2BASIC(event));

    struct _m_file *new_file = (struct _m_file*)calloc(1, sizeof(struct _m_file));
    new_file->eventid = event->eventid;
    new_file->fd = file->fd;

    hashmap_add(r->file_events, L2BASIC(new_file->fd), P2BASIC(new_file));

    if (mtime >= 0) {
        struct _h_timer *new_timer = (struct _h_timer*)calloc(1, sizeof(struct _h_timer));
        new_timer->eventid = event->eventid;
        new_timer->absolute_mtime = get_absolute_time(mtime);
        minheap_add(r->time_heap, P2BASIC(new_timer));
    }
    
    int ret = repoll_add_read_file(r->epfd, file->fd, true);
    if (ret == 0)
        return REACTER_OK;
    else
        return REACTER_ERR;
}

int reactor_asyn_connect(
    reactor_t r, struct rfile *file, struct sockaddr *addr, socklen_t len, int32_t mtime, connect_cb callback, void *data)
{
    set_nonblocking(file->fd);
    int ret = connect(file->fd, addr, len);
    if (ret == 0) {
        callback(file, file->fd, data);
        return REACTER_OK;
    } else if (ret < 0 && errno == EINPROGRESS) {
        struct revent *event = (struct revent*)calloc(1, sizeof(struct revent));
        event->eventid = _reactor_get_nextid(r);
        event->r = r;
        event->fd = file->fd;
        event->type = REVENT_CONNECT;
        event->mtime = mtime;
        event->callback = (void*)callback;
        event->data = data;
        event->delete_while_done = false;
        event->__next__ = NULL;

        hashmap_add(r->reactor_events, U2BASIC(event->eventid), P2BASIC(event));

        struct _m_file *new_file = (struct _m_file*)calloc(1, sizeof(struct _m_file));
        new_file->eventid = event->eventid;
        new_file->fd = file->fd;

        hashmap_add(r->file_events, L2BASIC(new_file->fd), P2BASIC(new_file));

        if (mtime >= 0) {
            struct _h_timer *new_timer = (struct _h_timer*)calloc(1, sizeof(struct _h_timer));
            new_timer->eventid = event->eventid;
            new_timer->absolute_mtime = get_absolute_time(mtime);
            minheap_add(r->time_heap, P2BASIC(new_timer));
        }

        ret = repoll_add_write_file(r->epfd, file->fd, true);
        if (ret == 0)
            return REACTER_OK;
        else
            return REACTER_ERR;
    } else {
        return REACTER_ERR;
    }
}

int reactor_add_timer(reactor_t r, struct rtimer *timer, timer_cb callback, void *data)
{
    if (hashmap_is_in(r->timer_events, L2BASIC(timer->timer_id)))
        return -1;

    struct revent *event = (struct revent*)calloc(1, sizeof(struct revent));
    event->eventid = _reactor_get_nextid(r);
    event->r = r;
    event->type = REVENT_TIMER;
    event->timer_id = timer->timer_id;
    event->mtime = timer->mtime;
    event->repeat = timer->repeat;
    event->callback = (void*)callback;
    event->data = data;
    event->delete_while_done = false;
    event->__next__ = NULL;

    hashmap_add(r->reactor_events, U2BASIC(event->eventid), P2BASIC(event));

    struct _m_timer *mtimer = (struct _m_timer*)calloc(1, sizeof(struct _m_timer));
    mtimer->eventid = event->eventid;
    mtimer->timer_id = timer->timer_id;

    hashmap_add(r->timer_events, L2BASIC(mtimer->timer_id), P2BASIC(mtimer));

    struct _h_timer *new_timer = (struct _h_timer*)calloc(1, sizeof(struct _h_timer));
    new_timer->eventid = event->eventid;
    new_timer->absolute_mtime = get_absolute_time(timer->mtime);

    return minheap_add(r->time_heap, P2BASIC(new_timer));
}

int reactor_del_timer(reactor_t r, int timer_id)
{
    if (!hashmap_is_in(r->timer_events, L2BASIC(timer_id)))
        return -1;

    struct _m_timer *timer = BASIC2P(hashmap_del(r->timer_events, L2BASIC(timer_id)), struct _m_timer*);
    struct revent *event = BASIC2P(hashmap_del(r->reactor_events, P2BASIC(timer->eventid)), struct revent*);
    struct _h_timer htimer;
    htimer.eventid = event->eventid;
    struct _h_timer *ptimer = BASIC2P(minheap_del(r->time_heap, P2BASIC(&htimer)), struct _h_timer*);
    free(ptimer);
    free(event);
    free(timer);
    return REACTER_OK;
}

int reactor_add_signal(reactor_t r, struct rsignal *signal, signal_cb callback, void *data)
{
    if (hashmap_is_in(r->signal_events, L2BASIC(signal->sig)))
        return -1;

    struct revent *event = (struct revent*)calloc(1, sizeof(struct revent));

    event->eventid = _reactor_get_nextid(r);
    event->r = r;
    event->sig = signal->sig;
    event->type = REVENT_SIGNAL;
    event->callback = (void*)callback;
    event->data = data;

    hashmap_add(r->reactor_events, U2BASIC(event->eventid), P2BASIC(event));
    
    struct _m_signal *new_signal = (struct _m_signal*)calloc(1, sizeof(struct _m_signal));
    new_signal->eventid = event->eventid;
    new_signal->sig = signal->sig;

    hashmap_add(r->signal_events, L2BASIC(new_signal->sig), P2BASIC(new_signal));

    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    sa.sa_handler = _sighandler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    int ret = sigaction(signal->sig, &sa, NULL);
    if (ret == 0)
        return REACTER_OK;
    else
        return REACTER_ERR;
}

int reactor_del_signal(reactor_t r, int sig)
{
    if (!hashmap_is_in(r->signal_events, L2BASIC(sig)))
        return -1;

    struct _m_signal *signal = BASIC2P(hashmap_del(r->signal_events, L2BASIC(sig)), struct _m_signal*);
    struct revent *event = BASIC2P(hashmap_del(r->reactor_events, U2BASIC(signal->eventid)), struct revent*);
    free(signal);
    free(event);

    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);

    int ret = sigaction(sig, &sa, NULL);
    if (ret == 0)
        return REACTER_OK;
    else
        return REACTER_ERR;
}

static int _get_signal_by_read_pipefd(int pipefd)
{
    uint8_t buffer[16];
    int ret = thorough_read(pipefd, buffer, 16);
    assert(ret == sizeof(int));
    
    int *psig = (int*)buffer;
    return *psig;
}

static struct revent *_deal_overtime_event(reactor_t r, struct _h_timer *timer)
{
    struct revent *event = BASIC2P(hashmap_del(r->reactor_events, U2BASIC(timer->eventid)), struct revent*);
    event->reason = REVENT_TIMEOUT;
    event->delete_while_done = true;
    if (event->type == REVENT_ACCEPT ||
            event->type == REVENT_READ ||
            event->type == REVENT_WRITE ||
            event->type == REVENT_CONNECT) {
        struct _m_file *file = BASIC2P(hashmap_del(r->file_events, L2BASIC(event->fd)), struct _m_file*);
        repoll_remove_file(r->epfd, event->fd);
        free(file);
    } else if (event->type == REVENT_TIMER) {
        struct _m_timer *timer = BASIC2P(hashmap_del(r->timer_events, L2BASIC(event->timer_id)), struct _m_timer*);
        free(timer);
    }
    return event;
}

static struct revent *_deal_signal_event(reactor_t r, int pipefd)
{
    int sig = _get_signal_by_read_pipefd(pipefd);
    struct _m_signal *signal = BASIC2P(hashmap_get_value(r->signal_events, L2BASIC(sig)), struct _m_signal*);
    struct revent *event = BASIC2P(hashmap_get_value(r->reactor_events, U2BASIC(signal->eventid)), struct revent*);
    event->reason = REVENT_READY;
    return event;
}

static struct revent *_deal_file_event(reactor_t r, int fd)
{
    struct _m_file *file = BASIC2P(hashmap_del(r->file_events, L2BASIC(fd)), struct _m_file*);
    struct revent *event = BASIC2P(hashmap_del(r->reactor_events, U2BASIC(file->eventid)), struct revent*);
    event->reason = REVENT_READY;
    event->delete_while_done = true;
    if (event->mtime >= 0) {
        struct _h_timer htimer;
        htimer.eventid = event->eventid;
        struct _h_timer *timer = BASIC2P(minheap_del(r->time_heap, P2BASIC(&htimer)), struct _h_timer*);
        free(timer);
    }
    repoll_remove_file(r->epfd, fd);
    free(file);
    return event;
}

int reactor_run(reactor_t r)
{
    repoll_event_t *evs = (repoll_event_t*)calloc(r->max_events, sizeof(repoll_event_t));
    struct _h_timer *timer;
    int32_t mtime;

    do {
        timer = BASIC2P(minheap_top(r->time_heap), struct _h_timer*);
        if (timer) {
            mtime = get_interval_time(timer->absolute_mtime);
            mtime = mtime > 0 ? mtime : 0;
        }
        else
            mtime = -1;

        int num_event = repoll_wait(r->epfd, evs, r->max_events, mtime);
        if (num_event < 0) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            else
                assert(num_event >= 0);
        }

        mtime = 0;
        struct revent *event;
        while (minheap_len(r->time_heap) > 0 && mtime <= 0) {
            timer = BASIC2P(minheap_top(r->time_heap), struct _h_timer*);
            mtime = get_interval_time(timer->absolute_mtime);
            if (mtime <= 0) {
                timer = BASIC2P(minheap_pop(r->time_heap), struct _h_timer*);
                event = _deal_overtime_event(r, timer);
                //list_insert_at_tail(r->activity_events, event);
                SLIST_INSERT_AT_TAIL(&r->activity_events, event);
                free(timer);
            }
        }

        for (int i = 0; i < num_event; i++) {
            if (evs[i].repoll_events & REPOLL_IN || evs[i].repoll_events & REPOLL_OUT) {
                if (evs[i].repoll_fd == _pipefd[0]) {
                    event = _deal_signal_event(r, _pipefd[0]);
                    //list_insert_at_tail(r->activity_events, event);
                    SLIST_INSERT_AT_TAIL(&r->activity_events, event);
                } else {
                    event = _deal_file_event(r, evs[i].repoll_fd);
                    //list_insert_at_tail(r->activity_events, event);
                    SLIST_INSERT_AT_TAIL(&r->activity_events, event);
                }
            } else {

            }
        }

        //list_iter_t it = list_iter_create(r->activity_events);
        //while ((event = list_iter_next(it)) != NULL) {
        event = SLIST_BEGIN(&r->activity_events);
        while (event != SLIST_END(&r->activity_events)) {
            switch (event->type) {
                case REVENT_ACCEPT:
                    revent_on_accept(event);
                    break;
                case REVENT_CONNECT:
                    revent_on_connect(event);
                    break;
                case REVENT_READ:
                    revent_on_read(event);
                    break;
                case REVENT_WRITE:
                    revent_on_write(event);
                    break;
                case REVENT_TIMER:
                    revent_on_timer(event);
                    break;
                case REVENT_SIGNAL:
                    revent_on_signal(event);
                    break;
                default:
                    fprintf(stderr, "Bad event type\n");
                    return -1;
                    break;
            }

            //event = list_del_at_head(r->activity_events);
            //struct revent *e = event;
            event = SLIST_NEXT(event);
            SLIST_ERASE_HEAD(&r->activity_events);
            //if (!hashmap_is_in(r->reactor_events, U2BASIC(e->eventid)))
            //    free(e);
        }
        //list_iter_destroy(&it);
    } while (r->loop);
    free(evs);
    return 0;
}

void reactor_stop(reactor_t r)
{
    r->loop = 0;
}

reactor_t reactor_create()
{
    return reactor_create_for_all(
        DFL_MAX_EVENTS,
       DFL_MAX_BUFFER_SIZE
    );
}

reactor_t reactor_create_for_all(
    int max_events,
    int max_buffer_size
)
{
    reactor_t reactor = (struct reactor_manager*)calloc(1, sizeof(struct reactor_manager));
    reactor->epfd = repoll_create();

    reactor->time_heap = minheap_create(_h_timer_little);
    reactor->file_events = hashmap_create(_m_int_hash, _m_int_equal);
    reactor->signal_events = hashmap_create(_m_int_hash, _m_int_equal);
    reactor->timer_events = hashmap_create(_m_int_hash, _m_int_equal);

    reactor->reactor_events = hashmap_create(_m_uint64_hash, _m_uint64_equal);
    //reactor->activity_events = list_create(_l_revent_equal);
    SLIST_INIT(&reactor->activity_events);

    reactor->loop = 1;
    reactor->next_eventid = 0;

    reactor->max_events = DFL_MAX_EVENTS;
    reactor->max_buffer_size = DFL_MAX_BUFFER_SIZE;
    
    if (pipe(_pipefd) < 0) {
        return NULL;
    }
    set_nonblocking(_pipefd[0]);
    repoll_add_read_file(reactor->epfd, _pipefd[0], false);

    return reactor;
}

void reactor_destroy(reactor_t *r)
{
    reactor_t reactor = *r;

    struct _h_timer *timer;
    while ((timer = BASIC2P(minheap_pop(reactor->time_heap), struct _h_timer*)) != NULL) {
        free(timer);
    }
    minheap_destroy(&reactor->time_heap);

    struct hashmap_pair *pair;
    hashmap_iter_t mit = hashmap_iter_create(reactor->file_events);
    while ((pair = hashmap_iter_next(mit)) != NULL) {
        free(BASIC2P(pair->value, void*));
    }
    hashmap_iter_destroy(&mit);
    mit = hashmap_iter_create(reactor->signal_events);
    while ((pair = hashmap_iter_next(mit)) != NULL) {
        free(BASIC2P(pair->value, void*));
        //free(pair->value);
    }
    hashmap_iter_destroy(&mit);
    mit = hashmap_iter_create(reactor->timer_events);
    while ((pair = hashmap_iter_next(mit)) != NULL) {
        free(BASIC2P(pair->value, void*));
        //free(pair->value);
    }
    hashmap_iter_destroy(&mit);
    mit = hashmap_iter_create(reactor->reactor_events);
    while ((pair = hashmap_iter_next(mit)) != NULL) {
        free(BASIC2P(pair->value, void*));
        //free(pair->value);
    }
    hashmap_iter_destroy(&mit);

    hashmap_destroy(&reactor->file_events);
    hashmap_destroy(&reactor->signal_events);
    hashmap_destroy(&reactor->timer_events);
    hashmap_destroy(&reactor->reactor_events);
    
    struct revent *event = SLIST_BEGIN(&reactor->activity_events);
    //while ((event = list_del_at_head(reactor->activity_events)) != NULL) {
    //    free(event);
    //}
    //list_destroy(&reactor->activity_events);
    while (event != SLIST_END(&reactor->activity_events)) {
        struct revent *e = event;
        event = SLIST_NEXT(event);
        free(e);
    }
    
    free(reactor);
    *r = NULL;
}

static reactor_t _g_reactor_instance = NULL;
static lock_t _g_instance_lock = LOCK_INITIALIZER;

reactor_t reactor_instance()
{
    if (_g_reactor_instance == NULL) {
        LOCK(&_g_instance_lock);
        _g_reactor_instance = reactor_create();
        UNLOCK(&_g_instance_lock);
    }

    return _g_reactor_instance;
}
