﻿#include "Pch.h"
#include "Game.h"
#include "Terrain.h"
#include "City.h"
#include "InsideLocation.h"
#include "Ability.h"
#include "Profiler.h"
#include "Portal.h"
#include "Level.h"
#include "SuperShader.h"
#include "GameGui.h"
#include "GameMessages.h"
#include "ParticleSystem.h"
#include "Render.h"
#include "TerrainShader.h"
#include "ResourceManager.h"
#include "SoundManager.h"
#include "PhysicCallbacks.h"
#include "GameResources.h"
#include "ParticleShader.h"
#include "GlowShader.h"
#include "PostfxShader.h"
#include "BasicShader.h"
#include "SkyboxShader.h"
#include "Pathfinding.h"
#include <SceneManager.h>
#include <Scene.h>
#include <Algorithm.h>
#include "DungeonMeshBuilder.h"
#include "DirectX.h"

//=================================================================================================
void Game::InitScene()
{
	blood_v[0].tex = Vec2(0, 0);
	blood_v[1].tex = Vec2(0, 1);
	blood_v[2].tex = Vec2(1, 0);
	blood_v[3].tex = Vec2(1, 1);
	for(int i = 0; i < 4; ++i)
		blood_v[i].pos.y = 0.f;

	dun_mesh_builder->Build();
}

