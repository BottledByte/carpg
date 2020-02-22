#include "Pch.h"
#include "GameCamera.h"

GameCamera::GameCamera(float springiness) : springiness(springiness), reset(2), free_rot(false)
{
}

void GameCamera::Reset()
{
	reset = 2;
	free_rot = false;
	from = real_from;
	to = real_to;
	tmp_dist = dist;
}

void GameCamera::UpdateRot(float dt, const Vec2& new_rot)
{
	if(reset == 0)
		d = 1.0f - exp(log(0.5f) * springiness * dt);
	else
	{
		d = 1.0f;
		reset = 1;
	}

	real_rot = new_rot;
	rot = Vec2::Slerp(rot, real_rot, d).Clip();
	tmp_dist += (dist - tmp_dist) * d;
}

void GameCamera::Update(float dt, const Vec3& new_from, const Vec3& new_to)
{
	real_from = new_from;
	real_to = new_to;

	if(reset == 0)
	{
		from += (real_from - from) * d;
		to += (real_to - to) * d;
	}
	else
	{
		from = real_from;
		to = real_to;
		reset = 0;
	}
}
