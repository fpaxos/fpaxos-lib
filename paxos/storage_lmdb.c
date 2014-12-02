#include "storage.h"
#include <lmdb.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>

struct storage
{
	MDB_env* env;
	MDB_txn* txn;
	MDB_dbi dbi;
	int acceptor_id;
};

static int
lmdb_compare_iid(const MDB_val* lhs, const MDB_val* rhs)
{
	iid_t lid;
	iid_t rid;
	assert(lhs->mv_size == sizeof(iid_t));
	assert(rhs->mv_size == sizeof(iid_t));
	lid = *((iid_t* ) lhs->mv_data);
	rid = *((iid_t* ) rhs->mv_data);
	return (lid == rid) ? 0 : (lid < rid) ? -1 : 1;
}

static int
storage_init(struct storage* s, char* db_env_path)
{
	int result;
	MDB_env* env = NULL;
	MDB_txn* txn = NULL;
	MDB_dbi dbi = 0;

	if ((result = mdb_env_create(&env)) != 0) {
	  paxos_log_error("Could not create lmdb environment. %s",
	    mdb_strerror(result));
	  goto error;
	}
	if ((result = mdb_env_open(env, db_env_path,
	  !paxos_config.lmdb_sync ? MDB_NOSYNC : 0,
	  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) != 0) {
	  paxos_log_error("Could not open lmdb environment at %s. %s",
	    db_env_path, mdb_strerror(result));
	  goto error;
	}
	if ((result = mdb_txn_begin(env, NULL, 0, &txn)) != 0) {
	  paxos_log_error("Could not start txn on lmdb environment at %s. %s",
	    db_env_path, mdb_strerror(result));
	  goto error;
	}
	if ((result = mdb_open(txn, NULL, 0, &dbi)) != 0) {
	  paxos_log_error("Could not open db on lmdb environment at %s. %s",
	    db_env_path, mdb_strerror(result));
	  goto error;
	}
	if ((result = mdb_set_compare(txn, dbi, lmdb_compare_iid)) != 0) {
	  paxos_log_error("Could setup compare function on lmdb "
	    "environment at %s. %s", db_env_path, mdb_strerror(result));
	  goto error;
	}
	if ((result = mdb_txn_commit(txn)) != 0) {
	  paxos_log_error("Could commit txn on lmdb environment at %s. %s",
	    db_env_path, mdb_strerror(result));
	  goto error;
	}

	s->env = env;
	s->dbi = dbi;

	return 0;
error:
	if (txn) {
	  mdb_txn_abort(txn);
	}
	if (dbi) {
	  mdb_close(env, dbi);
	}
	if (env) {
	  mdb_env_close(env);
	}
	return -1;
}

struct storage*
storage_open(int acceptor_id)
{
	struct storage* s = NULL;
	char* lmdb_env_path = NULL;
	struct stat sb;
	int dir_exists, result;
	size_t lmdb_env_path_length = strlen(paxos_config.lmdb_env_path) + 16;

	s = calloc(1, sizeof(struct storage));
	s->acceptor_id = acceptor_id;

	lmdb_env_path = malloc(lmdb_env_path_length);
	snprintf(lmdb_env_path, lmdb_env_path_length, "%s_%d",
	  paxos_config.lmdb_env_path, acceptor_id);

	dir_exists = (stat(lmdb_env_path, &sb) == 0);

	if (!dir_exists && (mkdir(lmdb_env_path, S_IRWXU) != 0)) {
	  paxos_log_error("Failed to create env dir %s: %s",
	    lmdb_env_path, strerror(errno));
	  goto error;
	}

	if ((result = storage_init(s, lmdb_env_path) != 0)) {
	  paxos_log_error("Failed to open DB handle");
	} else {
	  paxos_log_info("lmdb storage opened successfully");
	  goto cleanup_exit;
	}

error:
	if (s) {
	  storage_close(s);
	}

cleanup_exit:
	if (lmdb_env_path) {
	  free(lmdb_env_path);
	}

	return s;
}

int
storage_close(struct storage* s)
{
	if (s->txn) {
	  mdb_txn_abort(s->txn);
	}
	if (s->dbi) {
	  mdb_close(s->env, s->dbi);
	}
	if (s->env) {
	  mdb_env_close(s->env);
	}

	free(s);
	paxos_log_info("lmdb storage closed successfully");
	return 0;
}

void
storage_tx_begin(struct storage* s)
{
	int result;
	assert(s->txn == NULL);
	result = mdb_txn_begin(s->env, NULL, 0, &s->txn);
	assert(result == 0);
}

void
storage_tx_commit(struct storage* s)
{
	int result;
	assert(s->txn);
	result = mdb_txn_commit(s->txn);
	s->txn = NULL;
	assert(result == 0);
}

void
storage_free_record(struct storage* s, acceptor_record* r)
{
	free(r);
}

acceptor_record*
storage_get_record(struct storage* s, iid_t iid)
{
	int result;
	MDB_val key, data;
	acceptor_record* record_buffer = NULL;

	memset(&data, 0, sizeof(data));

	key.mv_data = &iid;
	key.mv_size = sizeof(iid);

	if ((result = mdb_get(s->txn, s->dbi, &key, &data)) != 0) {
	  if (result == MDB_NOTFOUND) {
	    paxos_log_debug("There is no record for iid: %d", iid);
	  } else {
	    paxos_log_error("Cloud not find record for iid: %d : %s",
	      iid, mdb_strerror(result));
	  }
	  return NULL;
	}

	record_buffer = (acceptor_record*) malloc(data.mv_size);
	assert(record_buffer != NULL);
	memcpy(record_buffer, data.mv_data, data.mv_size);
	assert(iid == record_buffer->iid);
	return record_buffer;
}

acceptor_record*
storage_save_accept(struct storage* s, accept_req* ar)
{
	int result;
	MDB_val key, data;
	acceptor_record* record_buffer;

	record_buffer = malloc(ACCEPT_RECORD_BUFF_SIZE(ar->value_size));
	assert(record_buffer != NULL);

	record_buffer->acceptor_id = s->acceptor_id;
	record_buffer->iid = ar->iid;
	record_buffer->ballot = ar->ballot;
	record_buffer->value_ballot = ar->ballot;
	record_buffer->is_final = 0;
	record_buffer->value_size = ar->value_size;
	memcpy(record_buffer->value, ar->value, ar->value_size);

	key.mv_data = &ar->iid;
	key.mv_size = sizeof(ar->iid);

	data.mv_data = record_buffer;
	data.mv_size = ACCEPT_ACK_SIZE(record_buffer);

	result = mdb_put(s->txn, s->dbi, &key, &data, 0);
	assert(result == 0);

	return record_buffer;
}

acceptor_record*
storage_save_prepare(struct storage* s, prepare_req* pr)
{
	int result;
	MDB_val key, data;

	acceptor_record* record_buffer = storage_get_record(s, pr->iid);
	if (record_buffer == NULL) {
	  record_buffer = malloc(ACCEPT_RECORD_BUFF_SIZE(0));
	  assert(record_buffer != NULL);

	  record_buffer->acceptor_id = s->acceptor_id;
	  record_buffer->iid = pr->iid;
	  record_buffer->value_ballot = 0;
	  record_buffer->is_final = 0;
	  record_buffer->value_size = 0;
	}
	record_buffer->ballot = pr->ballot;

	key.mv_data = &pr->iid;
	key.mv_size = sizeof(pr->iid);

	data.mv_data = record_buffer;
	data.mv_size = ACCEPT_RECORD_BUFF_SIZE(record_buffer->value_size);

	result = mdb_put(s->txn, s->dbi, &key, &data, 0);
	assert(result == 0);
	return record_buffer;
}

acceptor_record*
storage_save_final_value(struct storage* s, char* value, size_t size,
	iid_t iid, ballot_t b)
{
	int result;
	MDB_val key, data;
	acceptor_record* record_buffer;
	record_buffer = malloc(ACCEPT_RECORD_BUFF_SIZE(size));

	assert(record_buffer != NULL);

	record_buffer->iid = iid;
	record_buffer->ballot = b;
	record_buffer->value_ballot = b;
	record_buffer->is_final = 1;
	record_buffer->value_size = size;
	memcpy(record_buffer->value, value, size);

	key.mv_data = &iid;
	key.mv_size = sizeof(iid_t);

	data.mv_data = record_buffer;
	data.mv_size = ACCEPT_ACK_SIZE(record_buffer);

	result = mdb_put(s->txn, s->dbi, &key, &data, 0);
	assert(result == 0);

	return record_buffer;
}

iid_t
storage_get_max_iid(struct storage* s)
{
	int result;
	MDB_cursor* cursor = NULL;
	MDB_val key, data;
	iid_t max_iid = 0;

	if ((result = mdb_cursor_open(s->txn, s->dbi, &cursor)) != 0) {
	  paxos_log_error("Could not create cursor. %s",
	    mdb_strerror(result));
	  max_iid = 1;
	  goto cleanup_exit;
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	if ((result = mdb_cursor_get(cursor, &key, &data, MDB_LAST)) == 0) {
	  assert(key.mv_size = sizeof(iid_t));
	  max_iid = *(iid_t* )key.mv_data;
	}

	if (result != MDB_NOTFOUND) {
	  paxos_log_error("Could not read last entry. %s",
	    mdb_strerror(result));
	  return 0;
	}

cleanup_exit:
	if (cursor) {
	  mdb_cursor_close(cursor);
	}
	return (max_iid);
}