//=================================================================================================
void Game::ListDrawObjects(LevelArea& area, FrustumPlanes& frustum, bool outside)
{
	PROFILER_BLOCK("ListDrawObjects");

	TmpLevelArea& tmp_area = *area.tmp;

	draw_batch.Clear();
	draw_batch.camera = &game_level->camera;
	draw_batch.gather_lights = !outside && scene_mgr->use_lighting;
	ClearGrass();
	if(area.area_type == LevelArea::Type::Outside)
	{
		ListQuadtreeNodes();
		ListGrass();
	}

	// terrain
	if(area.area_type == LevelArea::Type::Outside && IsSet(draw_flags, DF_TERRAIN))
	{
		PROFILER_BLOCK("Terrain");
		uint parts = game_level->terrain->GetPartsCount();
		for(uint i = 0; i < parts; ++i)
		{
			if(frustum.BoxToFrustum(game_level->terrain->GetPart(i)->GetBox()))
				draw_batch.terrain_parts.push_back(i);
		}
	}

	// dungeon
	if(area.area_type == LevelArea::Type::Inside && IsSet(draw_flags, DF_TERRAIN))
	{
		PROFILER_BLOCK("Dungeon");
		dun_mesh_builder->ListVisibleParts(draw_batch, frustum);
	}

	// units
	if(IsSet(draw_flags, DF_UNITS))
	{
		PROFILER_BLOCK("Units");
		for(vector<Unit*>::iterator it = area.units.begin(), end = area.units.end(); it != end; ++it)
		{
			Unit& u = **it;
			ListDrawObjectsUnit(frustum, outside, u);
		}
	}

	// objects
	if(IsSet(draw_flags, DF_OBJECTS))
	{
		PROFILER_BLOCK("Objects");
		if(area.area_type == LevelArea::Type::Outside)
		{
			for(LevelPart* part : level_parts)
			{
				for(QuadObj& obj : part->objects)
				{
					const Object& o = *obj.obj;
					o.mesh->EnsureIsLoaded();
					if(frustum.SphereToFrustum(o.pos, o.GetRadius()))
						AddObjectToDrawBatch(area, o, frustum);
				}
			}
		}
		else
		{
			for(vector<Object*>::iterator it = area.objects.begin(), end = area.objects.end(); it != end; ++it)
			{
				const Object& o = **it;
				o.mesh->EnsureIsLoaded();
				if(frustum.SphereToFrustum(o.pos, o.GetRadius()))
					AddObjectToDrawBatch(area, o, frustum);
			}
		}
	}

	// items
	if(IsSet(draw_flags, DF_ITEMS))
	{
		PROFILER_BLOCK("Ground items");
		Vec3 pos;
		for(vector<GroundItem*>::iterator it = area.items.begin(), end = area.items.end(); it != end; ++it)
		{
			GroundItem& item = **it;
			if(!item.item)
			{
				ReportError(7, Format("GroundItem with null item at %g;%g;%g (count %d, team count %d).",
					item.pos.x, item.pos.y, item.pos.z, item.count, item.team_count));
				area.items.erase(it);
				break;
			}
			Mesh* mesh;
			pos = item.pos;
			if(IsSet(item.item->flags, ITEM_GROUND_MESH))
			{
				mesh = item.item->mesh;
				mesh->EnsureIsLoaded();
				pos.y -= mesh->head.bbox.v1.y;
			}
			else
				mesh = game_res->aBag;
			if(frustum.SphereToFrustum(item.pos, mesh->head.radius))
			{
				SceneNode* node = SceneNode::Get();
				node->type = SceneNode::NORMAL;
				node->SetMesh(mesh);
				if(IsSet(item.item->flags, ITEM_ALPHA))
					node->flags |= SceneNode::F_ALPHA_TEST;
				node->center = item.pos;
				node->mat = Matrix::Rotation(item.rot) * Matrix::Translation(pos);
				node->tex_override = nullptr;
				node->tint = Vec4(1, 1, 1, 1);
				if(!outside)
					GatherDrawBatchLights(area, node);
				if(pc->data.before_player == BP_ITEM && pc->data.before_player_ptr.item == &item)
				{
					if(use_glow)
					{
						GlowNode& glow = Add1(draw_batch.glow_nodes);
						glow.node = node;
						glow.type = GlowNode::Item;
						glow.ptr = &item;
						glow.alpha = IsSet(item.item->flags, ITEM_ALPHA);
					}
					else
						node->tint = Vec4(2, 2, 2, 1);
				}
				draw_batch.Add(node);
			}
		}
	}

	// usable objects
	if(IsSet(draw_flags, DF_USABLES))
	{
		PROFILER_BLOCK("Usables");
		for(vector<Usable*>::iterator it = area.usables.begin(), end = area.usables.end(); it != end; ++it)
		{
			Usable& use = **it;
			Mesh* mesh = use.GetMesh();
			mesh->EnsureIsLoaded();
			if(frustum.SphereToFrustum(use.pos, mesh->head.radius))
			{
				SceneNode* node = SceneNode::Get();
				node->type = SceneNode::NORMAL;
				node->SetMesh(mesh);
				node->center = use.pos;
				node->mat = Matrix::RotationY(use.rot) * Matrix::Translation(use.pos);
				node->tex_override = nullptr;
				node->tint = Vec4(1, 1, 1, 1);
				if(!outside)
					GatherDrawBatchLights(area, node);
				if(pc->data.before_player == BP_USABLE && pc->data.before_player_ptr.usable == &use)
				{
					if(use_glow)
					{
						GlowNode& glow = Add1(draw_batch.glow_nodes);
						glow.node = node;
						glow.type = GlowNode::Usable;
						glow.ptr = &use;
						glow.alpha = false;
					}
					else
						node->tint = Vec4(2, 2, 2, 1);
				}
				draw_batch.Add(node);
			}
		}
	}

	// chests
	if(IsSet(draw_flags, DF_USABLES))
	{
		PROFILER_BLOCK("Chests");
		for(vector<Chest*>::iterator it = area.chests.begin(), end = area.chests.end(); it != end; ++it)
		{
			Chest& chest = **it;
			chest.mesh_inst->mesh->EnsureIsLoaded();
			if(frustum.SphereToFrustum(chest.pos, chest.mesh_inst->mesh->head.radius))
			{
				SceneNode* node = SceneNode::Get();
				node->type = SceneNode::NORMAL;
				if(!chest.mesh_inst->groups[0].anim || chest.mesh_inst->groups[0].time == 0.f)
					node->SetMesh(chest.mesh_inst->mesh);
				else
				{
					chest.mesh_inst->SetupBones();
					node->SetMesh(chest.mesh_inst);
				}
				node->center = chest.pos;
				node->mat = Matrix::RotationY(chest.rot) * Matrix::Translation(chest.pos);
				node->tex_override = nullptr;
				node->tint = Vec4(1, 1, 1, 1);
				if(!outside)
					GatherDrawBatchLights(area, node);
				if(pc->data.before_player == BP_CHEST && pc->data.before_player_ptr.chest == &chest)
				{
					if(use_glow)
					{
						GlowNode& glow = Add1(draw_batch.glow_nodes);
						glow.node = node;
						glow.type = GlowNode::Chest;
						glow.ptr = &chest;
						glow.alpha = false;
					}
					else
						node->tint = Vec4(2, 2, 2, 1);
				}
				draw_batch.Add(node);
			}
		}
	}

	// doors
	if(IsSet(draw_flags, DF_USABLES))
	{
		for(vector<Door*>::iterator it = area.doors.begin(), end = area.doors.end(); it != end; ++it)
		{
			Door& door = **it;
			door.mesh_inst->mesh->EnsureIsLoaded();
			if(frustum.SphereToFrustum(door.pos, door.mesh_inst->mesh->head.radius))
			{
				SceneNode* node = SceneNode::Get();
				node->type = SceneNode::NORMAL;
				if(!door.mesh_inst->groups[0].anim || door.mesh_inst->groups[0].time == 0.f)
					node->SetMesh(door.mesh_inst->mesh);
				else
				{
					door.mesh_inst->SetupBones();
					node->SetMesh(door.mesh_inst);
				}
				node->center = door.pos;
				node->mat = Matrix::RotationY(door.rot) * Matrix::Translation(door.pos);
				node->tex_override = nullptr;
				node->tint = Vec4(1, 1, 1, 1);
				if(!outside)
					GatherDrawBatchLights(area, node);
				if(pc->data.before_player == BP_DOOR && pc->data.before_player_ptr.door == &door)
				{
					if(use_glow)
					{
						GlowNode& glow = Add1(draw_batch.glow_nodes);
						glow.node = node;
						glow.type = GlowNode::Door;
						glow.ptr = &door;
						glow.alpha = false;
					}
					else
						node->tint = Vec4(2, 2, 2, 1);
				}
				draw_batch.Add(node);
			}
		}
	}

	// bloods
	if(IsSet(draw_flags, DF_BLOOD))
	{
		for(Blood& blood : area.bloods)
		{
			if(blood.size > 0.f && frustum.SphereToFrustum(blood.pos, blood.size * blood.scale))
			{
				if(!outside)
					GatherDrawBatchLights(area, nullptr, blood.pos.x, blood.pos.y, blood.size * blood.scale, 0, blood.lights);
				draw_batch.bloods.push_back(&blood);
			}
		}
	}

	// bullets
	if(IsSet(draw_flags, DF_BULLETS))
	{
		for(vector<Bullet>::iterator it = tmp_area.bullets.begin(), end = tmp_area.bullets.end(); it != end; ++it)
		{
			Bullet& bullet = *it;
			if(bullet.mesh)
			{
				bullet.mesh->EnsureIsLoaded();
				if(frustum.SphereToFrustum(bullet.pos, bullet.mesh->head.radius))
				{
					SceneNode* node = SceneNode::Get();
					node->type = SceneNode::NORMAL;
					node->SetMesh(bullet.mesh);
					node->center = bullet.pos;
					node->mat = Matrix::Rotation(bullet.rot) * Matrix::Translation(bullet.pos);
					node->tint = Vec4(1, 1, 1, 1);
					node->tex_override = nullptr;
					if(!outside)
						GatherDrawBatchLights(area, node);
					draw_batch.Add(node);
				}
			}
			else
			{
				if(frustum.SphereToFrustum(bullet.pos, bullet.tex_size))
				{
					Billboard& bb = Add1(draw_batch.billboards);
					bb.pos = it->pos;
					bb.size = it->tex_size;
					bb.tex = it->tex;
				}
			}
		}
	}

	// traps
	if(IsSet(draw_flags, DF_TRAPS))
	{
		for(vector<Trap*>::iterator it = area.traps.begin(), end = area.traps.end(); it != end; ++it)
		{
			Trap& trap = **it;
			if((trap.state == 0 || (trap.base->type != TRAP_ARROW && trap.base->type != TRAP_POISON))
				&& (trap.obj.mesh->EnsureIsLoaded(), true)
				&& frustum.SphereToFrustum(trap.obj.pos, trap.obj.mesh->head.radius))
			{
				SceneNode* node = SceneNode::Get();
				node->type = SceneNode::NORMAL;
				node->SetMesh(trap.obj.mesh);
				int alpha = trap.obj.RequireAlphaTest();
				if(alpha == 0)
					node->flags |= SceneNode::F_ALPHA_TEST;
				else if(alpha == 1)
					node->flags |= SceneNode::F_ALPHA_TEST | SceneNode::F_NO_CULLING;
				node->center = trap.obj.pos;
				node->mat = Matrix::Transform(trap.obj.pos, trap.obj.rot, trap.obj.scale);
				node->tex_override = nullptr;
				node->tint = Vec4(1, 1, 1, 1);
				if(!outside)
					GatherDrawBatchLights(area, node);
				draw_batch.Add(node);
			}
			if(trap.base->type == TRAP_SPEAR && InRange(trap.state, 2, 4)
				&& (trap.obj2.mesh->EnsureIsLoaded(), true)
				&& frustum.SphereToFrustum(trap.obj2.pos, trap.obj2.mesh->head.radius))
			{
				SceneNode* node = SceneNode::Get();
				node->type = SceneNode::NORMAL;
				node->SetMesh(trap.obj2.mesh);
				int alpha = trap.obj2.RequireAlphaTest();
				if(alpha == 0)
					node->flags = SceneNode::F_ALPHA_TEST;
				else if(alpha == 1)
					node->flags = SceneNode::F_ALPHA_TEST | SceneNode::F_NO_CULLING;
				node->center = trap.obj2.pos;
				node->mat = Matrix::Transform(trap.obj2.pos, trap.obj2.rot, trap.obj2.scale);
				node->tex_override = nullptr;
				node->tint = Vec4(1, 1, 1, 1);
				if(!outside)
					GatherDrawBatchLights(area, node);
				draw_batch.Add(node);
			}
		}
	}

	// explosions
	if(IsSet(draw_flags, DF_EXPLOS))
	{
		game_res->aSpellball->EnsureIsLoaded();
		for(vector<Explo*>::iterator it = tmp_area.explos.begin(), end = tmp_area.explos.end(); it != end; ++it)
		{
			Explo& explo = **it;
			if(frustum.SphereToFrustum(explo.pos, explo.size))
			{
				SceneNode* node = SceneNode::Get();
				node->type = SceneNode::NORMAL;
				node->SetMesh(game_res->aSpellball);
				node->flags |= SceneNode::F_NO_LIGHTING | SceneNode::F_ALPHA_BLEND | SceneNode::F_NO_ZWRITE;
				node->center = explo.pos;
				node->radius *= explo.size;
				node->mat = Matrix::Scale(explo.size) * Matrix::Translation(explo.pos);
				node->tex_override = &explo.ability->tex_explode;
				node->tint = Vec4(1, 1, 1, 1.f - explo.size / explo.sizemax);
				draw_batch.Add(node);
			}
		}
	}

	// particles
	if(IsSet(draw_flags, DF_PARTICLES))
	{
		PROFILER_BLOCK("Particles");
		for(vector<ParticleEmitter*>::iterator it = tmp_area.pes.begin(), end = tmp_area.pes.end(); it != end; ++it)
		{
			ParticleEmitter& pe = **it;
			if(pe.alive && frustum.SphereToFrustum(pe.pos, pe.radius))
			{
				draw_batch.pes.push_back(&pe);
				if(draw_particle_sphere)
				{
					DebugSceneNode* debug_node = DebugSceneNode::Get();
					debug_node->mat = Matrix::Scale(pe.radius * 2) * Matrix::Translation(pe.pos) * game_level->camera.mat_view_proj;
					debug_node->type = DebugSceneNode::Sphere;
					debug_node->group = DebugSceneNode::ParticleRadius;
					draw_batch.debug_nodes.push_back(debug_node);
				}
			}
		}

		if(tmp_area.tpes.empty())
			draw_batch.tpes = nullptr;
		else
			draw_batch.tpes = &tmp_area.tpes;
	}
	else
		draw_batch.tpes = nullptr;

	// portals
	if(IsSet(draw_flags, DF_PORTALS) && area.area_type != LevelArea::Type::Building)
	{
		game_res->aPortal->EnsureIsLoaded();
		Portal* portal = game_level->location->portal;
		while(portal)
		{
			if(game_level->location->outside || game_level->dungeon_level == portal->at_level)
			{
				SceneNode* node = SceneNode::Get();
				node->type = SceneNode::NORMAL;
				node->SetMesh(game_res->aPortal);
				node->flags |= SceneNode::F_NO_LIGHTING | SceneNode::F_ALPHA_BLEND | SceneNode::F_NO_CULLING;
				node->center = portal->pos + Vec3(0, 0.67f + 0.305f, 0);
				node->mat = Matrix::Rotation(0, portal->rot, -portal_anim * PI * 2) * Matrix::Translation(node->center);
				node->tex_override = nullptr;
				node->tint = Vec4::One;
				draw_batch.Add(node);
			}
			portal = portal->next_portal;
		}
	}

	// areas
	if(IsSet(draw_flags, DF_AREA))
		ListAreas(area);

	// colliders
	if(draw_col)
	{
		for(vector<CollisionObject>::iterator it = tmp_area.colliders.begin(), end = tmp_area.colliders.end(); it != end; ++it)
		{
			DebugSceneNode::Type type = DebugSceneNode::MaxType;
			Vec3 scale;
			float rot = 0.f;

			switch(it->type)
			{
			case CollisionObject::RECTANGLE:
				{
					scale = Vec3(it->w, 1, it->h);
					type = DebugSceneNode::Box;
				}
				break;
			case CollisionObject::RECTANGLE_ROT:
				{
					scale = Vec3(it->w, 1, it->h);
					type = DebugSceneNode::Box;
					rot = it->rot;
				}
				break;
			case CollisionObject::SPHERE:
				{
					scale = Vec3(it->radius, 1, it->radius);
					type = DebugSceneNode::Cylinder;
				}
				break;
			default:
				break;
			}

			if(type != DebugSceneNode::MaxType)
			{
				DebugSceneNode* node = DebugSceneNode::Get();
				node->type = type;
				node->group = DebugSceneNode::Collider;
				node->mat = Matrix::Scale(scale) * Matrix::RotationY(rot) * Matrix::Translation(it->pt.x, 1.f, it->pt.y) * game_level->camera.mat_view_proj;
				draw_batch.debug_nodes.push_back(node);
			}
		}
	}

	// physics
	if(draw_phy)
	{
		const btCollisionObjectArray& cobjs = phy_world->getCollisionObjectArray();
		const int count = cobjs.size();

		for(int i = 0; i < count; ++i)
		{
			const btCollisionObject* cobj = cobjs[i];
			const btCollisionShape* shape = cobj->getCollisionShape();
			Matrix m_world;
			cobj->getWorldTransform().getOpenGLMatrix(&m_world._11);

			switch(shape->getShapeType())
			{
			case BOX_SHAPE_PROXYTYPE:
				{
					const btBoxShape* box = (const btBoxShape*)shape;
					DebugSceneNode* node = DebugSceneNode::Get();
					node->type = DebugSceneNode::Box;
					node->group = DebugSceneNode::Physic;
					node->mat = Matrix::Scale(ToVec3(box->getHalfExtentsWithMargin())) * m_world * game_level->camera.mat_view_proj;
					draw_batch.debug_nodes.push_back(node);
				}
				break;
			case CAPSULE_SHAPE_PROXYTYPE:
				{
					const btCapsuleShape* capsule = (const btCapsuleShape*)shape;
					float r = capsule->getRadius();
					float h = capsule->getHalfHeight();
					DebugSceneNode* node = DebugSceneNode::Get();
					node->type = DebugSceneNode::Capsule;
					node->group = DebugSceneNode::Physic;
					node->mat = Matrix::Scale(r, h + r, r) * m_world * game_level->camera.mat_view_proj;
					draw_batch.debug_nodes.push_back(node);
				}
				break;
			case CYLINDER_SHAPE_PROXYTYPE:
				{
					const btCylinderShape* cylinder = (const btCylinderShape*)shape;
					DebugSceneNode* node = DebugSceneNode::Get();
					node->type = DebugSceneNode::Cylinder;
					node->group = DebugSceneNode::Physic;
					Vec3 v = ToVec3(cylinder->getHalfExtentsWithoutMargin());
					node->mat = Matrix::Scale(v.x, v.y / 2, v.z) * m_world * game_level->camera.mat_view_proj;
					draw_batch.debug_nodes.push_back(node);
				}
				break;
			case TERRAIN_SHAPE_PROXYTYPE:
				break;
			case COMPOUND_SHAPE_PROXYTYPE:
				{
					const btCompoundShape* compound = (const btCompoundShape*)shape;
					int count = compound->getNumChildShapes();

					for(int i = 0; i < count; ++i)
					{
						const btCollisionShape* child = compound->getChildShape(i);
						if(child->getShapeType() == BOX_SHAPE_PROXYTYPE)
						{
							btBoxShape* box = (btBoxShape*)child;
							DebugSceneNode* node = DebugSceneNode::Get();
							node->type = DebugSceneNode::Box;
							node->group = DebugSceneNode::Physic;
							Matrix m_child;
							compound->getChildTransform(i).getOpenGLMatrix(&m_child._11);
							node->mat = Matrix::Scale(ToVec3(box->getHalfExtentsWithMargin())) * m_child * m_world * game_level->camera.mat_view_proj;
							draw_batch.debug_nodes.push_back(node);
						}
						else
						{
							// TODO
							assert(0);
						}
					}
				}
				break;
			case TRIANGLE_MESH_SHAPE_PROXYTYPE:
				{
					DebugSceneNode* node = DebugSceneNode::Get();
					const btBvhTriangleMeshShape* trimesh = (const btBvhTriangleMeshShape*)shape;
					node->type = DebugSceneNode::TriMesh;
					node->group = DebugSceneNode::Physic;
					node->mat = m_world * game_level->camera.mat_view_proj;
					node->mesh_ptr = (void*)trimesh->getMeshInterface();
					draw_batch.debug_nodes.push_back(node);
				}
			default:
				break;
			}
		}
	}

	draw_batch.Process();
}

