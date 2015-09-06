// dane poziomu lokacji
#include "Pch.h"
#include "Base.h"
#include "InsideLocationLevel.h"
#include "Game.h"
#include "SaveState.h"

//=================================================================================================
InsideLocationLevel::~InsideLocationLevel()
{
	delete[] map;
	DeleteElements(units);
	DeleteElements(chests);
	DeleteElements(doors);
	DeleteElements(useables);
	DeleteElements(items);
	DeleteElements(traps);
}

//=================================================================================================
Room* InsideLocationLevel::GetNearestRoom(const VEC3& pos)
{
	if(rooms.empty())
		return NULL;

	float dist, best_dist = 1000.f;
	Room* best_room = NULL;

	for(vector<Room>::iterator it = rooms.begin(), end = rooms.end(); it != end; ++it)
	{
		dist = it->Distance(pos);
		if(dist < best_dist)
		{
			if(dist == 0.f)
				return &*it;
			best_dist = dist;
			best_room = &*it;
		}
	}

	return best_room;
}

//=================================================================================================
Room* InsideLocationLevel::FindEscapeRoom(const VEC3& _my_pos, const VEC3& _enemy_pos)
{
	Room* my_room = GetNearestRoom(_my_pos),
		* enemy_room = GetNearestRoom(_enemy_pos);

	if(!my_room)
		return NULL;

	int id;
	if(enemy_room)
		id = GetRoomId(enemy_room);
	else
		id = -1;

	Room* best_room = NULL;
	float best_dist = 0.f, dist;
	VEC3 mid;

	for(vector<int>::iterator it = my_room->connected.begin(), end = my_room->connected.end(); it != end; ++it)
	{
		if(*it == id)
			continue;

		mid = rooms[*it].Center();

		dist = distance(_my_pos, mid) - distance(_enemy_pos, mid);
		if(dist < best_dist)
		{
			best_dist = dist;
			best_room = &rooms[*it];
		}
	}

	return best_room;
}

//=================================================================================================
Room* InsideLocationLevel::GetRoom(const INT2& pt)
{
	word room = map[pt(w)].room;
	if(room == (word)-1)
		return NULL;
	return &rooms[room];
}

//=================================================================================================
bool InsideLocationLevel::GetRandomNearWallTile(const Room& room, INT2& _tile, int& _rot, bool nocol)
{
	_rot = rand2()%4;

	int tries = 0;

	do
	{
		int tries2 = 10;

		switch(_rot)
		{
		case 2:
			// g�rna �ciana, obj \/
			do 
			{
				_tile.x = random(room.pos.x+1, room.pos.x+room.size.x-2);
				_tile.y = room.pos.y + 1;

				if(czy_blokuje2(map[_tile.x+(_tile.y-1)*w]) && !czy_blokuje21(map[_tile.x+_tile.y*w]) && (nocol || !czy_blokuje21(map[_tile.x+(_tile.y+1)*w])))
					return true;

				--tries2;
			}
			while(tries2 > 0);
			break;
		case 1:
			// prawa �ciana, obj <
			do 
			{
				_tile.x = room.pos.x + room.size.x - 2;
				_tile.y = random(room.pos.y + 1, room.pos.y + room.size.y - 2);

				if(czy_blokuje2(map[_tile.x+1+_tile.y*w]) && !czy_blokuje21(map[_tile.x+_tile.y*w]) && (nocol || !czy_blokuje21(map[_tile.x-1+_tile.y*w])))
					return true;

				--tries2;
			}
			while(tries2 > 0);
			break;
		case 0:
			// dolna �ciana, obj /|
			do 
			{
				_tile.x = random(room.pos.x + 1, room.pos.x + room.size.x - 2);
				_tile.y = room.pos.y + room.size.y - 2;

				if(czy_blokuje2(map[_tile.x+(_tile.y+1)*w]) && !czy_blokuje21(map[_tile.x+_tile.y*w]) && (nocol || !czy_blokuje21(map[_tile.x+(_tile.y-1)*w])))
					return true;

				--tries2;
			}
			while(tries2 > 0);
			break;
		case 3:
			// lewa �ciana, obj >
			do 
			{
				_tile.x = room.pos.x + 1;
				_tile.y = random(room.pos.y + 1, room.pos.y + room.size.y - 2);

				if(czy_blokuje2(map[_tile.x-1+_tile.y*w]) && !czy_blokuje21(map[_tile.x+_tile.y*w]) && (nocol || !czy_blokuje21(map[_tile.x+1+_tile.y*w])))
					return true;

				--tries2;
			}
			while(tries2 > 0);
			break;
		}

		++tries;
		_rot = (_rot+1)%4;
	}
	while(tries <= 3);

	return false;
}

