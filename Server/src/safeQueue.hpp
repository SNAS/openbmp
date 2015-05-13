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

    uint32_t        limit;

public:

    /**
     * Add thread safety to the methods that are used to search, add, remove, ...
     *    We aren't changing much here except for adding mutex locking/unlocking.
     *
     * \param limit     Limit number of messages in queue before push blocks. Zero
     *                  is unlimited.
     */

    safeQueue(uint32_t limit=0) : queue<type>() {

        this->limit = limit;

        // Initialize the mutex variable
        pthread_mutex_init(&mutex, NULL);
    }

    ~safeQueue(){
        // Free the mutex
        pthread_mutex_destroy(&mutex);
    }

    void push(type const &elem) {
        // Lock
        pthread_mutex_lock (&mutex);

        /*
         * Wait/block if limit has been reached
         */
        if (limit and size() >= limit) {
            pthread_mutex_unlock (&mutex);

            while (size() >= limit) {
                usleep(25000);
            }

            pthread_mutex_lock (&mutex);
        }

        // Add
        queue<type>::push(elem);

        // Unlock
        pthread_mutex_unlock (&mutex);
    }

    void pop() {

        // Lock
        pthread_mutex_lock (&mutex);

        // Before getting element, check if there are any
        if (queue<type>::size() > 0) {
            queue<type>::pop();
        }

        // Unlock
        pthread_mutex_unlock (&mutex);
    }

    size_t size() {
        return  queue<type>::size();
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
        bool rval = true;

        // Lock
        pthread_mutex_lock (&mutex);

        // Before getting element, check if there are any
        if (queue<type>::size() > 0) {

            // get front
            value = queue<type>::front();

        } else
            rval = false;

        // Unlock
        pthread_mutex_unlock (&mutex);

        return rval;
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
        while (size() <= 0) {
            usleep(25000);
        }

        return true;
    }

    void setLimit(uint32_t limit) {
        this->limit = limit;
    }
};

} /* namespace std */
#endif /*  */