//=================================================================================================
void Game::ListDrawObjectsUnit(FrustumPlanes& frustum, bool outside, Unit& u)
{
	u.mesh_inst->mesh->EnsureIsLoaded();
	if(!frustum.SphereToFrustum(u.visual_pos, u.GetSphereRadius()))
		return;

	// add stun effect
	if(u.IsAlive())
	{
		Effect* effect = u.FindEffect(EffectId::Stun);
		if(effect)
		{
			game_res->aStun->EnsureIsLoaded();
			SceneNode* node = SceneNode::Get();
			node->type = SceneNode::NORMAL;
			node->SetMesh(game_res->aStun);
			node->flags |= SceneNode::F_NO_LIGHTING | SceneNode::F_ALPHA_BLEND | SceneNode::F_NO_CULLING | SceneNode::F_NO_ZWRITE;
			node->center = u.GetHeadPoint();
			node->mat = Matrix::RotationY(effect->time * 3) * Matrix::Translation(node->center);
			node->tex_override = nullptr;
			node->tint = Vec4::One;
			draw_batch.Add(node);
		}
	}

	// ustaw kości
	u.mesh_inst->SetupBones();

	bool selected = (pc->data.before_player == BP_UNIT && pc->data.before_player_ptr.unit == &u)
		|| (game_state == GS_LEVEL && ((pc->data.ability_ready && pc->data.ability_ok && pc->data.ability_target == u)
			|| (pc->unit->action == A_CAST && pc->unit->act.cast.target == u)));

	// dodaj scene node
	SceneNode* node = SceneNode::Get();
	node->type = SceneNode::NORMAL;
	node->SetMesh(u.mesh_inst);
	node->center = u.visual_pos;
	node->mat = Matrix::RotationY(u.rot) * Matrix::Translation(u.visual_pos);
	node->tex_override = u.data->GetTextureOverride();
	node->tint = u.data->tint;

	// ustawienia światła
	if(!outside)
	{
		assert(u.area);
		GatherDrawBatchLights(*u.area, node);
	}
	if(selected)
	{
		if(use_glow)
		{
			GlowNode& glow = Add1(draw_batch.glow_nodes);
			glow.node = node;
			glow.type = GlowNode::Unit;
			glow.ptr = &u;
			glow.alpha = false;
		}
		else
		{
			node->tint.x *= 2;
			node->tint.y *= 2;
			node->tint.z *= 2;
		}
	}
	draw_batch.Add(node);
	if(u.HaveArmor() && u.GetArmor().armor_unit_type == ArmorUnitType::HUMAN && u.GetArmor().mesh)
		node->subs = Bit(1) | Bit(2);

	// pancerz
	if(u.HaveArmor() && u.GetArmor().mesh)
	{
		const Armor& armor = u.GetArmor();
		SceneNode* node2 = SceneNode::Get();
		node2->type = SceneNode::NORMAL;
		node2->SetMesh(armor.mesh, u.mesh_inst);
		node2->center = node->center;
		node2->mat = node->mat;
		node2->tex_override = armor.GetTextureOverride();
		node2->tint = Vec4(1, 1, 1, 1);
		node2->lights = node->lights;
		if(selected)
		{
			if(use_glow)
			{
				GlowNode& glow = Add1(draw_batch.glow_nodes);
				glow.node = node2;
				glow.type = GlowNode::Unit;
				glow.ptr = &u;
				glow.alpha = false;
			}
			else
				node2->tint = Vec4(2, 2, 2, 1);
		}
		draw_batch.Add(node2);
	}

	// przedmiot w dłoni
	Mesh* right_hand_item = nullptr;
	int right_hand_item_flags = 0;
	bool in_hand = false;

	switch(u.weapon_state)
	{
	case WeaponState::Hidden:
		break;
	case WeaponState::Taken:
		if(u.weapon_taken == W_BOW)
		{
			if(u.action == A_SHOOT)
			{
				if(u.animation_state != AS_SHOOT_SHOT)
					right_hand_item = game_res->aArrow;
			}
			else
				right_hand_item = game_res->aArrow;
		}
		else if(u.weapon_taken == W_ONE_HANDED)
			in_hand = true;
		break;
	case WeaponState::Taking:
		if(u.animation_state == AS_TAKE_WEAPON_MOVED)
		{
			if(u.weapon_taken == W_BOW)
				right_hand_item = game_res->aArrow;
			else
				in_hand = true;
		}
		break;
	case WeaponState::Hiding:
		if(u.animation_state == AS_TAKE_WEAPON_START)
		{
			if(u.weapon_hiding == W_BOW)
				right_hand_item = game_res->aArrow;
			else
				in_hand = true;
		}
		break;
	}

	if(u.used_item && u.action != A_USE_ITEM)
	{
		right_hand_item = u.used_item->mesh;
		if(IsSet(u.used_item->flags, ITEM_ALPHA))
			right_hand_item_flags = SceneNode::F_ALPHA_TEST;
	}

	Matrix mat_scale;
	if(u.human_data)
	{
		Vec2 scale = u.human_data->GetScale();
		scale.x = 1.f / scale.x;
		scale.y = 1.f / scale.y;
		mat_scale = Matrix::Scale(scale.x, scale.y, scale.x);
	}
	else
		mat_scale = Matrix::IdentityMatrix;

	// broń
	Mesh* mesh;
	if(u.HaveWeapon() && right_hand_item != (mesh = u.GetWeapon().mesh))
	{
		Mesh::Point* point = u.mesh_inst->mesh->GetPoint(in_hand ? NAMES::point_weapon : NAMES::point_hidden_weapon);
		assert(point);

		SceneNode* node2 = SceneNode::Get();
		node2->type = SceneNode::NORMAL;
		node2->SetMesh(mesh);
		node2->center = node->center;
		node2->mat = mat_scale * point->mat * u.mesh_inst->mat_bones[point->bone] * node->mat;
		node2->tex_override = nullptr;
		node2->tint = Vec4(1, 1, 1, 1);
		node2->lights = node->lights;
		if(selected)
		{
			if(use_glow)
			{
				GlowNode& glow = Add1(draw_batch.glow_nodes);
				glow.node = node2;
				glow.type = GlowNode::Unit;
				glow.ptr = &u;
				glow.alpha = false;
			}
			else
				node2->tint = Vec4(2, 2, 2, 1);
		}
		draw_batch.Add(node2);

		// hitbox broni
		if(draw_hitbox && u.weapon_state == WeaponState::Taken && u.weapon_taken == W_ONE_HANDED)
		{
			Mesh::Point* box = mesh->FindPoint("hit");
			assert(box && box->IsBox());

			DebugSceneNode* debug_node = DebugSceneNode::Get();
			debug_node->mat = box->mat * node2->mat * game_level->camera.mat_view_proj;
			debug_node->type = DebugSceneNode::Box;
			debug_node->group = DebugSceneNode::Hitbox;
			draw_batch.debug_nodes.push_back(debug_node);
		}
	}

	// tarcza
	if(u.HaveShield() && u.GetShield().mesh)
	{
		Mesh* shield = u.GetShield().mesh;
		Mesh::Point* point = u.mesh_inst->mesh->GetPoint(in_hand ? NAMES::point_shield : NAMES::point_shield_hidden);
		assert(point);

		SceneNode* node2 = SceneNode::Get();
		node2->type = SceneNode::NORMAL;
		node2->SetMesh(shield);
		node2->center = node->center;
		node2->mat = mat_scale * point->mat * u.mesh_inst->mat_bones[point->bone] * node->mat;
		node2->tex_override = nullptr;
		node2->tint = Vec4(1, 1, 1, 1);
		node2->lights = node->lights;
		if(selected)
		{
			if(use_glow)
			{
				GlowNode& glow = Add1(draw_batch.glow_nodes);
				glow.node = node2;
				glow.type = GlowNode::Unit;
				glow.ptr = &u;
				glow.alpha = false;
			}
			else
				node2->tint = Vec4(2, 2, 2, 1);
		}
		draw_batch.Add(node2);

		// hitbox tarczy
		if(draw_hitbox && u.weapon_state == WeaponState::Taken && u.weapon_taken == W_ONE_HANDED)
		{
			Mesh::Point* box = shield->FindPoint("hit");
			assert(box && box->IsBox());

			DebugSceneNode* debug_node = DebugSceneNode::Get();
			node->mat = box->mat * node2->mat * game_level->camera.mat_view_proj;
			debug_node->type = DebugSceneNode::Box;
			debug_node->group = DebugSceneNode::Hitbox;
			draw_batch.debug_nodes.push_back(debug_node);
		}
	}

	// jakiś przedmiot
	if(right_hand_item)
	{
		Mesh::Point* point = u.mesh_inst->mesh->GetPoint(NAMES::point_weapon);
		assert(point);

		SceneNode* node2 = SceneNode::Get();
		node2->type = SceneNode::NORMAL;
		node2->SetMesh(right_hand_item);
		node2->flags |= right_hand_item_flags;
		node2->center = node->center;
		node2->mat = mat_scale * point->mat * u.mesh_inst->mat_bones[point->bone] * node->mat;
		node2->tex_override = nullptr;
		node2->tint = Vec4(1, 1, 1, 1);
		node2->lights = node->lights;
		if(selected)
		{
			if(use_glow)
			{
				GlowNode& glow = Add1(draw_batch.glow_nodes);
				glow.node = node2;
				glow.type = GlowNode::Unit;
				glow.ptr = &u;
				glow.alpha = IsSet(right_hand_item_flags, SceneNode::F_ALPHA_TEST);
			}
			else
				node2->tint = Vec4(2, 2, 2, 1);
		}
		draw_batch.Add(node2);
	}

	// łuk
	if(u.HaveBow())
	{
		bool in_hand;

		switch(u.weapon_state)
		{
		case WeaponState::Hiding:
			in_hand = (u.weapon_hiding == W_BOW && u.animation_state == AS_TAKE_WEAPON_START);
			break;
		case WeaponState::Hidden:
			in_hand = false;
			break;
		case WeaponState::Taking:
			in_hand = (u.weapon_taken == W_BOW && u.animation_state == AS_TAKE_WEAPON_MOVED);
			break;
		case WeaponState::Taken:
			in_hand = (u.weapon_taken == W_BOW);
			break;
		}

		SceneNode* node2 = SceneNode::Get();
		node2->type = SceneNode::NORMAL;
		if(u.action == A_SHOOT)
		{
			u.bow_instance->SetupBones();
			node2->SetMesh(u.bow_instance);
		}
		else
			node2->SetMesh(u.GetBow().mesh);
		node2->center = node->center;
		Mesh::Point* point = u.mesh_inst->mesh->GetPoint(in_hand ? NAMES::point_bow : NAMES::point_shield_hidden);
		assert(point);
		Matrix m1;
		if(in_hand)
			m1 = Matrix::RotationZ(-PI / 2) * point->mat * u.mesh_inst->mat_bones[point->bone];
		else
			m1 = point->mat * u.mesh_inst->mat_bones[point->bone];
		node2->mat = mat_scale * m1 * node->mat;
		node2->tex_override = nullptr;
		node2->tint = Vec4(1, 1, 1, 1);
		node2->lights = node->lights;
		if(selected)
		{
			if(use_glow)
			{
				GlowNode& glow = Add1(draw_batch.glow_nodes);
				glow.node = node2;
				glow.type = GlowNode::Unit;
				glow.ptr = &u;
				glow.alpha = false;
			}
			else
				node2->tint = Vec4(2, 2, 2, 1);
		}
		draw_batch.Add(node2);
	}

	// włosy/broda/brwi u ludzi
	if(u.data->type == UNIT_TYPE::HUMAN)
	{
		Human& h = *u.human_data;

		// brwi
		SceneNode* node2 = SceneNode::Get();
		node2->type = SceneNode::NORMAL;
		node2->SetMesh(game_res->aEyebrows, node->mesh_inst);
		node2->center = node->center;
		node2->mat = node->mat;
		node2->tex_override = nullptr;
		node2->tint = h.hair_color * u.data->tint;
		node2->lights = node->lights;
		if(selected)
		{
			if(use_glow)
			{
				GlowNode& glow = Add1(draw_batch.glow_nodes);
				glow.node = node2;
				glow.type = GlowNode::Unit;
				glow.ptr = &u;
				glow.alpha = false;
			}
			else
			{
				node2->tint.x *= 2;
				node2->tint.y *= 2;
				node2->tint.z *= 2;
			}
		}
		draw_batch.Add(node2);

		// włosy
		if(h.hair != -1)
		{
			SceneNode* node3 = SceneNode::Get();
			node3->type = SceneNode::NORMAL;
			node3->SetMesh(game_res->aHair[h.hair], node->mesh_inst);
			node3->center = node->center;
			node3->mat = node->mat;
			node3->tex_override = nullptr;
			node3->tint = h.hair_color * u.data->tint;
			node3->lights = node->lights;
			if(selected)
			{
				if(use_glow)
				{
					GlowNode& glow = Add1(draw_batch.glow_nodes);
					glow.node = node3;
					glow.type = GlowNode::Unit;
					glow.ptr = &u;
					glow.alpha = false;
				}
				else
				{
					node3->tint.x *= 2;
					node3->tint.y *= 2;
					node3->tint.z *= 2;
				}
			}
			draw_batch.Add(node3);
		}

		// broda
		if(h.beard != -1)
		{
			SceneNode* node3 = SceneNode::Get();
			node3->type = SceneNode::NORMAL;
			node3->SetMesh(game_res->aBeard[h.beard], node->mesh_inst);
			node3->center = node->center;
			node3->mat = node->mat;
			node3->tex_override = nullptr;
			node3->tint = h.hair_color * u.data->tint;
			node3->lights = node->lights;
			if(selected)
			{
				if(use_glow)
				{
					GlowNode& glow = Add1(draw_batch.glow_nodes);
					glow.node = node3;
					glow.type = GlowNode::Unit;
					glow.ptr = &u;
					glow.alpha = false;
				}
				else
				{
					node3->tint.x *= 2;
					node3->tint.y *= 2;
					node3->tint.z *= 2;
				}
			}
			draw_batch.Add(node3);
		}

		// wąsy
		if(h.mustache != -1 && (h.beard == -1 || !g_beard_and_mustache[h.beard]))
		{
			SceneNode* node3 = SceneNode::Get();
			node3->type = SceneNode::NORMAL;
			node3->SetMesh(game_res->aMustache[h.mustache], node->mesh_inst);
			node3->center = node->center;
			node3->mat = node->mat;
			node3->tex_override = nullptr;
			node3->tint = h.hair_color * u.data->tint;
			node3->lights = node->lights;
			if(selected)
			{
				if(use_glow)
				{
					GlowNode& glow = Add1(draw_batch.glow_nodes);
					glow.node = node3;
					glow.type = GlowNode::Unit;
					glow.ptr = &u;
					glow.alpha = false;
				}
				else
				{
					node3->tint.x *= 2;
					node3->tint.y *= 2;
					node3->tint.z *= 2;
				}
			}
			draw_batch.Add(node3);
		}
	}

	// pseudo hitbox postaci
	if(draw_unit_radius)
	{
		float h = u.GetUnitHeight() / 2;
		DebugSceneNode* debug_node = DebugSceneNode::Get();
		debug_node->mat = Matrix::Scale(u.GetUnitRadius(), h, u.GetUnitRadius()) * Matrix::Translation(u.GetColliderPos() + Vec3(0, h, 0)) * game_level->camera.mat_view_proj;
		debug_node->type = DebugSceneNode::Cylinder;
		debug_node->group = DebugSceneNode::UnitRadius;
		draw_batch.debug_nodes.push_back(debug_node);
	}
	if(draw_hitbox)
	{
		float h = u.GetUnitHeight() / 2;
		Box box;
		u.GetBox(box);
		DebugSceneNode* debug_node = DebugSceneNode::Get();
		debug_node->mat = Matrix::Scale(box.SizeX() / 2, h, box.SizeZ() / 2) * Matrix::Translation(u.pos + Vec3(0, h, 0)) * game_level->camera.mat_view_proj;
		debug_node->type = DebugSceneNode::Box;
		debug_node->group = DebugSceneNode::Hitbox;
		draw_batch.debug_nodes.push_back(debug_node);
	}
}

