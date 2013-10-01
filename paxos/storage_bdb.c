/*
	Copyright (c) 2013, University of Lugano
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
    	* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of the copyright holders nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.	
*/


#include "storage.h"
#include <db.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include "xdr.h"
#include <rpc/xdr.h>
#include <rpc/types.h>

struct storage
{
	DB* db;
	DB_ENV* env;
	DB_TXN* txn;
	int acceptor_id;
	char buffer[PAXOS_MAX_VALUE_SIZE];
	acceptor_record record;
};

static int 
bdb_init_tx_handle(struct storage* s, char* db_env_path)
{
	int result;
	DB_ENV* dbenv; 
	
	// Create environment handle
	result = db_env_create(&dbenv, 0);
	if (result != 0) {
		paxos_log_error("DB_ENV creation failed: %s", db_strerror(result));
		return -1;
	}
	
	// Durability mode
	if (!paxos_config.bdb_sync)
		result = dbenv->set_flags(dbenv, DB_TXN_WRITE_NOSYNC, 1);
	
	if (result != 0) {
		paxos_log_error("DB_ENV set_flags failed: %s", db_strerror(result));
		return -1;
	}
	
	// Redirect errors to sdout
	dbenv->set_errfile(dbenv, stdout);

	// Set the size of the memory cache
	result = dbenv->set_cachesize(dbenv, 0, paxos_config.bdb_cachesize, 1);
	if (result != 0) {
		paxos_log_error("DB_ENV set_cachesize failed: %s",
			db_strerror(result));
		return -1;
	}
	
	// Environment open flags
	int flags;
	flags =
		DB_CREATE       |  /* Create if not existing */ 
		DB_RECOVER      |  /* Run normal recovery. */
		DB_INIT_LOCK    |  /* Initialize the locking subsystem */
		DB_INIT_LOG     |  /* Initialize the logging subsystem */
		DB_INIT_TXN     |  /* Initialize the transactional subsystem. */
		DB_THREAD       |  /* Cause the environment to be free-threaded */  
		DB_REGISTER 	|
		DB_INIT_MPOOL;     /* Initialize the memory pool (in-memory cache) */

	// Open the DB environment
	result = dbenv->open(dbenv, 
		db_env_path,            /* Environment directory */
		flags,                  /* Open flags */
		0);                     /* Default file permissions */

	if (result != 0) {
		paxos_log_error("DB_ENV open failed: %s", db_strerror(result));
		return -1;
	}

	paxos_log_info("Berkeley DB storage opened successfully");
	
	s->env = dbenv;
	return 0;
}

static int
bdb_init_db(struct storage* s, char* db_path)
{
	int result;
	DB* dbp;
	
	// Create the DB file
	result = db_create(&(s->db), s->env, 0);
	if (result != 0) {
		paxos_log_error("Berkeley DB storage call to db_create failed: %s", 
			db_strerror(result));
		return -1;
	}
	
	dbp = s->db;
    
	// DB flags
	int flags = DB_CREATE;          /*Create if not existing */

	storage_tx_begin(s);

	// Open the DB file
	result = dbp->open(dbp,
		s->txn,          /* Transaction pointer */
		db_path,         /* On-disk file that holds the database. */
		NULL,            /* Optional logical database name */
		DB_BTREE,        /* Database access method */
		flags,           /* Open flags */
		0);              /* Default file permissions */

	storage_tx_commit(s);

	if (result != 0) {
		paxos_log_error("Berkeley DB storage open failed: %s",
			db_strerror(result));
		return -1;
	}
	
	return 0;
}

struct storage*
storage_open(int acceptor_id)
{
	struct storage* s;
	
	s = malloc(sizeof(struct storage));
	memset(s, 0, sizeof(struct storage));
	
	s->acceptor_id = acceptor_id;
	
	// Create path to db file in db dir
	char* db_env_path;
	asprintf(&db_env_path, "%s_%d", paxos_config.bdb_env_path, acceptor_id);
	char* db_filename = paxos_config.bdb_db_filename;
	
	struct stat sb;
	// Check if the environment dir and db file exists
	int dir_exists = (stat(db_env_path, &sb) == 0);

	// Create the directory if it does not exist
	if (!dir_exists && (mkdir(db_env_path, S_IRWXU) != 0)) {
		paxos_log_error("Failed to create env dir %s: %s",
			db_env_path, strerror(errno));
		return NULL;
	} 
	// Delete and recreate an empty dir if not recovering
	if (paxos_config.bdb_trash_files && dir_exists) {
		char rm_command[600];
		sprintf(rm_command, "rm -r %s", db_env_path);
		
		if ((system(rm_command) != 0) || 
			(mkdir(db_env_path, S_IRWXU) != 0)) {
			paxos_log_error("Failed to recreate empty env dir %s: %s",
				db_env_path, strerror(errno));
		}
	}
	
	char * db_file = db_filename;
	int ret = bdb_init_tx_handle(s, db_env_path);
	
	if (ret != 0) {
		paxos_log_error("Failed to open DB handle");
	}
	
	if (bdb_init_db(s, db_file) != 0) {
		paxos_log_error("Failed to open DB file");
		return NULL;
	}
	
	free(db_env_path);
	
	return s;
}

int
storage_close(struct storage* s)
{	
	int result = 0;
	DB* dbp = s->db;
	DB_ENV* dbenv = s->env;
	
	if (dbp->close(dbp, 0) != 0) {
		paxos_log_error("DB_ENV close failed");
		result = -1;
	}
	
	if (dbenv->close(dbenv, 0) != 0) {
		paxos_log_error("DB close failed");
		result = -1;
	}
	 
	free(s);
	paxos_log_info("Berkeley DB storage closed successfully");
	return result;
}

