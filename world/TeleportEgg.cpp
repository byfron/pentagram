/*
Copyright (C) 2003 The Pentagram team

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

#include "TeleportEgg.h"
#include "World.h"

DEFINE_DYNAMIC_CAST_CODE(TeleportEgg,Egg);

TeleportEgg::TeleportEgg()
{

}


TeleportEgg::~TeleportEgg()
{

}

uint16 TeleportEgg::hatch()
{
	if (!teleporter) return 0; // teleport target

	// find right destination egg (teleport_id) in new map (mapnum)
	// teleport to destination egg

	return 0;
}