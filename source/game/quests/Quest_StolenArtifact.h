#pragma once

//-----------------------------------------------------------------------------
#include "Quest.h"

//-----------------------------------------------------------------------------
class Quest_StolenArtifact : public Quest_Dungeon
{
public:
	enum Progress
	{
		None,
		Started,
		Finished,
		Timeout
	};

	void Start();
	DialogEntry* GetDialog(int type2);
	void SetProgress(int prog2);
	cstring FormatString(const string& str);
	bool IsTimedout() const;
	bool IfHaveQuestItem2(cstring id) const;
	const Item* GetQuestItem();
	void Save(HANDLE file);
	void Load(HANDLE file);

private:
	const Item* item;
	OtherItem quest_item;
	SPAWN_GROUP group;
};
