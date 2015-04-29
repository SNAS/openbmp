/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 *
 */
#ifndef SAFEQUEUE_HPP_
#define SAFEQUEUE_HPP_

#include <pthread.h>

#include <cstdlib>
#include <queue>

namespace std {
/**
 * Extends std::queue to make it thread safe
 *
 * At this point we are not making the operator overloads thread safe, therefore
 *    they should not be used.
 */
template <typename type>
class safeQueue : public queue<type> {
private:
    pthread_mutex_t mutex;              // Pthread mutex lock for reading and modifying the trie.
    pthread_mutex_t mutex_wait;         // Pthread mutex lock for a controlled block/wait when the tree is empty
    pthread_mutex_t mutex_limitWait;    // Pthread mutex lock for a controlled block/wait when limit has been reached

    bool            waitOn;             // Indicator that the wait lock is on
    bool            limitWaitOn;        // Indicator that the wait lock is being used and should be unlocked

    uint32_t        limit;

public:

    /**
     * Add thread safety to the methods that are used to search, add, remove, ...
     *    We aren't changing much here except for adding mutex locking/unlocking.
     *
     * \param limit     Limit number of messages in queue before push blocks. Zero
     *                  is unlimited.
     */

    safeQueue(int limit=0) : queue<type>() {

        this->limit = limit;

        // Initialize the mutex variable
        pthread_mutex_init(&mutex, NULL);
        pthread_mutex_init(&mutex_wait, NULL);
        pthread_mutex_init(&mutex_limitWait, NULL);

        // Indicate that the wait is on since the queue is empty
        pthread_mutex_lock(&mutex_wait);
        waitOn = true;

        // This lock should always be set
        pthread_mutex_lock(&mutex_limitWait);
        limitWaitOn = false;
    }

    ~safeQueue(){
        // Free the mutex
        pthread_mutex_destroy(&mutex);
        pthread_mutex_destroy(&mutex_wait);
        pthread_mutex_destroy(&mutex_limitWait);
    }

    void push(type const &elem) {
        // Lock
        pthread_mutex_lock (&mutex);

        /*
         * Wait/block if limit has been reached
         */
        if (limit and size() >= limit) {
            limitWaitOn = true;
            pthread_mutex_unlock (&mutex);

            pthread_mutex_lock(&mutex_limitWait);
            pthread_mutex_unlock(&mutex_limitWait);

            pthread_mutex_lock (&mutex);
        }


        // Add
        queue<type>::push(elem);

        /*
         * If a new entry is added, release the lock if it's on
         */
        if (waitOn) {
            pthread_mutex_unlock(&mutex_wait);
            waitOn = false;
        }

        // Unlock
        pthread_mutex_unlock (&mutex);
    }

    void pop() {

        // Lock
        pthread_mutex_lock (&mutex);

        // Add
        queue<type>::pop();

        // Unlock the limit wait lock
        if (limitWaitOn and size() < limit) {
            pthread_mutex_unlock(&mutex_limitWait);
            pthread_mutex_lock(&mutex_limitWait);
            limitWaitOn = false;
        }

        // Set the wait lock if the queue is empty
        if (queue<type>::size() <= 0 && !waitOn) {
            pthread_mutex_lock(&mutex_wait);
            waitOn = true;
        }

        // Unlock
        pthread_mutex_unlock (&mutex);
    }

    size_t size() {
        size_t size;

        size = queue<type>::size();

        return size;
    }

   /**
    * front
    *     Thread safe method that returns a copy of the front object
    *     in the arg passed as value.
    *
    * ARGS:
    *    value =  Reference to allocated <type>.
    *
    * RETURNS:
    *     true if the value was updated, false if not
    */
    bool front(type &value) {

        // Lock
        pthread_mutex_lock (&mutex);

        // get front
        value = queue<type>::front();

        // Unlock
        pthread_mutex_unlock (&mutex);

        return true;
    }


    /**
     * back
     *     Thread safe method that returns a copy of the back object
     *     in the arg passed as value.
     *
     * ARGS:
     *    value = reference to allocated <type>.
     *
     * RETURNS:
     *     true if the value was updated, false if not
     */
    bool back(type &value) {

        // Lock
        pthread_mutex_lock (&mutex);

        // get back
        value = queue<type>::back();

        // Unlock
        pthread_mutex_unlock (&mutex);

        return true;
    }



    /**
     * PopFront is a bit different in that the mutex lock handles getting
     *    the object and then pop'ing it afterwards.
     *
     * ARGS:
     *    value = reference to allocated <type>
     *
     * RETURNS:
     *     true if the value was updated, false if not
     */
    bool popFront(type &value) {
        bool rval = true;

        // Lock
        pthread_mutex_lock (&mutex);

        // Before getting element, check if there are any
        if (queue<type>::size() > 0) {
            // get front
            value = queue<type>::front();

            // pop the front object
            queue<type>::pop();

            // Unlock the limit wait lock
            if (limitWaitOn and size() < limit) {
                pthread_mutex_unlock(&mutex_limitWait);
                pthread_mutex_lock(&mutex_limitWait);
            }

            // Set the wait lock if the queue is empty
            if (queue<type>::size() <= 0 && !waitOn) {
                pthread_mutex_lock(&mutex_wait);
                waitOn = true;
            }

        } else
            rval = false;

        // Unlock
        pthread_mutex_unlock (&mutex);

        return rval;
    }

    /**
     * new method to wait for the mutex lock.  When the queue is empty the lock is on.
     *    calling this method will cause the caller to block until there are new entries
     */
    bool wait() {
        pthread_mutex_lock(&mutex_wait);
        pthread_mutex_unlock(&mutex_wait);
        return true;
    }

    void setLimit(uint32_t limit) {
        this->limit = limit;
    }
};

} /* namespace std */
#endif /*  */
