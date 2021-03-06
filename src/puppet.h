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

#ifndef PUPPET_H
#define PUPPET_H
#include <cstdint>
#include <string>

#define PUPPET_SIZE 0x91
#define PUPPET_SIZE_BOX (PUPPET_SIZE + 0x4)
#define PUPPET_SIZE_PARTY (PUPPET_SIZE_BOX + 0xA)

enum PuppetMark
{
    MARK_NONE = 0,
    MARK_RED,
    MARK_BLUE,
    MARK_BLACK,
    MARK_WHITE,
    MARK_GREEN
};

enum CostumeType
{
    COSTUME_NORMAL = 0,
    COSTUME_ALT_COLOR,
    COSTUME_ALT_OUTFIT,
    COSTUME_WEDDING_DRESS
};

/* instance of a saved puppet */
class Puppet
{
private:
    /* NOTE: these are Shift-JIS, character set conversion is performed by
     * the getter/setter functions */
    char trainer_name_raw_[32];
    char puppet_nickname_raw_[32];

public:
    uint32_t trainer_id;
    uint32_t secret_id;
    uint16_t catch_location;	/* 0 = starter puppet */
    uint8_t caught_year, caught_month, caught_day, caught_hour, caught_minute; /* year = number of years since 1900 (as from localtime(time())) */
    uint16_t puppet_id;
    uint8_t style_index, ability_index, mark;
    uint8_t ivs[6];			/* range 0x00 to 0x0F each. */
    uint8_t unknown_0x57;
    uint32_t exp;
    uint16_t happiness, pp;
    uint8_t costume_index;	/* normal, alt color, alt costume, wedding dress */
    uint8_t evs[6];
    uint16_t held_item_id;
    uint16_t skills[4];
    uint8_t unknown_0x71[32];
    //uint32_t hash;

    /* party puppets only, be sure to set these when saving a new puppet to the party */
    uint8_t level;				/* display only, actual level is derived from exp */
    uint16_t hp;				/* current hp remaining, does not affect the actual stat*/
    uint8_t sp[4];				/* current sp remaining */
    uint8_t status_effects[2];	/* 0 = none */
    uint8_t unknown_0x9e;

    Puppet();
    Puppet(const void *data, bool party) {read(data, party);}

    /* setting 'party' to true will cause it to read/write data that is present
     * only for party puppets such as hp/sp values.
     * if 'party' is true, 'data' must be at least PUPPET_SIZE_PARTY bytes.
     * otherwise it must be at least PUPPET_SIZE bytes. */
    void read(const void *data, bool party);
    void write(void *dest, bool party) const;

    inline bool has_heart_mark() const {return (unknown_0x71[0] != 0);}
    inline void set_heart_mark(bool marked) {unknown_0x71[0] = marked ? 1 : 0;}

    /* NOTE: these functions perform conversion to/from
     * Shift-JIS internally. */
    std::wstring trainer_name() const;
    void set_trainer_name(const std::wstring& name);

    std::wstring puppet_nickname() const;
    void set_puppet_nickname(const std::wstring& name);
    void set_puppet_nickname(const std::string& name);
};

#endif // PUPPET_H
