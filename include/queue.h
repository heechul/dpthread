/**
 * Simple queue library 
 *
 * Author: Heechul Yun <heechul@illinois.edu> 
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 */ 
#ifndef QUEUE_H
#define QUEUE_H

typedef struct _Queue {
	int head;
	int tail;
	void **array;
	int size; 
} TQueue;

/* false - 0, true - 1*/ 

void  InitQ( TQueue* queue, int size);
void  InitQ2( TQueue* queue, TQueue* src);
int AddQ( TQueue* queue, void* item );
void* DelQ( TQueue* queue );
void* GetHeadQ( TQueue *queue );
int IsEmptyQ( TQueue* queue );
void  ClearQ( TQueue *queue, int freeObjs);
void  DestroyQ( TQueue* queue);

#endif /* QUEUE_H */
