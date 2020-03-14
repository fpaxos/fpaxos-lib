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


#include "standard_stable_storage.h"
#include "storage_utils.h"
#include <lmdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include <paxos_types.h>
#include <epoch_stable_storage.h>
#include "paxos_message_conversion.h"

const int TRIM_ID_KEY = -1;
const int MAX_INSTANCE_KEY = -2;
const int EPOCH_PREFIX = INT_MIN;
const int CURRENT_EPOCH_KEY = -3;


struct lmdb_storage
{
	MDB_env* env;
	MDB_txn* txn;
	MDB_dbi dbi;
	int acceptor_id;
    mdb_size_t num_non_instance_vals;
};




static int
lmdb_storage_get_max_instance(struct lmdb_storage *lmdb_storage, iid_t *retrieved_instance) {
    assert(lmdb_storage->txn != NULL);

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
    assert(lmdb_storage->txn != NULL);

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

  //  paxos_log_debug("last inited instance: %u", max_instance);
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
    assert(lmdb_storage->txn != NULL);

    if (lmdb_storage->txn) {
        mdb_txn_abort(lmdb_storage->txn);
        lmdb_storage->txn = NULL;
	}
}

// 0 means not found, 1 means found
static int
lmdb_storage_get_instance_info(struct lmdb_storage *lmdb_storage, iid_t iid, paxos_accepted *out)
{
    assert(lmdb_storage->txn != NULL);

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

    assert(&out->promise_ballot != NULL);
	assert(iid == out->iid);

	return 1;
}

static int
lmdb_storage_store_instance_info(struct lmdb_storage *lmdb_storage, paxos_accepted *acc)
{
    assert(lmdb_storage->txn != NULL);

    int result;
	MDB_val key, data;
	char* buffer = paxos_accepted_to_buffer(acc);

	key.mv_data = &acc->iid;
	key.mv_size = sizeof(iid_t);

	data.mv_data = buffer;
	data.mv_size = sizeof(struct paxos_accepted) + acc->value.paxos_value_len;

  //  lmdb_storage_tx_commit(lmdb_storage);
 //   lmdb_storage_tx_begin(lmdb_storage);
    result = mdb_put(lmdb_storage->txn, lmdb_storage->dbi, &key, &data, 0);

   // struct paxos_accepted test_accepted;
   // lmdb_storage_get_instance_info(lmdb_storage, acc->iid, &test_accepted);

   // assert(acc->iid == test_accepted.iid);
   // assert(ballot_equal(&acc->promise_ballot, test_accepted.promise_ballot));
   // assert(ballot_equal(&acc->value_ballot, test_accepted.value_ballot));

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
    assert(lmdb_storage->txn != NULL);

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
    assert(lmdb_storage->txn != NULL);

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
    assert(lmdb_storage->txn != NULL);

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
    assert(lmdb_storage->txn != NULL);

    iid_t trim_instance = 0;
    lmdb_storage_get_trim_instance(lmdb_storage, &trim_instance);

    if (number_of_instances_retrieved == NULL) {
        number_of_instances_retrieved = calloc(1, sizeof(int));
    }

    if (retreved_instances == NULL) {
        retreved_instances = calloc(1, sizeof(struct paxos_accepted*));
    }

    *number_of_instances_retrieved = 0;

    iid_t max_inited_instance;
    lmdb_storage_get_max_instance(lmdb_storage, &max_inited_instance);

    iid_t max_possible_instances = max_inited_instance - trim_instance;

    (*retreved_instances) = calloc(max_possible_instances, sizeof(struct paxos_accepted));
    int index = 0;
    for(int current_iid = trim_instance + 1; current_iid <= max_inited_instance; current_iid++) {
        struct paxos_accepted *current_instance = calloc(1, sizeof(struct paxos_accepted));
        int found = lmdb_storage_get_instance_info(lmdb_storage, current_iid, current_instance);
        if (found) {
            (*number_of_instances_retrieved) = (*number_of_instances_retrieved) + 1;
            paxos_accepted_copy(&(*retreved_instances)[(*number_of_instances_retrieved) - 1], current_instance);
            index++;
            paxos_log_debug("Retrieved instance %u from stable storage", current_instance->iid);
        }
        free(current_instance);
    }

    // cleanup
    if (*number_of_instances_retrieved < max_inited_instance) {
        (*retreved_instances) = realloc((*retreved_instances), sizeof(struct paxos_accepted) * (*number_of_instances_retrieved));
    }
    return 0;
}


