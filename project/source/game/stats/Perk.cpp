#include "Pch.h"
#include "Core.h"
#include "Perk.h"
#include "Attribute.h"
#include "Skill.h"
#include "Language.h"
#include "PlayerController.h"
#include "CreatedCharacter.h"
#include "Unit.h"
#include "Net.h"
#include "PlayerInfo.h"
#include "Gui.h"
#include "Game.h"

//-----------------------------------------------------------------------------
PerkInfo PerkInfo::perks[(int)Perk::Max] = {
	PerkInfo(Perk::BadBack, "bad_back", PerkInfo::Flaw | PerkInfo::History),
	PerkInfo(Perk::ChronicDisease, "chronic_disease", PerkInfo::Flaw | PerkInfo::History),
	PerkInfo(Perk::Sluggish, "sluggish", PerkInfo::Flaw | PerkInfo::History),
	PerkInfo(Perk::SlowLearner, "slow_learner", PerkInfo::Flaw | PerkInfo::History),
	PerkInfo(Perk::Asocial, "asocial", PerkInfo::Flaw | PerkInfo::History),
	PerkInfo(Perk::Poor, "poor", PerkInfo::Flaw | PerkInfo::History),
	PerkInfo(Perk::Talent, "talent", PerkInfo::History | PerkInfo::RequireFormat, Perk::None, PerkInfo::Attribute),
	PerkInfo(Perk::Skilled, "skilled", PerkInfo::History),
	PerkInfo(Perk::SkillFocus, "skill_focus", PerkInfo::History | PerkInfo::RequireFormat, Perk::None, PerkInfo::Skill),
	PerkInfo(Perk::AlchemistApprentice, "alchemist", PerkInfo::History),
	PerkInfo(Perk::Wealthy, "wealthy", PerkInfo::History),
	PerkInfo(Perk::VeryWealthy, "very_wealthy", PerkInfo::History, Perk::Wealthy),
	PerkInfo(Perk::FilthyRich, "filthy_rich", PerkInfo::History, Perk::VeryWealthy),
	PerkInfo(Perk::FamilyHeirloom, "heirloom", PerkInfo::History),
	PerkInfo(Perk::Leader, "leader", PerkInfo::History),
	PerkInfo(Perk::MilitaryTraining, "military_training", PerkInfo::History),
	PerkInfo(Perk::StrongBack, "strong_back", 0),
	PerkInfo(Perk::StrongerBack, "stronger_back", 0, Perk::StrongBack),
	PerkInfo(Perk::Aggressive, "aggressive", 0),
	PerkInfo(Perk::VeryAggressive, "very_aggressive", 0, Perk::Aggressive),
	PerkInfo(Perk::Berserker, "berserker", 0, Perk::VeryAggressive),
	PerkInfo(Perk::HeavyHitter, "heavy_hitter", 0),
	PerkInfo(Perk::Careful, "careful", 0),
	PerkInfo(Perk::Mobility, "mobility", 0),
	PerkInfo(Perk::EnhancedMobility, "enhanced_mobility", 0, Perk::Mobility),
	PerkInfo(Perk::Tought, "tought", 0),
	PerkInfo(Perk::Toughter, "toughter", 0, Perk::Tought),
	PerkInfo(Perk::Toughtest, "toughtest", 0, Perk::Toughter),
	PerkInfo(Perk::HardSkin, "hard_skin", 0),
	PerkInfo(Perk::IronSkin, "iron_skin", 0, Perk::HardSkin),
	PerkInfo(Perk::DiamondSkin, "diamond_skin", 0, Perk::IronSkin),
	PerkInfo(Perk::ExtraordinaryHealth, "extraordinary_health", 0),
	PerkInfo(Perk::PerfectHealth, "perfect_health", 0, Perk::ExtraordinaryHealth),
	PerkInfo(Perk::Energetic, "energetic", 0),
	PerkInfo(Perk::VeryEnergetic, "very_energetic", Perk::Energetic),
	PerkInfo(Perk::Adaptation, "adaptation", 0),
	PerkInfo(Perk::HealthyDiet, "healthy_diet", 0),
	PerkInfo(Perk::Strongman, "strongman", 0),
	PerkInfo(Perk::BodyBuilder, "body_builder", 0),
	PerkInfo(Perk::MiracleDiet, "miracle_diet", 0, Perk::HealthyDiet),
	PerkInfo(Perk::Flexible, "flexible", 0),
	PerkInfo(Perk::DecryptingRunes, "decrypting_runes", 0)
};

