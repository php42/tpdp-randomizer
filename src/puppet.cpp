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

#include "puppet.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "textconvert.h"
#include "endian.h"

Puppet::Puppet()
{
	memset(this, 0, sizeof(Puppet));

	/* apparent default values for empty party puppets */
	happiness = 1;
	level = 1;
	hp = 0x0B;
}

void Puppet::read(const void *data, bool party)
{
	const uint8_t *buf = (const uint8_t*)data;

	trainer_id = read_le32(buf);
	secret_id = read_le32(&buf[4]);
	memcpy(trainer_name_raw_, &buf[8], sizeof(trainer_name_raw_));
	catch_location = read_le16(&buf[0x28]);
	caught_year = buf[0x2a];
	caught_month = buf[0x2b];
	caught_day = buf[0x2c];
	caught_hour = buf[0x2d];
	caught_minute = buf[0x2e];
	memcpy(puppet_nickname_raw_, &buf[0x2f], sizeof(puppet_nickname_raw_));
	puppet_id = read_le16(&buf[0x4f]);
	style_index = buf[0x51];
	ability_index = buf[0x52];
	mark = buf[0x53];
	for(int i = 0; i < 6; ++i)
		ivs[i] = (buf[0x54 + (i / 2)] >> ((i % 2) * 4)) & 0x0f;
	unknown_0x57 = buf[0x57];
	exp = read_le32(&buf[0x58]);
	happiness = read_le16(&buf[0x5c]);
	pp = read_le16(&buf[0x5e]);
	costume_index = buf[0x60];
	memcpy(evs, &buf[0x61], sizeof(evs));
	held_item_id = read_le16(&buf[0x67]);
	for(int i = 0; i < 4; ++i)
		skills[i] = read_le16(&buf[0x69 + (i * 2)]);
	memcpy(unknown_0x71, &buf[0x71], sizeof(unknown_0x71));

	if(party)
	{
		level = buf[0x95];
		hp = read_le16(&buf[0x96]);
		memcpy(sp, &buf[0x98], sizeof(sp));
		memcpy(status_effects, &buf[0x9c], sizeof(status_effects));
		unknown_0x9e = buf[0x9e];
	}
	else
	{
		level = 0;
		hp = 0;
		memset(sp, 0, sizeof(sp));
		memset(status_effects, 0, sizeof(status_effects));
		unknown_0x9e = 0;
	}
}

void Puppet::write(void *data, bool party) const
{
	uint8_t *buf = (uint8_t*)data;

	write_le32(buf, trainer_id);
	write_le32(&buf[4], secret_id);
	memcpy(&buf[8], trainer_name_raw_, sizeof(trainer_name_raw_));
	write_le16(&buf[0x28], catch_location);
	buf[0x2a] = caught_year;
	buf[0x2b] = caught_month;
	buf[0x2c] = caught_day;
	buf[0x2d] = caught_hour;
	buf[0x2e] = caught_minute;
	memcpy(&buf[0x2f], puppet_nickname_raw_, sizeof(puppet_nickname_raw_));
	write_le16(&buf[0x4f], puppet_id);
	buf[0x51] = style_index;
	buf[0x52] = ability_index;
	buf[0x53] = mark;
	memset(&buf[0x54], 0, 3);
	for(int i = 0; i < 6; ++i)
		buf[0x54 + (i / 2)] |= (ivs[i] << ((i % 2) * 4));
	buf[0x57] = unknown_0x57;
	write_le32(&buf[0x58], exp);
	write_le16(&buf[0x5c], happiness);
	write_le16(&buf[0x5e], pp);
	buf[0x60] = costume_index;
	memcpy(&buf[0x61], evs, sizeof(evs));
	write_le16(&buf[0x67], held_item_id);
	for(int i = 0; i < 4; ++i)
		write_le16(&buf[0x69 + (i * 2)], skills[i]);
	memcpy(&buf[0x71], unknown_0x71, sizeof(unknown_0x71));

	if(party)
	{
		buf[0x95] = level;
		write_le16(&buf[0x96], hp);
		memcpy(&buf[0x98], sp, sizeof(sp));
		memcpy(&buf[0x9c], status_effects, sizeof(status_effects));
		buf[0x9e] = unknown_0x9e;
	}
}

std::wstring Puppet::trainer_name() const
{
	return sjis_to_utf(trainer_name_raw_);
}

void Puppet::set_trainer_name(const std::wstring& name)
{
	std::string str = utf_to_sjis(name);
	snprintf(trainer_name_raw_, 32, "%s", str.c_str());
}

std::wstring Puppet::puppet_nickname() const
{
	return sjis_to_utf(puppet_nickname_raw_);
}

void Puppet::set_puppet_nickname(const std::wstring& name)
{
	std::string str = utf_to_sjis(name);
	snprintf(puppet_nickname_raw_, 32, "%s", str.c_str());
}

void Puppet::set_puppet_nickname(const std::string& name)
{
    snprintf(puppet_nickname_raw_, 32, "%s", name.c_str());
}
