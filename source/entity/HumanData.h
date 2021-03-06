// dane cz�owieka
#pragma once

//-----------------------------------------------------------------------------
// Ilo�� br�d, w�os�w i w�s�w
static const int MAX_BEARD = 6;
static const int MAX_HAIR = 6;
static const int MAX_MUSTACHE = 3;

//-----------------------------------------------------------------------------
// Ustawienia brodow�s�w
extern bool g_beard_and_mustache[MAX_BEARD - 1];

//-----------------------------------------------------------------------------
// Dost�pne kolory w�os�w
extern const Vec4 g_hair_colors[];
extern const uint n_hair_colors;

//-----------------------------------------------------------------------------
// Dane cz�owieka
struct Human
{
	int hair, beard, mustache;
	Vec4 hair_color;
	float height; // 0...2
	vector<Matrix> mat_scale;

	Vec2 GetScale();
	void ApplyScale(MeshInstance* mesh_inst);
	void Save(FileWriter& f);
	void Load(FileReader& f);
};

//-----------------------------------------------------------------------------
// Jak Human ale bez macierzy, u�ywane do zapami�tania jak wygl�da�a jaka� posta�
struct HumanData
{
	int hair, beard, mustache;
	Vec4 hair_color;
	float height;

	void Get(const Human& h);
	void Set(Human& h) const;
	void CopyFrom(HumanData& hd);
	void Save(FileWriter& f) const;
	void Load(FileReader& f);
	void Write(BitStreamWriter& f) const;
	// 0 - ok, 1 - ready error, 2 - value error
	int Read(BitStreamReader& f);
	void Random();
};
