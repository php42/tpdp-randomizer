/*
	Copyright (C) 2016 php42

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GAMEDATA_H
#define GAMEDATA_H
#include <cstdint>
#include <string>
#include <vector>
#include <set>

#define SKILL_DATA_SIZE 0x77
#define STYLE_DATA_SIZE 0x65
#define PUPPET_DATA_SIZE 0x1F1

typedef std::vector<std::wstring> CSVEntry;

enum PuppetStyleType
{
	STYLE_NONE = 0,
	STYLE_NORMAL,
	STYLE_POWER,
	STYLE_DEFENSE,
	STYLE_ASSIST,
	STYLE_SPEED,
	STYLE_EXTRA,
    STYLE_MAX
};

enum ElementType
{
	ELEMENT_NONE = 0,
	ELEMENT_VOID,
	ELEMENT_FIRE,
	ELEMENT_WATER,
	ELEMENT_NATURE,
	ELEMENT_EARTH,
	ELEMENT_STEEL,
	ELEMENT_WIND,
	ELEMENT_ELECTRIC,
	ELEMENT_LIGHT,
	ELEMENT_DARK,
	ELEMENT_NETHER,
	ELEMENT_POISON,
	ELEMENT_FIGHTING,
	ELEMENT_ILLUSION,
	ELEMENT_SOUND,
	ELEMENT_DREAM,
	ELEMENT_WARPED,
    ELEMENT_MAX
};

enum SkillType
{
	SKILL_TYPE_FOCUS = 0,
	SKILL_TYPE_SPREAD,
	SKILL_TYPE_STATUS,
    SKILL_TYPE_MAX
};

/* base data for skills */
class SkillData
{
public:
	//char name[32];									/* for reference, name field is 32 bytes long */
	uint8_t element, power, accuracy, sp;
	int8_t priority;
	uint16_t type;										/* focus, spread, status */
	uint16_t effect_id, effect_chance, effect_target;	/* best guess. chance seems to be ignored on status effect skills. target = 0 for self, 1 for opponent */
	//uint8_t unknown_0x2d[74];							/* for reference, 74 bytes of padding or who knows what */

	SkillData() : element(0), power(0), accuracy(0), sp(0), priority(0), type(0), effect_id(0), effect_chance(0), effect_target(0) {}
	SkillData(const void *data) {read(data);}

	void read(const void *data);
    void write(void *data);
};

/* base data for each variant of a particular puppet */
class StyleData
{
public:
	uint8_t style_type, element1, element2;	/* style_type = normal, power, defense, etc */
	uint8_t base_stats[6];
	uint16_t abilities[2];
	uint16_t style_skills[11];				/* lv. 0, 0, 0, 0, 30, 36, 42, 49, 56, 63, 70 */
    uint16_t lv100_skill;                   /* skill learned at level 100 */
	uint8_t skill_compat_table[16];			/* bitfield of 128 boolean values indicating ability to learn skill cards */
	uint16_t lv70_skills[8];				/* extra skills at level 70 */

    std::set<unsigned int> skillset;        /* set of all skills ids this puppet can learn by levelling */

    StyleData();
	StyleData(const void *data) {read(data);}

	void read(const void *data);
    void write(void *data);
};

/* base data for each puppet (including variants) */
class PuppetData
{
public:
	//char name[32];
	uint8_t cost;                   /* exp cost modifier */
	uint16_t base_skills[5];        /* skills available from lvl 1 */
	uint16_t item_drop_table[4];	/* items dropped when defeated in the wild */
	uint16_t id;                    /* id isn't actually parsed, but is indicated by its position in the file */
	StyleData styles[4];

	PuppetData();
	PuppetData(const void *data) {read(data);}

	void read(const void *data);
    void write(void *data);
};

#endif // GAMEDATA_H