//-----------------------------------------------------------------------------
cstring TakenPerk::txIncreasedAttrib, TakenPerk::txIncreasedSkill, TakenPerk::txDecreasedAttrib, TakenPerk::txDecreasedSkill, TakenPerk::txDecreasedSkills,
TakenPerk::txPerksRemoved;

//=================================================================================================
PerkInfo* PerkInfo::TryGet(const AnyString& id)
{
	for(PerkInfo& pi : PerkInfo::perks)
	{
		if(id == pi.id)
			return &pi;
	}

	return nullptr;
}

//=================================================================================================
void PerkInfo::Validate(uint& err)
{
	int index = 0;
	for(PerkInfo& pi : PerkInfo::perks)
	{
		if(pi.perk_id != (Perk)index)
		{
			Warn("Test: Perk %s: id mismatch.", pi.id);
			++err;
		}
		if(pi.name.empty())
		{
			Warn("Test: Perk %s: empty name.", pi.id);
			++err;
		}
		if(pi.desc.empty())
		{
			Warn("Test: Perk %s: empty desc.", pi.id);
			++err;
		}
		++index;
	}
}

//=================================================================================================
void TakenPerk::LoadText()
{
	txIncreasedAttrib = Str("increasedAttrib");
	txIncreasedSkill = Str("increasedSkill");
	txDecreasedAttrib = Str("decreasedAttrib");
	txDecreasedSkill = Str("decreasedSkill");
	txDecreasedSkills = Str("decreasedSkills");
	txPerksRemoved = Str("perksRemoved");
}

//=================================================================================================
void TakenPerk::GetDesc(string& s) const
{
	switch(perk)
	{
	case Perk::Talent:
		s = Format("%s: %s", txIncreasedAttrib, AttributeInfo::attributes[value].name.c_str());
		break;
	case Perk::SkillFocus:
		s = Format("%s: %s", txIncreasedSkill, SkillInfo::skills[value].name.c_str());
		break;
	default:
		s.clear();
		break;
	}
}

//=================================================================================================
cstring TakenPerk::FormatName()
{
	PerkInfo& p = PerkInfo::perks[(int)perk];

	switch(perk)
	{
	case Perk::Talent:
		return Format("%s (%s)", p.name.c_str(), AttributeInfo::attributes[value].name.c_str());
	case Perk::SkillFocus:
		return Format("%s (%s)", p.name.c_str(), SkillInfo::skills[value].name.c_str());
	default:
		assert(0);
		return p.name.c_str();
	}
}

//=================================================================================================
// Check value of perk sent in mp
bool TakenPerk::Validate()
{
	auto& info = PerkInfo::perks[(int)perk];
	switch(info.required_value)
	{
	case PerkInfo::Attribute:
		if(value < 0 || value >= (int)Attribute::MAX)
		{
			Error("Perk '%s', invalid attribute %d.", info.id, value);
			return false;
		}
		break;
	case PerkInfo::Skill:
		if(value < 0 || value >= (int)Skill::MAX)
		{
			Error("Perk '%s', invalid skill %d.", info.id, value);
			return false;
		}
		break;
	}

	return true;
}

