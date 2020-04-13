#include "Pch.h"
#include "Item.h"
#include "Crc.h"
#include "ResourceManager.h"
#include "Net.h"
#include "GameResources.h"
#include "ScriptException.h"
#include "Quest.h"
#include "QuestManager.h"

const Item* Item::gold;
ItemsMap Item::items;
std::map<string, Item*> item_aliases;
vector<ItemList*> ItemList::lists;
vector<Weapon*> Weapon::weapons;
vector<Bow*> Bow::bows;
vector<Shield*> Shield::shields;
vector<Armor*> Armor::armors;
vector<Amulet*> Amulet::amulets;
vector<Ring*> Ring::rings;
vector<Consumable*> Consumable::consumables;
vector<OtherItem*> OtherItem::others;
vector<OtherItem*> OtherItem::artifacts;
vector<BookScheme*> BookScheme::book_schemes;
vector<Book*> Book::books;
vector<StartItem> StartItem::start_items;
std::map<const Item*, Item*> better_items;
std::map<int, Recipe*> Recipe::hash_recipes;

//-----------------------------------------------------------------------------
// adding new types here will require changes in CreatedCharacter::GetStartingItems
WeaponTypeInfo WeaponTypeInfo::info[] = {
	nullptr, 0.3f, 0.7f, 0.4f, 1.1f, 0.002f, SkillId::SHORT_BLADE, 40.f,
	nullptr, 0.75f, 0.25f, 0.33f, 1.f, 0.0016f, SkillId::LONG_BLADE, 50.f,
	nullptr, 0.85f, 0.15f, 0.29f, 0.9f, 0.00125f, SkillId::BLUNT, 65.f,
	nullptr, 0.8f, 0.2f, 0.31f, 0.95f, 0.0014f, SkillId::AXE, 60.f,
};

vector<const Item*> items_to_add;

//=================================================================================================
Item& Item::operator = (const Item& i)
{
	assert(type == i.type);
	mesh = i.mesh;
	tex = i.tex;
	weight = i.weight;
	value = i.value;
	flags = i.flags;
	effects = i.effects;
	switch(type)
	{
	case IT_WEAPON:
		{
			Weapon& w = ToWeapon();
			const Weapon& w2 = i.ToWeapon();
			w.dmg = w2.dmg;
			w.dmg_type = w2.dmg_type;
			w.req_str = w2.req_str;
			w.weapon_type = w2.weapon_type;
			w.material = w2.material;
		}
		break;
	case IT_BOW:
		{
			Bow& b = ToBow();
			const Bow& b2 = i.ToBow();
			b.dmg = b2.dmg;
			b.req_str = b2.req_str;
			b.speed = b2.speed;
		}
		break;
	case IT_SHIELD:
		{
			Shield& s = ToShield();
			const Shield& s2 = i.ToShield();
			s.block = s2.block;
			s.req_str = s2.req_str;
			s.material = s2.material;
		}
		break;
	case IT_ARMOR:
		{
			Armor& a = ToArmor();
			const Armor& a2 = i.ToArmor();
			a.def = a2.def;
			a.req_str = a2.req_str;
			a.mobility = a2.mobility;
			a.material = a2.material;
			a.armor_type = a2.armor_type;
			a.armor_unit_type = a2.armor_unit_type;
			a.tex_override = a2.tex_override;
		}
		break;
	case IT_AMULET:
		{
			Amulet& a = ToAmulet();
			const Amulet& a2 = i.ToAmulet();
			for(int i = 0; i < MAX_ITEM_TAGS; ++i)
				a.tag[i] = a2.tag[i];
		}
		break;
	case IT_RING:
		{
			Ring& r = ToRing();
			const Ring& r2 = i.ToRing();
			for(int i = 0; i < MAX_ITEM_TAGS; ++i)
				r.tag[i] = r2.tag[i];
		}
		break;
	case IT_OTHER:
		{
			OtherItem& o = ToOther();
			const OtherItem& o2 = i.ToOther();
			o.other_type = o2.other_type;
		}
		break;
	case IT_CONSUMABLE:
		{
			Consumable& c = ToConsumable();
			const Consumable& c2 = i.ToConsumable();
			c.time = c2.time;
			c.cons_type = c2.cons_type;
			c.ai_type = c2.ai_type;
		}
		break;
	case IT_BOOK:
		{
			Book& b = ToBook();
			const Book& b2 = i.ToBook();
			b.scheme = b2.scheme;
			b.runic = b2.runic;
		}
		break;
	case IT_GOLD:
		break;
	default:
		assert(0);
		break;
	}
	return *this;
}