//=================================================================================================
void InsideLocationLevel::SaveLevel(HANDLE file, bool local)
{
	WriteFile(file, &w, sizeof(w), &tmp, NULL);
	WriteFile(file, &h, sizeof(h), &tmp, NULL);
	WriteFile(file, map, sizeof(Pole)*w*h, &tmp, NULL);

	uint ile;

	// jednostki
	ile = units.size();
	WriteFile(file, &ile, sizeof(ile), &tmp, NULL);
	for(vector<Unit*>::iterator it = units.begin(), end = units.end(); it != end; ++it)
		(*it)->Save(file, local);

	// skrzynie
	ile = chests.size();
	WriteFile(file, &ile, sizeof(ile), &tmp, NULL);
	for(vector<Chest*>::iterator it = chests.begin(), end = chests.end(); it != end; ++it)
		(*it)->Save(file, local);

	// obiekty
	ile = objects.size();
	WriteFile(file, &ile, sizeof(ile), &tmp, NULL);
	for(vector<Object>::iterator it = objects.begin(), end = objects.end(); it != end; ++it)
		it->Save(file);

	// drzwi
	ile = doors.size();
	WriteFile(file, &ile, sizeof(ile), &tmp, NULL);
	for(vector<Door*>::iterator it = doors.begin(), end = doors.end(); it != end; ++it)
		(*it)->Save(file, local);

	// przedmioty
	ile = items.size();
	WriteFile(file, &ile, sizeof(ile), &tmp, NULL);
	for(vector<GroundItem*>::iterator it = items.begin(), end = items.end(); it != end; ++it)
		(*it)->Save(file);

	// u�ywalne
	ile = useables.size();
	WriteFile(file, &ile, sizeof(ile), &tmp, NULL);
	for(vector<Useable*>::iterator it = useables.begin(), end = useables.end(); it != end; ++it)
		(*it)->Save(file, local);

	// krew
	File f(file);
	ile = bloods.size();
	WriteFile(file, &ile, sizeof(ile), &tmp, NULL);
	for(vector<Blood>::iterator it = bloods.begin(), end = bloods.end(); it != end; ++it)
		it->Save(f);

	// �wiat�a
	f << lights.size();
	for(vector<Light>::iterator it = lights.begin(), end = lights.end(); it != end; ++it)
		it->Save(f);

	// pokoje
	ile = rooms.size();
	WriteFile(file, &ile, sizeof(ile), &tmp, NULL);
	for(vector<Room>::iterator it = rooms.begin(), end = rooms.end(); it != end; ++it)
		it->Save(file);

	// pu�apki
	ile = traps.size();
	WriteFile(file, &ile, sizeof(ile), &tmp, NULL);
	for(vector<Trap*>::iterator it = traps.begin(), end = traps.end(); it != end; ++it)
		(*it)->Save(file, local);

	WriteFile(file, &staircase_up, sizeof(staircase_up), &tmp, NULL);
	WriteFile(file, &staircase_down, sizeof(staircase_down), &tmp, NULL);
	WriteFile(file, &staircase_up_dir, sizeof(staircase_up_dir), &tmp, NULL);
	WriteFile(file, &staircase_down_dir, sizeof(staircase_down_dir), &tmp, NULL);
	WriteFile(file, &staircase_down_in_wall, sizeof(staircase_down_in_wall), &tmp, NULL);
}

