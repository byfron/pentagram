/*
Copyright (C) 2002 The Pentagram team

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

#ifndef PROCESS_H
#define PROCESS_H

class Process {
	bool active;
	bool terminated;

public:
	friend class Kernel;

	// returns true if screen needs to be repainted
	virtual bool run(const uint32 framenum) = 0;

	Process() : active(false) { }
	virtual ~Process() { }

	bool is_active() const { return active; }

	void terminate() { terminated = true; }

};


#endif