//=================================================================================================
const Item* ItemList::Get() const
{
	if(is_priority)
	{
		return RandomItemWeight(items, total,
			[](const ItemList::Entry& e) { return e.chance; },
			[](const ItemList::Entry& e) { return e.item; });
	}
	else
		return items[Rand() % items.size()].item;
}

//=================================================================================================
const Item* ItemList::GetLeveled(int level) const
{
	assert(is_leveled);

	if(level < 1)
		level = 1;
	int best_lvl = -1;

	for(const Entry& e : items)
	{
		if(e.level <= level && e.level >= best_lvl)
		{
			if(e.level > best_lvl)
			{
				items_to_add.clear();
				best_lvl = e.level;
			}
			items_to_add.push_back(e.item);
		}
	}

	if(!items_to_add.empty())
	{
		const Item* best = items_to_add[Rand() % items_to_add.size()];
		items_to_add.clear();
		return best;
	}

	return nullptr;
}

//=================================================================================================
void ItemList::Get(int count, const Item** result) const
{
	assert(count > 0 && result && !is_leveled);
	assert(!is_priority); // TODO

	for(const Entry& e : items)
		items_to_add.push_back(e.item);

	int index = 0;
	for(; index < count && !items_to_add.empty(); ++index)
	{
		int items_index = Rand() % items_to_add.size();
		result[index] = items_to_add[items_index];
		RemoveElementIndex(items_to_add, items_index);
	}

	for(; index < count; ++index)
		result[index] = nullptr;

	items_to_add.clear();
}

//=================================================================================================
const Item* ItemList::GetByIndex(int index) const
{
	if(index < 0 || index >= (int)items.size())
		throw ScriptException("Invalid index.");
	return items[index].item;
}

//=================================================================================================
ItemList* ItemList::TryGet(Cstring id)
{
	for(ItemList* lis : lists)
	{
		if(lis->id == id)
			return lis;
	}
	return nullptr;
}

//=================================================================================================
bool ItemCmp(const Item* a, const Item* b)
{
	assert(a && b);
	if(a->type == b->type)
	{
		if(a->type == IT_WEAPON)
		{
			WEAPON_TYPE w1 = a->ToWeapon().weapon_type,
				w2 = b->ToWeapon().weapon_type;
			if(w1 != w2)
				return w1 < w2;
		}
		else if(a->type == IT_ARMOR)
		{
			ArmorUnitType aut1 = a->ToArmor().armor_unit_type,
				aut2 = b->ToArmor().armor_unit_type;
			if(aut1 != aut2)
				return aut1 < aut2;
			ARMOR_TYPE at1 = a->ToArmor().armor_type,
				at2 = b->ToArmor().armor_type;
			if(at1 != at2)
				return at1 < at2;
		}
		else if(a->type == IT_CONSUMABLE)
		{
			ConsumableType c1 = a->ToConsumable().cons_type,
				c2 = b->ToConsumable().cons_type;
			if(c1 != c2)
				return c1 < c2;
		}
		else if(a->type == IT_OTHER)
		{
			OtherType o1 = a->ToOther().other_type,
				o2 = b->ToOther().other_type;
			if(o1 != o2)
				return o1 < o2;
		}
		if(a->value != b->value)
			return a->value > b->value;
		else
			return strcoll(a->name.c_str(), b->name.c_str()) < 0;
	}
	else
		return a->type < b->type;
}

//=================================================================================================
float Item::GetEffectPower(EffectId effect) const
{
	float power = 0.f;
	for(const ItemEffect& e : effects)
	{
		if(e.effect == effect)
			power += e.power;
	}
	return power;
}

//=================================================================================================
void Item::CreateCopy(Item& item) const
{
	game_res->PreloadItem(this);

	switch(type)
	{
	case IT_OTHER:
		{
			Item& i = item;
			const Item& i2 = *this;
			OtherItem& o = i.ToOther();
			const OtherItem& o2 = i2.ToOther();
			
			i.mesh = i2.mesh;
			i.tex = i2.tex;
			i.desc = i2.desc;
			i.flags = i2.flags;
			i.id = i2.id;
			i.name = i2.name;
			o.other_type = o2.other_type;
			i.quest_id = i2.quest_id;
			i.type = i2.type;
			i.value = i2.value;
			i.weight = i2.weight;
			i.icon = i2.icon;
			i.state = ResourceState::NotLoaded;
		}
		break;
	default:
		assert(0); // not implemented
		break;
	}

	if(Net::IsServer() || net->mp_load)
	{
		NetChange& c = Add1(Net::changes);
		c.type = NetChange::REGISTER_ITEM;
		c.item2 = &item;
		c.base_item = this;
	}
}