//=================================================================================================
void InsideLocationLevel::LoadLevel(HANDLE file, bool local)
{
	ReadFile(file, &w, sizeof(w), &tmp, NULL);
	ReadFile(file, &h, sizeof(h), &tmp, NULL);
	map = new Pole[w*h];
	ReadFile(file, map, sizeof(Pole)*w*h, &tmp, NULL);

	if(LOAD_VERSION == V_0_2)
	{
		for(int i=0; i<w*h; ++i)
		{
			if(map[i].type >= KRATKA_PODLOGA)
				map[i].type = (POLE)(map[i].type+1);
		}
	}

	// jednostki
	uint ile;
	ReadFile(file, &ile, sizeof(ile), &tmp, NULL);
	units.resize(ile);
	for(vector<Unit*>::iterator it = units.begin(), end = units.end(); it != end; ++it)
	{
		*it = new Unit;
		Unit::AddRefid(*it);
		(*it)->Load(file, local);
	}

	// skrzynie
	ReadFile(file, &ile, sizeof(ile), &tmp, NULL);
	chests.resize(ile);
	for(vector<Chest*>::iterator it = chests.begin(), end = chests.end(); it != end; ++it)
	{
		*it = new Chest;
		(*it)->Load(file, local);
	}

	// obiekty
	ReadFile(file, &ile, sizeof(ile), &tmp, NULL);
	objects.resize(ile);
	int index = 0;
	static vector<int> objs_need_update;
	for(vector<Object>::iterator it = objects.begin(), end = objects.end(); it != end; ++it, ++index)
	{
		if(!it->Load(file))
			objs_need_update.push_back(index);
	}

	// drzwi
	ReadFile(file, &ile, sizeof(ile), &tmp, NULL);
	doors.resize(ile);
	for(vector<Door*>::iterator it = doors.begin(), end = doors.end(); it != end; ++it)
	{
		*it = new Door;
		(*it)->Load(file, local);
	}

	// przedmioty
	ReadFile(file, &ile, sizeof(ile), &tmp, NULL);
	items.resize(ile);
	for(vector<GroundItem*>::iterator it = items.begin(), end = items.end(); it != end; ++it)
	{
		*it = new GroundItem;
		(*it)->Load(file);
	}

	// u�ywalne
	ReadFile(file, &ile, sizeof(ile), &tmp, NULL);
	useables.resize(ile);
	for(vector<Useable*>::iterator it = useables.begin(), end = useables.end(); it != end; ++it)
	{
		*it = new Useable;
		Useable::AddRefid(*it);
		(*it)->Load(file, local);
	}

	// krew
	File f(file);
	ReadFile(file, &ile, sizeof(ile), &tmp, NULL);
	bloods.resize(ile);
	for(vector<Blood>::iterator it = bloods.begin(), end = bloods.end(); it != end; ++it)
		it->Load(f);

	// �wiat�a
	f >> ile;
	lights.resize(ile);
	for(vector<Light>::iterator it = lights.begin(), end = lights.end(); it != end; ++it)
		it->Load(f);

	// pokoje
	ReadFile(file, &ile, sizeof(ile), &tmp, NULL);
	rooms.resize(ile);
	for(vector<Room>::iterator it = rooms.begin(), end = rooms.end(); it != end; ++it)
		it->Load(file);

	// pu�apki
	ReadFile(file, &ile, sizeof(ile), &tmp, NULL);
	traps.resize(ile);
	for(vector<Trap*>::iterator it = traps.begin(), end = traps.end(); it != end; ++it)
	{
		*it = new Trap;
		(*it)->Load(file, local);
	}

	ReadFile(file, &staircase_up, sizeof(staircase_up), &tmp, NULL);
	ReadFile(file, &staircase_down, sizeof(staircase_down), &tmp, NULL);
	ReadFile(file, &staircase_up_dir, sizeof(staircase_up_dir), &tmp, NULL);
	ReadFile(file, &staircase_down_dir, sizeof(staircase_down_dir), &tmp, NULL);
	ReadFile(file, &staircase_down_in_wall, sizeof(staircase_down_in_wall), &tmp, NULL);

	// aktualizuj obiekty
	if(!objs_need_update.empty())
	{
		for(vector<int>::reverse_iterator it = objs_need_update.rbegin(), end = objs_need_update.rend(); it != end; ++it)
		{
			Object& o = objects[*it];
			Useable* u = new Useable;
			u->pos = o.pos;
			u->rot = o.rot.y;
			u->user = NULL;
			u->netid = Game::Get().useable_netid_counter++;
			if(IS_SET(o.base->flagi, OBJ_ZYLA_ZELAZA))
				u->type = U_ZYLA_ZELAZA;
			else
				u->type = U_ZYLA_ZLOTA;
			useables.push_back(u);

			objects.erase(objects.begin()+*it);
		}

		objs_need_update.clear();
	}

	// konwersja krzese� w sto�ki
	if(LOAD_VERSION < V_0_2_12)
	{
		for(vector<Useable*>::iterator it = useables.begin(), end = useables.end(); it != end; ++it)
		{
			Useable& u = **it;
			if(u.type == U_KRZESLO)
				u.type = U_STOLEK;
		}
	}

	// konwersja �awy w obr�con� �aw� i ustawienie wariantu
	if(LOAD_VERSION < V_0_2_20)
	{
		for(vector<Useable*>::iterator it = useables.begin(), end = useables.end(); it != end; ++it)
		{
			Useable& u = **it;
			if(u.type == U_LAWA)
			{
				u.type = U_LAWA_DIR;
				u.variant = rand2()%2;
			}
		}
	}
}