//=================================================================================================
void Game::AddObjectToDrawBatch(LevelArea& area, const Object& o, FrustumPlanes& frustum)
{
	SceneNode* node = SceneNode::Get();
	if(!o.IsBillboard())
	{
		node->type = SceneNode::NORMAL;
		node->mat = Matrix::Transform(o.pos, o.rot, o.scale);
	}
	else
	{
		node->type = SceneNode::BILLBOARD;
		node->mat = Matrix::CreateLookAt(o.pos, game_level->camera.from);
	}

	node->SetMesh(o.mesh);
	int alpha = o.RequireAlphaTest();
	if(alpha == 0)
		node->flags |= SceneNode::F_ALPHA_TEST;
	else if(alpha == 1)
		node->flags |= SceneNode::F_ALPHA_TEST | SceneNode::F_NO_CULLING;
	node->tex_override = nullptr;
	node->tint = Vec4(1, 1, 1, 1);
	if(!IsSet(o.mesh->head.flags, Mesh::F_SPLIT))
	{
		node->center = o.pos;
		node->radius = o.GetRadius();
		if(area.area_type != LevelArea::Type::Outside)
			GatherDrawBatchLights(area, node);
		draw_batch.Add(node);
	}
	else
	{
		Mesh& mesh = *o.mesh;
		if(IsSet(mesh.head.flags, Mesh::F_TANGENTS))
			node->flags |= SceneNode::F_TANGENTS;

		// for simplicity original node in unused and freed at end
		for(int i = 0; i < mesh.head.n_subs; ++i)
		{
			const Vec3 pos = Vec3::Transform(mesh.splits[i].pos, node->mat);
			const float radius = mesh.splits[i].radius * o.scale;
			if(frustum.SphereToFrustum(pos, radius))
			{
				SceneNode* node2 = SceneNode::Get();
				node2->type = SceneNode::NORMAL;
				node2->mesh = node->mesh;
				node2->mesh_inst = node->mesh_inst;
				node2->flags = node->flags;
				node2->center = pos;
				node2->radius = radius;
				node2->mat = node->mat;
				node2->tint = node->tint;
				node2->tex_override = node->tex_override;
				draw_batch.Add(node2, i);
				if(area.area_type != LevelArea::Type::Outside)
					GatherDrawBatchLights(area, node2);
			}
		}

		node->Free();
	}
}

