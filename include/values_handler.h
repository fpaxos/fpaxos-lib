#ifndef VALUES_HANDLER_H_23R78MJT
#define VALUES_HANDLER_H_23R78MJT

typedef struct vh_value_wrapper_t {
    size_t value_size;
    struct vh_value_wrapper_t * next;
    // int client_socket;
    char value[0];
} vh_value_wrapper;

int vh_init();
void vh_shutdown();
vh_value_wrapper * vh_wrap_value(char * value, size_t size);
int vh_value_compare(vh_value_wrapper * vw1, vh_value_wrapper * vw2);
void vh_enqueue_value(char * value, size_t value_size);
void vh_push_back_value(vh_value_wrapper * vw);
vh_value_wrapper * vh_get_next_pending();
int vh_pending_list_size();
void vh_notify_client(unsigned int result, vh_value_wrapper * vw);
long unsigned int vh_get_dropped_count();
#endif /* end of include guard: VALUES_HANDLER_H_23R78MJT */