//=================================================================================================
Item* Item::CreateCopy() const
{
	switch(type)
	{
	case IT_OTHER:
		{
			Item* o = new Item(IT_OTHER);
			o->SetProperty<OtherItem>(new OtherItem);
			CreateCopy(*o);
			return o;
		}
		break;
	case IT_WEAPON:
	case IT_BOW:
	case IT_SHIELD:
	case IT_ARMOR:
	case IT_CONSUMABLE:
	case IT_GOLD:
	case IT_BOOK:
	default:
		// not implemented yet, YAGNI!
		assert(0);
		return nullptr;
	}
}

//=================================================================================================
Item* Item::QuestCopy(Quest* quest, const string& name)
{
	Item* item = CreateCopy();
	item->id = Format("$%s", id.c_str());
	item->name = name;
	item->quest_id = quest->id;
	quest_mgr->AddQuestItem(item);
	return item;
}

//=================================================================================================
void Item::Rename(cstring name)
{
	assert(name);
	this->name = name;
	if(Net::IsOnline())
	{
		NetChange& c = Add1(Net::changes);
		c.type = NetChange::RENAME_ITEM;
		c.base_item = this;
	}
}

//=================================================================================================
void Item::Validate(uint& err)
{
	for(auto it : Item::items)
	{
		const Item& item = *it.second;

		if(item.name.empty())
		{
			++err;
			Warn("Test: Missing item '%s' name.", item.id.c_str());
		}

		if(item.type == IT_BOOK && item.ToBook().text.empty())
		{
			++err;
			Warn("Test: Missing book '%s' text.", item.id.c_str());
		}
	}
}

//=================================================================================================
const Item* StartItem::GetStartItem(SkillId skill, int value, bool mage)
{
	auto it = std::lower_bound(StartItem::start_items.begin(), StartItem::start_items.end(), StartItem(skill),
		[](const StartItem& si1, const StartItem& si2) { return si1.skill > si2.skill; });
	if(it == StartItem::start_items.end())
		return nullptr;
	const Item* best = nullptr;
	int best_value = -2;
	if(mage)
	{
		auto start_it = it;
		while(true)
		{
			if(it->mage)
			{
				if(it->value == HEIRLOOM)
				{
					if(value == HEIRLOOM)
						return it->item;
				}
				else if(it->value > best_value && it->value <= value)
				{
					best = it->item;
					best_value = it->value;
				}
			}
			++it;
			if(it == StartItem::start_items.end() || it->skill != skill)
				break;
		}
		if(best)
			return best;
		it = start_it;
	}
	while(true)
	{
		if(!it->mage)
		{
			if(it->value == HEIRLOOM)
			{
				if(value == HEIRLOOM)
					return it->item;
			}
			else if(it->value > best_value && it->value <= value)
			{
				best = it->item;
				best_value = it->value;
			}
		}
		++it;
		if(it == StartItem::start_items.end() || it->skill != skill)
			break;
	}
	return best;
}

//=================================================================================================
Item* Item::TryGet(Cstring id)
{
	// search item
	auto it = items.find(id);
	if(it != Item::items.end())
		return it->second;

	// search item by old id
	auto it2 = item_aliases.find(id.s);
	if(it2 != item_aliases.end())
		return it2->second;

	return nullptr;
}

//=================================================================================================
BookScheme* BookScheme::TryGet(Cstring id)
{
	for(auto scheme : book_schemes)
	{
		if(scheme->id == id)
			return scheme;
	}

	return nullptr;
}

//=================================================================================================
const Item* Book::GetRandom()
{
	if(Rand() % 2 == 0)
		return nullptr;
	if(Rand() % 50 == 0)
		return ItemList::GetItem("rare_books");
	else
		return ItemList::GetItem("books");
}

//=================================================================================================
const Item* FindItemOrList(Cstring id, ItemList*& lis)
{
	auto item = Item::TryGet(id);
	if(item)
	{
		lis = nullptr;
		return item;
	}

	lis = ItemList::TryGet(id);
	return nullptr;
}

//=================================================================================================
Recipe* Recipe::Get(int hash)
{
	auto it = hash_recipes.find(hash);
	if(it != hash_recipes.end())
		return it->second;
	return nullptr;
}

Recipe* Recipe::GetS(const string& id)
{
	Recipe* recipe = TryGet(id);
	if (!recipe)
		throw ScriptException("Invalid recipe '%s'.", id);
	return recipe;
}