//=================================================================================================
void Game::ListAreas(LevelArea& area)
{
	if(area.area_type == LevelArea::Type::Outside)
	{
		if(game_level->city_ctx)
		{
			if(IsSet(game_level->city_ctx->flags, City::HaveExit))
			{
				for(vector<EntryPoint>::const_iterator entry_it = game_level->city_ctx->entry_points.begin(), entry_end = game_level->city_ctx->entry_points.end();
					entry_it != entry_end; ++entry_it)
				{
					const EntryPoint& e = *entry_it;
					Area& a = Add1(draw_batch.areas);
					a.v[0] = Vec3(e.exit_region.v1.x, e.exit_y, e.exit_region.v2.y);
					a.v[1] = Vec3(e.exit_region.v2.x, e.exit_y, e.exit_region.v2.y);
					a.v[2] = Vec3(e.exit_region.v1.x, e.exit_y, e.exit_region.v1.y);
					a.v[3] = Vec3(e.exit_region.v2.x, e.exit_y, e.exit_region.v1.y);
				}
			}

			for(vector<InsideBuilding*>::const_iterator it = game_level->city_ctx->inside_buildings.begin(), end = game_level->city_ctx->inside_buildings.end(); it != end; ++it)
			{
				const InsideBuilding& ib = **it;
				Area& a = Add1(draw_batch.areas);
				a.v[0] = Vec3(ib.enter_region.v1.x, ib.enter_y, ib.enter_region.v2.y);
				a.v[1] = Vec3(ib.enter_region.v2.x, ib.enter_y, ib.enter_region.v2.y);
				a.v[2] = Vec3(ib.enter_region.v1.x, ib.enter_y, ib.enter_region.v1.y);
				a.v[3] = Vec3(ib.enter_region.v2.x, ib.enter_y, ib.enter_region.v1.y);
			}
		}

		if(!game_level->city_ctx || !IsSet(game_level->city_ctx->flags, City::HaveExit))
		{
			const float H1 = -10.f;
			const float H2 = 30.f;

			// góra
			{
				Area& a = Add1(draw_batch.areas);
				a.v[0] = Vec3(33.f, H1, 256.f - 33.f);
				a.v[1] = Vec3(33.f, H2, 256.f - 33.f);
				a.v[2] = Vec3(256.f - 33.f, H1, 256.f - 33.f);
				a.v[3] = Vec3(256.f - 33.f, H2, 256.f - 33.f);
			}

			// dół
			{
				Area& a = Add1(draw_batch.areas);
				a.v[0] = Vec3(33.f, H1, 33.f);
				a.v[1] = Vec3(256.f - 33.f, H1, 33.f);
				a.v[2] = Vec3(33.f, H2, 33.f);
				a.v[3] = Vec3(256.f - 33.f, H2, 33.f);
			}

			// lewa
			{
				Area& a = Add1(draw_batch.areas);
				a.v[0] = Vec3(33.f, H1, 33.f);
				a.v[1] = Vec3(33.f, H2, 33.f);
				a.v[2] = Vec3(33.f, H1, 256.f - 33.f);
				a.v[3] = Vec3(33.f, H2, 256.f - 33.f);
			}

			// prawa
			{
				Area& a = Add1(draw_batch.areas);
				a.v[0] = Vec3(256.f - 33.f, H1, 256.f - 33.f);
				a.v[1] = Vec3(256.f - 33.f, H2, 256.f - 33.f);
				a.v[2] = Vec3(256.f - 33.f, H1, 33.f);
				a.v[3] = Vec3(256.f - 33.f, H2, 33.f);
			}
		}
		draw_batch.area_range = 10.f;
	}
	else if(area.area_type == LevelArea::Type::Inside)
	{
		InsideLocation* inside = static_cast<InsideLocation*>(game_level->location);
		InsideLocationLevel& lvl = inside->GetLevelData();

		if(inside->HaveUpStairs())
		{
			Area& a = Add1(draw_batch.areas);
			a.v[0] = a.v[1] = a.v[2] = a.v[3] = PtToPos(lvl.staircase_up);
			switch(lvl.staircase_up_dir)
			{
			case GDIR_DOWN:
				a.v[0] += Vec3(-0.85f, 2.87f, 0.85f);
				a.v[1] += Vec3(0.85f, 2.87f, 0.85f);
				a.v[2] += Vec3(-0.85f, 0.83f, 0.85f);
				a.v[3] += Vec3(0.85f, 0.83f, 0.85f);
				break;
			case GDIR_UP:
				a.v[0] += Vec3(0.85f, 2.87f, -0.85f);
				a.v[1] += Vec3(-0.85f, 2.87f, -0.85f);
				a.v[2] += Vec3(0.85f, 0.83f, -0.85f);
				a.v[3] += Vec3(-0.85f, 0.83f, -0.85f);
				break;
			case GDIR_RIGHT:
				a.v[0] += Vec3(-0.85f, 2.87f, -0.85f);
				a.v[1] += Vec3(-0.85f, 2.87f, 0.85f);
				a.v[2] += Vec3(-0.85f, 0.83f, -0.85f);
				a.v[3] += Vec3(-0.85f, 0.83f, 0.85f);
				break;
			case GDIR_LEFT:
				a.v[0] += Vec3(0.85f, 2.87f, 0.85f);
				a.v[1] += Vec3(0.85f, 2.87f, -0.85f);
				a.v[2] += Vec3(0.85f, 0.83f, 0.85f);
				a.v[3] += Vec3(0.85f, 0.83f, -0.85f);
				break;
			}
		}
		if(inside->HaveDownStairs())
		{
			Area& a = Add1(draw_batch.areas);
			a.v[0] = a.v[1] = a.v[2] = a.v[3] = PtToPos(lvl.staircase_down);
			switch(lvl.staircase_down_dir)
			{
			case GDIR_DOWN:
				a.v[0] += Vec3(-0.85f, 0.45f, 0.85f);
				a.v[1] += Vec3(0.85f, 0.45f, 0.85f);
				a.v[2] += Vec3(-0.85f, -1.55f, 0.85f);
				a.v[3] += Vec3(0.85f, -1.55f, 0.85f);
				break;
			case GDIR_UP:
				a.v[0] += Vec3(0.85f, 0.45f, -0.85f);
				a.v[1] += Vec3(-0.85f, 0.45f, -0.85f);
				a.v[2] += Vec3(0.85f, -1.55f, -0.85f);
				a.v[3] += Vec3(-0.85f, -1.55f, -0.85f);
				break;
			case GDIR_RIGHT:
				a.v[0] += Vec3(-0.85f, 0.45f, -0.85f);
				a.v[1] += Vec3(-0.85f, 0.45f, 0.85f);
				a.v[2] += Vec3(-0.85f, -1.55f, -0.85f);
				a.v[3] += Vec3(-0.85f, -1.55f, 0.85f);
				break;
			case GDIR_LEFT:
				a.v[0] += Vec3(0.85f, 0.45f, 0.85f);
				a.v[1] += Vec3(0.85f, 0.45f, -0.85f);
				a.v[2] += Vec3(0.85f, -1.55f, 0.85f);
				a.v[3] += Vec3(0.85f, -1.55f, -0.85f);
				break;
			}
		}
		draw_batch.area_range = 5.f;
	}
	else
	{
		// exit from building
		Area& a = Add1(draw_batch.areas);
		const Box2d& region = static_cast<InsideBuilding&>(area).exit_region;
		a.v[0] = Vec3(region.v1.x, 0.1f, region.v2.y);
		a.v[1] = Vec3(region.v2.x, 0.1f, region.v2.y);
		a.v[2] = Vec3(region.v1.x, 0.1f, region.v1.y);
		a.v[3] = Vec3(region.v2.x, 0.1f, region.v1.y);
		draw_batch.area_range = 5.f;
	}

	// action area2
	if(pc->data.ability_ready)
		PrepareAreaPath();
	else if(pc->unit->action == A_CAST && Any(pc->unit->act.cast.ability->type, Ability::Point, Ability::Ray, Ability::Summon))
		pc->unit->target_pos = pc->RaytestTarget(pc->unit->act.cast.ability->range);
	else if(pc->unit->weapon_state == WeaponState::Taken
		&& ((pc->unit->weapon_taken == W_BOW && Any(pc->unit->action, A_NONE, A_SHOOT))
			|| (pc->unit->weapon_taken == W_ONE_HANDED && IsSet(pc->unit->GetWeapon().flags, ITEM_WAND)
				&& Any(pc->unit->action, A_NONE, A_ATTACK, A_CAST, A_BLOCK, A_BASH))))
		pc->unit->target_pos = pc->RaytestTarget(50.f);

	if(draw_hitbox && pc->ShouldUseRaytest())
	{
		Vec3 pos;
		if(pc->data.ability_ready)
			pos = pc->data.ability_point;
		else
			pos = pc->unit->target_pos;
		DebugSceneNode* node = DebugSceneNode::Get();
		node->mat = Matrix::Scale(0.25f) * Matrix::Translation(pos) * game_level->camera.mat_view_proj;
		node->type = DebugSceneNode::Sphere;
		node->group = DebugSceneNode::ParticleRadius;
		draw_batch.debug_nodes.push_back(node);
	}
}

//=================================================================================================
void Game::PrepareAreaPath()
{
	PlayerController::CanUseAbilityResult result = pc->CanUseAbility(pc->data.ability_ready);
	if(result != PlayerController::CanUseAbilityResult::Yes)
	{
		if(!(result == PlayerController::CanUseAbilityResult::TakeWand && pc->unit->action == A_TAKE_WEAPON && pc->unit->weapon_taken == W_ONE_HANDED))
		{
			pc->data.ability_ready = nullptr;
			sound_mgr->PlaySound2d(game_res->sCancel);
		}
		return;
	}

	if(pc->unit->action == A_CAST && pc->unit->animation_state == AS_CAST_ANIMATION)
		return;

	Ability& ability = *pc->data.ability_ready;
	switch(ability.type)
	{
	case Ability::Charge:
		{
			Area2* area_ptr = Area2::Get();
			Area2& area = *area_ptr;
			area.ok = 2;
			draw_batch.areas2.push_back(area_ptr);

			const Vec3 from = pc->unit->GetPhysicsPos();
			const float h = 0.06f;
			const float rot = Clip(pc->unit->rot + PI + pc->data.ability_rot);
			const int steps = 10;

			// find max line
			float t;
			Vec3 dir(sin(rot) * ability.range, 0, cos(rot) * ability.range);
			bool ignore_units = IsSet(ability.flags, Ability::IgnoreUnits);
			game_level->LineTest(pc->unit->cobj->getCollisionShape(), from, dir, [this, ignore_units](btCollisionObject* obj, bool)
			{
				int flags = obj->getCollisionFlags();
				if(IsSet(flags, CG_TERRAIN))
					return LT_IGNORE;
				if(IsSet(flags, CG_UNIT))
				{
					Unit* unit = reinterpret_cast<Unit*>(obj->getUserPointer());
					if(ignore_units || unit == pc->unit || !unit->IsStanding())
						return LT_IGNORE;
				}
				return LT_COLLIDE;
			}, t);

			float len = ability.range * t;

			if(game_level->location->outside && pc->unit->area->area_type == LevelArea::Type::Outside)
			{
				// build line on terrain
				area.points.clear();
				area.faces.clear();

				Vec3 active_pos = pc->unit->pos;
				Vec3 step = dir * t / (float)steps;
				Vec3 unit_offset(pc->unit->pos.x, 0, pc->unit->pos.z);
				float len_step = len / steps;
				float active_step = 0;
				Matrix mat = Matrix::RotationY(rot);
				for(int i = 0; i < steps; ++i)
				{
					float current_h = game_level->terrain->GetH(active_pos) + h;
					area.points.push_back(Vec3::Transform(Vec3(-ability.width, current_h, active_step), mat) + unit_offset);
					area.points.push_back(Vec3::Transform(Vec3(+ability.width, current_h, active_step), mat) + unit_offset);

					active_pos += step;
					active_step += len_step;
				}

				for(int i = 0; i < steps - 1; ++i)
				{
					area.faces.push_back(i * 2);
					area.faces.push_back((i + 1) * 2);
					area.faces.push_back(i * 2 + 1);
					area.faces.push_back((i + 1) * 2);
					area.faces.push_back(i * 2 + 1);
					area.faces.push_back((i + 1) * 2 + 1);
				}
			}
			else
			{
				// build line on flat terrain
				area.points.resize(4);
				area.points[0] = Vec3(-ability.width, h, 0);
				area.points[1] = Vec3(-ability.width, h, len);
				area.points[2] = Vec3(ability.width, h, 0);
				area.points[3] = Vec3(ability.width, h, len);

				Matrix mat = Matrix::RotationY(rot);
				for(int i = 0; i < 4; ++i)
					area.points[i] = Vec3::Transform(area.points[i], mat) + pc->unit->pos;

				area.faces.clear();
				area.faces.push_back(0);
				area.faces.push_back(1);
				area.faces.push_back(2);
				area.faces.push_back(1);
				area.faces.push_back(2);
				area.faces.push_back(3);
			}

			pc->data.ability_ok = true;
		}
		break;

	case Ability::Summon:
		pc->data.ability_point = pc->RaytestTarget(pc->data.ability_ready->range);
		if(pc->data.range_ratio < 1.f)
		{
			Area2* area = Area2::Get();
			draw_batch.areas2.push_back(area);

			const float radius = ability.width / 2;
			Vec3 dir = pc->data.ability_point - pc->unit->pos;
			dir.y = 0;
			float dist = dir.Length();
			dir.Normalize();

			Vec3 from = pc->data.ability_point;
			from.y = 0;

			bool ok = false;
			for(int i = 0; i < 20; ++i)
			{
				game_level->global_col.clear();
				game_level->GatherCollisionObjects(*pc->unit->area, game_level->global_col, from, radius);
				if(!game_level->Collide(game_level->global_col, from, radius))
				{
					ok = true;
					break;
				}
				from -= dir * 0.1f;
				dist -= 0.1f;
				if(dist <= 0.f)
					break;
			}

			if(ok)
			{
				pc->data.ability_ok = true;
				if(game_level->terrain)
					game_level->terrain->SetH(from);
				pc->data.ability_point = from;
				area->ok = 2;
			}
			else
			{
				pc->data.ability_ok = false;
				area->ok = 0;
			}
			PrepareAreaPathCircle(*area, pc->data.ability_point, radius);
		}
		else
			pc->data.ability_ok = false;
		break;

	case Ability::Target:
		{
			Unit* target;
			if(GKey.KeyDownAllowed(GK_MOVE_BACK))
			{
				// cast on self
				target = pc->unit;
			}
			else
			{
				// raytest - can be cast on alive units or dead team members
				const float range = ability.range + game_level->camera.dist;
				RaytestClosestUnitCallback clbk([=](Unit* unit)
				{
					if(unit == pc->unit)
						return false;
					if(!unit->IsAlive() && !unit->IsTeamMember())
						return false;
					return true;
				});
				Vec3 from = game_level->camera.from;
				Vec3 dir = (game_level->camera.to - from).Normalized();
				from += dir * game_level->camera.dist;
				Vec3 to = from + dir * range;
				phy_world->rayTest(ToVector3(from), ToVector3(to), clbk);
				target = clbk.hit;
			}

			if(target)
			{
				pc->data.ability_ok = true;
				pc->data.ability_point = target->pos;
				pc->data.ability_target = target;
			}
			else
			{
				pc->data.ability_ok = false;
				pc->data.ability_target = nullptr;
			}
		}
		break;

	case Ability::Ray:
	case Ability::Point:
		pc->data.ability_point = pc->RaytestTarget(pc->data.ability_ready->range);
		pc->data.ability_ok = true;
		pc->data.ability_target = nullptr;
		break;
	}
}

