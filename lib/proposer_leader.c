struct event p1_check_event;
struct timeval p1_check_interval;

struct event p2_check_event;
struct timeval p2_check_interval;


#ifndef LEADER_EVENTS_UPDATE_INTERVAL
//Leader events display is disabled
static void empty_fun() {};
#define COUNT_EVENT(E) empty_fun()
#else
//Leader events display is enabled
struct leader_event_counters {
    long unsigned int p1_timeout;
    long unsigned int p2_timeout;
    long unsigned int p2_waits_p1;
};
struct leader_event_counters lead_counters;
struct event print_events_event;
struct timeval print_events_interval;
#define COUNT_EVENT(E) (lead_counters.E += 1)
static void clear_event_counters() {
    lead_counters.p1_timeout = 0;    
    lead_counters.p2_timeout = 0;
    lead_counters.p2_waits_p1 = 0;
}

static void 
leader_print_event_counters(int fd, short event, void *arg) {
    UNUSED_ARG(fd);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);
    printf("-----------------------------------------------\n");
    printf("current_iid:%u\n", current_iid);
    printf("Phase 1_____________________:\n");
    printf("p1_timeout:%lu\n", lead_counters.p1_timeout);
    printf("p1_info.pending_count:%u\n", p1_info.pending_count);
    printf("p1_info.ready_count:%u\n", p1_info.ready_count);
    printf("p1_info.highest_open:%u\n", p1_info.highest_open);
    printf("Phase 2_____________________:\n");    
    printf("p2_timeout:%lu\n", lead_counters.p2_timeout);
    printf("p2_waits_p1:%lu\n", lead_counters.p2_waits_p1);
    printf("p2_info.open_count:%u\n", p2_info.open_count);
    printf("p2_info.next_unused_iid:%u\n", p2_info.next_unused_iid);
    printf("Misc._______________________:\n");
    printf("dropped_count:%lu\n", vh_get_dropped_count());
    printf("-----------------------------------------------\n");
    
    //Keep printing if the current leader is still this proposer
    int ret;
    ret = event_add(&print_events_event, &print_events_interval);
    assert(ret == 0);

}
#endif



/*-------------------------------------------------------------------------*/
// Timing routines
/*-------------------------------------------------------------------------*/

static void
leader_set_expiration(p_inst_info * ii, unsigned int usec_interval) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    struct timeval * deadline = &ii->timeout;
    const unsigned int a_second = 1000000; 

    //Set seconds
    deadline->tv_sec = current_time.tv_sec + (usec_interval / a_second);
    
    //Sum microsecs
    unsigned int usec_sum;
    usec_sum = current_time.tv_usec + (usec_interval % a_second);
    
    //If sum of mircosecs exceeds 10d6, add another second
    if(usec_sum > a_second) {
        deadline->tv_sec += 1; 
    }

    //Set microseconds
    deadline->tv_usec = (usec_sum % a_second);
}

static int
leader_is_expired(struct timeval * deadline, struct timeval * time_now) {
    return (deadline->tv_sec < time_now->tv_sec ||
            (deadline->tv_sec == time_now->tv_sec &&
            deadline->tv_usec < time_now->tv_usec));
}

/*-------------------------------------------------------------------------*/
// Phase 1 routines
/*-------------------------------------------------------------------------*/