void
storage_tx_begin(struct storage* s)
{
	int result;
	result = s->env->txn_begin(s->env, NULL, &s->txn, 0);
	assert(result == 0);	
}

void
storage_tx_commit(struct storage* s)
{
	int result;
	// Since it's either read only or write only
	// and there is no concurrency, should always commit!
	result = s->txn->commit(s->txn, 0);
	assert(result == 0);
}

acceptor_record* 
storage_get_record(struct storage* s, iid_t iid)
{
	int flags, result;
	DBT dbkey, dbdata;
	DB* dbp = s->db;
	DB_TXN* txn = s->txn;

	memset(&dbkey, 0, sizeof(DBT));
	memset(&dbdata, 0, sizeof(DBT));

	// Key is iid
	dbkey.data = &iid;
	dbkey.size = sizeof(iid_t);
    
	// Data is copied to our buffer
	dbdata.data = s->buffer;
	dbdata.ulen = PAXOS_MAX_VALUE_SIZE;
	dbdata.flags = DB_DBT_USERMEM;

	flags = 0;
	result = dbp->get(dbp, txn, &dbkey, &dbdata, flags);
    
	if (result == DB_NOTFOUND || result == DB_KEYEMPTY) {
		paxos_log_debug("The record for iid: %d does not exist", iid);
		return NULL;
	} else if (result != 0) {
		paxos_log_error("Error while reading record with iid %u : %s",
			iid, db_strerror(result));
		return NULL;
	}
	
	XDR xdr;
	xdrmem_create(&xdr, s->buffer, PAXOS_MAX_VALUE_SIZE, XDR_DECODE);
	memset(&s->record, 0, sizeof(acceptor_record));
	if (!xdr_paxos_accepted(&xdr, &s->record)) {
		paxos_log_error("Error while decoding record for instance %d", iid);
		return NULL;
	}
	
	xdr_destroy(&xdr);
	assert(iid == s->record.iid);
	return &s->record;
}

static int
store_record(struct storage* s, acceptor_record* rec)
{
	XDR xdr;
	DBT dbkey, dbdata;
	
	xdrmem_create(&xdr, s->buffer, PAXOS_MAX_VALUE_SIZE, XDR_ENCODE);
	if (!xdr_paxos_accepted(&xdr, rec)) {
		paxos_log_error("Error while encoding record for instance %d", 
			rec->iid);
		return -1;
	}
	
	memset(&dbkey, 0, sizeof(DBT));
	memset(&dbdata, 0, sizeof(DBT));
	
	// Key is the iid
	dbkey.data = &rec->iid;
	dbkey.size = sizeof(iid_t);
        
	// Data is the encoded buffer
	dbdata.data = &s->buffer;
	dbdata.size = xdr_getpos(&xdr);
	
	// Store permanently
	int rv = s->db->put(s->db, s->txn, &dbkey, &dbdata, 0);
	if (rv != 0) {
		paxos_log_error("BDB->put() failed! %s", db_strerror(rv));
		return rv;
	}

	xdr_destroy(&xdr);
	return 0;
}

acceptor_record*
storage_save_accept(struct storage* s, paxos_accept* ar)
{
	s->record = (acceptor_record) {
		ar->iid,
		ar->ballot,
		ar->ballot,
		0,
		{ ar->value.value_len, ar->value.value_val }
	};
	
	if (store_record(s, &s->record) != 0)
		return NULL;
    
	return &s->record;
}

acceptor_record*
storage_save_prepare(struct storage* s, paxos_prepare* pr, acceptor_record* rec)
{
	if (rec == NULL) { // Record does not exist yet
		s->record = (acceptor_record) {
			pr->iid,
			pr->ballot,
			0,
			0,
			{ 0, NULL }
		};
	} else {
		s->record.ballot = pr->ballot;
	}	
	
	if (store_record(s, &s->record) != 0)
		return NULL;
    
	return &s->record;
}

acceptor_record*
storage_save_final_value(struct storage* s, char* value, size_t size, 
	iid_t iid, ballot_t b)
{
	// int flags, result;
	// DBT dbkey, dbdata;
	// DB* dbp = s->db;
	// DB_TXN* txn = s->txn;
	// acceptor_record* record_buffer = (acceptor_record*)s->record_buf;
	// 
	// //Store as acceptor_record (== accept_ack)
	// record_buffer->iid = iid;
	// record_buffer->ballot = b;
	// record_buffer->value_ballot = b;
	// record_buffer->is_final = 1;
	// record_buffer->value->size = size;
	// memcpy(record_buffer->value, value, size);
	// 
	// memset(&dbkey, 0, sizeof(DBT));
	// memset(&dbdata, 0, sizeof(DBT));
	// 
	// //Key is iid
	// dbkey.data = &iid;
	// dbkey.size = sizeof(iid_t);
	// 
	// //Data is our buffer
	// dbdata.data = record_buffer;
	// dbdata.size = sizeof(record_buffer);
	// 
	// //Store permanently
	// flags = 0;
	// 	result = dbp->put(dbp, 
	// 	txn, 
	// 	&dbkey, 
	// 	&dbdata, 
	// 	0);
	// 
	// assert(result == 0);    
	// return record_buffer;
	
	return NULL;
}
