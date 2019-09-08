#pragma once

//-----------------------------------------------------------------------------
#include "Event.h"

//-----------------------------------------------------------------------------
// Location types
//-----------------------------------------------------------------------------
// Changes required after adding new location:
// + World
// + LocationGeneratorFactory
// + Location::GenerateName
// + WorldMapGui::LoadData
// + LevelArea
enum LOCATION
{
	L_CITY, // miasto otoczone kamiennym murem i wie�yczki, przez �rodek biegnie kamienna droga
	L_VILLAGE_OLD, // removed (left for backward compability)
	L_CAVE, // jaskinia, jakie� zwierz�ta/potwory/ewentualnie bandyci
	L_CAMP, // ob�z bandyt�w/poszukiwaczy przyg�d/wojska (tymczasowa lokacja)
	L_DUNGEON, // podziemia, r�nej g��boko�ci, posiadaj� pomieszczenia o okre�lonym celu (skarbiec, sypialnie itp), zazwyczaj bandyci lub opuszczone
	L_CRYPT, // krypta, ma dwa cele: skarbiec lub wi�zienie dla nieumar�ych, zazwyczaj s� tu nieumarli, czasami przy wej�ciu bandyci lub potwory
	L_FOREST, // las, zazwyczaj pusty, czasem potwory lub bandyci maj� tu ma�y ob�z
	L_MOONWELL, // jak las ale ze wzg�rzem na �rodku i fontann�
	L_ENCOUNTER, // losowe spotkanie na drodze
	L_MAX,
	L_NULL
};

//-----------------------------------------------------------------------------
enum LOCATION_IMAGE
{
	LI_CITY,
	LI_VILLAGE,
	LI_CAVE,
	LI_CAMP,
	LI_DUNGEON,
	LI_CRYPT,
	LI_FOREST,
	LI_MOONWELL,
	LI_TOWER,
	LI_LABYRINTH,
	LI_MINE,
	LI_SAWMILL,
	LI_DUNGEON2,
	LI_ACADEMY,
	LI_MAX
};

//-----------------------------------------------------------------------------
enum LOCATION_TOKEN : byte
{
	LT_NULL,
	LT_OUTSIDE,
	LT_CITY,
	LT_VILLAGE_OLD,
	LT_CAVE,
	LT_SINGLE_DUNGEON,
	LT_MULTI_DUNGEON,
	LT_CAMP
};

//-----------------------------------------------------------------------------
// stan lokacji
enum LOCATION_STATE
{
	LS_HIDDEN,
	LS_UNKNOWN,
	LS_KNOWN,
	LS_VISITED,
	LS_ENTERED,
	LS_CLEARED
};

//-----------------------------------------------------------------------------
// przypisanie takiego quest do lokacji spowoduje �e nie zostanie zaj�ta przez inny quest
#define ACTIVE_QUEST_HOLDER 0xFFFFFFFE

//-----------------------------------------------------------------------------
// struktura opisuj�ca lokacje na mapie �wiata
struct Location
{
	int index;
	LOCATION type; // typ lokacji
	LOCATION_STATE state; // stan lokacji
	Vec2 pos; // pozycja na mapie �wiata
	string name; // nazwa lokacji
	Quest_Dungeon* active_quest; // aktywne zadanie zwi�zane z t� lokacj�
	int last_visit; // worldtime from last time when team entered location or -1
	int st; // poziom trudno�ci
	uint seed;
	UnitGroup* group; // cannot be null (use UnitGroup::empty)
	Portal* portal;
	LOCATION_IMAGE image;
	vector<Event> events;
	bool reset; // resetowanie lokacji po wej�ciu
	bool outside; // czy poziom jest otwarty
	bool dont_clean;
	bool loaded_resources;

	Location(bool outside) : active_quest(nullptr), last_visit(-1), reset(false), state(LS_UNKNOWN), outside(outside), st(0), group(nullptr),
		portal(nullptr), dont_clean(false), loaded_resources(false)
	{
	}

	virtual ~Location();

	// virtual functions to implement
	virtual void Apply(vector<std::reference_wrapper<LevelArea>>& areas) = 0;
	virtual void Save(GameWriter& f, bool local);
	virtual void Load(GameReader& f, bool local, LOCATION_TOKEN token);
	virtual void Write(BitStreamWriter& f) = 0;
	virtual bool Read(BitStreamReader& f) = 0;
	virtual bool FindUnit(Unit* unit, int* level = nullptr) = 0;
	virtual Unit* FindUnit(UnitData* data, int& at_level) = 0;
	virtual bool CheckUpdate(int& days_passed, int worldtime);
	virtual LOCATION_TOKEN GetToken() const { return LT_NULL; }
	virtual int GetRandomLevel() const { return -1; }
	virtual int GetLastLevel() const { return 0; }
	virtual bool RequireLoadingResources(bool* to_set);

	Portal* GetPortal(int index);
	Portal* TryGetPortal(int index) const;
	void WritePortals(BitStreamWriter& f) const;
	bool ReadPortals(BitStreamReader& f, int at_level);
	LOCATION_IMAGE GetImage() const { return image; }
	const string& GetName() const { return name; }
	void SetVisited()
	{
		if(state == LS_KNOWN)
			state = LS_VISITED;
	}
	void SetKnown();
	void SetImage(LOCATION_IMAGE image);
	void SetName(cstring name);
	void SetNameS(const string& name) { SetName(name.c_str()); }
	void SetNamePrefix(cstring prefix);
	void AddEventHandler(Quest_Scripted* quest, EventType type);
	void RemoveEventHandler(Quest_Scripted* quest, bool cleanup);
	void RemoveEventHandlerS(Quest_Scripted* quest) { RemoveEventHandler(quest, false); }
};

//-----------------------------------------------------------------------------
struct LocationEventHandler
{
	enum Event
	{
		CLEARED
	};

	// return true to prevent adding default news about clearing location
	virtual bool HandleLocationEvent(Event event) = 0;
	virtual int GetLocationEventHandlerQuestRefid() = 0;
};