static void
leader_check_p1_pending() {
    iid_t iid_iterator;
    p_inst_info * ii;

    //Create an empty prepare batch in send buffer
    sendbuf_clear(to_acceptors, prepare_reqs, this_proposer_id);
    LOG(DBG, ("Checking pending phase 1 from %u to %u\n",
        current_iid, p1_info.highest_open));
    
    //Get current time for checking expired    
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    
    for(iid_iterator = current_iid; 
        iid_iterator <= p1_info.highest_open; iid_iterator++) {

        //Get instance from state array
        ii = GET_PRO_INSTANCE(iid_iterator);
        assert(ii->iid == iid_iterator);
        
        //Still pending -> it's expired
        if(ii->status == p1_pending && leader_is_expired(&ii->timeout, &time_now)) {
            LOG(DBG, ("Phase 1 of instance %u expired!\n", ii->iid));

            //Reset fields used for previous phase 1
            ii->promises_bitvector = 0;
            ii->promises_count = 0;
            ii->p1_value_ballot = 0;
            if(ii->p1_value != NULL) {
                PAX_FREE(ii->p1_value);
            }
            ii->p1_value = NULL;
            
            //Ballot is incremented
            ii->my_ballot = NEXT_BALLOT(ii->my_ballot);


            //Send prepare to acceptors
            sendbuf_add_prepare_req(to_acceptors, ii->iid, ii->my_ballot);        
            leader_set_expiration(ii, P1_TIMEOUT_INTERVAL);
            
            COUNT_EVENT(p1_timeout);
        }
    }    

    //Send if something is still there
    sendbuf_flush(to_acceptors);
}

//Opens instances at the "end" of the proposer state array 
//Those instances were not opened before
static void
leader_open_instances_p1() {
    int active_count = p1_info.pending_count + p1_info.ready_count;
    
    assert(active_count >= 0);
    
    if(active_count >= (PROPOSER_PREEXEC_WIN_SIZE/2)) {
        //More than half are active/pending
        // Wait before opening more
        return;
    }
    
    //Create an empty prepare batch in send buffer
    sendbuf_clear(to_acceptors, prepare_reqs, this_proposer_id);
    
    //How many new instances to open now
    unsigned int to_open = PROPOSER_PREEXEC_WIN_SIZE - active_count;
    assert(to_open >= (PROPOSER_PREEXEC_WIN_SIZE/2));

    iid_t i, curr_iid;
    p_inst_info * ii;
    for(i = 1; i <= to_open; i++) {
        //Get instance from state array
        curr_iid = i + p1_info.highest_open; 
        ii = GET_PRO_INSTANCE(curr_iid);
        assert(ii->status == empty);
        
        //Create initial record
        ii->iid = curr_iid;
        ii->status = p1_pending;
        ii->my_ballot = FIRST_BALLOT;
        //Send prepare to acceptors
        sendbuf_add_prepare_req(to_acceptors, ii->iid, ii->my_ballot);
        leader_set_expiration(ii, P1_TIMEOUT_INTERVAL);       
    }

    //Send if something is still there
    sendbuf_flush(to_acceptors);
    
    //Keep track of pending count
    p1_info.pending_count += to_open;

    //Set new higher bound for checking
    p1_info.highest_open += to_open; 
    
    LOG(DBG, ("Opened %d new instances\n", to_open));

    
}

static void leader_set_next_p1_check() {
    int ret;
    ret = event_add(&p1_check_event, &p1_check_interval);
    assert(ret == 0);
}

//This function is invoked periodically
// (periodic_repeat_interval) to retrasmit the most 
// recent accept
static void
leader_periodic_p1_check(int fd, short event, void *arg)
{
    UNUSED_ARG(fd);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);
    
    //All instances in status p1_pending are expired
    // increment ballot and re-send prepare_req
    leader_check_p1_pending();
    
    //Try to open new instances if some were used
    leader_open_instances_p1();
    
    //Set next check timeout for calling this function
    leader_set_next_p1_check();
    
}

/*-------------------------------------------------------------------------*/
// Phase 2 routines
/*-------------------------------------------------------------------------*/
static void leader_set_next_p2_check() {
    int ret;
    ret = event_add(&p2_check_event, &p2_check_interval);
    assert(ret == 0);
}

