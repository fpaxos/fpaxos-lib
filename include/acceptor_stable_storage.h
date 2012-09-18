#ifndef ACCEPTOR_STABLE_STORAGE_H_C2XN5QX9
#define ACCEPTOR_STABLE_STORAGE_H_C2XN5QX9

int stablestorage_init(int acceptor_id);
void stablestorage_do_recovery();
int stablestorage_shutdown();

void stablestorage_tx_begin();
void stablestorage_tx_end();

acceptor_record * stablestorage_get_record(iid_t iid);

acceptor_record * stablestorage_save_accept(accept_req * ar);
acceptor_record * stablestorage_save_prepare(prepare_req * pr, acceptor_record * rec);

acceptor_record * stablestorage_save_final_value(char * value, size_t size, iid_t iid, ballot_t ballot);

#endif /* end of include guard: ACCEPTOR_STABLE_STORAGE_H_C2XN5QX9 */