//=================================================================================================
Room& InsideLocationLevel::GetFarRoom(bool have_down_stairs)
{
	if(have_down_stairs)
	{
		Room* gora = GetNearestRoom(VEC3(2.f*staircase_up.x + 1, 0, 2.f*staircase_up.y + 1));
		Room* dol = GetNearestRoom(VEC3(2.f*staircase_down.x + 1, 0, 2.f*staircase_down.y + 1));
		int best_dist, dist;
		Room* best = NULL;

		for(vector<Room>::iterator it = rooms.begin(), end = rooms.end(); it != end; ++it)
		{
			if(it->corridor)
				continue;
			dist = distance(it->pos, gora->pos) + distance(it->pos, dol->pos);
			if(!best || dist > best_dist)
			{
				best_dist = dist;
				best = &*it;
			}
		}

		return *best;
	}
	else
	{
		Room* gora = GetNearestRoom(VEC3(2.f*staircase_up.x + 1, 0, 2.f*staircase_up.y + 1));
		int best_dist, dist;
		Room* best = NULL;

		for(vector<Room>::iterator it = rooms.begin(), end = rooms.end(); it != end; ++it)
		{
			if(it->corridor)
				continue;
			dist = distance(it->pos, gora->pos);
			if(!best || dist > best_dist)
			{
				best_dist = dist;
				best = &*it;
			}
		}

		return *best;
	}
}

//=================================================================================================
void InsideLocationLevel::BuildRefidTable()
{
	for(vector<Unit*>::iterator it = units.begin(), end = units.end(); it != end; ++it)
	{
		(*it)->refid = (int)Unit::refid_table.size();
		Unit::refid_table.push_back(*it);
	}

	for(vector<Useable*>::iterator it = useables.begin(), end = useables.end(); it != end; ++it)
	{
		(*it)->refid = (int)Useable::refid_table.size();
		Useable::refid_table.push_back(*it);
	}
}

//=================================================================================================
int InsideLocationLevel::FindRoomId(int target)
{
	int index = 0;
	for(vector<Room>::iterator it = rooms.begin(), end = rooms.end(); it != end; ++it, ++index)
	{
		if(it->target == target)
			return index;
	}

	return -1;
}

//=================================================================================================
bool InsideLocationLevel::IsTileNearWall(const INT2& pt) const
{
	assert(pt.x > 0 && pt.y > 0 && pt.x < w-1 && pt.y < h-1);

	return map[pt.x-1+pt.y*w].IsWall() ||
		   map[pt.x+1+pt.y*w].IsWall() ||
		   map[pt.x+(pt.y-1)*w].IsWall() ||
		   map[pt.x+(pt.y+1)*w].IsWall();
}

//=================================================================================================
bool InsideLocationLevel::IsTileNearWall(const INT2& pt, int& dir) const
{
	assert(pt.x > 0 && pt.y > 0 && pt.x < w-1 && pt.y < h-1);

	int kierunek = 0;

	if(map[pt.x-1+pt.y*w].IsWall())
		kierunek |= (1<<GDIR_LEFT);
	if(map[pt.x+1+pt.y*w].IsWall())
		kierunek |= (1<<GDIR_RIGHT);
	if(map[pt.x+(pt.y-1)*w].IsWall())
		kierunek |= (1<<GDIR_DOWN);
	if(map[pt.x+(pt.y+1)*w].IsWall())
		kierunek |= (1<<GDIR_UP);

	if(kierunek == 0)
		return false;

	int i = rand2()%4;
	while(true)
	{
		if(IS_SET(kierunek, 1<<i))
		{
			dir = i;
			return true;
		}
		i = (++i)%4;
	}

	return true;
}

//=================================================================================================
void InsideLocationLevel::RemoveUnit(Unit* unit)
{
	assert(unit);

	for(vector<Unit*>::iterator it = units.begin(), end = units.end(); it != end; ++it)
	{
		if(*it == unit)
		{
			units.erase(it);
			delete unit;
			return;
		}
	}
}