//=================================================================================================
// Check if unit can take perk
bool TakenPerk::CanTake(PerkContext& ctx)
{
	auto& info = PerkInfo::perks[(int)perk];

	// can take history perk only at startup
	if(IS_SET(info.flags, PerkInfo::History) && !ctx.startup)
		return false;

	// can take more then 2 flaws
	if(IS_SET(info.flags, PerkInfo::Flaw) && ctx.cc && ctx.cc->perks_max >= 4 && !ctx.reapply)
		return false;

	// check if perk require parent perk
	if(info.parent != Perk::None && !ctx.HavePerk(info.parent) && !ctx.reapply)
		return false;

	switch(perk)
	{
	case Perk::FamilyHeirloom:
	case Perk::Wealthy:
		return !ctx.HavePerk(Perk::Poor);
	case Perk::BadBack:
		return !ctx.HavePerk(Perk::StrongBack) && ctx.CanMod(Attribute::STR);
	case Perk::ChronicDisease:
		return ctx.CanMod(Attribute::END);
	case Perk::Sluggish:
		return ctx.CanMod(Attribute::DEX);
	case Perk::SlowLearner:
		return ctx.CanMod(Attribute::INT);
	case Perk::Asocial:
		return ctx.CanMod(Attribute::CHA);
	case Perk::Poor:
		return !ctx.HavePerk(Perk::Wealthy) && !ctx.HavePerk(Perk::FamilyHeirloom);
	case Perk::StrongBack:
		return !ctx.HavePerk(Perk::BadBack) && ctx.Have(Attribute::STR, 60);
	case Perk::StrongerBack:
		return ctx.Have(Attribute::STR, 80);
	case Perk::Aggressive:
		return ctx.Have(Attribute::STR, 80);
	case Perk::VeryAggressive:
		return ctx.Have(Attribute::STR, 100);
	case Perk::Berserker:
		return ctx.Have(Attribute::STR, 60);
	case Perk::HeavyHitter:
		return ctx.Have(Attribute::STR, 70);
	case Perk::Careful:
	case Perk::Mobility:
		return ctx.Have(Attribute::DEX, 60);
	case Perk::EnhancedMobility:
		return ctx.Have(Attribute::DEX, 80);
	case Perk::Tought:
	case Perk::HardSkin:
		return ctx.Have(Attribute::END, 60);
	case Perk::Toughter:
	case Perk::IronSkin:
		return ctx.Have(Attribute::END, 80);
	case Perk::Toughtest:
	case Perk::DiamondSkin:
	case Perk::PerfectHealth:
		return ctx.Have(Attribute::END, 100);
	case Perk::Adaptation:
		return ctx.Have(Attribute::END, 90);
	case Perk::ExtraordinaryHealth:
		return !ctx.HavePerk(Perk::ChronicDisease) && ctx.Have(Attribute::END, 75);
	case Perk::Energetic:
		return ctx.Have(Attribute::END, 55);
	case Perk::VeryEnergetic:
		return ctx.Have(Attribute::END, 85);
	case Perk::HealthyDiet:
		return ctx.Have(Skill::ATHLETICS, 25);
	case Perk::Strongman:
		return ctx.Have(Skill::ATHLETICS, 50);
	case Perk::BodyBuilder:
		return ctx.Have(Skill::ATHLETICS, 75);
	case Perk::MiracleDiet:
		return ctx.Have(Skill::ATHLETICS, 100);
	case Perk::Flexible:
		return ctx.Have(Skill::ACROBATICS, 50);
	case Perk::DecryptingRunes:
		return ctx.Have(Skill::LITERACY, 25);
	default:
		return true;
	}
}