//=================================================================================================
void Game::PrepareAreaPathCircle(Area2& area, float radius, float range, float rot)
{
	const float h = 0.06f;
	const int circle_points = 10;

	area.points.resize(circle_points + 1);
	area.points[0] = Vec3(0, h, 0 + range);
	float angle = 0;
	for(int i = 0; i < circle_points; ++i)
	{
		area.points[i + 1] = Vec3(sin(angle) * radius, h, cos(angle) * radius + range);
		angle += (PI * 2) / circle_points;
	}

	bool outside = (game_level->location->outside && pc->unit->area->area_type == LevelArea::Type::Outside);
	Matrix mat = Matrix::RotationY(rot);
	for(int i = 0; i < circle_points + 1; ++i)
	{
		area.points[i] = Vec3::Transform(area.points[i], mat) + pc->unit->pos;
		if(outside)
			area.points[i].y = game_level->terrain->GetH(area.points[i]) + h;
	}

	area.faces.clear();
	for(int i = 0; i < circle_points; ++i)
	{
		area.faces.push_back(0);
		area.faces.push_back(i + 1);
		area.faces.push_back((i + 2) == (circle_points + 1) ? 1 : (i + 2));
	}
}

//=================================================================================================
void Game::PrepareAreaPathCircle(Area2& area, const Vec3& pos, float radius)
{
	const float h = 0.06f;
	const int circle_points = 10;

	area.points.resize(circle_points + 1);
	area.points[0] = pos + Vec3(0, h, 0);
	float angle = 0;
	for(int i = 0; i < circle_points; ++i)
	{
		area.points[i + 1] = pos + Vec3(sin(angle) * radius, h, cos(angle) * radius);
		angle += (PI * 2) / circle_points;
	}

	bool outside = (game_level->location->outside && pc->unit->area->area_type == LevelArea::Type::Outside);
	if(outside)
	{
		for(int i = 0; i < circle_points + 1; ++i)
			area.points[i].y = game_level->terrain->GetH(area.points[i]) + h;
	}

	area.faces.clear();
	for(int i = 0; i < circle_points; ++i)
	{
		area.faces.push_back(0);
		area.faces.push_back(i + 1);
		area.faces.push_back((i + 2) == (circle_points + 1) ? 1 : (i + 2));
	}
}

//=================================================================================================
void Game::GatherDrawBatchLights(LevelArea& area, SceneNode* node)
{
	assert(node);
	GatherDrawBatchLights(area, node, node->center.x, node->center.z, node->radius, node->subs & SceneNode::SPLIT_MASK, node->lights);
}

//=================================================================================================
void Game::GatherDrawBatchLights(LevelArea& area, SceneNode* node, float x, float z, float radius, int sub, array<Light*, 3>& lights)
{
	assert(radius > 0);

	TopN<Light*, 3, float, std::less<>> best(nullptr, game_level->camera.zfar);

	if(area.masks.empty())
	{
		for(Light& light : area.lights)
		{
			float dist = Distance(x, z, light.pos.x, light.pos.z);
			if(dist < light.range + radius)
				best.Add(&light, dist);
		}
	}
	else
	{
		const Vec2 obj_pos(x, z);
		const bool is_split = (node && IsSet(node->mesh->head.flags, Mesh::F_SPLIT));

		for(GameLight& light : area.lights)
		{
			Vec2 light_pos;
			float dist;
			bool ok = true, masked = false;
			if(!is_split)
			{
				dist = Distance(x, z, light.pos.x, light.pos.z);
				if(dist > light.range + radius || !best.CanAdd(dist))
					continue;
				if(!IsZero(dist))
				{
					light_pos = light.pos.XZ();
					float range_sum = 0.f;

					// are there any masks between object and light?
					for(LightMask& mask : area.masks)
					{
						if(LineToRectangleSize(obj_pos, light_pos, mask.pos, mask.size))
						{
							// move light to one side of mask
							Vec2 new_pos, new_pos2;
							float new_dist[2];
							if(mask.size.x > mask.size.y)
							{
								new_pos.x = mask.pos.x - mask.size.x;
								new_pos2.x = mask.pos.x + mask.size.x;
								new_pos.y = new_pos2.y = mask.pos.y;
							}
							else
							{
								new_pos.x = new_pos2.x = mask.pos.x;
								new_pos.y = mask.pos.y - mask.size.y;
								new_pos2.y = mask.pos.y + mask.size.y;
							}
							new_dist[0] = Vec2::Distance(light_pos, new_pos) + Vec2::Distance(new_pos, obj_pos);
							new_dist[1] = Vec2::Distance(light_pos, new_pos2) + Vec2::Distance(new_pos2, obj_pos);
							if(new_dist[1] < new_dist[0])
								new_pos = new_pos2;

							// recalculate distance
							range_sum += Vec2::Distance(light_pos, new_pos);
							dist = range_sum + Distance(x, z, new_pos.x, new_pos.y);
							if(!best.CanAdd(dist))
							{
								ok = false;
								break;
							}
							light_pos = new_pos;
							masked = true;
						}
					}
				}
			}
			else
			{
				const Vec2 sub_size = node->mesh->splits[sub].box.SizeXZ();
				light_pos = light.pos.XZ();
				dist = DistanceRectangleToPoint(obj_pos, sub_size, light_pos);
				if(dist > light.range + radius || !best.CanAdd(dist))
					continue;
				if(!IsZero(dist))
				{
					float range_sum = 0.f;

					// are there any masks between object and light?
					for(LightMask& mask : area.masks)
					{
						if(LineToRectangleSize(obj_pos, light_pos, mask.pos, mask.size))
						{
							// move light to one side of mask
							Vec2 new_pos, new_pos2;
							float new_dist[2];
							if(mask.size.x > mask.size.y)
							{
								new_pos.x = mask.pos.x - mask.size.x;
								new_pos2.x = mask.pos.x + mask.size.x;
								new_pos.y = new_pos2.y = mask.pos.y;
							}
							else
							{
								new_pos.x = new_pos2.x = mask.pos.x;
								new_pos.y = mask.pos.y - mask.size.y;
								new_pos2.y = mask.pos.y + mask.size.y;
							}
							new_dist[0] = Vec2::Distance(light_pos, new_pos) + DistanceRectangleToPoint(obj_pos, sub_size, new_pos);
							new_dist[1] = Vec2::Distance(light_pos, new_pos2) + DistanceRectangleToPoint(obj_pos, sub_size, new_pos2);
							if(new_dist[1] < new_dist[0])
								new_pos = new_pos2;

							// recalculate distance
							range_sum += Vec2::Distance(light_pos, new_pos);
							dist = range_sum + DistanceRectangleToPoint(obj_pos, sub_size, new_pos);
							if(!best.CanAdd(dist))
							{
								ok = false;
								break;
							}
							light_pos = new_pos;
							masked = true;
						}
					}
				}
			}

			if(!ok)
				continue;

			if(masked)
			{
				float range = light.range - Vec2::Distance(light_pos, light.pos.XZ());
				if(range > 0)
				{
					Light* tmp_light = DrawBatch::light_pool.Get();
					tmp_light->color = light.color;
					tmp_light->pos = Vec3(light_pos.x, light.pos.y, light_pos.y);
					tmp_light->range = range;
					best.Add(tmp_light, dist);
					draw_batch.tmp_lights.push_back(tmp_light);
				}
			}
			else
				best.Add(&light, dist);
		}
	}

	for(int i = 0; i < 3; ++i)
		lights[i] = best[i];
}

//=================================================================================================
void Game::DrawScene(bool outside)
{
	PROFILER_BLOCK("DrawScene");

	scene_mgr->scene = game_level->scene;
	scene_mgr->scene->use_light_dir = outside;
	scene_mgr->camera = &game_level->camera;

	// niebo
	if(outside && IsSet(draw_flags, DF_SKYBOX))
		skybox_shader->Draw(*game_res->aSkybox, game_level->camera);

	// terrain
	if(!draw_batch.terrain_parts.empty())
	{
		PROFILER_BLOCK("DrawTerrain");
		terrain_shader->Draw(game_level->scene, &game_level->camera, game_level->terrain, draw_batch.terrain_parts);
	}

	// dungeon
	if(!draw_batch.dungeon_parts.empty())
	{
		PROFILER_BLOCK("DrawDugneon");
		DrawDungeon(draw_batch.dungeon_parts, draw_batch.dungeon_part_groups);
	}

	// nodes
	if(!draw_batch.nodes.empty())
	{
		PROFILER_BLOCK("DrawSceneNodes");
		scene_mgr->DrawSceneNodes(draw_batch.nodes, draw_batch.node_groups);
	}

	// grass
	DrawGrass();

	// debug nodes
	if(!draw_batch.debug_nodes.empty())
		DrawDebugNodes(draw_batch.debug_nodes);
	if(pathfinding->IsDebugDraw())
	{
		basic_shader->Prepare(game_level->camera);
		pathfinding->Draw(basic_shader);
	}

	// blood
	if(!draw_batch.bloods.empty())
		DrawBloods(draw_batch.bloods, outside);

	// particles
	if(!draw_batch.billboards.empty() || !draw_batch.pes.empty() || draw_batch.tpes)
	{
		particle_shader->Begin(game_level->camera);
		if(!draw_batch.billboards.empty())
			particle_shader->DrawBillboards(draw_batch.billboards);
		if(draw_batch.tpes)
			particle_shader->DrawTrailParticles(*draw_batch.tpes);
		if(!draw_batch.pes.empty())
			particle_shader->DrawParticles(draw_batch.pes);
		particle_shader->End();
	}

	// alpha nodes
	if(!draw_batch.alpha_nodes.empty())
		scene_mgr->DrawAlphaSceneNodes(draw_batch.alpha_nodes);

	// areas
	if(!draw_batch.areas.empty() || !draw_batch.areas2.empty())
		DrawAreas(draw_batch.areas, draw_batch.area_range, draw_batch.areas2);
}

