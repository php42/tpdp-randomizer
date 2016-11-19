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

#include "gamedata.h"
#include <cstdint>
#include "util.h"

void SkillData::read(const void *data)
{
	const uint8_t *buf = (const uint8_t*)data;

	element = buf[32];
	power = buf[33];
	accuracy = buf[34];
	sp = buf[35];
	priority = buf[36];
	type = read_le16(&buf[37]);
	effect_id = read_le16(&buf[39]);
	effect_chance = read_le16(&buf[41]);
	effect_target = read_le16(&buf[43]);
}

void SkillData::write(void *data)
{
    uint8_t *buf = (uint8_t*)data;

    buf[32] = element;
    buf[33] = power;
    buf[34] = accuracy;
    buf[35] = sp;
    buf[36] = priority;
    write_le16(&buf[37], type);
}

StyleData::StyleData() : style_type(0), element1(0), element2(0), lv100_skill(0)
{
	memset(base_stats, 0, sizeof(base_stats));
    memset(abilities, 0, sizeof(abilities));
    memset(style_skills, 0, sizeof(style_skills));
    memset(skill_compat_table, 0, sizeof(skill_compat_table));
    memset(lv70_skills, 0, sizeof(lv70_skills));
}

void StyleData::read(const void *data)
{
	const uint8_t *buf = (const uint8_t*)data;

	style_type = buf[0];
	element1 = buf[1];
	element2 = buf[2];

	for(int i = 0; i < 6; ++i)
		base_stats[i] = buf[3 + i];

	abilities[0] = read_le16(&buf[9]);
	abilities[1] = read_le16(&buf[11]);

	for(int i = 0; i < 11; ++i)
		style_skills[i] = read_le16(&buf[17 + (i * 2)]);

    lv100_skill = read_le16(&buf[0x2D]);

	for(int i = 0; i < 16; ++i)
		skill_compat_table[i] = buf[49 + i];

	for(int i = 0; i < 8; ++i)
		lv70_skills[i] = read_le16(&buf[65 + (i * 2)]);
}

void StyleData::write(void * data)
{
    uint8_t *buf = (uint8_t*)data;

    buf[0] = style_type;
    buf[1] = element1;
    buf[2] = element2;

    for(int i = 0; i < 6; ++i)
        buf[3 + i] = base_stats[i];

    write_le16(&buf[9], abilities[0]);
    write_le16(&buf[11], abilities[1]);

    for(int i = 0; i < 11; ++i)
        write_le16(&buf[17 + (i * 2)], style_skills[i]);

    write_le16(&buf[0x2D], lv100_skill);

    for(int i = 0; i < 16; ++i)
        buf[49 + i] = skill_compat_table[i];

    for(int i = 0; i < 8; ++i)
        write_le16(&buf[65 + (i * 2)], lv70_skills[i]);
}

const char *StyleData::type_string()
{
    switch(style_type)
    {
    case STYLE_NORMAL:
        return "Normal";
    case STYLE_POWER:
        return "Power";
    case STYLE_DEFENSE:
        return "Defense";
    case STYLE_ASSIST:
        return "Assist";
    case STYLE_SPEED:
        return "Speed";
    case STYLE_EXTRA:
        return "Extra";
    default:
        return "None";
    }

    return "Error";
}

PuppetData::PuppetData()
{
	cost = 0;
	memset(base_skills, 0, sizeof(base_skills));
	memset(item_drop_table, 0, sizeof(item_drop_table));
	id = 0;
}

void PuppetData::read(const void *data)
{
	const uint8_t *buf = (const uint8_t*)data;

	cost = buf[32];

	for(int i = 0; i < 5; ++i)
		base_skills[i] = read_le16(&buf[33 + (i * 2)]);

	for(int i = 0; i < 4; ++i)
		item_drop_table[i] = read_le16(&buf[43 + (i * 2)]);

    /* there's an ID here, but it's wrong (?) */
	id = read_le16(&buf[51]);

	for(int i = 0; i < 4; ++i)
		styles[i].read(&buf[93 + (i * STYLE_DATA_SIZE)]);
}

void PuppetData::write(void *data)
{
    uint8_t *buf = (uint8_t*)data;

    buf[32] = cost;

    for(int i = 0; i < 5; ++i)
        write_le16(&buf[33 + (i * 2)], base_skills[i]);

    for(int i = 0; i < 4; ++i)
        write_le16(&buf[43 + (i * 2)], item_drop_table[i]);

    //write_le16(&buf[51], id);

    for(int i = 0; i < 4; ++i)
        styles[i].write(&buf[93 + (i * STYLE_DATA_SIZE)]);
}
