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

#include "ActorAnimProcess.h"
#include "GameData.h"
#include "MainShapeFlex.h"
#include "AnimDat.h"
#include "AnimAction.h"
#include "Actor.h"
#include "Direction.h"
#include "World.h"

// p_dynamic_cast stuff
DEFINE_DYNAMIC_CAST_CODE(ActorAnimProcess,Process);

ActorAnimProcess::ActorAnimProcess(Actor* actor_, uint32 action, uint32 dir_)
{
	assert(actor_);
	actor = actor_->getObjId();
	dir = dir_;

	uint32 shape = actor_->getShape();
	animaction = GameData::get_instance()->getMainShapes()->
		getAnim(shape, action);

	if (dir > 7) {
		// invalid direction
		animaction = 0;
	}

	currentindex = 0;

	if (actor_->getActorFlags() & Actor::ACT_ANIMLOCK) {
		//! What do we do if actor was already animating?
		//! don't do this animation or kill the previous one?
		//! Or maybe wait until the previous one finishes?

		// for now, just don't play this one.
		animaction = 0;
	}

	if (animaction) {
		actor_->setActorFlag(Actor::ACT_ANIMLOCK);

		if (actor_->lastanim == action && actor_->direction == dir_ &&
			animaction->size > 1)
			currentindex++; // skip first frame if repeating an animation

		actor_->lastanim = action;
		actor_->direction = dir_;
	}
}

bool ActorAnimProcess::run(const uint32 framenum)
{
	if (!animaction) {
		// non-existant animation
		terminate();
		return false;
	}

	//!! this needs timing
	// without main timing I can't really determine how many 'ticks' 
	// each frame should last.

	AnimFrame& f = animaction->frames[dir][currentindex];
	Actor *a = p_dynamic_cast<Actor*>(World::get_instance()->getObject(actor));

	if (!a) {
		// actor gone
		terminate();
		return false;
	}

	sint32 x, y, z;
	a->getLocation(x,y,z);

	x += 2 * x_fact[dir] * f.deltadir;
	y += 2 * y_fact[dir] * f.deltadir;
	z += f.deltaz;

	a->move(x,y,z);
	a->setFrame(f.frame);

	if (f.is_flipped()) {
		a->setFlag(Item::FLG_FLIPPED);
	} else {
		a->clearFlag(Item::FLG_FLIPPED);
	}

	currentindex++;

	if (currentindex >= animaction->size) {
		//? do we need to terminate now or when we're about to show the next
		// frame?
		terminate();
	}



	return true;
}


void ActorAnimProcess::terminate()
{
	Actor *a = p_dynamic_cast<Actor*>(World::get_instance()->getObject(actor));
	if (a) {
		if (animaction) { // if we were really animating...
			a->clearActorFlag(Actor::ACT_ANIMLOCK);
		}
	}

	Process::terminate();
}