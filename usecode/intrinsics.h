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

#ifndef INTRINSICS_H
#define INTRINSICS_H

typedef uint32 (*Intrinsic)(const uint8* args, unsigned int argsize);

#define INTRINSIC(x) static uint32 x (const uint8* args, unsigned int argsize)

// TODO: range checking on args

#define ARG_UINT8(x)  uint8  x = (*args++);
#define ARG_UINT16(x) uint16 x = (*args++); x += ((*args++) << 8);
#define ARG_UINT32(x) uint32 x = (*args++); x += ((*args++) << 8); \
                     x+= ((*args++) << 16); x += ((*args++) << 24);
#define ARG_SINT8(x)  sint8  x = (*args++);
#define ARG_SINT16(x) sint16 x = (*args++); x += ((*args++) << 8);
#define ARG_SINT32(x) sint32 x = (*args++); x += ((*args++) << 8); \
                     x+= ((*args++) << 16); x += ((*args++) << 24);

#define ARG_OBJECT(x) ARG_UINT32(ucptr_##x); \
                      uint16 id_##x = UCMachine::ptrToObject(ucptr_##x); \
                      Object* x = World::get_instance()->getObject(id_##x);
#define ARG_ITEM(x)   ARG_OBJECT(obj_##x); \
                      Item* x = p_dynamic_cast<Item*>(obj_##x);
#define ARG_CONTAINER(x) ARG_OBJECT(obj_##x); \
                      Container* x = p_dynamic_cast<Container*>(obj_##x);
#define ARG_ACTOR(x)  ARG_OBJECT(obj_##x); \
                      Actor* x = p_dynamic_cast<Actor*>(obj_##x);


#define ARG_STRING(x) ARG_UINT32(ucptr_##x); \
                      uint16 id_##x = UCMachine::ptrToObject(ucptr_##x); \
                      std::string x = UCMachine::get_instance()->getString(id_##x);

#define ARG_LIST(x)   ARG_UINT16(id_##x); \
                      UCList* x = UCMachine::get_instance()->getList(id_##x);

#define ARG_NULL8(x)  args+=1;
#define ARG_NULL16(x) args+=2;
#define ARG_NULL32(x) args+=4;

#endif
