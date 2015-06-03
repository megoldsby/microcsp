
/**
 *  Microcsp 
 *  Copyright (c) 2015 Michael E. Goldsby
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**************************************************************
 +
 |  atomic operations
 +
 *************************************************************/

#ifndef ATOMIC_H
#define ATOMIC_H

#include <stdatomic.h>

// stores value in destination
#define STORE(dest_p, val) \
    atomic_store_explicit(dest_p, val, memory_order_release)

// returns value of source
#define LOAD(src_p) \
    atomic_load_explicit(src_p, memory_order_acquire)  

// stores value in target, returning previous value of target
#define EXCH(target_p, val) \
    atomic_exchange_explicit(target_p, val, memory_order_acq_rel) 

// if target has expected value, stores desired value in target
// and returns true; otherwise returns false and stores actual
// value of target in expected
#define CAS(target_p, expected_p, desired) \
    atomic_compare_exchange_strong_explicit(target_p, expected_p, desired, \
        memory_order_acq_rel, memory_order_relaxed)

// adds value to target and returns previous value of target
#define INCR(target_p, val) \
    atomic_fetch_add_explicit(target_p, src, memory_order_acq_rel)

// subtracts value from target and returns previous value of target
#define DECR(target_p, val) \
    atomic_fetch_sub_explicit(target_p, src, memory_order_acq_rel)

// sets memory fence between signal handler and normal code
#define SIGFENCE \
    atomic_signal_fence(memory_order_seq_cst)

#endif