static void
leader_execute_p2(p_inst_info * ii) {
    
    if(ii->p1_value == NULL && ii->p2_value == NULL) {
        //Happens when p1 completes without value        
        //Assign a p2_value and execute
        ii->p2_value = vh_get_next_pending();
        assert(ii->p2_value != NULL);
        
    } else if (ii->p1_value != NULL) {
        //Only p1 value is present, MUST execute p2 with it
        //Save it as p2 value and execute
        ii->p2_value = ii->p1_value;
        ii->p1_value = NULL;
        ii->p1_value_ballot = 0;
        
    } else if (ii->p2_value != NULL) {
        // Only p2 valye is present
        //Do phase 2 with it
        
    } else {
        // There are both p1 and p2 value
        //Compare them
        if(vh_value_compare(ii->p1_value, ii->p2_value) == 0) {
            // Same value, just delete p1_value
            PAX_FREE(ii->p1_value);
            ii->p1_value = NULL;
            ii->p1_value_ballot = 0;
        } else {
            // Different values
            // p2_value is pushed back to pending list
            vh_push_back_value(ii->p2_value);
            // Must execute p2 with p1 value
            ii->p2_value = ii->p1_value;
            ii->p1_value = NULL;
            ii->p1_value_ballot = 0;            
        }
    }
    //Change instance status
    ii->status = p2_pending;

    //Send the accept request
    sendbuf_add_accept_req(to_acceptors, ii->iid, ii->my_ballot, ii->p2_value->value, ii->p2_value->value_size);
    
    //Set the deadline for this instance
    leader_set_expiration(ii, P2_TIMEOUT_INTERVAL);
}

// Scan trough p1_ready that have a value
// assigned or found, execute phase2
static void
leader_open_instances_p2_expired() {
    unsigned int count = 0;
    
    //Create a batch of accept requests
    sendbuf_clear(to_acceptors, accept_reqs, this_proposer_id);
        
    //Start new phase 2 for all instances found in status p1_ready
    // if they are in the range below, phase2 timed-out and 
    // we went successfully trough phase1 again
    iid_t iid;
    p_inst_info * ii;
    
    for(iid = current_iid; iid < p2_info.next_unused_iid; iid++) {
    
        ii = GET_PRO_INSTANCE(iid);
        assert(ii->iid == iid);

        //This instance is in status p1_ready but it's in the range
        // of previously opened phase2, it's now time to retry
        if(ii->status == p1_ready) {
            assert(ii->p2_value != NULL || ii->p2_value != NULL);
            leader_execute_p2(ii);
            //Count opened
            count += 1;
        }
    }
    
    //Count p1_ready that were consumed
    p1_info.ready_count -= count;
    
    //Flush last accept_req batch
    sendbuf_flush(to_acceptors);
    
    LOG(DBG, ("Opened %u old (timed-out) instances\n", count));
    
}


static void
leader_open_instances_p2_new() {
    unsigned int count = 0;
    p_inst_info * ii;

    //For better batching, opening new instances at the end
    // is preferred when more than 1 can be opened together
    unsigned int treshold = (PROPOSER_P2_CONCURRENCY/3)*2;
    if (p2_info.open_count > treshold) {
        LOG(DBG, ("Skipping Phase2 open, %u are still active (tresh:%u)\n", p2_info.open_count, treshold));
        return;
    }
    LOG(DBG, ("Could open %u p2 instances\n", 
        (PROPOSER_P2_CONCURRENCY - p2_info.open_count)));

    //Create a batch of accept requests
    sendbuf_clear(to_acceptors, accept_reqs, this_proposer_id);
    
    //Start new phase 2 while there is some value from 
    // client to send and we can open more concurrent instances
    while((count + p2_info.open_count) <= PROPOSER_P2_CONCURRENCY) {

        ii = GET_PRO_INSTANCE(p2_info.next_unused_iid);
        assert(ii->p2_value == NULL);
        
        //No value to send for next unused, stop
        if(ii->p1_value == NULL &&
            vh_pending_list_size() == 0) {
                LOG(DBG, ("No value to use for next instance\n"));
                break;
        }
        
        //Next unused is not ready, stop
        if(ii->status != p1_ready || ii->iid != p2_info.next_unused_iid) {
            LOG(DBG, ("Next instance to use for P2 (iid:%u) is not ready yet\n", p2_info.next_unused_iid));
            COUNT_EVENT(p2_waits_p1);
            break;
        }


        //Executes phase2, sending an accept request
        //Using the found value or getting the next from list
        leader_execute_p2(ii);
        
        //Count opened
        count += 1;
        //Update next to use
        p2_info.next_unused_iid += 1;
    }
    
    //Count p1_ready that were consumed
    p1_info.ready_count -= count;
    //Count newly opened
    p2_info.open_count += count;
    //Flush last accept_req batch
    sendbuf_flush(to_acceptors);
    if(count > 0) {
        LOG(DBG, ("Opened %u new instances\n", count));
    }
}

