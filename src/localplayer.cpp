/*
BlockPlanet
Copyright (C) 2012 MiJyn, Joel Leclerc <mijyn@mail.com>
Contributors:
Jeija
Licensed under GPLv3


Based on:
Minetest-c55
Copyright (C) 2010-2011 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "localplayer.h"
#include "main.h" // For g_settings
#include "event.h"
#include "collision.h"
#include "gamedef.h"
#include "nodedef.h"
#include "settings.h"
#include "map.h"

/*
	LocalPlayer
*/

LocalPlayer::LocalPlayer(IGameDef *gamedef):
	Player(gamedef),
	m_sneak_node(32767,32767,32767),
	m_sneak_node_exists(false)
{
	// Initialize hp to 0, so that no hearts will be shown if server
	// doesn't support health points
	hp = 0;
}

LocalPlayer::~LocalPlayer()
{
}

void LocalPlayer::move(f32 dtime, Map &map, f32 pos_max_d,
		core::list<CollisionInfo> *collision_info)
{
	INodeDefManager *nodemgr = m_gamedef->ndef();

	v3f position = getPosition();
	v3f oldpos = position;
	v3s16 oldpos_i = floatToInt(oldpos, BS);

	v3f old_speed = m_speed;

	/*std::cout<<"oldpos_i=("<<oldpos_i.X<<","<<oldpos_i.Y<<","
			<<oldpos_i.Z<<")"<<std::endl;*/

	/*
		Calculate new position
	*/
	position += m_speed * dtime;
	
	// Skip collision detection if a special movement mode is used
	//bool fly_allowed = m_gamedef->checkLocalPrivilege("fly");
	//bool free_move = fly_allowed && g_settings->getBool("free_move");
	//bool free_move = fly_allowed && is_flying;
	//bool fast_allowed = m_gamedef->checkLocalPrivilege("fast");
	//bool fast_move = fast_allowed && g_settings->getBool("fast_move");
	//if(free_move)
	//{
	//	setPosition(position);
	//	return;
	//}

	/*
		Collision detection
	*/
	
	// Player position in nodes
	v3s16 pos_i = floatToInt(position, BS);
	
	/*
		Check if player is in water (the oscillating value)
	*/
	try{
		// If in water, the threshold of coming out is at higher y
		if(in_water)
		{
			v3s16 pp = floatToInt(position + v3f(0,BS*0.1,0), BS);
			in_water = nodemgr->get(map.getNode(pp).getContent()).isLiquid();
		}
		// If not in water, the threshold of going in is at lower y
		else
		{
			v3s16 pp = floatToInt(position + v3f(0,BS*0.5,0), BS);
			in_water = nodemgr->get(map.getNode(pp).getContent()).isLiquid();
		}
	}
	catch(InvalidPositionException &e)
	{
		in_water = false;
	}

	/*
		Check if player is in water (the stable value)
	*/
	try{
		v3s16 pp = floatToInt(position + v3f(0,0,0), BS);
		in_water_stable = nodemgr->get(map.getNode(pp).getContent()).isLiquid();
	}
	catch(InvalidPositionException &e)
	{
		in_water_stable = false;
	}

	/*
	        Check if player is climbing
	*/

	try {
		v3s16 pp = floatToInt(position + v3f(0,0.5*BS,0), BS);
		v3s16 pp2 = floatToInt(position + v3f(0,-0.2*BS,0), BS);
		is_climbing = ((nodemgr->get(map.getNode(pp).getContent()).climbable ||
		nodemgr->get(map.getNode(pp2).getContent()).climbable));
	}
	catch(InvalidPositionException &e)
	{
		is_climbing = false;
	}

	//is_flying = free_move;
	//is_sprinting = fast_move;

	/*
		Collision uncertainty radius
		Make it a bit larger than the maximum distance of movement
	*/
	//f32 d = pos_max_d * 1.1;
	// A fairly large value in here makes moving smoother
	f32 d = 0.15*BS;

	// This should always apply, otherwise there are glitches
	assert(d > pos_max_d);

	float player_radius = BS*0.30;
	float player_height = BS*1.55;
	
	// Maximum distance over border for sneaking
	f32 sneak_max = BS*0.4;

	/*
		If sneaking, player has larger collision radius to keep from
		falling
	*/
	/*if(control.sneak)
		player_radius = sneak_max + d*1.1;*/
	
	/*
		If sneaking, keep in range from the last walked node and don't
		fall off from it
	*/
	if(control.sneak && m_sneak_node_exists && !is_flying && !is_climbing)
	{
		f32 maxd = 0.5*BS + sneak_max;
		v3f lwn_f = intToFloat(m_sneak_node, BS);
		position.X = rangelim(position.X, lwn_f.X-maxd, lwn_f.X+maxd);
		position.Z = rangelim(position.Z, lwn_f.Z-maxd, lwn_f.Z+maxd);
		
		f32 min_y = lwn_f.Y + 0.5*BS;
		if(position.Y < min_y)
		{
			position.Y = min_y;

			//v3f old_speed = m_speed;

			if(m_speed.Y < 0)
				m_speed.Y = 0;

			/*if(collision_info)
			{
				// Report fall collision
				if(old_speed.Y < m_speed.Y - 0.1)
				{
					CollisionInfo info;
					info.t = COLLISION_FALL;
					info.speed = m_speed.Y - old_speed.Y;
					collision_info->push_back(info);
				}
			}*/
		}
	}

	/*
		Calculate player collision box (new and old)
	*/
	core::aabbox3d<f32> playerbox(
		position.X - player_radius,
		position.Y - 0.0,
		position.Z - player_radius,
		position.X + player_radius,
		position.Y + player_height,
		position.Z + player_radius
	);
	core::aabbox3d<f32> playerbox_old(
		oldpos.X - player_radius,
		oldpos.Y - 0.0,
		oldpos.Z - player_radius,
		oldpos.X + player_radius,
		oldpos.Y + player_height,
		oldpos.Z + player_radius
	);

	/*
		If the player's feet touch the topside of any node, this is
		set to true.

		Player is allowed to jump when this is true.
	*/
	bool touching_ground_was = touching_ground;
	touching_ground = false;

	/*std::cout<<"Checking collisions for ("
			<<oldpos_i.X<<","<<oldpos_i.Y<<","<<oldpos_i.Z
			<<") -> ("
			<<pos_i.X<<","<<pos_i.Y<<","<<pos_i.Z
			<<"):"<<std::endl;*/
	
	bool standing_on_unloaded = false;
	
	/*
		Go through every node around the player
	*/
	for(s16 y = oldpos_i.Y - 1; y <= oldpos_i.Y + 2; y++)
	for(s16 z = oldpos_i.Z - 1; z <= oldpos_i.Z + 1; z++)
	for(s16 x = oldpos_i.X - 1; x <= oldpos_i.X + 1; x++)
	{
		bool is_unloaded = false;
		try{
			// Player collides into walkable nodes
			if(nodemgr->get(map.getNode(v3s16(x,y,z))).walkable == false)
				continue;
		}
		catch(InvalidPositionException &e)
		{
			is_unloaded = true;
			// Doing nothing here will block the player from
			// walking over map borders
		}

		core::aabbox3d<f32> nodebox = getNodeBox(v3s16(x,y,z), BS);
		
		/*
			See if the player is touching ground.

			Player touches ground if player's minimum Y is near node's
			maximum Y and player's X-Z-area overlaps with the node's
			X-Z-area.

			Use 0.15*BS so that it is easier to get on a node.
		*/
		if(
				//fabs(nodebox.MaxEdge.Y-playerbox.MinEdge.Y) < d
				fabs(nodebox.MaxEdge.Y-playerbox.MinEdge.Y) < 0.15*BS
				&& nodebox.MaxEdge.X-d > playerbox.MinEdge.X
				&& nodebox.MinEdge.X+d < playerbox.MaxEdge.X
				&& nodebox.MaxEdge.Z-d > playerbox.MinEdge.Z
				&& nodebox.MinEdge.Z+d < playerbox.MaxEdge.Z
		){
			touching_ground = true;
			if(is_unloaded)
				standing_on_unloaded = true;
		}
		
		// If player doesn't intersect with node, ignore node.
		if(playerbox.intersectsWithBox(nodebox) == false)
			continue;
		
		/*
			Go through every axis
		*/
		v3f dirs[3] = {
			v3f(0,0,1), // back-front
			v3f(0,1,0), // top-bottom
			v3f(1,0,0), // right-left
		};
		for(u16 i=0; i<3; i++)
		{
			/*
				Calculate values along the axis
			*/
			f32 nodemax = nodebox.MaxEdge.dotProduct(dirs[i]);
			f32 nodemin = nodebox.MinEdge.dotProduct(dirs[i]);
			f32 playermax = playerbox.MaxEdge.dotProduct(dirs[i]);
			f32 playermin = playerbox.MinEdge.dotProduct(dirs[i]);
			f32 playermax_old = playerbox_old.MaxEdge.dotProduct(dirs[i]);
			f32 playermin_old = playerbox_old.MinEdge.dotProduct(dirs[i]);
			
			/*
				Check collision for the axis.
				Collision happens when player is going through a surface.
			*/
			/*f32 neg_d = d;
			f32 pos_d = d;
			// Make it easier to get on top of a node
			if(i == 1)
				neg_d = 0.15*BS;
			bool negative_axis_collides =
				(nodemax > playermin && nodemax <= playermin_old + neg_d
					&& m_speed.dotProduct(dirs[i]) < 0);
			bool positive_axis_collides =
				(nodemin < playermax && nodemin >= playermax_old - pos_d
					&& m_speed.dotProduct(dirs[i]) > 0);*/
			bool negative_axis_collides =
				(nodemax > playermin && nodemax <= playermin_old + d
					&& m_speed.dotProduct(dirs[i]) < 0);
			bool positive_axis_collides =
				(nodemin < playermax && nodemin >= playermax_old - d
					&& m_speed.dotProduct(dirs[i]) > 0);
			bool main_axis_collides =
					negative_axis_collides || positive_axis_collides;
			
			/*
				Check overlap of player and node in other axes
			*/
			bool other_axes_overlap = true;
			for(u16 j=0; j<3; j++)
			{
				if(j == i)
					continue;
				f32 nodemax = nodebox.MaxEdge.dotProduct(dirs[j]);
				f32 nodemin = nodebox.MinEdge.dotProduct(dirs[j]);
				f32 playermax = playerbox.MaxEdge.dotProduct(dirs[j]);
				f32 playermin = playerbox.MinEdge.dotProduct(dirs[j]);
				if(!(nodemax - d > playermin && nodemin + d < playermax))
				{
					other_axes_overlap = false;
					break;
				}
			}
			
			/*
				If this is a collision, revert the position in the main
				direction.
			*/
			if(other_axes_overlap && main_axis_collides)
			{
				//v3f old_speed = m_speed;

				m_speed -= m_speed.dotProduct(dirs[i]) * dirs[i];
				position -= position.dotProduct(dirs[i]) * dirs[i];
				position += oldpos.dotProduct(dirs[i]) * dirs[i];
				
				/*if(collision_info)
				{
					// Report fall collision
					if(old_speed.Y < m_speed.Y - 0.1)
					{
						CollisionInfo info;
						info.t = COLLISION_FALL;
						info.speed = m_speed.Y - old_speed.Y;
						collision_info->push_back(info);
					}
				}*/
			}
		
		}
	} // xyz

	/*
		Check the nodes under the player to see from which node the
		player is sneaking from, if any.
	*/
	{
		v3s16 pos_i_bottom = floatToInt(position - v3f(0,BS/2,0), BS);
		v2f player_p2df(position.X, position.Z);
		f32 min_distance_f = 100000.0*BS;
		// If already seeking from some node, compare to it.
		/*if(m_sneak_node_exists)
		{
			v3f sneaknode_pf = intToFloat(m_sneak_node, BS);
			v2f sneaknode_p2df(sneaknode_pf.X, sneaknode_pf.Z);
			f32 d_horiz_f = player_p2df.getDistanceFrom(sneaknode_p2df);
			f32 d_vert_f = fabs(sneaknode_pf.Y + BS*0.5 - position.Y);
			// Ignore if player is not on the same level (likely dropped)
			if(d_vert_f < 0.15*BS)
				min_distance_f = d_horiz_f;
		}*/
		v3s16 new_sneak_node = m_sneak_node;
		for(s16 x=-1; x<=1; x++)
		for(s16 z=-1; z<=1; z++)
		{
			v3s16 p = pos_i_bottom + v3s16(x,0,z);
			v3f pf = intToFloat(p, BS);
			v2f node_p2df(pf.X, pf.Z);
			f32 distance_f = player_p2df.getDistanceFrom(node_p2df);
			f32 max_axis_distance_f = MYMAX(
					fabs(player_p2df.X-node_p2df.X),
					fabs(player_p2df.Y-node_p2df.Y));
					
			if(distance_f > min_distance_f ||
					max_axis_distance_f > 0.5*BS + sneak_max + 0.1*BS)
				continue;

			try{
				// The node to be sneaked on has to be walkable
				if(nodemgr->get(map.getNode(p)).walkable == false)
					continue;
				// And the node above it has to be nonwalkable
				if(nodemgr->get(map.getNode(p+v3s16(0,1,0))).walkable == true)
					continue;
			}
			catch(InvalidPositionException &e)
			{
				continue;
			}

			min_distance_f = distance_f;
			new_sneak_node = p;
		}
		
		bool sneak_node_found = (min_distance_f < 100000.0*BS*0.9);
		
		if(control.sneak && m_sneak_node_exists && !is_flying && !is_climbing)
		{
			if(sneak_node_found)
				m_sneak_node = new_sneak_node;
		}
		else
		{
			m_sneak_node = new_sneak_node;
			m_sneak_node_exists = sneak_node_found;
		}

		/*
			If sneaking, the player's collision box can be in air, so
			this has to be set explicitly
		*/
		if(sneak_node_found && control.sneak)
			touching_ground = true;
	}
	
	/*
		Set new position
	*/
	setPosition(position);
	
	/*
		Report collisions
	*/
	if(collision_info)
	{
		// Report fall collision
		if(old_speed.Y < m_speed.Y - 0.1 && !standing_on_unloaded && !is_flying)
		{
			CollisionInfo info;
			info.t = COLLISION_FALL;
			info.speed = m_speed.Y - old_speed.Y;
			collision_info->push_back(info);
		}
	}

	if(!touching_ground_was && touching_ground){
		MtEvent *e = new SimpleTriggerEvent("PlayerRegainGround");
		m_gamedef->event()->put(e);
	}

	{
		camera_barely_in_ceiling = false;
		v3s16 camera_np = floatToInt(getEyePosition(), BS);
		MapNode n = map.getNodeNoEx(camera_np);
		if(n.getContent() != CONTENT_IGNORE){
			if(nodemgr->get(n).walkable){
				camera_barely_in_ceiling = true;
			}
		}
	}
}

