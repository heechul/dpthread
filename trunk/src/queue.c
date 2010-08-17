/**
 * Simple queue library 
 *
 * Author: Heechul Yun <heechul@illinois.edu> 
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

void  InitQ( TQueue* queue, int size)
{
	queue->head = 0;
	queue->tail = 0;
	queue->size = size;
	queue->array = (void **)malloc(sizeof(void *) * size); 
}

void  InitQ2( TQueue* queue, TQueue* src)
{
	queue->head = src->head;
	queue->tail = src->tail;
	queue->size = src->size;
	queue->array = (void **)malloc(sizeof(void *) * src->size);
	memcpy(queue->array, src->array, sizeof(void *)*queue->size); 
}

int AddQ( TQueue* queue, void* item )
{
	if ( (queue->tail+1) % queue->size == queue->head ) // queue full
		return 0;
	queue->array[queue->tail] = item;
	queue->tail = (queue->tail + 1) % queue->size;
	return 1; 
}

void* DelQ( TQueue* queue )
{
	void* item;
	if ( queue->head == queue->tail ) // queue empty
		return NULL;
	item = queue->array[queue->head];
	queue->head = (queue->head + 1) % queue->size;
	return item;
}

void* GetHeadQ( TQueue *queue )
{
	void* item;
	if ( queue->head == queue->tail ) // queue empty
		return NULL;
	item = queue->array[queue->head];
	return item;  
}

int IsEmptyQ( TQueue *queue )
{
	if ( queue->head == queue->tail ) return 1;
	else return 0; 
}

void  ClearQ( TQueue *queue, int freeObjs)
{
	void* itm; 
	while ( IsEmptyQ(queue) == 0 ) {
		itm = DelQ(queue);
		if ( freeObjs == 1 )
			free(itm);
	}
}

void  DestroyQ( TQueue* queue)
{
	queue->size = 0; 
	free(queue->array); 
}