static void
leader_periodic_p2_check(int fd, short event, void *arg) {
    UNUSED_ARG(fd);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);
    
    // create a prepare batch for expired instances
    sendbuf_clear(to_acceptors, prepare_reqs, this_proposer_id);
    
    struct timeval now;
    gettimeofday(&now, NULL);
    // from current to highest open, check deadline
    // if instances in status p2_pending
    iid_t i;
    p_inst_info * ii;
    for(i = current_iid; i < p2_info.next_unused_iid; i++) {
        ii = GET_PRO_INSTANCE(i);
        
        //Not p2_pending, skip
        if(ii->status != p2_pending) {
            continue;
        }
        
        //Check if it was closed in the meanwhile 
        // (but not delivered yet)
        if(learner_is_closed(i)) {
            ii->status = p2_completed;
            p2_info.open_count -= 1;
            //The rest (i.e. answering client)
            // is done when the value is actually delivered
            LOG(VRB, ("Instance %u closed, waiting for deliver\n", i));
            continue;
        }
        
        //Not expired yet, skip
        if(!leader_is_expired(&ii->timeout, &now)) {
            continue;
        }
        
        //Expired and not closed: must restart from phase 1
        ii->status = p1_pending;
        p1_info.pending_count += 1;
        ii->my_ballot = NEXT_BALLOT(ii->my_ballot);
        //Send prepare to acceptors
        sendbuf_add_prepare_req(to_acceptors, ii->iid, ii->my_ballot);
        leader_set_expiration(ii, P1_TIMEOUT_INTERVAL);
        
        LOG(VRB, ("Instance %u restarts from phase 1\n", i));

        COUNT_EVENT(p2_timeout);

    }
    
    //Flush last message if any
    sendbuf_flush(to_acceptors);
    
    //Open new instances
    leader_open_instances_p2_new();
    
    //Set next invokation of this function
    leader_set_next_p2_check();

}

/*-------------------------------------------------------------------------*/
// Deliver callback
/*-------------------------------------------------------------------------*/
static void 
leader_deliver(char * value, size_t size, iid_t iid, ballot_t ballot, int proposer) {
    UNUSED_ARG(ballot);
    UNUSED_ARG(proposer);
    LOG(DBG, ("Instance %u delivered to Leader\n", iid));

    //Verify that the value is the one found or associated
    p_inst_info * ii = GET_PRO_INSTANCE(iid);
    //Instance not even initialized, skip
    if(ii->iid != iid) {
        return;
    }
    
    if(ii->status == p1_pending) {
        p1_info.pending_count -= 1;
    }
    
    if(p2_info.next_unused_iid == iid) {
        p2_info.next_unused_iid += 1;
    }
    
    int opened_by_me = (ii->status == p1_pending && ii->p2_value != NULL) ||
        (ii->status == p1_ready && ii->p2_value != NULL) ||
        (ii->status == p2_pending);
    if(opened_by_me) {
        p2_info.open_count -= 1;
    }

    int my_val = (ii->p2_value != NULL) &&
        (ii->p2_value->value_size == size) &&
        (memcmp(value, ii->p2_value->value, size) == 0);

    if(my_val) {
    //Our value accepted, notify client that submitted it
        vh_notify_client(0, ii->p2_value);
    } else if(ii->p2_value != NULL) {
    //Different value accepted, push back our value
        vh_push_back_value(ii->p2_value);
        ii->p2_value = NULL;
    } else {
        //We assigned no value to this instance, 
        //it comes from somebody else??
    }

    //Clear current instance
    pro_clear_instance_info(ii);
    
    //If enough instances are ready to 
    // be opened, start phase2 for them
    leader_open_instances_p2_new();
}


