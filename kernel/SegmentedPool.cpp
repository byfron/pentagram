/*
Copyright (C) 2003-2005 The Pentagram team

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
#include "pent_include.h"

#include "SegmentedPool.h"

DEFINE_RUNTIME_CLASSTYPE_CODE(SegmentedPool,Pool);

//	Memory is aligned to the next largest multiple of sizeof(x) from
//  the base address plus the size. Although, this may not be very helpful
//  if the base address is not a multiple of sizeof(X).
//	example: sizeof(x) = 0x8, object size = 0xFFE2:
//			0xFFE2 + 0x8 - 1 = 0xFFE9;
//			0xFFE9 & ~(0x8 - 0x1) -> 0xFFE9 & 0xFFF8 = 0xFFE8

#define OFFSET_ALIGN(X) ( (X + sizeof(uintptr) - 1) & ~(sizeof(uintptr) - 1) )

// We pad both the PoolNode and the memory to align it.

SegmentedPool::SegmentedPool(size_t nodeCapacity_, uint32 nodes_) 
	: Pool(), nodes(nodes_), freeNodeCount(nodes_)
{
	uint32 i;

	// Give it its real capacity.
	nodeCapacity = OFFSET_ALIGN(nodeCapacity_);
	nodes = nodes_;
	
	// Node offesets are aligned to the next uintptr offset after the real size
	nodeOffset = OFFSET_ALIGN(sizeof(SegmentedPoolNode)) + nodeCapacity;

	startOfPool = new uint8[nodeOffset * nodes_];
	endOfPool = startOfPool + (nodeOffset * nodes_);

/*
	con.Printf("Pool Info:\n start %X\tend %X\n nodeOffset %X\t nodeCapacity %X\n nodes %X\n",
			startOfPool, endOfPool, nodeOffset, nodeCapacity, nodes);
*/

	firstFree = reinterpret_cast<SegmentedPoolNode*>(startOfPool);
	firstFree->pool = this;
	firstFree->size = 0;

	lastFree = firstFree;

	for (i = 1; i < nodes_; ++i)
	{
		lastFree->nextFree = reinterpret_cast<SegmentedPoolNode*>(startOfPool + i * nodeOffset);
		lastFree = lastFree->nextFree;

		lastFree->pool = this;
		lastFree->size = 0;
	}

	lastFree->nextFree = 0;
}

SegmentedPool::~SegmentedPool()
{
	assert(isEmpty());

	delete [] startOfPool;
}

void * SegmentedPool::allocate(size_t size)
{
	SegmentedPoolNode* node;

	if (isFull() || size > nodeCapacity)
		return 0;

	--freeNodeCount;
	node = firstFree;
	node->size = size;

	if (isFull())
	{
		firstFree = 0;
		lastFree = 0;
	}
	else
	{
		firstFree = firstFree->nextFree;
	}

	node->nextFree = 0;

//	con.Printf("Allocating Node 0x%08X\n", node);
	return (reinterpret_cast<uint8 *>(node) + OFFSET_ALIGN(sizeof(SegmentedPoolNode)) );
}

void SegmentedPool::deallocate(void * ptr)
{
	SegmentedPoolNode* node;

	if (inPool(ptr))
	{
		node = getPoolNode(ptr);
		node->size = 0;
		assert(node->pool == this);

//	con.Printf("Free Node 0x%08X\n", node);
		if (isFull())
		{
			firstFree = node;
			lastFree = node;
		}
		else
		{
			lastFree->nextFree = node;
			lastFree = lastFree->nextFree;
		}
		++freeNodeCount;
	}
}

SegmentedPoolNode* SegmentedPool::getPoolNode(void * ptr)
{
	uint32 pos = (reinterpret_cast<uint8 *>(ptr) - startOfPool) / nodeOffset;
	return reinterpret_cast<SegmentedPoolNode*>(startOfPool + pos*nodeOffset);
}

