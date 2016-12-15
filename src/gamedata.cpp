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
#include "textconvert.h"
#include <cassert>

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

std::wstring StyleData::style_string() const
{
	switch(style_type)
	{
    case STYLE_NORMAL:
        return L"Normal";
    case STYLE_POWER:
        return L"Power";
    case STYLE_DEFENSE:
        return L"Defense";
	case STYLE_ASSIST:
		return L"Assist";
    case STYLE_SPEED:
        return L"Speed";
	case STYLE_EXTRA:
		return L"Extra";
    case STYLE_NONE:
        return L"None";
	default:
		return L"UNKNOWN";
	}
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

bool ItemData::parse(const CSVEntry& data, bool ynk)
{
    if((!ynk) && (data.size() != 11))
        return false;
    else if((ynk) && (data.size() != 12))
        return false;

    if(data[0].empty() || !iswdigit(data[0][0]))
        return false;

    id = std::stoi(data[0]);
    name = data[1];
    price = std::stoi(data[2]);
    type = std::stoi(data[3]);	/* 255 = unimplemented (id 0 "nothing" also uses this value) */
    combat = (std::stoi(data[4]) != 0);
    common = (std::stoi(data[5]) != 0);
    can_discard = (std::stoi(data[6]) != 0);
    held = (std::stoi(data[7]) != 0);
    reincarnation = (std::stoi(data[8]) != 0);
    skill_id = std::stoi(data[9]);
    if(!ynk)
        description = data[10];
    else
        description = data[11];

    return true;
}

bool CSVFile::parse(const void *data, std::size_t len)
{
    clear();

    std::wstring file = sjis_to_utf((const char*)data, (const char*)data + len);

    int num_splits = 0;

    /* determine field count from first line. */
    for(auto& i : file)
    {
        if(i == L',')
            ++num_splits;
        else if(i == L'\n')
            break;
    }

    std::size_t str_begin = 0, str_end = file.find(L"\r\n");
    while(str_end != std::wstring::npos)
    {
        CSVEntry entry;
        std::size_t split_begin = str_begin, split_end = file.find(L',', str_begin);
        for(int i = 0; i < num_splits; ++i)
        {
            if(split_end > str_end)
            {
                clear();
                return false;
            }

            entry.push_back(std::move(file.substr(split_begin, split_end - split_begin)));
            split_begin = split_end + 1;
            split_end = file.find(L',', split_begin);
        }

        entry.push_back(std::move(file.substr(split_begin, str_end - split_begin)));
        value_map_.push_back(std::move(entry));
        str_begin = str_end + 2;
        str_end = file.find(L"\r\n", str_begin);
    }

    return true;
}

std::string CSVFile::to_string() const
{
    std::wstring temp;

    for(const auto& i : value_map_)
    {
        if(i.empty())
            continue;

        for(std::size_t j = 0; j < i.size() - 1; ++j)
        {
            temp += i[j];
            temp += L',';
        }
        temp += i.back();
        temp += L"\r\n";
    }

    return utf_to_sjis(temp);
}

std::wstring element_string(int element)
{
    switch(element)
    {
    case ELEMENT_NONE:
        return L"None";
    case ELEMENT_VOID:
        return L"Void";
    case ELEMENT_FIRE:
        return L"Fire";
    case ELEMENT_WATER:
        return L"Water";
    case ELEMENT_NATURE:
        return L"Nature";
    case ELEMENT_EARTH:
        return L"Earth";
    case ELEMENT_STEEL:
        return L"Steel";
    case ELEMENT_WIND:
        return L"Wind";
    case ELEMENT_ELECTRIC:
        return L"Electric";
    case ELEMENT_LIGHT:
        return L"Light";
    case ELEMENT_DARK:
        return L"Dark";
    case ELEMENT_NETHER:
        return L"Nether";
    case ELEMENT_POISON:
        return L"Poison";
    case ELEMENT_FIGHTING:
        return L"Fighting";
    case ELEMENT_ILLUSION:
        return L"Illusion";
    case ELEMENT_SOUND:
        return L"Sound";
    case ELEMENT_DREAM:
        return L"Dream";
    case ELEMENT_WARPED:
        return L"Warped";
    default:
        assert(false);
        return L"Unknown";
    }
}
