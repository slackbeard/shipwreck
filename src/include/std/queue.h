#pragma once

#include <std/types.h>

#define MAX_RACE_TRIES 10

struct Message {
	int msg_id;
	int data;
	volatile Message &operator =(const volatile Message &other) volatile {
		msg_id = other.msg_id;	
		data = other.data;
		return *this;
	}
};


struct QueueRange {
	union {
		struct {
			ushort head;
			ushort tail;
		};
		uint ival;
	};
	uint size() volatile {
		return tail - head;
	}	
	operator int() volatile {
		return this->ival;
	}
	QueueRange &operator =(QueueRange &qr) volatile{
		ival = qr.ival;
		return * (QueueRange *) this;
	}
	QueueRange &operator =(int i) volatile {
		ival = i;
		return * (QueueRange *) this;
	}
	bool operator ==(QueueRange &qr) volatile {
		return ival == qr.ival;
	}
	
};

template <int N=0>
struct MsgQueue {
	uint max_size;
	QueueRange q_locked;
	QueueRange q_safe;
	Message queue[N];
	MsgQueue() {
		max_size = N;
		q_safe = 0;
		q_locked = 0;
	}
	Message *get(int i) {
		return &queue[i % max_size];
	}
	uint size() {
		return q_safe.size();
	}
	void enqueue(Message *msg) {
		uint tail = q_locked.tail % max_size;
		queue[tail].msg_id = msg->msg_id;
		queue[tail].data = msg->data;
		q_locked.tail++;	
		q_safe.tail = q_locked.tail;
	}

	QueueRange dequeue(uint num=1) {
		// dequeue up to `num` messages.
		// returns the consumed range. This range should be `release()`ed when done

		if (q_safe.size() == 0) return (QueueRange) {0,0};

		uint my_num = (num > q_safe.size())?q_safe.size():num;
		ushort old_head = q_safe.head;
		q_safe.head += my_num;
		// if the queue was empty or we exhausted MAX_RACE_TRIES, return a range of 0
		return (QueueRange){old_head, q_safe.head};
	}

	void release(uint num=1) {
		if (num > (q_safe.head - q_locked.head)) return;
		q_locked.head += num;
	}

};

template <typename T, int N=0>
struct SharedQueue {
	uint max_size;
	volatile QueueRange q_locked;
	volatile QueueRange q_safe;
	volatile uint safe_tail_mtx;
	volatile uint locked_head_mtx;
	volatile T queue[N];

	SharedQueue() {
		q_locked.ival = 0;
		q_safe.ival = 0;
		safe_tail_mtx = 0;
		locked_head_mtx = 0;
		max_size = N;
		release({0, 0});
	}
	uint size() {
		// use a snapshot so size is at least consistent with changes to head & tail
		QueueRange tmp = *(QueueRange *) &q_safe;
		return tmp.size();
	}

	volatile T *get(int i) {
		return &queue[i % N];
	}


	int enqueue(T *msg) {

		for (uint i = 0; i < MAX_RACE_TRIES; i++) {

			// get a snapshot of the locked range:
			QueueRange q_tmp = *(QueueRange *) &q_locked;
			QueueRange q_new = q_tmp;

			uint my_length = q_new.tail - q_new.head;

			// If the new length would exceed max size, queue is full
			if ((my_length + 1) > max_size)  return -1;

			q_new.tail++;

			// we get an index by atomically incrementing the locked tail
			if (q_tmp.ival == __sync_val_compare_and_swap(&q_locked.ival, q_tmp.ival, q_new.ival)) {
				// if q_tmp is what we assumed it was, then we won the race:

				uint my_index = q_tmp.tail;
				*(T *)&queue[my_index % max_size] = *msg;
				//queue[my_index % max_size] = *msg;
				return my_index;
				
			}

		}

		// if we looped over max_size without finding a free index, the queue was effectively full
		return -1;

	}

	int gather_new_data() {
		// Discover newly queued data:

		uint my_tail_mtx = __sync_lock_test_and_set(&safe_tail_mtx, 1);
		uint new_found = 0;
		if (my_tail_mtx == 0) {
			// lock acquired

			// snapshot locked range
			QueueRange q_tmp = *(QueueRange *) &q_locked;

			int q_size = q_locked.tail - q_safe.tail;

			// increment q_safe.tail until we run out of data:
			for (; new_found < q_size; new_found++) {

				if (*(int *) &queue[q_safe.tail % max_size] == 0) break;

				q_safe.tail++;
			}
			__sync_lock_release(&safe_tail_mtx);
		}
		return new_found;
	}

	QueueRange dequeue(uint num=1) {
		// dequeue up to `num` messages.
		// returns the consumed range. This range should be `release()`ed when done

		// get a snapshot of the safe range
		QueueRange q_tmp = *(QueueRange *) &q_safe;

		// Lock the next index within the q_safe range for this consumer
		for (int i = 0; i < MAX_RACE_TRIES; i++) {


			// Use unsigned diff rather than perform a comparison (ie. `if (head >= tail) ...`) 
			// because these integers will wrap. 
			// We always enforce `tail > head` so we can assume `tail - head` is the correct length.
			uint my_length = q_tmp.tail - q_tmp.head;

			// If there is not at least 1 entry, the queue is empty
			if (my_length <= 0)  break;

			// limit the number we consume:
			if (num > my_length) num = my_length;

			QueueRange q_new = q_tmp;
			q_new.head += num;

			// we lock an index by atomically incrementing safe head
			QueueRange q_old = q_tmp;
			q_tmp =  __sync_val_compare_and_swap((int *) &q_safe, q_tmp, q_new);
			if (q_old == q_tmp) {
				// if q_tmp is what we assumed it was, then we won the race:

				// return the range that we locked:
				q_tmp.tail = q_new.head;
				return q_tmp;
			}

			// else, q_tmp has new value of the shared range q_safe
		}

		// if the queue was empty or we exhausted MAX_RACE_TRIES, return a range of 0
		return (QueueRange) {0,0};
	}

	void release(QueueRange qr) {
		for (int i = qr.head; i < qr.tail; i++) {
			int *ptr = (int *) &queue[i % max_size];
			// mark as free
			*ptr = 0;
		}
	}

	int garbage_collect() {
		// Garbage collect recently consumed data:
		// returns 0 if not run, 1 if run.
		uint my_head_mtx = __sync_lock_test_and_set(&locked_head_mtx, 1);
		if (my_head_mtx == 0) {

			int q_size = q_safe.head - q_locked.head;

			for (int i = 0; i < q_size; i++) { 
				if (*(int *) &queue[q_locked.head % max_size] != 0) break;
				q_locked.head++;
			}
			__sync_lock_release(&locked_head_mtx);
			return 1;
		}
		return 0;	
	}
};


template <int N=0>
struct SharedMsgQueue: public SharedQueue<Message, N> {
};