/*-------------------------------------------------------------------------*/
// Initialization/shutdown
/*-------------------------------------------------------------------------*/

static int
leader_init() {
    LOG(0, ("Proposer %d promoted to leader\n", this_proposer_id));
    
#ifdef LEADER_EVENTS_UPDATE_INTERVAL
    clear_event_counters();
    evtimer_set(&print_events_event, leader_print_event_counters, NULL);
    evutil_timerclear(&print_events_interval);
    print_events_interval.tv_sec = (LEADER_EVENTS_UPDATE_INTERVAL / 1000000);
    print_events_interval.tv_usec = (LEADER_EVENTS_UPDATE_INTERVAL % 1000000);
    leader_print_event_counters(0, 0, NULL);
#endif

    //Initialize values handler
    if(vh_init()!= 0) {
        printf("Values handler initialization failed!\n");
        return -1;
    }

    // Reset phase 1 counters
    p1_info.pending_count = 0;
    p1_info.ready_count = 0;
    // Set so that next p1 to open is current_iid
    p1_info.highest_open = current_iid - 1;
    
    //Initialize timer and corresponding event for
    // checking timeouts of instances, phase 1
    evtimer_set(&p1_check_event, leader_periodic_p1_check, NULL);
    evutil_timerclear(&p1_check_interval);
    p1_check_interval.tv_sec = 0; //(P1_TIMEOUT_INTERVAL/3 / 1000000);
    p1_check_interval.tv_usec = 10000; //(P1_TIMEOUT_INTERVAL/3 % 1000000);
    
    //Check pending, open new, set next timeout
    leader_periodic_p1_check(0, 0, NULL);
    
    //Reset phase 2 counters
    p2_info.next_unused_iid = current_iid;
    p2_info.open_count = 0;
    
    //Initialize timer and corresponding event for
    // checking timeouts of instances, phase 2
    evtimer_set(&p2_check_event, leader_periodic_p2_check, NULL);
    evutil_timerclear(&p2_check_interval);
    p2_check_interval.tv_sec = (((int)P2_CHECK_INTERVAL) / 1000000);
    p2_check_interval.tv_usec = ((P2_CHECK_INTERVAL) % 1000000);
    leader_set_next_p2_check();
    
    LOG(VRB, ("Leader is ready\n"));
    return 0;        
}

static void
leader_shutdown() {
    LOG(0, ("Proposer %d dropping leadership\n", this_proposer_id));

    evtimer_del(&p1_check_event);
    evtimer_del(&p2_check_event);
    
#ifdef LEADER_EVENTS_UPDATE_INTERVAL
    evtimer_del(&print_events_event);
#endif

    //Iterate over currently open instances 
    p_inst_info * ii;
    unsigned int i;
    for(i = current_iid; i <= p1_info.highest_open; i++) {
        ii = GET_PRO_INSTANCE(i);
        
        if(ii->status != p2_completed && ii->p2_value != NULL) {
            // A value was assigned to this instance, but it did 
            // not complete. Send back to the pending list for now
            vh_push_back_value(ii->p2_value);
            ii->p2_value = NULL;
        }
        //Clear all instances
        pro_clear_instance_info(ii);
    }
        
    //This will clear all values in the pending list
    // and notify the respective clients
    vh_shutdown();
}