//=================================================================================================
// nie zoptymalizowane, póki co wyświetla jeden obiekt (lub kilka ale dobrze posortowanych w przypadku postaci z przedmiotami)
void Game::DrawGlowingNodes(const vector<GlowNode>& glow_nodes, bool use_postfx)
{
	PROFILER_BLOCK("DrawGlowingNodes");

	IDirect3DDevice9* device = render->GetDevice();
	ID3DXEffect* effect = glow_shader->effect;

	// ustaw flagi renderowania
	render->SetAlphaBlend(false);
	render->SetAlphaTest(false);
	render->SetNoCulling(false);
	render->SetNoZWrite(true);
	V(device->SetRenderState(D3DRS_STENCILENABLE, TRUE));
	V(device->SetRenderState(D3DRS_STENCILREF, 1));
	V(device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE));
	V(device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS));

	// ustaw render target
	SURFACE render_surface;
	if(!render->IsMultisamplingEnabled())
		V(postfx_shader->tex[0]->GetSurfaceLevel(0, &render_surface));
	else
		render_surface = postfx_shader->surf[0];
	V(device->SetRenderTarget(0, render_surface));
	V(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0, 0));
	V(device->BeginScene());

	// renderuj wszystkie obiekty
	int prev_mode = -1;
	Vec4 glow_color;
	uint passes;

	for(const GlowNode& glow : glow_nodes)
	{
		render->SetAlphaTest(glow.alpha);

		// animowany czy nie?
		Mesh* mesh = glow.node->mesh;
		if(IsSet(glow.node->flags, SceneNode::F_ANIMATED))
		{
			if(prev_mode != 1)
			{
				if(prev_mode != -1)
				{
					V(effect->EndPass());
					V(effect->End());
				}
				prev_mode = 1;
				V(effect->SetTechnique(glow_shader->techGlowAni));
				V(effect->Begin(&passes, 0));
				V(effect->BeginPass(0));
			}
			vector<Matrix>& mat_bones = glow.node->mesh_inst->mat_bones;
			V(effect->SetMatrixArray(glow_shader->hMatBones, (D3DXMATRIX*)mat_bones.data(), mat_bones.size()));
		}
		else
		{
			if(prev_mode != 0)
			{
				if(prev_mode != -1)
				{
					V(effect->EndPass());
					V(effect->End());
				}
				prev_mode = 0;
				V(effect->SetTechnique(glow_shader->techGlowMesh));
				V(effect->Begin(&passes, 0));
				V(effect->BeginPass(0));
			}
		}

		// wybierz kolor
		if(glow.type == GlowNode::Unit)
		{
			Unit& unit = *static_cast<Unit*>(glow.ptr);
			if(pc->unit->IsEnemy(unit))
				glow_color = Vec4(1, 0, 0, 1);
			else if(pc->unit->IsFriend(unit))
				glow_color = Vec4(0, 1, 0, 1);
			else
				glow_color = Vec4(1, 1, 0, 1);
		}
		else
			glow_color = Vec4(1, 1, 1, 1);

		// ustawienia shadera
		Matrix m1 = glow.node->mat * game_level->camera.mat_view_proj;
		V(effect->SetMatrix(glow_shader->hMatCombined, (D3DXMATRIX*)&m1));
		V(effect->SetVector(glow_shader->hColor, (D3DXVECTOR4*)&glow_color));
		V(effect->CommitChanges());

		// ustawienia modelu
		V(device->SetVertexDeclaration(render->GetVertexDeclaration(mesh->vertex_decl)));
		V(device->SetStreamSource(0, mesh->vb, 0, mesh->vertex_size));
		V(device->SetIndices(mesh->ib));

		// render mesh
		if(glow.alpha)
		{
			// for glow need to set texture per submesh
			for(int i = 0; i < mesh->head.n_subs; ++i)
			{
				if(i == 0 || glow.type != GlowNode::Door)
				{
					V(effect->SetTexture(glow_shader->hTex, mesh->subs[i].tex->tex));
					V(effect->CommitChanges());
					V(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, mesh->subs[i].min_ind, mesh->subs[i].n_ind, mesh->subs[i].first * 3, mesh->subs[i].tris));
				}
			}
		}
		else
		{
			V(effect->SetTexture(glow_shader->hTex, game_res->tBlack->tex));
			V(effect->CommitChanges());
			for(int i = 0; i < mesh->head.n_subs; ++i)
			{
				if(i == 0 || glow.type != GlowNode::Door)
					V(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, mesh->subs[i].min_ind, mesh->subs[i].n_ind, mesh->subs[i].first * 3, mesh->subs[i].tris));
			}
		}
	}

	V(effect->EndPass());
	V(effect->End());
	V(device->EndScene());

	V(device->SetRenderState(D3DRS_ZENABLE, FALSE));
	V(device->SetRenderState(D3DRS_STENCILENABLE, FALSE));

	//======================================================================
	// w teksturze są teraz wyrenderowane obiekty z kolorem glow
	// trzeba rozmyć teksturę, napierw po X
	effect = postfx_shader->effect;

	TEX tex;
	if(!render->IsMultisamplingEnabled())
	{
		render_surface->Release();
		V(postfx_shader->tex[1]->GetSurfaceLevel(0, &render_surface));
		tex = postfx_shader->tex[0];
	}
	else
	{
		SURFACE tex_surface;
		V(postfx_shader->tex[0]->GetSurfaceLevel(0, &tex_surface));
		V(device->StretchRect(render_surface, nullptr, tex_surface, nullptr, D3DTEXF_NONE));
		tex_surface->Release();
		tex = postfx_shader->tex[0];
		render_surface = postfx_shader->surf[1];
	}
	V(device->SetRenderTarget(0, render_surface));
	V(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0, 0));

	// ustawienia shadera
	V(effect->SetTechnique(postfx_shader->techBlurX));
	V(effect->SetTexture(postfx_shader->hTex, tex));
	// chcę żeby rozmiar efektu był % taki sam w każdej rozdzielczości (już tak nie jest)
	const float base_range = 2.5f;
	const float range_x = (base_range / 1024.f);// *(wnd_size.x/1024.f);
	const float range_y = (base_range / 768.f);// *(wnd_size.x/768.f);
	V(effect->SetVector(postfx_shader->hSkill, (D3DXVECTOR4*)&Vec4(range_x, range_y, 0, 0)));
	V(effect->SetFloat(postfx_shader->hPower, 1));

	// ustawienia modelu
	V(device->SetVertexDeclaration(render->GetVertexDeclaration(VDI_TEX)));
	V(device->SetStreamSource(0, postfx_shader->vbFullscreen, 0, sizeof(VTex)));

	// renderowanie
	V(device->BeginScene());
	V(effect->Begin(&passes, 0));
	V(effect->BeginPass(0));
	V(device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2));
	V(effect->EndPass());
	V(effect->End());
	V(device->EndScene());

	//======================================================================
	// rozmywanie po Y
	if(!render->IsMultisamplingEnabled())
	{
		render_surface->Release();
		V(postfx_shader->tex[0]->GetSurfaceLevel(0, &render_surface));
		tex = postfx_shader->tex[1];
	}
	else
	{
		SURFACE tex_surface;
		V(postfx_shader->tex[0]->GetSurfaceLevel(0, &tex_surface));
		V(device->StretchRect(render_surface, nullptr, tex_surface, nullptr, D3DTEXF_NONE));
		tex_surface->Release();
		tex = postfx_shader->tex[0];
		render_surface = postfx_shader->surf[0];
	}
	V(device->SetRenderTarget(0, render_surface));

	// ustawienia shadera
	V(effect->SetTechnique(postfx_shader->techBlurY));
	V(effect->SetTexture(postfx_shader->hTex, tex));

	// renderowanie
	V(device->BeginScene());
	V(effect->Begin(&passes, 0));
	V(effect->BeginPass(0));
	V(device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2));
	V(effect->EndPass());
	V(effect->End());
	V(device->EndScene());

	//======================================================================
	// Renderowanie tekstury z glow na ekran gry
	// ustaw normalny render target
	if(!render->IsMultisamplingEnabled())
	{
		render_surface->Release();
		tex = postfx_shader->tex[1];
	}
	else
	{
		SURFACE tex_surface;
		V(postfx_shader->tex[0]->GetSurfaceLevel(0, &tex_surface));
		V(device->StretchRect(render_surface, nullptr, tex_surface, nullptr, D3DTEXF_NONE));
		tex_surface->Release();
		tex = postfx_shader->tex[0];
	}
	if(!use_postfx)
	{
		V(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &render_surface));
		V(device->SetRenderTarget(0, render_surface));
		render_surface->Release();
	}
	else
	{
		if(!render->IsMultisamplingEnabled())
		{
			V(postfx_shader->tex[2]->GetSurfaceLevel(0, &render_surface));
			render_surface->Release();
		}
		else
			render_surface = postfx_shader->surf[2];
		V(device->SetRenderTarget(0, render_surface));
	}

	// ustaw potrzebny render state
	V(device->SetRenderState(D3DRS_STENCILENABLE, TRUE));
	V(device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP));
	V(device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL));
	V(device->SetRenderState(D3DRS_STENCILREF, 0));
	render->SetAlphaBlend(true);
	V(device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
	V(device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
	V(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));

	// ustawienia shadera
	V(effect->SetTexture(postfx_shader->hTex, tex));

	// renderowanie
	V(device->BeginScene());
	V(effect->Begin(&passes, 0));
	V(effect->BeginPass(0));
	V(device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2));
	V(effect->EndPass());
	V(effect->End());
	V(device->EndScene());

	// przywróć ustawienia
	V(device->SetRenderState(D3DRS_ZENABLE, TRUE));
	V(device->SetRenderState(D3DRS_STENCILENABLE, FALSE));
	V(device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
	V(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
}

//=================================================================================================
void Game::DrawDungeon(const vector<DungeonPart>& parts, const vector<DungeonPartGroup>& groups)
{
	IDirect3DDevice9* device = render->GetDevice();
	SuperShader* shader = scene_mgr->super_shader;

	render->SetAlphaBlend(false);
	render->SetAlphaTest(false);
	render->SetNoCulling(false);
	render->SetNoZWrite(false);

	V(device->SetVertexDeclaration(render->GetVertexDeclaration(VDI_TANGENT)));
	V(device->SetStreamSource(0, dun_mesh_builder->vbDungeon, 0, sizeof(VTangent)));
	V(device->SetIndices(dun_mesh_builder->ibDungeon));

	const bool use_fog = scene_mgr->use_fog && scene_mgr->use_lighting;

	int last_mode = -1;
	ID3DXEffect* e = nullptr;
	bool first = true;
	TexOverride* last_override = nullptr;
	uint passes;

	for(vector<DungeonPart>::const_iterator it = parts.begin(), end = parts.end(); it != end; ++it)
	{
		const DungeonPart& dp = *it;
		int mode = dp.tex_o->GetIndex();

		// change shader
		if(mode != last_mode)
		{
			last_mode = mode;
			if(!first)
			{
				V(e->EndPass());
				V(e->End());
			}
			e = shader->GetShader(shader->GetShaderId(false, true, use_fog, scene_mgr->use_specularmap && dp.tex_o->specular != nullptr,
				scene_mgr->use_normalmap && dp.tex_o->normal != nullptr, scene_mgr->use_lighting, false));
			if(first)
			{
				first = false;
				V(e->SetVector(shader->hTint, (D3DXVECTOR4*)&Vec4::One));
				V(e->SetVector(shader->hAmbientColor, (D3DXVECTOR4*)&game_level->scene->GetAmbientColor()));
				V(e->SetVector(shader->hFogColor, (D3DXVECTOR4*)&game_level->scene->GetFogColor()));
				V(e->SetVector(shader->hFogParams, (D3DXVECTOR4*)&game_level->scene->GetFogParams()));
				V(e->SetVector(shader->hCameraPos, (D3DXVECTOR4*)&game_level->camera.from));
				V(e->SetVector(shader->hSpecularColor, (D3DXVECTOR4*)&Vec4::One));
				V(e->SetFloat(shader->hSpecularIntensity, 0.2f));
				V(e->SetFloat(shader->hSpecularHardness, 10));
			}
			V(e->Begin(&passes, 0));
			V(e->BeginPass(0));
		}

		// set textures
		if(last_override != dp.tex_o)
		{
			last_override = dp.tex_o;
			V(e->SetTexture(shader->hTexDiffuse, last_override->diffuse->tex));
			if(scene_mgr->use_normalmap && last_override->normal)
				V(e->SetTexture(shader->hTexNormal, last_override->normal->tex));
			if(scene_mgr->use_specularmap && last_override->specular)
				V(e->SetTexture(shader->hTexSpecular, last_override->specular->tex));
		}

		// set matrices
		const DungeonPartGroup& group = groups[dp.group];
		V(e->SetMatrix(shader->hMatCombined, (D3DXMATRIX*)&group.mat_combined));
		V(e->SetMatrix(shader->hMatWorld, (D3DXMATRIX*)&group.mat_world));

		// set lights
		Lights lights;
		for(int i = 0; i < 3; ++i)
		{
			if(group.lights[i])
				memcpy(&lights.ld[i], group.lights[i], sizeof(Light));
			else
			{
				lights.ld[i].pos = Vec3::Zero;
				lights.ld[i].range = 1.f;
				lights.ld[i].color = Vec4::Zero;
			}
		}
		V(e->SetRawValue(shader->hLights, &lights, 0, sizeof(Lights)));

		// draw
		V(e->CommitChanges());
		V(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 76, dp.start_index, dp.primitive_count));
	}

	V(e->EndPass());
	V(e->End());
}