//=================================================================================================
// Apply perk to unit
bool TakenPerk::Apply(PerkContext& ctx)
{
	auto& info = PerkInfo::perks[(int)perk];

	// hide parent perk, remove it effect
	if(info.parent != Perk::None)
		ctx.HidePerk(info.parent);

	switch(perk)
	{
	case Perk::MilitaryTraining:
		ctx.AddEffect(this, EffectType::Health, 50.f);
		ctx.AddEffect(this, EffectType::Attack, 5.f);
		ctx.AddEffect(this, EffectType::Defense, 5.f);
		break;
	case Perk::Talent:
		if(ctx.validate && ctx.cc && ctx.cc->a[value].mod)
		{
			Error("Perk 'talent', attribute %d is already modified.", value);
			return false;
		}
		ctx.Mod((Attribute)value, 5);
		break;
	case Perk::Skilled:
		if(ctx.cc)
		{
			ctx.cc->update_skills = true;
			ctx.cc->sp += 3;
			ctx.cc->sp_max += 3;
		}
		break;
	case Perk::SkillFocus:
		if(ctx.validate && ctx.cc && ctx.cc->s[value].mod)
		{
			Error("Perk 'skill_focus', skill %d is already modified.", value);
			return false;
		}
		ctx.Mod((Skill)value, 5);
		break;
	case Perk::Wealthy:
		if(ctx.pc && !hidden && ctx.startup)
			ctx.pc->unit->gold += 1000;
		break;
	case Perk::VeryWealthy:
		if(ctx.pc && !hidden && ctx.startup)
			ctx.pc->unit->gold += 5000;
		break;
	case Perk::FilthyRich:
		if(ctx.pc && !hidden && ctx.startup)
			ctx.pc->unit->gold += 100'000;
		break;
	case Perk::BadBack:
		ctx.Mod(Attribute::STR, -5);
		ctx.AddEffect(this, EffectType::Carry, 0.75f);
		break;
	case Perk::ChronicDisease:
		ctx.Mod(Attribute::END, -5);
		ctx.AddEffect(this, EffectType::NaturalHealingMod, 0.5f);
		break;
	case Perk::Sluggish:
		ctx.Mod(Attribute::DEX, -5);
		ctx.AddEffect(this, EffectType::Mobility, -25.f);
		break;
	case Perk::SlowLearner:
		ctx.Mod(Attribute::INT, -5);
		ctx.AddFlag(PF_SLOW_LERNER);
		break;
	case Perk::Asocial:
		ctx.Mod(Attribute::CHA, -5);
		ctx.AddFlag(PF_ASOCIAL);
		break;
	case Perk::Poor:
		if(ctx.pc && ctx.startup)
			ctx.pc->unit->gold /= 10;
		break;
	case Perk::StrongBack:
		ctx.AddEffect(this, EffectType::Carry, 1.25f);
		break;
	case Perk::StrongerBack:
		ctx.AddEffect(this, EffectType::Carry, 1.5f);
		break;
	case Perk::Aggressive:
		ctx.AddEffect(this, EffectType::Attack, 10.f);
		break;
	case Perk::VeryAggressive:
		ctx.AddEffect(this, EffectType::Attack, 30.f);
		break;
	case Perk::Berserker:
		ctx.AddEffect(this, EffectType::Attack, 60.f);
		break;
	case Perk::HeavyHitter:
		ctx.AddFlag(PF_HEAVY_HITTER);
	case Perk::Careful:
		ctx.AddEffect(this, EffectType::Defense, 10.f);
		break;
	case Perk::Mobility:
		ctx.AddEffect(this, EffectType::Mobility, 10.f);
		break;
	case Perk::EnhancedMobility:
		ctx.AddEffect(this, EffectType::Mobility, 25.f);
		break;
	case Perk::Tought:
		ctx.AddEffect(this, EffectType::Health, 100.f);
		break;
	case Perk::Toughter:
		ctx.AddEffect(this, EffectType::Health, 250.f);
		break;
	case Perk::Toughtest:
		ctx.AddEffect(this, EffectType::Health, 500.f);
		break;
	case Perk::HardSkin:
		ctx.AddEffect(this, EffectType::Defense, 10.f);
		break;
	case Perk::IronSkin:
		ctx.AddEffect(this, EffectType::Defense, 30.f);
		break;
	case Perk::DiamondSkin:
		ctx.AddEffect(this, EffectType::Defense, 60.f);
		break;
	case Perk::ExtraordinaryHealth:
		ctx.AddEffect(this, EffectType::Regeneration, 1.f);
		break;
	case Perk::PerfectHealth:
		ctx.AddEffect(this, EffectType::Regeneration, 5.f);
		break;
	case Perk::Energetic:
		ctx.AddEffect(this, EffectType::Stamina, 50.f);
		break;
	case Perk::VeryEnergetic:
		ctx.AddEffect(this, EffectType::Stamina, 150.f);
		break;
	case Perk::HealthyDiet:
		ctx.AddEffect(this, EffectType::NaturalHealingMod, 1.5f);
		ctx.AddFlag(PF_HEALTHY_DIET);
		break;
	case Perk::Strongman:
		ctx.AddEffect(this, EffectType::Carry, 1.25f);
		break;
	case Perk::BodyBuilder:
		ctx.AddEffect(this, EffectType::Health, 200.f);
		break;
	case Perk::Flexible:
		ctx.AddEffect(this, EffectType::Mobility, 25.f);
		break;
	case Perk::MiracleDiet:
		ctx.AddEffect(this, EffectType::NaturalHealingMod, 2.f);
		ctx.AddFlag(PF_MIRACLE_DIET);
		break;
	}

	if(!ctx.reapply)
	{
		if(ctx.cc)
		{
			if(IS_SET(info.flags, PerkInfo::Flaw))
			{
				++ctx.cc->perks;
				++ctx.cc->perks_max;
			}
			else
				--ctx.cc->perks;
			ctx.cc->taken_perks.push_back(*this);
		}
		else
		{
			ctx.pc->unit->statsx->perks.push_back(*this);
			if(!ctx.startup && !ctx.pc->is_local)
			{
				NetChangePlayer& c = Add1(ctx.pc->player_info->changes);
				c.type = NetChangePlayer::ADD_PERK;
				c.id = (byte)perk;
				c.ile = value;
			}
		}
	}

	return true;
}