static struct lmdb_storage *
lmdb_storage_new_write_ahead_ballots(int acceptor_id) {
    struct lmdb_storage *s = calloc(1, sizeof(struct lmdb_storage));
    s->acceptor_id = acceptor_id;
    s->num_non_instance_vals = 2;
    return s;
}


static struct lmdb_storage *
lmdb_storage_new_standard(int acceptor_id) {
    struct lmdb_storage *s = calloc(1, sizeof(struct lmdb_storage));
    s->acceptor_id = acceptor_id;
    s->num_non_instance_vals = 2;
    return s;
}


static struct lmdb_storage *
lmdb_storage_new_write_ahead_epochs(int acceptor_id) {
    struct lmdb_storage *s = calloc(1, sizeof(struct lmdb_storage));
    s->acceptor_id = acceptor_id;
    s->num_non_instance_vals = 3;
    return s;
}


void initialise_standard_lmdb_function_pointers(struct standard_stable_storage *s) {
    s->api.open = (int (*)(void *)) lmdb_storage_open;
    s->api.close = (void (*)(void *)) lmdb_storage_close;
    s->api.tx_begin = (int (*)(void *)) lmdb_storage_tx_begin;
    s->api.tx_commit = (int (*)(void *)) lmdb_storage_tx_commit;
    s->api.tx_abort = (void (*)(void *)) lmdb_storage_tx_abort;
    s->api.get_instance_info = (int (*)(void *, iid_t, paxos_accepted *)) lmdb_storage_get_instance_info;
    s->api.store_instance_info = (int (*)(void *, const struct paxos_accepted *)) lmdb_storage_store_instance_info;
    s->api.store_trim_instance = (int (*)(void *, const iid_t)) lmdb_storage_put_trim_instance;
    s->api.get_trim_instance = (int (*)(void *, iid_t *)) lmdb_storage_get_trim_instance;
    s->api.get_all_untrimmed_instances_info = (int (*)(void *, struct paxos_accepted**, int *)) lmdb_storage_get_all_untrimmed_instances_info;
    s->api.get_max_instance = (int (*)(void *, iid_t *)) lmdb_storage_get_max_instance;
   // s->api.put_max_instance = (int (*)(void *, const int iid_t)) lmdb_storage_put_max_instance; not needed because should be private
    // todo trim instances upto and including
}



// Creates a new LMDB storage for usage by something else
//void
//storage_init_lmdb(struct standard_stable_storage *s, int acceptor_id) {
void storage_init_lmdb_write_ahead_ballots(struct standard_stable_storage *s, int acceptor_id){
    s->handle =  lmdb_storage_new_write_ahead_ballots(acceptor_id);
    initialise_standard_lmdb_function_pointers(s);
}


void storage_init_lmdb_standard(struct standard_stable_storage* s, int acceptor_id) {
    s->handle = lmdb_storage_new_standard(acceptor_id);
    initialise_standard_lmdb_function_pointers(s);
}


static int lmdb_get_current_epoch(struct lmdb_storage* lmdb_storage, uint32_t * retreived_epoch) {
    assert(lmdb_storage->txn != NULL);

    int result;
    MDB_val key, data;
    memset(&data, 0, sizeof(data));

    key.mv_data = (void *) &CURRENT_EPOCH_KEY; // mv_data is the pointer to where the key (literal) is held
    key.mv_size = sizeof(CURRENT_EPOCH_KEY);

    if ((result = mdb_get(lmdb_storage->txn, lmdb_storage->dbi, &key, &data)) != 0) {
        if (result != MDB_NOTFOUND) { // something else went wrong so freak out
            paxos_log_error("mdb_get failed: %s", mdb_strerror(result));
            return -100;//assert(result == 0); // return an error code
        } else {
            *retreived_epoch = 0; // the trim instance has not been found so return 0
        }
    } else {
        *retreived_epoch = *(uint32_t *) data.mv_data; // return the found trim instance
    }
    return 1;
}

