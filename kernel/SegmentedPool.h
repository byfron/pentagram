/*
Copyright (C) 2003-2006 The Pentagram team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef SEGMENTED_POOL_H
#define SEGMENTED_POOL_H

#include "Pool.h"
// Think about adding magic to the front of the
// PoolNode to identify them.

class SegmentedPool;

struct SegmentedPoolNode;

/**
 * A pool with memory broken into even length segments.
 * SegmentedPool only allocate memory one segment at a time.
 * If the requested memory is larger than a segment, allocation will fail
 * and return 0.
 */
class SegmentedPool: public Pool
{
public:
	SegmentedPool(size_t nodeCapacity, uint32 nodes);
	virtual ~SegmentedPool();

	ENABLE_RUNTIME_CLASSTYPE();

	virtual void * allocate(size_t size);
	virtual void deallocate(void * ptr);

	virtual bool isFull() {return freeNodeCount == 0;}
	virtual bool isEmpty() {return freeNodeCount == nodes;}

	virtual bool inPool(void * ptr) {return (ptr > startOfPool && ptr < endOfPool);}

	void printInfo();

	size_t getNodeCapacity() {return nodeCapacity;}

	SegmentedPoolNode* getPoolNode(void * ptr);
private:
	uint8 * startOfPool;
	uint8 * endOfPool;

	size_t nodeOffset;
	size_t nodeCapacity;
	uint32 nodes;
	uint32 freeNodeCount;

	SegmentedPoolNode* firstFree;
	SegmentedPoolNode* lastFree;
};

#endif