//=================================================================================================
// Remove perk from unit
void TakenPerk::Remove(PerkContext& ctx)
{
	// remove unit effects
	if(ctx.pc)
		ctx.pc->unit->RemovePerkEffects(perk);

	// reapply parent perk
	auto& info = PerkInfo::perks[(int)perk];
	if(info.parent != Perk::None)
	{
		auto old = ctx.HidePerk(info.parent, false);
		if(old)
		{
			ctx.reapply = true;
			old->Apply(ctx);
		}
	}

	bool revalidate = false;

	switch(perk)
	{
	case Perk::Talent:
		ctx.Mod((Attribute)value, -5, false);
		revalidate = true;
		break;
	case Perk::Skilled:
		if(ctx.cc)
		{
			ctx.cc->update_skills = true;
			ctx.cc->sp -= 3;
			ctx.cc->sp_max -= 3;
			revalidate = true;
		}
		break;
	case Perk::SkillFocus:
		ctx.Mod((Skill)value, -5, false);
		break;
	case Perk::BadBack:
		ctx.Mod(Attribute::STR, -5, false);
		break;
	case Perk::ChronicDisease:
		ctx.Mod(Attribute::END, -5, false);
		break;
	case Perk::Sluggish:
		ctx.Mod(Attribute::DEX, -5, false);
		break;
	case Perk::SlowLearner:
		ctx.Mod(Attribute::INT, -5, false);
		ctx.RemoveFlag(PF_SLOW_LERNER);
		break;
	case Perk::Asocial:
		ctx.Mod(Attribute::CHA, -5, false);
		ctx.RemoveFlag(PF_ASOCIAL);
		break;
	case Perk::HeavyHitter:
		ctx.RemoveFlag(PF_HEAVY_HITTER);
		break;
	case Perk::HealthyDiet:
		ctx.RemoveFlag(PF_HEALTHY_DIET);
		break;
	case Perk::MiracleDiet:
		ctx.RemoveFlag(PF_MIRACLE_DIET);
		break;
	}

	if(ctx.cc)
	{
		if(revalidate)
		{
			static vector<Perk> removed;
			ctx.reapply = true;
			for(auto& p : ctx.cc->taken_perks)
			{
				if(!p.CanTake(ctx))
				{
					removed.push_back(p.perk);
					p.Remove(ctx);
				}
			}
			if(!removed.empty())
			{
				string str = txPerksRemoved;
				for(Perk perk : removed)
				{
					str += '\n';
					str += PerkInfo::perks[(int)perk].name;
				}
				GUI.SimpleDialog(str.c_str(), (Control*)Game::Get().create_character);
			}
			removed.clear();
		}

		PerkInfo& info = PerkInfo::perks[(int)perk];
		if(IS_SET(info.flags, PerkInfo::Flaw))
		{
			--ctx.cc->perks;
			--ctx.cc->perks_max;
		}
		else
			++ctx.cc->perks;
		ctx.cc->taken_perks.erase(ctx.cc->taken_perks.begin() + ctx.index);
	}
	else
	{
		ctx.pc->unit->statsx->perks.erase(ctx.pc->unit->statsx->perks.begin() + ctx.index);
		if(!ctx.pc->is_local)
		{
			NetChangePlayer& c = Add1(ctx.pc->player_info->changes);
			c.type = NetChangePlayer::REMOVE_PERK;
			c.id = (int)perk;
		}
	}
}