void LocalPlayer::move(f32 dtime, Map &map, f32 pos_max_d)
{
	move(dtime, map, pos_max_d, NULL);
}

void LocalPlayer::applyControl(float dtime)
{
	// Random constants
	f32 maxspeed = 33.0 * BS;

	setPitch(control.pitch);
	setYaw(control.yaw);
	
	v3f move_direction = v3f(0,0,8);
	move_direction.rotateXZBy(getYaw());

	v3f speed = getSpeed();

	//bool fly_allowed = m_gamedef->checkLocalPrivilege("fly");
	//bool fast_allowed = m_gamedef->checkLocalPrivilege("fast");

	//bool free_move = fly_allowed && g_settings->getBool("free_move");
	//bool fast_move = fast_allowed && g_settings->getBool("fast_move");
	bool continuous_forward = g_settings->getBool("continuous_forward");

	if (!in_water || is_flying)
	{
		speed.X=speed.X*0.85;
		speed.Z=speed.Z*0.85;
	} else {
		speed.X=speed.X*0.5;
		speed.Z=speed.Z*0.5;
	}
	if (speed.X<0 && speed.X+0.1>0) {speed.X=0;}
	if (speed.Z<0 && speed.Z+0.1>0) {speed.Z=0;}

	if (is_flying || is_climbing)
	{
		speed.Y=speed.Y*0.85;
		if (speed.Y<0 && speed.Y+0.1>0) {speed.Y=0;}
	}

	// If free movement and fast movement, always move fast

	if(continuous_forward)
		speed += move_direction;

	if(control.up)
	{
		speed += move_direction;
		if (is_sprinting)
		{
			speed += move_direction*0.5;
		}
	}
	if(control.down)
	{
		speed -= move_direction;
	}
	if(control.left)
	{
		speed += move_direction.crossProduct(v3f(0,1,0));
	}
	if(control.right)
	{
		speed += move_direction.crossProduct(v3f(0,-1,0));
	}

	if(control.jump)
	{
		if(is_flying)
		{
			speed.Y = 10*BS;
			if (is_sprinting)
				speed.Y = 15*BS;
		}
		else
		{
			//NOTE: The d value in move() affects jump height by
			//raising the height at which the jump speed is kept
			//at its starting value
			if(speed.Y >= -0.5*BS)
			if(touching_ground)
			{
				speed.Y = 6.5*BS;
				MtEvent *e = new SimpleTriggerEvent("PlayerJump");
				m_gamedef->event()->put(e);
			}
			// Use the oscillating value for getting out of water
			// (so that the player doesn't fly on the surface)
			if(in_water)
			{
				speed.Y += 0.2*BS;
				if (speed.Y > 0) speed.Y+=0.2*BS;
			}
			if(is_climbing)
			{
				speed.Y = 4*BS;
			}
		}
	}

	// Auxiliary button 1 (E)
	/*if(control.aux1)
	{
		if(is_climbing)
		{
			speed.Y = -3*BS;
		}
		if(fast_move && !free_move)
		{
			speed.X=speed.X*1.11;
			speed.Z=speed.Z*1.11;
		}
	}*/

	if(is_sprinting && is_flying)
	{
		speed.X = speed.X*1.1;
		speed.Z = speed.Z*1.1;
	}

	if((in_water_stable || in_water) && !is_flying)
	{
		speed.Y = speed.Y*0.95-0.1*BS;
	}

	if(control.sneak)
	{
		if(is_flying)
		{
			speed.Y = -10*BS;
			if (is_sprinting)
			{
				speed.Y = -15*BS;
			}
		}
		else if(is_climbing)
		{
			speed.Y = -3*BS;
		}
		else
		{
			speed.X = speed.X / 1.5;
			speed.Z = speed.Z / 1.5;
		}
	}
	

	if(abs(speed.X)>maxspeed) {speed.X=maxspeed*(abs(speed.X)/speed.X);}
	if(abs(speed.Y)>maxspeed) {speed.Y=maxspeed*(abs(speed.Y)/speed.Y);}
	if(abs(speed.Z)>maxspeed) {speed.Z=maxspeed*(abs(speed.Z)/speed.Z);}
	setSpeed(speed);
}

v3s16 LocalPlayer::getStandingNodePos()
{
	if(m_sneak_node_exists)
		return m_sneak_node;
	return floatToInt(getPosition(), BS);
}

