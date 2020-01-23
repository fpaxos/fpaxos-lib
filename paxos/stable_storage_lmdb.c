/*
 * Copyright (c) 2014, Xiaoguang Sun <sun dot xiaoguang at yoyosys dot com>
 * Copyright (c) 2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "stable_storage.h"
#include "storage_utils.h"
#include <lmdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include <stable_storage.h>
#include <paxos_types.h>

#define PROMISE_KEY_PREFIX 1;
#define ACCEPTED_KEY_PREFIX 0;
const int TRIM_ID_KEY = -1;
const int MAX_INSTANCE_KEY = -2;


struct lmdb_storage
{
	MDB_env* env;
	MDB_txn* txn;
	MDB_dbi dbi;
	int acceptor_id;
};


static int
lmdb_storage_get_max_instance(struct lmdb_storage *lmdb_storage, iid_t *retrieved_instance) {
    int result;
    MDB_val key, data;
    memset(&data, 0, sizeof(data));

    key.mv_data = (void *) &MAX_INSTANCE_KEY; // mv_data is the pointer to where the key (literal) is held
    key.mv_size = sizeof(MAX_INSTANCE_KEY);

    if ((result = mdb_get(lmdb_storage->txn, lmdb_storage->dbi, &key, &data)) != 0) {
        if (result != MDB_NOTFOUND) { // something else went wrong so freak out
            paxos_log_error("mdb_get failed: %s", mdb_strerror(result));
            return 1;//assert(result == 0); // return an error code
        } else {
            *retrieved_instance = 0; // the trim instance has not been found so return 0
        }
    } else {
        *retrieved_instance = *(iid_t *) data.mv_data; // return the found trim instance
    }
    return 0;
}

static int
lmdb_storage_put_max_instance(struct lmdb_storage *lmdb_storage, iid_t max_instance) {
    int result;

    MDB_val key, data;

    key.mv_data = (void *) &MAX_INSTANCE_KEY;
    key.mv_size = sizeof(MAX_INSTANCE_KEY);

    data.mv_data = (void *) &max_instance;
    data.mv_size = sizeof(iid_t);

    result = mdb_put(lmdb_storage->txn, lmdb_storage->dbi, &key, &data, 0);
    if (result != 0)
        paxos_log_error("%s\n", mdb_strerror(result));
    assert(result == 0);

    paxos_log_debug("last inited instance: %u", max_instance);
    return 0;
}


static void lmdb_storage_close(struct lmdb_storage *lmdb_storage);

static int
lmdb_compare_iid(const MDB_val* lhs, const MDB_val* rhs)
{
	iid_t lid, rid;
	assert(lhs->mv_size == sizeof(iid_t));
	assert(rhs->mv_size == sizeof(iid_t));
	lid = *((iid_t*) lhs->mv_data);
	rid = *((iid_t*) rhs->mv_data);
	return (lid == rid) ? 0 : (lid < rid) ? -1 : 1;
}

static int
lmdb_storage_init(struct lmdb_storage* s, char* db_env_path)
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
	if ((result = mdb_env_set_mapsize(env, paxos_config.lmdb_mapsize)) != 0) {
		paxos_log_error("Could not set lmdb map size. %s", mdb_strerror(result));
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
	if ((result = mdb_open(txn, NULL, MDB_INTEGERKEY, &dbi)) != 0) {
		paxos_log_error("Could not open db on lmdb environment at %s. %s",
		db_env_path, mdb_strerror(result));
		goto error;
	}
	if ((result = mdb_set_compare(txn, dbi, lmdb_compare_iid)) != 0) {
		paxos_log_error("Could not setup compare function on lmdb "
			"environment at %s. %s", db_env_path, mdb_strerror(result));
		goto error;
	}
	if ((result = mdb_txn_commit(txn)) != 0) {
		paxos_log_error("Could not commit txn on lmdb environment at %s. %s",
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


static int
lmdb_storage_open(struct lmdb_storage *lmdb_storage)
{
	char* lmdb_env_path = NULL;
	struct stat sb;
	int dir_exists, result;
	size_t lmdb_env_path_length = strlen(paxos_config.lmdb_env_path) + 16;

	lmdb_env_path = malloc(lmdb_env_path_length);
	snprintf(lmdb_env_path, lmdb_env_path_length, "%s_%d",
             paxos_config.lmdb_env_path, lmdb_storage->acceptor_id);

	// Trash files -- testing only
	if (paxos_config.trash_files) {
		char rm_command[600];
		sprintf(rm_command, "rm -r %s", lmdb_env_path);
		system(rm_command);
	}

	dir_exists = (stat(lmdb_env_path, &sb) == 0);

	if (!dir_exists){
	    paxos_log_info("%s does not exist");
	}

	if (!dir_exists && (mkdir(lmdb_env_path, S_IRWXU) != 0)) {
		paxos_log_error("Failed to create env dir %s: %s",
			lmdb_env_path, strerror(errno));
		result = -1;
		goto error;
	}

    if ((result = lmdb_storage_init(lmdb_storage, lmdb_env_path) != 0)) {
		paxos_log_error("Failed to open DB handle");
	} else {
		paxos_log_info("lmdb storage opened successfully");
		goto cleanup_exit;
	}

error:
    if (lmdb_storage) {
        lmdb_storage_close(lmdb_storage);
	}

cleanup_exit:
	free(lmdb_env_path);
	return result;
}

static void
lmdb_storage_close(struct lmdb_storage *lmdb_storage) {
    if (lmdb_storage->txn) {
        mdb_txn_abort(lmdb_storage->txn);
    }
    if (lmdb_storage->dbi) {
        mdb_close(lmdb_storage->env, lmdb_storage->dbi);
    }
    if (lmdb_storage->env) {
        mdb_env_close(lmdb_storage->env);
    }
    free(lmdb_storage);
	paxos_log_info("lmdb storage closed successfully");
}

static int
lmdb_storage_tx_begin(struct lmdb_storage *lmdb_storage) {
    assert(lmdb_storage->txn == NULL);
    return mdb_txn_begin(lmdb_storage->env, NULL, 0, &lmdb_storage->txn);
}

static int
lmdb_storage_tx_commit(struct lmdb_storage *lmdb_storage)
{
	int result;
    assert(lmdb_storage->txn);
    result = mdb_txn_commit(lmdb_storage->txn);
    lmdb_storage->txn = NULL;
	return result;
}

static void
lmdb_storage_tx_abort(struct lmdb_storage *lmdb_storage) {
    if (lmdb_storage->txn) {
        mdb_txn_abort(lmdb_storage->txn);
        lmdb_storage->txn = NULL;
	}
}

// 0 means not found, 1 means found
static int
lmdb_storage_get_instance_info(struct lmdb_storage *lmdb_storage, iid_t iid, paxos_accepted *out)
{
	int result;
	MDB_val key, data;

	memset(&data, 0, sizeof(data));

	key.mv_data = &iid;
	key.mv_size = sizeof(iid_t);

    if ((result = mdb_get(lmdb_storage->txn, lmdb_storage->dbi, &key, &data)) != 0) {
        if (result == MDB_NOTFOUND) {
            paxos_log_debug("There is no record for iid: %d", iid);
        } else {
            paxos_log_error("Could not find record for iid: %d : %s",
                            iid, mdb_strerror(result));
        }
        return 0;
    }

    paxos_accepted_from_buffer(data.mv_data, out);
	assert(iid == out->iid);

	return 1;
}

static int
lmdb_storage_store_instance_info(struct lmdb_storage *lmdb_storage, paxos_accepted *acc)
{
	int result;
	MDB_val key, data;
	char* buffer = paxos_accepted_to_buffer(acc);

	key.mv_data = &acc->iid;
	key.mv_size = sizeof(iid_t);

	data.mv_data = buffer;
	data.mv_size = sizeof(paxos_accepted) + acc->value.paxos_value_len;


    result = mdb_put(lmdb_storage->txn, lmdb_storage->dbi, &key, &data, 0);

    iid_t max_inited_instance;
    lmdb_storage_get_max_instance(lmdb_storage, &max_inited_instance);

    if (acc->iid > max_inited_instance) {
        lmdb_storage_put_max_instance(lmdb_storage, acc->iid);
    }

    free(buffer);
	return result;
}

static int
lmdb_storage_get_trim_instance(struct lmdb_storage *lmdb_storage, iid_t *retrieved_instance)
{
    if (retrieved_instance == NULL)
        retrieved_instance = malloc(sizeof(iid_t));
	int result;
	MDB_val key, data;
    memset(&data, 0, sizeof(data));

    key.mv_data = (void *) &TRIM_ID_KEY; // mv_data is the pointer to where the key (literal) is held
    key.mv_size = sizeof(TRIM_ID_KEY);

    if ((result = mdb_get(lmdb_storage->txn, lmdb_storage->dbi, &key, &data)) != 0) {
        if (result != MDB_NOTFOUND) { // something else when wrong so freak out
			paxos_log_error("mdb_get failed: %s", mdb_strerror(result));
            return 1;//assert(result == 0); // return an error code
		} else {
            *retrieved_instance = 0; // the trim instance has not been found so it is 0
		}
	} else {
        *retrieved_instance = *(iid_t *) data.mv_data; // return the found trim instance
    }
    return 0; // success
}

static int
lmdb_storage_put_trim_instance(struct lmdb_storage *lmdb_storage, iid_t iid)
{
	int result;

	MDB_val key, data;

    key.mv_data = (void *) &TRIM_ID_KEY;
    key.mv_size = sizeof(TRIM_ID_KEY);

	data.mv_data = &iid;
	data.mv_size = sizeof(iid_t);

    result = mdb_put(lmdb_storage->txn, lmdb_storage->dbi, &key, &data, 0);
	if (result != 0)
		paxos_log_error("%s\n", mdb_strerror(result));
	assert(result == 0);

	return 0;
}

// Tells the storage to delete everything before the iid passed
static int
lmdb_storage_trim(struct lmdb_storage *lmdb_storage, iid_t iid) {
    int result;
    iid_t min = 0;
    MDB_cursor *cursor = NULL;
    MDB_val key, data;

    if (iid == 0)
        return 0;

    lmdb_storage_put_trim_instance(lmdb_storage, iid);

    if ((result = mdb_cursor_open(lmdb_storage->txn, lmdb_storage->dbi, &cursor)) != 0) {
        paxos_log_error("Could not create cursor. %s", mdb_strerror(result));
        goto cleanup_exit;
    }

    key.mv_data = &min;
    key.mv_size = sizeof(iid_t);

    do {
        if ((result = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
            assert(key.mv_size = sizeof(iid_t));
            min = *(iid_t *) key.mv_data;
        } else {
            goto cleanup_exit;
        }

        if (min != 0 && min <= iid) {
            if (mdb_cursor_del(cursor, 0) != 0) {
                paxos_log_error("mdb_cursor_del failed. %s",
                                mdb_strerror(result));
                goto cleanup_exit;
            }
        }
    } while (min <= iid);

    cleanup_exit:
    if (cursor) {
        mdb_cursor_close(cursor);
    }
    return 0;
}

static int
lmdb_storage_get_all_untrimmed_instances_info(struct lmdb_storage *lmdb_storage,
                                              struct paxos_accepted **retreved_instances,
                                              int *number_of_instances_retrieved) {
    iid_t trim_instance = 0;
    lmdb_storage_get_trim_instance(lmdb_storage, &trim_instance);

    MDB_cursor *cursor;
    mdb_cursor_open(lmdb_storage->txn, lmdb_storage->dbi, &cursor);

    MDB_val key;
    MDB_val retrieved_data;// calloc(1, sizeof(MDB_val));
    memset(&retrieved_data, 0, sizeof(retrieved_data));

    key.mv_data = &trim_instance;
    key.mv_size = sizeof(iid_t);
    int error = mdb_cursor_get(cursor, &key, &retrieved_data, MDB_SET); // get instance after trim instance

    struct MDB_stat goose;
    mdb_env_stat(lmdb_storage->env, &goose);


    int index = 0;
    if (number_of_instances_retrieved == NULL || *number_of_instances_retrieved != 0) {
        number_of_instances_retrieved = calloc(1, sizeof(int));
        *number_of_instances_retrieved = 0;
    }

    *number_of_instances_retrieved = *number_of_instances_retrieved + 1;

    *retreved_instances = calloc(1, sizeof(struct paxos_accepted));
    if (error != MDB_NOTFOUND)
        paxos_accepted_from_buffer(retrieved_data.mv_data, &(*retreved_instances)[index]);

    paxos_log_debug("Retrieved instance %u from stable storage", (*retreved_instances)[index].iid);
    index++;

    while ((error = mdb_cursor_get(cursor, &key, &retrieved_data, MDB_NEXT)) != MDB_NOTFOUND && index <= (goose.ms_entries - 2)) {
        *number_of_instances_retrieved = *number_of_instances_retrieved + 1;
        (*retreved_instances) =     realloc((*retreved_instances), (*number_of_instances_retrieved * sizeof(struct paxos_accepted))); // adjust array size
        paxos_accepted_from_buffer(retrieved_data.mv_data, &(*retreved_instances)[index]);
        paxos_log_debug("Retrieved instance %u from stable storage", (*retreved_instances)[index].iid);
        index++;
    }

    mdb_cursor_close(cursor);
    return 0;
}


static struct lmdb_storage *
lmdb_storage_new(int acceptor_id) {
    struct lmdb_storage *s = calloc(1, sizeof(struct lmdb_storage));
    s->acceptor_id = acceptor_id;
    return s;
}


void
storage_init_lmdb(struct stable_storage *s, int acceptor_id) {
    s->handle = (void (*) (int )) lmdb_storage_new(acceptor_id);
    s->stable_storage_api.open = (int (*)(void *)) lmdb_storage_open;
    s->stable_storage_api.close = (void (*)(void *)) lmdb_storage_close;
    s->stable_storage_api.tx_begin = (int (*)(void *)) lmdb_storage_tx_begin;
    s->stable_storage_api.tx_commit = (int (*)(void *)) lmdb_storage_tx_commit;
    s->stable_storage_api.tx_abort = (void (*)(void *)) lmdb_storage_tx_abort;
    s->stable_storage_api.get_instance_info = (int (*)(void *, iid_t, paxos_accepted *)) lmdb_storage_get_instance_info;
    s->stable_storage_api.store_instance_info = (int (*)(void *, const struct paxos_accepted *)) lmdb_storage_store_instance_info;
    s->stable_storage_api.store_trim_instance = (int (*)(void *, const iid_t)) lmdb_storage_trim;
    s->stable_storage_api.get_trim_instance = (int (*)(void *, iid_t *)) lmdb_storage_get_trim_instance;
    s->stable_storage_api.get_all_untrimmed_instances_info = (int (*)(void *, paxos_accepted **,
                                                                      int *)) lmdb_storage_get_all_untrimmed_instances_info;
    s->stable_storage_api.get_max_instance = (int (*)(void *, iid_t *)) lmdb_storage_get_max_instance;
//	s->paxos_storage =


}