//=================================================================================================
bool PerkContext::HavePerk(Perk perk)
{
	if(cc)
		return cc->HavePerk(perk);
	else
	{
		for(auto& p : pc->unit->statsx->perks)
		{
			if(p.perk == perk)
				return true;
		}
		return false;
	}
}

//=================================================================================================
TakenPerk* PerkContext::FindPerk(Perk perk)
{
	if(cc)
	{
		for(auto& tp : cc->taken_perks)
		{
			if(tp.perk == perk)
				return &tp;
		}
	}
	else
	{
		for(auto& tp : pc->unit->statsx->perks)
		{
			if(tp.perk == perk)
				return &tp;
		}
	}
	return nullptr;
}

//=================================================================================================
TakenPerk* PerkContext::HidePerk(Perk perk, bool hide)
{
	auto taken_perk = FindPerk(perk);
	if(taken_perk && taken_perk->hidden != hide)
	{
		taken_perk->hidden = hide;
		if(!startup && Net::IsServer() && !pc->is_local)
		{
			NetChangePlayer& c = Add1(pc->player_info->changes);
			c.type = NetChangePlayer::HIDE_PERK;
			c.id = (int)perk;
			c.ile = (hide ? 1 : 0);
		}

		if(pc)
		{
			// remove effect
			pc->unit->RemovePerkEffects(taken_perk->perk);
		}
	}
	return taken_perk;
}

//=================================================================================================
bool PerkContext::CanMod(Attribute attrib)
{
	if(!cc)
		return true;
	return !cc->a[(int)attrib].mod;
}

//=================================================================================================
bool PerkContext::CanMod(Skill skill)
{
	if(!cc)
		return true;
	return !cc->s[(int)skill].mod;
}

//=================================================================================================
void PerkContext::Mod(Attribute attrib, int value, bool mod)
{
	if(cc)
		cc->a[(int)attrib].Mod(value, mod);
	else
		pc->unit->SetBase(attrib, value, startup, true);
}

//=================================================================================================
void PerkContext::Mod(Skill skill, int value, bool mod)
{
	if(cc)
	{
		cc->s[(int)skill].Mod(value, mod);
		cc->to_update.push_back(skill);
	}
	else
		pc->unit->SetBase(skill, value, startup, true);
}

//=================================================================================================
bool PerkContext::Have(Attribute attrib, int value)
{
	if(cc)
		return cc->a[(int)attrib].value >= value;
	else
		return pc->unit->GetBase(attrib) >= value;
}

//=================================================================================================
bool PerkContext::Have(Skill skill, int value)
{
	if(cc)
		return cc->s[(int)skill].value >= value;
	else
		return pc->unit->GetBase(skill) >= value;
}

//=================================================================================================
void PerkContext::AddFlag(PerkFlags flag)
{
	if(cc || IS_SET(pc->unit->statsx->perk_flags, flag))
		return;
	pc->unit->statsx->perk_flags |= flag;
	if(!pc->is_local)
	{
		NetChangePlayer& c = Add1(pc->player_info->changes);
		c.type = NetChangePlayer::STAT_CHANGED;
		c.id = (byte)ChangedStatType::PERK_FLAGS;
		c.a = 0;
		c.ile = pc->unit->statsx->perk_flags;
	}
}

//=================================================================================================
void PerkContext::RemoveFlag(PerkFlags flag)
{
	if(cc || !IS_SET(pc->unit->statsx->perk_flags, flag))
		return;
	pc->unit->statsx->perk_flags &= ~flag;
	if(!pc->is_local)
	{
		NetChangePlayer& c = Add1(pc->player_info->changes);
		c.type = NetChangePlayer::STAT_CHANGED;
		c.id = (byte)ChangedStatType::PERK_FLAGS;
		c.a = 0;
		c.ile = pc->unit->statsx->perk_flags;
	}
}

//=================================================================================================
void PerkContext::AddEffect(TakenPerk* perk, EffectType effect, float value)
{
	if(cc)
		return;
	Effect* e = Effect::Get();
	e->effect = effect;
	e->value = -1;
	e->power = value;
	e->time = 0;
	e->source = EffectSource::Perk;
	e->source_id = (int)perk->perk;
	e->refs = 1;
	pc->unit->AddEffect(e);
}