static int lmdb_store_current_epoch(struct lmdb_storage* lmdb_storage, uint32_t epoch) {
    assert(lmdb_storage->txn != NULL);

    int result;

    MDB_val key, data;

    key.mv_data = (void *) &CURRENT_EPOCH_KEY;
    key.mv_size = sizeof(CURRENT_EPOCH_KEY);

    data.mv_data = &epoch;
    data.mv_size = sizeof(epoch);

    result = mdb_put(lmdb_storage->txn, lmdb_storage->dbi, &key, &data, 0);
    if (result != 0)
        paxos_log_error("%s\n", mdb_strerror(result));
    assert(result == 0);

    return 0;
}

static int lmdb_store_accept_epoch(struct lmdb_storage* lmdb_storage, iid_t instance, uint32_t epoch) {
    assert(lmdb_storage->txn != NULL);

    int result;

    MDB_val key, data;

    int accept_epoch_key = EPOCH_PREFIX + instance;

    key.mv_data = (void *) &accept_epoch_key; // mv_data is the pointer to where the key (literal) is held
    key.mv_size = sizeof(int);

    data.mv_data = &epoch;
    data.mv_size = sizeof(iid_t);

    result = mdb_put(lmdb_storage->txn, lmdb_storage->dbi, &key, &data, 0);
    if (result != 0)
        paxos_log_error("%s\n", mdb_strerror(result));
    assert(result == 0);

    return 0;
}

static int lmdb_get_accept_epoch(struct lmdb_storage* lmdb_storage, iid_t instance, uint32_t* retrieved_epoch) {
    assert(lmdb_storage->txn != NULL);
    int result;
    MDB_val key, data;
    memset(&data, 0, sizeof(data));

    int accept_epoch_key = EPOCH_PREFIX + instance;

    key.mv_data = (void *) &accept_epoch_key; // mv_data is the pointer to where the key (literal) is held
    key.mv_size = sizeof(int);

    if ((result = mdb_get(lmdb_storage->txn, lmdb_storage->dbi, &key, &data)) != 0) {
        if (result != MDB_NOTFOUND) { // something else went wrong so freak out
            paxos_log_error("mdb_get failed: %s", mdb_strerror(result));
            return 1;//assert(result == 0); // return an error code
        } else {
            *retrieved_epoch = 0; // the trim instance has not been found so return 0
        }
    } else {
        *retrieved_epoch = *(iid_t *) data.mv_data; // return the found trim instance
    }
    return 0;
}

void epoch_stable_storage_lmdb_init(struct epoch_stable_storage* storage, int acceptor_id){

    // BETTER SOLUTION NEEDED IF WANT TO DO RETURNING OF CHOSEN INSTANCES
    // SHould make a storage union - so anything could be given (union of prepares, accept, and epoch_accept)
  // need to add in method for this  storage_init_lmdb_write_ahead_epochs&storage->standard_storage, acceptor_id);
    storage->extended_handle = storage->standard_storage.handle; // same handle ;)
    storage->extended_api.store_current_epoch = (int (*) (void *, uint32_t)) lmdb_store_current_epoch;
    storage->extended_api.get_current_epoch = (int (*) (void *, uint32_t*)) lmdb_get_current_epoch;

    storage->extended_api.get_accept_epoch = (int (*) (void*, iid_t, uint32_t*)) lmdb_get_accept_epoch;
    storage->extended_api.store_accept_epoch = (int (*) (void*, iid_t, const uint32_t)) lmdb_store_accept_epoch;
}