//=================================================================================================
void Game::DrawDebugNodes(const vector<DebugSceneNode*>& nodes)
{
	IDirect3DDevice9* device = render->GetDevice();
	ID3DXEffect* effect = basic_shader->effect;

	render->SetAlphaTest(false);
	render->SetAlphaBlend(false);
	render->SetNoCulling(true);
	render->SetNoZWrite(false);

	V(device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME));
	V(device->SetRenderState(D3DRS_ZENABLE, FALSE));

	uint passes;
	V(effect->SetTechnique(basic_shader->techSimple));
	V(effect->Begin(&passes, 0));
	V(effect->BeginPass(0));

	static Mesh* meshes[DebugSceneNode::MaxType] = {
		game_res->aBox,
		game_res->aCylinder,
		game_res->aSphere,
		game_res->aCapsule
	};

	static Vec4 colors[DebugSceneNode::MaxGroup] = {
		Vec4(0,0,0,1),
		Vec4(1,1,1,1),
		Vec4(0,1,0,1),
		Vec4(153.f / 255,217.f / 255,164.f / 234,1.f),
		Vec4(163.f / 255,73.f / 255,164.f / 255,1.f)
	};

	for(vector<DebugSceneNode*>::const_iterator it = nodes.begin(), end = nodes.end(); it != end; ++it)
	{
		const DebugSceneNode& node = **it;

		V(effect->SetVector(basic_shader->hColor, (D3DXVECTOR4*)&colors[node.group]));
		V(effect->SetMatrix(basic_shader->hMatCombined, (D3DXMATRIX*)&node.mat));

		if(node.type == DebugSceneNode::TriMesh)
		{
			// currently only dungeon mesh is supported here
			assert(reinterpret_cast<btTriangleIndexVertexArray*>(node.mesh_ptr) == game_level->dungeon_shape_data);
			V(device->SetVertexDeclaration(render->GetVertexDeclaration(VDI_POS)));
			V(effect->CommitChanges());

			V(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, game_level->dungeon_shape_pos.size(), game_level->dungeon_shape_index.size() / 3, game_level->dungeon_shape_index.data(),
				D3DFMT_INDEX32, game_level->dungeon_shape_pos.data(), sizeof(Vec3)));
		}
		else
		{
			Mesh* mesh = meshes[node.type];
			V(device->SetVertexDeclaration(render->GetVertexDeclaration(mesh->vertex_decl)));
			V(device->SetStreamSource(0, mesh->vb, 0, mesh->vertex_size));
			V(device->SetIndices(mesh->ib));
			V(effect->CommitChanges());

			for(int i = 0; i < mesh->head.n_subs; ++i)
				V(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, mesh->subs[i].min_ind, mesh->subs[i].n_ind, mesh->subs[i].first * 3, mesh->subs[i].tris));
		}
	}

	V(effect->EndPass());
	V(effect->End());

	V(device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID));
	V(device->SetRenderState(D3DRS_ZENABLE, TRUE));
}

//=================================================================================================
void Game::DrawBloods(const vector<Blood*>& bloods, bool outside)
{
	IDirect3DDevice9* device = render->GetDevice();
	SuperShader* shader = scene_mgr->super_shader;

	render->SetAlphaTest(false);
	render->SetAlphaBlend(true);
	render->SetNoCulling(false);
	render->SetNoZWrite(true);

	const bool use_fog = scene_mgr->use_lighting && scene_mgr->use_fog;

	ID3DXEffect* e = shader->GetShader(
		shader->GetShaderId(false, false, use_fog, false, false, !outside && scene_mgr->use_lighting, outside && scene_mgr->use_lighting));
	V(device->SetVertexDeclaration(render->GetVertexDeclaration(VDI_DEFAULT)));
	V(e->SetVector(shader->hTint, (D3DXVECTOR4*)&Vec4::One));

	uint passes;
	V(e->Begin(&passes, 0));
	V(e->BeginPass(0));

	for(vector<Blood*>::const_iterator it = bloods.begin(), end = bloods.end(); it != end; ++it)
	{
		const Blood& blood = **it;

		// set blood vertices
		for(int i = 0; i < 4; ++i)
			blood_v[i].normal = blood.normal;

		const float s = blood.size * blood.scale,
			r = blood.rot;

		if(blood.normal.Equal(Vec3(0, 1, 0)))
		{
			blood_v[0].pos.x = s * sin(r + 5.f / 4 * PI);
			blood_v[0].pos.z = s * cos(r + 5.f / 4 * PI);
			blood_v[1].pos.x = s * sin(r + 7.f / 4 * PI);
			blood_v[1].pos.z = s * cos(r + 7.f / 4 * PI);
			blood_v[2].pos.x = s * sin(r + 3.f / 4 * PI);
			blood_v[2].pos.z = s * cos(r + 3.f / 4 * PI);
			blood_v[3].pos.x = s * sin(r + 1.f / 4 * PI);
			blood_v[3].pos.z = s * cos(r + 1.f / 4 * PI);
		}
		else
		{
			const Vec3 front(sin(r), 0, cos(r)), right(sin(r + PI / 2), 0, cos(r + PI / 2));
			Vec3 v_x, v_z, v_lx, v_rx, v_lz, v_rz;
			v_x = blood.normal.Cross(front);
			v_z = blood.normal.Cross(right);
			if(v_x.x > 0.f)
			{
				v_rx = v_x * s;
				v_lx = -v_x * s;
			}
			else
			{
				v_rx = -v_x * s;
				v_lx = v_x * s;
			}
			if(v_z.z > 0.f)
			{
				v_rz = v_z * s;
				v_lz = -v_z * s;
			}
			else
			{
				v_rz = -v_z * s;
				v_lz = v_z * s;
			}

			blood_v[0].pos = v_lx + v_lz;
			blood_v[1].pos = v_lx + v_rz;
			blood_v[2].pos = v_rx + v_lz;
			blood_v[3].pos = v_rx + v_rz;
		}

		// setup shader
		Matrix m1 = Matrix::Translation(blood.pos);
		Matrix m2 = m1 * game_level->camera.mat_view_proj;
		V(e->SetMatrix(shader->hMatCombined, (D3DXMATRIX*)&m2));
		V(e->SetMatrix(shader->hMatWorld, (D3DXMATRIX*)&m1));
		V(e->SetTexture(shader->hTexDiffuse, game_res->tBloodSplat[blood.type]->tex));

		// lights
		if(!outside)
			shader->ApplyLights(blood.lights);

		// draw
		V(e->CommitChanges());
		V(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, blood_v, sizeof(VDefault)));
	}

	V(e->EndPass());
	V(e->End());
}

//=================================================================================================
void Game::DrawAreas(const vector<Area>& areas, float range, const vector<Area2*>& areas2)
{
	IDirect3DDevice9* device = render->GetDevice();
	ID3DXEffect* effect = basic_shader->effect;

	render->SetAlphaTest(false);
	render->SetAlphaBlend(true);
	render->SetNoCulling(true);
	render->SetNoZWrite(true);

	V(device->SetVertexDeclaration(render->GetVertexDeclaration(VDI_POS)));

	V(effect->SetTechnique(basic_shader->techArea));
	V(effect->SetMatrix(basic_shader->hMatCombined, (D3DXMATRIX*)&game_level->camera.mat_view_proj));
	V(effect->SetVector(basic_shader->hColor, (D3DXVECTOR4*)&Vec4(0, 1, 0, 0.5f)));
	Vec4 playerPos(pc->unit->pos, 1.f);
	playerPos.y += 0.75f;
	V(effect->SetVector(basic_shader->hPlayerPos, (D3DXVECTOR4*)&playerPos));
	V(effect->SetFloat(basic_shader->hRange, range));
	uint passes;
	V(effect->Begin(&passes, 0));
	V(effect->BeginPass(0));

	for(const Area& area : areas)
		V(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, (const void*)&area.v[0], sizeof(Vec3)));

	V(effect->EndPass());
	V(effect->End());

	if(!areas2.empty())
	{
		V(effect->Begin(&passes, 0));
		V(effect->BeginPass(0));
		V(effect->SetFloat(basic_shader->hRange, 100.f));

		static const Vec4 colors[3] = {
			Vec4(1, 0, 0, 0.5f),
			Vec4(1, 1, 0, 0.5f),
			Vec4(0, 0.58f, 1.f, 0.5f)
		};

		for(auto* area2 : areas2)
		{
			V(effect->SetVector(basic_shader->hColor, (D3DXVECTOR4*)&colors[area2->ok]));
			V(effect->CommitChanges());
			V(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, area2->points.size(), area2->faces.size() / 3, area2->faces.data(), D3DFMT_INDEX16,
				area2->points.data(), sizeof(Vec3)));
		}

		V(effect->EndPass());
		V(effect->End());
	}
}

//=================================================================================================
void Game::UvModChanged()
{
	game_level->terrain->uv_mod = uv_mod;
	game_level->terrain->RebuildUv();
}
