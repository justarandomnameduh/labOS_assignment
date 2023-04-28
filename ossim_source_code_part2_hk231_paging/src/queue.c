#include <stdio.h>
#include <stdlib.h>
#include "include/queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        (q->proc)[q->size] = proc;
        q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        if(empty(q))
                return NULL;
        struct pcb_t* tmp = (q->proc)[0];
        int swap_index = -1;
        for(int i = 0; i < q->size; i++){
                if(tmp->priority < (q->proc)[i]->priority){
                        swap_index = i;
                        tmp = (q->proc)[i];
                }
        }
        if(swap_index != q->size - 1){
                (q->proc)[swap_index] = (q->proc)[q->size-1];
                (q->proc)[q->size-1] = tmp;
        }
        q->size--;
        return (q->proc)[q->size];
}
