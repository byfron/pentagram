/*
Copyright (C) 2002,2003 The Pentagram team

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

#include "Kernel.h"
#include "Process.h"
#include "idMan.h"

#include "UCProcess.h"

#include "Actor.h"

typedef std::list<Process *>::iterator ProcessIterator;

Kernel* Kernel::kernel = 0;

Kernel::Kernel()
{
	assert(kernel == 0);
	kernel = this;
	pIDs = new idMan(1,32767);
	current_process = processes.end();

	objects.resize(65536);

	//!CONSTANTS
	objIDs = new idMan(256,65534);	// Want range of 256 to 65534
	objIDs->reserveID(666);			// 666 is reserved for the Guardian Bark hack
	actorIDs = new idMan(1,255);
}

Kernel::~Kernel()
{
	kernel = 0;

	delete pIDs;
	delete objIDs;
	delete actorIDs;
}

uint16 Kernel::addProcess(Process* proc)
{
	for (ProcessIterator it = processes.begin(); it != processes.end(); ++it) {
		if (*it == proc)
			return 0;
	}

	// Get a pID
	proc->pid = pIDs->getNewID();

#if 0
	perr << "[Kernel] Adding process " << proc
		 << ", pid = " << proc->pid << std::endl;
#endif

//	processes.push_back(proc);
	setNextProcess(proc);
//	proc->active = true;

	return proc->pid;
}

void Kernel::removeProcess(Process* proc)
{
	//! the way to remove processes has to be thought over sometime
	//! we probably want to flag them as terminated before actually
	//! removing/deleting it or something
	//! also have to look out for deleting processes while iterating
	//! over the list. (Hence the special 'erase' in runProcs below, which
	//! is very std::list-specific, incidentally)

	for (ProcessIterator it = processes.begin(); it != processes.end(); ++it) {
		if (*it == proc) {
			proc->active = false;
			
			perr << "[Kernel] Removing process " << proc << std::endl;

			processes.erase(it);

			// Clear pid
			pIDs->clearID(proc->pid);

			return;
		}
	}
}


//Q: is returning a 'dirty' flag really useful?
bool Kernel::runProcesses(uint32 framenum)
{
	if (processes.size() == 0) {
		return true;//
		perr << "Process queue is empty?! Aborting.\n";

		//! do this in a cleaner way
		exit(0);
	}

	bool dirty = false;
	current_process = processes.begin();
	while (current_process != processes.end()) {
		Process* p = *current_process;

		if (p->terminate_deferred)
			p->terminate();
		if (!p->terminated)
			if (p->run(framenum)) dirty = true;
		if (p->terminated) {
			// process is killed, so remove it from the list
			current_process = processes.erase(current_process);

			// Clear pid
			pIDs->clearID(p->pid);

			//! is this the right place to delete processes?
			delete p;
		}
		else
			++current_process;
	}

	return dirty;
}

void Kernel::setNextProcess(Process* proc)
{
	if (current_process != processes.end() && *current_process == proc) return;

	if (proc->active) {
		for (ProcessIterator it = processes.begin();
			 it != processes.end(); ++it) {
			if (*it == proc) {
				processes.erase(it);
				break;
			}
		}
	} else {
		proc->active = true;
	}

	if (current_process == processes.end()) {
		processes.push_front(proc);
	} else {
		ProcessIterator t = current_process;
		++t;

		processes.insert(t, proc);
	}
}

Process* Kernel::getProcess(uint16 pid)
{
	for (ProcessIterator it = processes.begin(); it != processes.end(); ++it) {
		Process* p = *it;
		if (p->pid == pid)
			return p;
	}
	return 0;
}

void Kernel::kernelStats()
{
	unsigned int npccount = 0, objcount = 0;

	//!constants
	for (unsigned int i = 1; i < 256; i++) {
		if (objects[i] != 0)
			npccount++;
	}
	for (unsigned int i = 256; i < objects.size(); i++) {
		if (objects[i] != 0)
			objcount++;
	}

	pout << "Kernel memory stats:" << std::endl;
	pout << "Processes  : " << processes.size() << "/32765" << std::endl;
	pout << "NPCs       : " << npccount << "/255" << std::endl;
	pout << "Objects    : " << objcount << "/65279" << std::endl;
}


uint32 Kernel::getNumProcesses(uint16 objid, uint16 processtype)
{
	if(objid==0 && processtype==6)
		return processes.size();
	
	uint32 count = 0;

	for (ProcessIterator it = processes.begin(); it != processes.end(); ++it)
	{
		Process* p = *it;

		if ((objid == 0 || objid == p->item_num) &&
			(processtype == 6 || processtype == p->type))
			count++;
	}

	return count;
}

void Kernel::killProcesses(uint16 objid, uint16 processtype)
{
	for (ProcessIterator it = processes.begin(); it != processes.end(); ++it)
	{
		Process* p = *it;

		if ((objid == 0 || objid == p->item_num) &&
			(processtype == 6 || processtype == p->type))
			p->terminate();
	}
}

uint16 Kernel::assignObjId(Object* obj)
{
	uint16 new_objid = objIDs->getNewID();
	// failure???
	if (new_objid != 0) {
		assert(objects[new_objid] == 0);
		objects[new_objid] = obj;
	}
	return new_objid;
}

uint16 Kernel::assignActorObjId(Actor* actor, uint16 new_objid)
{
	if (new_objid == 0xFFFF)
		new_objid = actorIDs->getNewID();
	else
		actorIDs->reserveID(new_objid);

	// failure???
	if (new_objid != 0) {
		assert(objects[new_objid] == 0);
		objects[new_objid] = actor;
	}
	return new_objid;
}

void Kernel::clearObjId(uint16 objid)
{
	// need to make this assert check only permanent NPCs
//	assert(objid >= 256); // !constant
	if (objid >= 256) // !constant
		objIDs->clearID(objid);
	else
		actorIDs->clearID(objid);

	objects[objid] = 0;
}

Object* Kernel::getObject(uint16 objid) const
{
	return objects[objid];
}

uint32 Kernel::I_getNumProcesses(const uint8* args, unsigned int /*argsize*/)
{
	ARG_UINT16(item);
	ARG_UINT16(type);

	return Kernel::get_instance()->getNumProcesses(item, type);
}

uint32 Kernel::I_resetRef(const uint8* args, unsigned int /*argsize*/)
{
	ARG_UINT16(item);
	ARG_UINT16(type);

	Kernel::get_instance()->killProcesses(item, type);
	return 0;
}
