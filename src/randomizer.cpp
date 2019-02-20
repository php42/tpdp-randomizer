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

/* Reader beware, insanity lies ahead! */

#include "randomizer.h"
#include "filesystem.h"
#include "archive.h"
#include "gamedata.h"
#include "puppet.h"
#include "endian.h"
#include "textconvert.h"
#include <cmath>
#include <algorithm>
#include <cassert>
#include <cwchar>
#include <type_traits>
#include <sstream>
#include <utility>

static const int g_cost_exp_modifiers[] = {70, 85, 100, 115, 130};
static const int g_cost_exp_modifiers_ynk[] = {85, 92, 100, 107, 115};
static const unsigned int g_sign_skills[] = {127, 179, 233, 273, 327, 375, 421, 474, 524, 566, 623, 680, 741, 782, 817};

static inline bool is_sign_skill(unsigned int id)
{
    for(auto i : g_sign_skills)
        if(id == i)
            return true;

    return false;
}

template<typename T>
void subtract_set(std::vector<T>& vec, const std::set<T>& s)
{
    auto it = vec.begin();
    while(it != vec.end())
    {
        if(s.count(*it))
        {
            it = vec.erase(it);
            continue;
        }
        ++it;
    }
}

template<typename T>
void subtract_set(std::set<T>& s1, const std::set<T>& s2)
{
    for(auto i : s2)
        s1.erase(i);
}

void Randomizer::export_locations(const std::wstring& filepath)
{
    std::string out("\xEF\xBB\xBF"); /* UTF-8 BOM to make MS Notepad happy */
    std::wstring temp;

    for(const auto& it : loc_map_)
    {
        if(it.first >= puppet_names_.size())
            continue;

        temp += puppet_names_[it.first];
        temp += L":\r\n";

        for(auto& j : it.second)
        {
            temp += j;
            temp += L"\r\n";
        }

        temp += L"\r\n";
    }

    out += utf_narrow(temp);
    if(!write_file(filepath, out.c_str(), out.length()))
        error(std::wstring(L"Failed to write to file: ") + filepath + L"\nPlease make sure you have write permission");
}

void Randomizer::export_puppets(const std::wstring& filepath)
{
    std::string out("\xEF\xBB\xBF"); /* UTF-8 BOM to make MS Notepad happy */
    std::wstring temp;

    for(const auto& it : puppets_)
    {
        const PuppetData& puppet(it.second);
        for(const auto& style : puppet.styles)
        {
            if(style.style_type == 0)
                continue;
            temp += style.style_string() + L' ' + puppet_names_.at(puppet.id) + L" (";
            temp += element_string(style.element1);
            if(style.element2)
                temp += L'/' + element_string(style.element2);
            temp += L") " + std::to_wstring((puppet.cost * 10) + 80) + L" Cost\r\n";
            temp += L"\tHP: " + std::to_wstring(style.base_stats[0]) + L"\r\n";
            temp += L"\tFo.Atk: " + std::to_wstring(style.base_stats[1]) + L"\r\n";
            temp += L"\tFo.Def: " + std::to_wstring(style.base_stats[2]) + L"\r\n";
            temp += L"\tSp.Atk: " + std::to_wstring(style.base_stats[3]) + L"\r\n";
            temp += L"\tSp.Def: " + std::to_wstring(style.base_stats[4]) + L"\r\n";
            temp += L"\tSpeed: " + std::to_wstring(style.base_stats[5]) + L"\r\n\r\n";

            temp += L"\tAbilities:\r\n";
            for(auto i : style.abilities)
            {
                if(i > 0)
                    temp += L"\t\t" + ability_names_[i] + L"\r\n";
            }

            temp += L"\r\n\tSkills:\r\n";
            for(auto i : style.skillset)
            {
                if(i > 0)
                    temp += L"\t\t" + skill_names_[i] + L"\r\n";
            }

            temp += L"\r\n\tSkill Cards:\r\n";
            for(unsigned int i = 0; i < 16; ++i)
            {
                for(unsigned int j = 0; j < 8; ++j)
                {
                    int item_id = 385 + (8 * i) + j;
                    if(!items_.count(item_id) || !items_[item_id].is_valid())
                        continue;

                    if(style.skill_compat_table[i] & (1 << j))
                    {
                        auto card_num = (i * 8) + j + 1;
                        if(is_ynk_ && (card_num >= 114)) // fix for sign skills in YnK
                            card_num -= 8;
                        temp += L"\t\t#" + std::to_wstring(card_num) + L' ';
                        temp += skill_names_[items_[item_id].skill_id] + L"\r\n";
                    }
                }
            }

            temp += L"\r\n\r\n";
        }
    }

    out += utf_narrow(temp);
    write_file(filepath, out.c_str(), out.length());
}

bool Randomizer::export_compat(Archive& arc, const std::wstring& filepath)
{
    if(!rand_export_compat_)
        return true;

    auto compat = arc.get_file(is_ynk_ ? "doll/Compatibility.csv" : "doll/elements/Compatibility.csv");

    CSVFile csv(compat.data(), compat.size());
    if(csv.num_lines() < 18 || csv.num_fields() < 18 || csv.num_lines() > 19 || csv.num_fields() > 19)
    {
        error(L"Error parsing compatibility.csv");
        return false;
    }

    CSVFile new_csv;

    wchar_t *elements[] = { L"", L"", L"Void", L"Fire", L"Water", L"Nature", L"Earth", L"Steel", L"Wind", L"Electric", L"Light", L"Dark", L"Nether", L"Poison", L"Fighting", L"Illusion", L"Sound", L"Dream", L"Warped" };
    wchar_t *short_elements[] = {L"", L"", L"Voi", L"Fir", L"Wtr", L"Ntr", L"Ear", L"Stl", L"Wnd", L"Ele", L"Lgt", L"Drk", L"Nth", L"Poi", L"Fgt", L"Ilu", L"Snd", L"Drm", L"Wrp"};
    wchar_t *markers[] = { L"X", L"R", L" ", L" ", L"W" };

    new_csv.data().emplace_back();
    for(auto i = 2; i < (is_ynk_ ? 19 : 18); ++i)
        new_csv.back().push_back(short_elements[i]);

    unsigned int dist_stats[5] = { 0 };

    for(std::size_t line = 2; line < csv.num_lines(); ++line) // skip descriptor and null element
    {
        new_csv.data().emplace_back();
        for(std::size_t field = 2; field < csv.num_fields(); ++field) // skip descriptor and null element
        {
            auto val = std::stoul(csv[line][field]);
            if(val >= 5)
            {
                error(L"Error parsing compatibility.csv");
                return false;
            }
            new_csv.back().push_back(L" " + std::wstring(markers[val]) + L" ");

            ++dist_stats[val];
        }
        new_csv.back().push_back(elements[line]);
    }

    auto out = new_csv.to_string();
    for(auto& i : out)
        if(i == ',')
            i = '|';

    out += "\r\nX = immune, R = not effective, blank = neutral, W = super effective.\r\nrow->column\r\n"
           "\r\nImmunities: " + std::to_string(dist_stats[0]) + "\r\nResistances: " + std::to_string(dist_stats[1]) +
           "\r\nWeaknesses: " + std::to_string(dist_stats[4]) + "\r\nNeutral: " + std::to_string(dist_stats[2]) + "\r\n";

    write_file(filepath, out.data(), out.size());

    return true;
}

void Randomizer::clear()
{
    loc_map_.clear();
    puppets_.clear();
    skills_.clear();
    items_.clear();
    valid_puppet_ids_.clear();
    puppet_id_pool_.clear();
    puppet_names_.clear();
    skill_names_.clear();
    ability_names_.clear();
    valid_skills_.clear();
    valid_abilities_.clear();
    skillcard_ids_.clear();
    held_item_ids_.clear();
    normal_stats_.clear();
    evolved_stats_.clear();
    old_costs_.clear();
    location_names_.clear();
}

bool Randomizer::open_archive(Archive& arc, const std::wstring& path)
{
    try
    {
        arc.open(path);
    }
    catch(const ArcError& ex)
    {
        error(L"Failed to open file: " + path + L"\r\n" + utf_widen(ex.what()));
        return false;
    }

    return true;
}

bool Randomizer::save_archive(Archive & arc, const std::wstring & path)
{
    if(!arc.save(path))
    {
        error(std::wstring(L"Could not write to file: ") + path + L"\r\nPlease make sure you have write permission to the game folder.");
        return false;
    }

    return true;
}

void Randomizer::set_progress_bar(int percent)
{
    gui_->set_progress_bar(percent);
}

void Randomizer::increment_progress_bar()
{
    gui_->increment_progress_bar();
}

/* detect valid puppet/skill/etc IDs from the game data.
 * this eliminates a dependecy on prebuilt tables and should work
 * for any version of the game. */
bool Randomizer::parse_puppets(Archive& archive)
{
    ArcFile file;
    if(!(file = archive.get_file("doll/dolldata.dbs")))
    {
        error(L"Error unpacking dolldata.dbs from game data");
        return false;
    }

    const char *buf = file.data();

    /* there's a lot of unimplemented stuff floating around the files,
     * so find all skills which are actually used by puppets */
    for(unsigned int pos = 0; pos < file.size(); pos += PUPPET_DATA_SIZE)
    {
        PuppetData puppet(&buf[pos]);
        puppet.id = (pos / PUPPET_DATA_SIZE);
        if(puppet.styles[0].style_type == 0) /* not a real puppet */
            continue;

        valid_puppet_ids_.push_back(puppet.id);

        for(auto i : puppet.base_skills)
            valid_skills_.insert(i);

        for(auto& style : puppet.styles)
        {
            if(style.style_type == 0)
                continue;

            style.skillset = puppet.styles[0].skillset;

            valid_skills_.insert(style.lv100_skill);
            style.skillset.insert(style.lv100_skill);
            for(auto i : style.style_skills)
            {
                valid_skills_.insert(i);
                style.skillset.insert(i);
            }

            for(auto i : style.lv70_skills)
            {
                valid_skills_.insert(i);
                style.skillset.insert(i);
            }

            for(auto i : puppet.base_skills)
                style.skillset.insert(i);

            for(auto i : style.abilities)
                valid_abilities_.insert(i);

            for(auto i : style.base_stats)
            {
                if(style.style_type == STYLE_NORMAL)
                    normal_stats_.push_back(i);
                else
                    evolved_stats_.push_back(i);
            }
        }

        puppets_[puppet.id] = std::move(puppet);
    }

    valid_skills_.erase(0);
    valid_abilities_.erase(0);

    if(rand_healthy_)
        valid_abilities_.erase(313); /* remove frail health from the pool */

    std::shuffle(normal_stats_.begin(), normal_stats_.end(), gen_);
    std::shuffle(evolved_stats_.begin(), evolved_stats_.end(), gen_);
    puppet_id_pool_.assign(valid_puppet_ids_, gen_);

    return true;
}

/* detect valid item IDs and skillcards from the game data */
bool Randomizer::parse_items(Archive& archive)
{
    ArcFile file;
    if(!(file = archive.get_file("item/ItemData.csv")))
    {
        error(L"Error unpacking ItemData.csv from game data");
        return false;
    }

    CSVFile csv;
    if(!csv.parse(file.data(), file.size()) || (csv.num_fields() < 10))
    {
        error(L"Error parsing ItemData.csv");
        return false;
    }

    if(rand_skillcards_)
    {
        auto skill_pool = valid_skills_;
        try
        {
            for(auto& it : csv.data())
            {
                if((it[3] == L"4") && (it[9] != L"0"))
                    skill_pool.insert(std::stoul(it[9]));
            }
        }
        catch(const std::exception&)
        {
            error(L"Error parsing ItemData.csv");
            return false;
        }

        skill_pool.erase(0);
		if(rand_skillcards_ > 1)
			for(auto i : g_sign_skills)
				skill_pool.erase(i);

        std::vector<unsigned int> skills(skill_pool.begin(), skill_pool.end());
		std::shuffle(skills.begin(), skills.end(), gen_);

        for(auto& it : csv.data())
        {
            if((it[3] == L"4") && (it[9] != L"0") && ((rand_skillcards_ < 2) || !is_sign_skill(std::stoul(it[9]))))
            {
				assert(!skills.empty());
				if(skills.empty())
					continue;
                it[9] = std::to_wstring(skills.back());
                skills.pop_back();
            }
        }

        std::string temp = csv.to_string();
        archive.repack_file("item/ItemData.csv", temp.c_str(), temp.length());
    }

    /* there are some items that otherwise appear to be real items but aren't actually implemented in-game
     * they follow the naming convention of the other unimplemented items i.e. "ItemXXX", so we filter those out too */
    for(const auto& it : csv.data())
    {
        ItemData item;
        if(!item.parse(it, is_ynk_) || (item.name.find(L"Item") != std::wstring::npos) || !item.is_valid())
            continue;
        if((item.type == 4) && (item.skill_id != 0))
            skillcard_ids_.insert(item.id);
        else if(item.held)
            held_item_ids_.insert(item.id);
        items_[item.id] = item;
    }

    return true;
}

bool Randomizer::parse_skill_names(Archive& archive)
{
    ArcFile file;
    CSVFile csv;

    if(!(file = archive.get_file(is_ynk_ ? "doll/SkillData.csv" : "doll/skill/SkillData.csv")))
    {
        error(L"Error unpacking SkillData.csv from game data");
        return false;
    }

    if(!csv.parse(file.data(), file.size()) || (csv.num_fields() < (is_ynk_ ? 2U : 1U)))
    {
        error(L"Error parsing SkillData.csv");
        return false;
    }

    if(is_ynk_)
    {
        try
        {
            for(auto& it : csv.data())
                skill_names_[std::stol(it[0])] = it[1];
        }
        catch(const std::exception&)
        {
            error(L"Error parsing SkillData.csv");
            return false;
        }
    }
    else
    {
        int index = 0;
        for(auto& it : csv.data())
            skill_names_[index++] = it[0];
    }

    return true;
}

bool Randomizer::parse_ability_names(Archive& archive)
{
    ArcFile file;
    CSVFile csv;

    if(!(file = archive.get_file(is_ynk_ ? "doll/AbilityData.csv" : "doll/ability/AbilityData.csv")))
    {
        error(L"Error unpacking SkillData.csv from game data");
        return false;
    }

    if(!csv.parse(file.data(), file.size()) || csv.num_fields() < 2)
    {
        error(L"Error parsing AbilityData.csv");
        return false;
    }

    try
    {
        for(auto& it : csv.data())
            ability_names_[std::stol(it[0])] = it[1];
    }
    catch(const std::exception&)
    {
        error(L"Error parsing AbilityData.csv");
        return false;
    }

    return true;
}

bool Randomizer::randomize_puppets(Archive& archive)
{
    if(!rand_puppets_)
        return true;

    ArcFile file;
    if(!(file = archive.get_file("doll/dolldata.dbs")))
    {
        error(L"Error unpacking dolldata.dbs from game data");
        return false;
    }
    std::set<unsigned int> valid_base_skills, valid_normal_skills, valid_evolved_skills, valid_lv70_skills, valid_lv100_skills;
    std::vector<unsigned int> ability_deck(valid_abilities_.begin(), valid_abilities_.end());
    std::bernoulli_distribution chance25(0.25); /* 25% chance */
    std::bernoulli_distribution chance35(0.35); /* 35% chance */
    std::bernoulli_distribution chance60(0.6);  /* 60% chance */
    std::bernoulli_distribution chance75(0.75); /* 75% chance */
    std::bernoulli_distribution chance90(0.9);  /* 90% chance */
    std::uniform_int_distribution<unsigned int> element(1, is_ynk_ ? ELEMENT_WARPED : ELEMENT_SOUND);
    std::uniform_int_distribution<unsigned int> pick_stat(0, 5);
    std::uniform_int_distribution<unsigned int> gen_stat(0, 0xff);
    std::uniform_int_distribution<unsigned int> gen_quota(0, 64);
    std::uniform_int_distribution<unsigned int> gen_cost(0, 4);

    /* stat distribution statistics */
#ifndef NDEBUG
    unsigned int a[256] = {0};
    int64_t accum = 0;
    int amin = 0, amax = 0;
#endif

    /* initialize skill pools for randomization.
     * skill pools are populated from skills possesed by puppets found in the game data.
     * this eliminates a dependency on pre-built tables, and should work for any version
     * of the game. */
    for(const auto& it : puppets_)
    {
        const PuppetData& puppet(it.second);

        for(auto i : puppet.base_skills)
            valid_base_skills.insert(i);

        for(const auto& style : puppet.styles)
        {
            if(style.style_type == 0)
                continue;

            valid_lv100_skills.insert(style.lv100_skill);

            if(style.style_type == STYLE_NORMAL)
            {
                for(auto i : style.style_skills)
                    valid_normal_skills.insert(i);
            }
            else
            {
                for(auto i : style.style_skills)
                    valid_evolved_skills.insert(i);
            }

            for(auto i : style.lv70_skills)
                valid_lv70_skills.insert(i);
        }
    }

    valid_base_skills.erase(0);
    valid_normal_skills.erase(0);
    valid_evolved_skills.erase(0);
    valid_lv70_skills.erase(0);
    valid_lv100_skills.erase(0);

    int step = puppets_.size() / 25;
    int count = 0;

    /* begin puppet randomization */
    for(auto& it : puppets_)
    {
        PuppetData& puppet(it.second);

        /* update the progress bar */
        if(++count > step)
        {
            increment_progress_bar();
            count = 0;
        }

        /* pre-randomize typings */
        if(rand_types_)
        {
            for(auto& style : puppet.styles)
            {
                style.element1 = element(gen_);
                if(!chance75(gen_))
                    style.element2 = 0;
                else
                {
                    style.element2 = element(gen_);
                    if(style.element1 == style.element2)
                        style.element2 = 0;
                }
            }
        }

        /* randomize cost */
        if(rand_cost_)
        {
            /* we'll need to adjust the exp for trainer puppets, so save the original costs */
            old_costs_[puppet.id] = puppet.cost;

            if(rand_cost_ > 1)
                puppet.cost = 4;
            else
                puppet.cost = gen_cost(gen_);
        }

        /* randomize move sets */
        if(rand_skillsets_)
        {
            IDDeck skill_deck;
            if(rand_true_rand_skills_)
                skill_deck.assign(valid_skills_, gen_);
            else
                skill_deck.assign(valid_base_skills, gen_);

            /* moves shared by all styles of a particular puppet */
            for(auto& i : puppet.base_skills)
            {
                if(i != 0)
                {
                    if(rand_prefer_same_type_ && chance60(gen_))
                    {
                        auto val = get_stab_skill(skill_deck, puppet.styles[0].element1, puppet.styles[0].element2);
                        i = (val) ? *val : skill_deck.draw(i); // if we find a stab skill, use it. otherwise draw a random skill. keep original if deck is empty.
                    }
                    else
                    {
                        i = skill_deck.draw(i); // draw a random skill. keep original if deck is empty.
                    }
                }
            }
        }

        /* randomize each style of a particular puppet */
        for(auto& style : puppet.styles)
        {
            if(style.style_type == 0)
                continue;

            /* randomize style-specific moves */
            if(rand_skillsets_)
            {
                style.skillset.clear();
                style.skillset = puppet.styles[0].skillset;
                IDSet skill_set;
                IDDeck skill_deck;

                for(auto& i : puppet.base_skills)
                    style.skillset.insert(i);

                /* level 100 move */
                if((style.lv100_skill != 0) && !valid_lv100_skills.empty())
                {
                    if(rand_true_rand_skills_)
                        skill_set = valid_skills_;
                    else
                        skill_set = valid_lv100_skills;
                    subtract_set(skill_set, style.skillset);
                    skill_deck.assign(skill_set, gen_);

                    if(rand_prefer_same_type_ && chance60(gen_))
                    {
                        auto val = get_stab_skill(skill_deck, style.element1, style.element2);
                        style.lv100_skill = (val) ? *val : skill_deck.draw(style.lv100_skill);
                    }
                    else
                    {
                        style.lv100_skill = skill_deck.draw(style.lv100_skill);
                    }
                }
                style.skillset.insert(style.lv100_skill);

                if(rand_true_rand_skills_)
                    skill_set = valid_skills_;
                else if(style.style_type == STYLE_NORMAL)
                {
                    skill_set = valid_base_skills;
                    for(auto i : valid_normal_skills)
                        skill_set.insert(i);
                }
                else
                    skill_set = valid_evolved_skills;
                subtract_set(skill_set, style.skillset);
                skill_deck.assign(skill_set, gen_);

                /* ensure every puppet starts with at least one damaging move */
                if((style.style_type == STYLE_NORMAL) && rand_starting_move_)
                {
                    style.style_skills[0] = 56; /* default to yin energy if we don't find a match below */
                    for(auto it = skill_deck.begin(); it != skill_deck.end(); ++it)
                    {
                        auto e = skills_[*it].element;
                        if((skills_[*it].type != SKILL_TYPE_STATUS) && (skills_[*it].power > 0) && ((rand_starting_move_ != 1) || (e == style.element1) || (e == style.element2)))
                        {
                            style.style_skills[0] = *it;
                            skill_deck.erase(it);
                            break;
                        }
                    }

                    style.skillset.insert(style.style_skills[0]);
                }

                /* fill in the rest of the moves */
                for(int j = (((style.style_type == STYLE_NORMAL) && rand_starting_move_) ? 1 : 0); j < 11; ++j)
                {
                    auto& i(style.style_skills[j]);
                    if(!i)
                        continue;

                    if(rand_prefer_same_type_ && chance60(gen_))
                    {
                        auto val = get_stab_skill(skill_deck, style.element1, style.element2);
                        i = (val) ? *val : skill_deck.draw(i);
                    }
                    else
                    {
                        i = skill_deck.draw(i);
                    }

                    style.skillset.insert(i);
                }

                if(rand_true_rand_skills_)
                    skill_set = valid_skills_;
                else
                    skill_set = valid_lv70_skills;
                subtract_set(skill_set, style.skillset);
                skill_deck.assign(skill_set, gen_);

                /* level 70 moves */
                for(auto& i : style.lv70_skills)
                {
                    if(!i)
                        continue;

                    if(rand_prefer_same_type_ && chance60(gen_))
                    {
                        auto val = get_stab_skill(skill_deck, style.element1, style.element2);
                        i = (val) ? *val : skill_deck.draw(i);
                    }
                    else
                    {
                        i = skill_deck.draw(i);
                    }

                    style.skillset.insert(i);
                }

                style.skillset.erase(0);

                /* skillcard moves */
                memset(style.skill_compat_table, 0, sizeof(style.skill_compat_table));
                for(auto i : skillcard_ids_)
                {
                    unsigned int compat_index = (i - 385) / 8;
                    unsigned int offset = (i - 385) - (compat_index * 8);
                    if(compat_index >= 16)
                        continue;

                    if(rand_prefer_same_type_)
                    {
                        auto e = skills_[items_[i].skill_id].element;
                        bool same_element = ((e == style.element1) || (e == style.element2));
                        if((same_element && chance60(gen_)) || ((!same_element) && chance25(gen_)))
                            style.skill_compat_table[compat_index] |= (1 << offset);
                    }
                    else if(chance35(gen_))
                    {
                        style.skill_compat_table[compat_index] |= (1 << offset);
                    }
                }
            }

            /* randomize abilities */
            if(rand_abilities_)
            {
                memset(style.abilities, 0, sizeof(style.abilities));
                std::shuffle(ability_deck.begin(), ability_deck.end(), gen_);
                size_t index = 0;
                for(int i = 0; i < 2; ++i)
                {
                    /* don't give were-hakutaku to anyone other than keine */
                    while((ability_deck[index] == 311) && (puppet.id != 62))
                        ++index;

                    /* don't give mode shift to anyone other than rika */
                    while((ability_deck[index] == 379) && (puppet.id != 10))
                        ++index;

                    /* don't give three bodies to anyone other than hecatia */
                    while((ability_deck[index] == 382) && (puppet.id != 131))
                        ++index;

                    assert(index < ability_deck.size());

                    if((i == 0) || chance90(gen_))
                        style.abilities[i] = ability_deck[index++];
                }
            }

            /* randomize stats */
            if(rand_stats_)
            {
                if(rand_quota_)
                {
                    memset(style.base_stats, 0, sizeof(style.base_stats));
                    unsigned int sum = 0;
                    while(sum < stat_quota_)
                    {
                        auto& i(style.base_stats[pick_stat(gen_)]);

                        unsigned int temp = gen_quota(gen_);
                        if((temp + (unsigned int)i) > 0xff)
                            temp = 0xff - i;
                        if((temp + sum) >= stat_quota_)
                        {
                            temp = stat_quota_ - sum;
                            i += temp;
                            sum += temp;
                            break;
                        }
                        i += temp;
                        sum += temp;
                    }
                }
                else if(rand_true_rand_stats_)
                {
                    for(auto& i : style.base_stats)
                        i = gen_stat(gen_);
                }
                else if(rand_stat_scaling_)
                {
                    double scale_factor = double(stat_ratio_) / 100.0;
                    for(auto& i : style.base_stats)
                    {
                        int temp = std::uniform_int_distribution<int>(i - std::lround(i * scale_factor), i + std::lround(i * scale_factor))(gen_);
                        if(temp < 0)
                            temp = 0;
                        if(temp > 0xff)
                            temp = 0xff;

                        /* distribution statistics */
                        #ifndef NDEBUG
                        auto diff = (temp - i);
                        ++a[abs(diff)];
                        accum += diff;
                        if(diff > amax)
                            amax = diff;
                        if(diff < amin)
                            amin = diff;
                        #endif // !NDEBUG

                        i = temp;
                    }
                }
                else
                {
                    if(style.style_type == STYLE_NORMAL)
                    {
                        for(auto& i : style.base_stats)
                        {
                            assert(!normal_stats_.empty());
                            if(normal_stats_.empty())
                                i = gen_stat(gen_);
                            i = normal_stats_.back();
                            normal_stats_.pop_back();
                        }
                    }
                    else
                    {
                        for(auto& i : style.base_stats)
                        {
                            assert(!evolved_stats_.empty());
                            if(evolved_stats_.empty())
                                i = gen_stat(gen_);
                            i = evolved_stats_.back();
                            evolved_stats_.pop_back();
                        }
                    }
                }
            }
        }

        /* write our changes to the file buffer */
        puppet.write(&file.data()[puppet.id * PUPPET_DATA_SIZE]);
    }

    /* replace the puppet data file in the archive with our modified version */
    if(!archive.repack_file(file))
    {
        error(L"Error repacking dolldata.dbs");
        return false;
    }

    return true;
}

/* .dod files contain data for one trainer battle
 * this function randomizes the trainer puppets in a .dod file */
void Randomizer::randomize_dod_file(void *src, const void *rand_data)
{
    char *buf = (char*)src + 0x2C;
    char *endbuf = buf + (6 * PUPPET_SIZE_BOX);
    IDDeck item_deck(held_item_ids_, gen_);
    std::uniform_int_distribution<int> iv(0, 0xf);
    std::uniform_int_distribution<int> ev(0, 64);
    std::uniform_int_distribution<int> pick_ev(0, 5);
    std::uniform_int_distribution<int> id(0, valid_puppet_ids_.size() - 1);
    std::uniform_int_distribution<int> mark(1, 5);
    std::uniform_int_distribution<int> costume(0, is_ynk_ ? COSTUME_WEDDING_DRESS : COSTUME_ALT_OUTFIT);
    std::bernoulli_distribution item_chance(trainer_item_chance_ / 100.0);
    std::bernoulli_distribution coin_flip(0.5);
    std::bernoulli_distribution skillcard_chance(trainer_sc_chance_ / 100.0);

    unsigned int max_lvl = 0;
    double lvl_mul = double(level_mod_) / 100.0;
    for(char *pos = buf; pos < endbuf; pos += PUPPET_SIZE_BOX)
    {
        decrypt_puppet(pos, rand_data, PUPPET_SIZE);

        Puppet puppet(pos, false);

        /* if we've changed puppet costs, trainer puppets will have exp based on a different cost value.
         * use the old cost value to determine the correct level. */
        assert(!rand_cost_ || !puppet.puppet_id || old_costs_.count(puppet.puppet_id));
        unsigned int lvl = (rand_cost_) ? level_from_exp(old_costs_[puppet.puppet_id], puppet.exp) : level_from_exp(puppets_[puppet.puppet_id], puppet.exp);

        if(level_mod_ != 100)
            lvl = (unsigned int)(double(lvl) * lvl_mul);
        if(lvl > 100)
            lvl = 100;
        if(lvl > max_lvl)
            max_lvl = lvl;

        if((puppet.exp == 0) || (puppet.puppet_id == 0))
            lvl = max_lvl;

        if(lvl < 30)
            puppet.style_index = 0;

        puppet.costume_index = costume(gen_);
        puppet.mark = mark(gen_);

        if(((puppet.puppet_id == 0) && rand_full_party_) || ((puppet.puppet_id != 0) && rand_trainers_))
        {
            PuppetData& data(puppets_[valid_puppet_ids_[id(gen_)]]);
            puppet.puppet_id = data.id;

            assert(data.max_style_index() > 0);
            if(lvl >= 30)
                puppet.style_index = std::uniform_int_distribution<int>(1, data.max_style_index())(gen_);
            else
                puppet.style_index = 0;

            const StyleData& style(data.styles[puppet.style_index]);

            std::set<unsigned int> skill_set = style.skillset;
            std::set<unsigned int> skillcards;

            for(unsigned int i = 0; i < 16; ++i)
            {
                for(unsigned int j = 0; j < 8; ++j)
                {
                    if(style.skill_compat_table[i] & (1 << j))
                    {
                        skillcards.insert(items_[385 + (8 * i) + j].skill_id);
                    }
                }
            }

            /* remove skills that are too high level for the current puppet */
            if(rand_strict_trainers_)
            {
                auto iter = skill_set.begin();
                while(iter != skill_set.end())
                {
                    auto required_lvl = data.level_to_learn(puppet.style_index, *iter);
                    assert(required_lvl >= 0);
                    if((unsigned int)required_lvl > lvl)
                        iter = skill_set.erase(iter);
                    else
                        ++iter;
                }
            }

            /* no duplicates */
            for(auto i : skill_set)
                skillcards.erase(i);

            /* when using shuffle method, pool all skills together */
            if(rand_trainer_sc_shuffle_)
                skill_set.insert(skillcards.begin(), skillcards.end());

            IDDeck skill_deck(skill_set, gen_);
            IDDeck skillcard_deck;

            if(!rand_trainer_sc_shuffle_)
                skillcard_deck.assign(skillcards, gen_);

            bool has_sign_skill = false;
            for(auto& i : puppet.skills)
            {
                if(!rand_trainer_sc_shuffle_ && skillcard_chance(gen_))
                    i = skillcard_deck.draw(0);
                else
                    i = skill_deck.draw(0);
                
                /* check if we have a sign skill now, and if so remove all other sign skills from the pool */
                if(!has_sign_skill && is_sign_skill(i))
                {
                    has_sign_skill = true;

                    for(auto j = skill_deck.begin(); j != skill_deck.end();)
                    {
                        if(is_sign_skill(*j))
                        {
                            j = skill_deck.erase(j);
                            continue;
                        }
                        ++j;
                    }

                    for(auto j = skillcard_deck.begin(); j != skillcard_deck.end();)
                    {
                        if(is_sign_skill(*j))
                        {
                            j = skillcard_deck.erase(j);
                            continue;
                        }
                        ++j;
                    }
                }
            }

            for(auto& i : puppet.ivs)
                i = iv(gen_);

            memset(puppet.evs, 0, sizeof(puppet.evs));
            int total = 0;
            while(total < 130)
            {
                int j = ev(gen_);
                int k = pick_ev(gen_);
                if((puppet.evs[k] + j) > 64)
                    j = 64 - puppet.evs[k];
                if((total + j) > 130)
                    j = 130 - total;
                total += j;
                puppet.evs[k] += j;
            }

            puppet.ability_index = coin_flip(gen_) ? 1 : 0;
            if(data.styles[puppet.style_index].abilities[puppet.ability_index] == 0)
                puppet.ability_index = 0;

            /* TODO: allow leaving items unchanged */
            if(item_chance(gen_))
                puppet.held_item_id = item_deck.draw(0);
            else
                puppet.held_item_id = 0;

            assert(puppet.puppet_id < puppet_names_.size());
            if(puppet.puppet_id < puppet_names_.size())
                puppet.set_puppet_nickname(puppet_names_[puppet.puppet_id]);
        }

        if(puppet.puppet_id)
        {
            puppet.exp = exp_for_level(puppets_[puppet.puppet_id], lvl);
            assert(((lvl < 30) && (puppet.style_index == 0)) || (lvl >= 30));
            puppet.write(pos, false);
        }

        encrypt_puppet(pos, rand_data, PUPPET_SIZE);
    }
}

/* searches through the archive for all .dod (trainer battle) files
 * and feeds them to randomize_dod_file() */
bool Randomizer::randomize_trainers(Archive& archive, ArcFile& rand_data)
{
    if(rand_trainers_ || rand_full_party_ || (level_mod_ != 100) || rand_cost_)
    {
        int dir_index = archive.get_index("script/dollOperator");
        if(dir_index < 0)
        {
            error(L"dollOperator directory missing from game data");
            return false;
        }

        int index = archive.dir_begin(dir_index);
        int end_index = archive.dir_end(dir_index);
        if((index < 0) || (end_index < 0))
        {
            error(L"Error enumerating dollOperator directory");
            return false;
        }

        int step = (end_index - index) / 25;
        int count = 0;
        for(; index < end_index; ++index)
        {
            /* update progress bar */
            if(++count > step)
            {
                increment_progress_bar();
                count = 0;
            }

            if(archive.get_filename(index).find(".DOD") == std::string::npos)
                continue;

            ArcFile file;
            if(!(file = archive.get_file(index)))
            {
                error(L"Error iterating dollOperator directory");
                return false;
            }

            randomize_dod_file(file.data(), rand_data.data());

            if(!archive.repack_file(file))
            {
                error(L"Error repacking .dod file");
                return false;
            }
        }
    }

    return true;
}

bool Randomizer::randomize_skills(Archive& archive)
{
    ArcFile file;
    if(!(file = archive.get_file(is_ynk_ ? "doll/SkillData.sbs" : "doll/skill/SkillData.sbs")))
    {
        error(L"Error unpacking SkillData.sbs from game data");
        return false;
    }

    std::vector<unsigned int> power_deck, acc_deck, sp_deck, prio_deck;
    char *buf = file.data();

    std::set<unsigned int> skills = valid_skills_;
    for(auto i : skillcard_ids_)
        skills.insert(items_[i].skill_id);

    for(auto i : skills)
    {
        SkillData skill(&buf[i * SKILL_DATA_SIZE]);

        power_deck.push_back(skill.power);
        acc_deck.push_back(skill.accuracy);
        sp_deck.push_back(skill.sp);
        prio_deck.push_back(skill.priority);
        skills_[i] = skill;
    }

    if(!rand_skills_)
        return true;

    std::shuffle(power_deck.begin(), power_deck.end(), gen_);
    std::shuffle(acc_deck.begin(), acc_deck.end(), gen_);
    std::shuffle(sp_deck.begin(), sp_deck.end(), gen_);
    std::shuffle(prio_deck.begin(), prio_deck.end(), gen_);

    std::uniform_int_distribution<int> element(1, is_ynk_ ? ELEMENT_WARPED : ELEMENT_DREAM);
    std::bernoulli_distribution type(0.5);

    int step = skills.size() / 25;
    int count = 0;
    unsigned int index = 0;

    for(auto& it : skills_)
    {
        SkillData& skill(it.second);

        if(++count > step)
        {
            increment_progress_bar();
            count = 0;
        }

        if(rand_skill_element_)
            skill.element = element(gen_);

        assert(index < power_deck.size());
        if(rand_skill_power_)
            skill.power = power_deck[index];

        assert(index < acc_deck.size());
        if(rand_skill_acc_)
            skill.accuracy = acc_deck[index];

        assert(index < sp_deck.size());
        if(rand_skill_sp_)
            skill.sp = sp_deck[index];

        assert(index < prio_deck.size());
        if(rand_skill_prio_)
            skill.priority = prio_deck[index];

        if(rand_skill_type_ && (skill.type != SKILL_TYPE_STATUS))
            skill.type = type(gen_) ? SKILL_TYPE_FOCUS : SKILL_TYPE_SPREAD;

        skill.write(&buf[it.first * SKILL_DATA_SIZE]);
        ++index;
    }

    if(!archive.repack_file(file))
    {
        error(L"Error repacking SkillData.sbs");
        return false;
    }

    return true;
}

/* .mad files describe the wild puppet encounters in a particular location */
void Randomizer::randomize_mad_file(void *data)
{
    MADData mad(data);
    std::uniform_int_distribution<unsigned int> gen_normal(1, 10);
    std::uniform_int_distribution<unsigned int> gen_special(1, 5);
    std::uniform_int_distribution<unsigned int> gen_weight(1, 20); //max in base tpdp is ~25, reduced for less drastic RNG
	std::vector<MADEncounter> encounters;
	std::vector<MADEncounter> special_encounters;

	/* scan for existing puppets */
	for(int i = 0; i < 10; ++i)
	{
		if(mad.puppet_ids[i])
			encounters.emplace_back(mad, i, false);

		if((i < 5) && mad.special_puppet_ids[i])
			special_encounters.emplace_back(mad, i, true);
	}

	/* skip this file if no puppets live here */
	if(encounters.empty() && special_encounters.empty())
		return;

	/* adjust puppet levels */
	if(level_mod_ != 100)
	{
		double mod = double(level_mod_) / 100.0;
		for(auto& i : encounters)
		{
			double newlvl = (double(i.level) * mod);
			if(newlvl > 100)
				newlvl = 100;
			i.level = (uint8_t)newlvl;
		}
		for(auto& i : special_encounters)
		{
			double newlvl = (double(i.level) * mod);
			if(newlvl > 100)
				newlvl = 100;
			i.level = (uint8_t)newlvl;
		}
	}

	/* encounter randomization */
	if(rand_encounters_)
	{
		if(rand_encounters_ == 1)
		{
			/* since there's no way to tell what areas have what type of grass,
			 * we won't add any puppets to any grass type if we don't find some there already */
			unsigned int num_encounters = encounters.size() ? gen_normal(gen_) : 0;
			unsigned int num_special = special_encounters.size() ? gen_special(gen_) : 0;
			unsigned int max_level = 0;
			unsigned int max_special_level = 0;
            unsigned int weight_sum = 0;
            unsigned int special_weight_sum = 0;

			/* find the highest level puppets in each grass type. new puppets will be generated at this level */
			for(auto& i : encounters)
				if(i.level > max_level)
					max_level = i.level;

			for(auto& i : special_encounters)
				if(i.level > max_special_level)
					max_special_level = i.level;

			assert(max_level || !num_encounters);
			assert(max_special_level || !num_special);

			/* we've randomized the number of puppets in this area, add/remove puppets as necessary */
			encounters.resize(num_encounters);
			special_encounters.resize(num_special);

			/* we're generating entirely new puppets, set new indices for them */
			for(unsigned int i = 0; i < 10; ++i)
			{
				if(i < encounters.size())
					encounters[i].index = i;
				if(i < special_encounters.size())
					special_encounters[i].index = i;
			}

			/* randomize encounter rates and set levels */
			for(auto& i : encounters)
			{
				i.level = max_level;
				i.weight = gen_weight(gen_);
                weight_sum += i.weight;
			}
			for(auto& i : special_encounters)
			{
				i.level = max_special_level;
				i.weight = gen_weight(gen_);
                special_weight_sum += i.weight;
			}

            /* sum of weights determines frequency of encounters (?), if set too low encounters don't spawn
             * make sure sum of weights is large enough to spawn encounters */
            if(!encounters.empty() && (weight_sum < 30))
            {
                unsigned int d = (unsigned int)std::ceil(double(30 - weight_sum) / double(encounters.size()));
                for(auto& i : encounters)
                    i.weight += d;
            }
            if(!special_encounters.empty() && (special_weight_sum < 30))
            {
                unsigned int d = (unsigned int)std::ceil(double(30 - special_weight_sum) / double(special_encounters.size()));
                for(auto& i : special_encounters)
                    i.weight += d;
            }
		}

		/* randomize puppets and styles */
		for(auto& i : encounters)
		{
			i.id = puppet_id_pool_.draw(gen_);

			if(i.level >= 32)
				i.style = std::uniform_int_distribution<int>(0, puppets_[i.id].max_style_index())(gen_);
			else
				i.style = 0;
		}
		for(auto& i : special_encounters)
		{
			i.id = puppet_id_pool_.draw(gen_);

			if(i.level >= 32)
				i.style = std::uniform_int_distribution<int>(0, puppets_[i.id].max_style_index())(gen_);
			else
				i.style = 0;
		}
	}

	/* dump statistics */
	if(rand_export_locations_)
	{
		int weight_sum = 0;
		int special_weight_sum = 0;
		std::wstring loc_name;

		mad.location_name[31] = 0; /* ensure null-terminated */
		if(mad.location_name[0])
			loc_name = sjis_to_utf(mad.location_name);

		if(!loc_name.empty())
		{
			location_names_.insert(loc_name);
			auto c = location_names_.count(loc_name);
			if(c > 1)
				loc_name += L" [" + std::to_wstring(c) + L"]";
		}
		else
			loc_name = L"Unknown Location";

		for(auto& i : encounters)
			weight_sum += i.weight;
		for(auto& i : special_encounters)
			special_weight_sum += i.weight;

		for(auto& i : encounters)
		{
			std::wostringstream percentage;
			percentage.precision(3);
			percentage << (((double)i.weight / (double)weight_sum) * 100.0);

			/* text string describing the puppets that may be caught in this location (used with "export catch locations" option) */
			loc_map_[i.id].insert(loc_name + L" (" + puppets_[i.id].styles[i.style].style_string() + L") " + percentage.str() + L'%' + L" lvl " + std::to_wstring(i.level));
		}

		loc_name += L" (blue grass)";

		for(auto& i : special_encounters)
		{
			std::wostringstream percentage;
			percentage.precision(3);
			percentage << (((double)i.weight / (double)special_weight_sum) * 100.0);

			/* text string describing the puppets that may be caught in this location (used with "export catch locations" option) */
			loc_map_[i.id].insert(loc_name + L" (" + puppets_[i.id].styles[i.style].style_string() + L") " + percentage.str() + L'%' + L" lvl " + std::to_wstring(i.level));
		}
	}

	mad.clear_encounters();

	for(auto& i : encounters)
		i.write(mad, i.index, false);
	for(auto& i : special_encounters)
		i.write(mad, i.index, true);

    mad.write(data);
}

/* randomize how effective each element is against other elements.
 * the data for this is a csv text file (Compatibility.csv) arranged like a multiplication table.
 * row is source, column is target. see the type chart on the wiki for reference.
 * 0 = immune, 1 = not effective, 2 = neutral, 4 = super effective. */
bool Randomizer::randomize_compatibility(Archive& archive)
{
    if(!rand_compat_)
        return true;

    ArcFile file;
    if(!(file = archive.get_file(is_ynk_ ? "doll/Compatibility.csv" : "doll/elements/Compatibility.csv")))
    {
        error(L"Error unpacking compatibility.csv from game data");
        return false;
    }

    CSVFile csv(file.data(), file.size());
    if(csv.num_lines() < 18 || csv.num_fields() < 18)
    {
        error(L"Error parsing compatibility.csv");
        return false;
    }

    wchar_t chars[] = {L'0', L'1', L'2', L'4'};

    /* weight randomization towards neutral */
    std::discrete_distribution<int> dist({ 6, 35, 165, 35 });

    for(std::size_t line = 2; line < csv.num_lines(); ++line) // skip descriptor and null element
    {
        for(std::size_t field = 2; field < csv.num_fields(); ++field) // skip descriptor and null element
        {
            auto r = dist(gen_);
            csv[line][field] = chars[r];
        }
    }

    auto new_csv = csv.to_string();

    if(!archive.repack_file(file.file_index(), new_csv.data(), new_csv.size()))
    {
        error(L"Error repacking compatibility.csv");
        return false;
    }

    return true;
}

/* searches through the archive for .mad files and feeds them to randomize_mad_file() */
bool Randomizer::randomize_wild_puppets(Archive& archive)
{
    if(rand_encounters_ || rand_export_locations_ || (level_mod_ != 100))
    {
        int dir_index = archive.get_index("map/data");
        if(dir_index < 0)
        {
            error(L"map data directory missing from game data");
            return false;
        }

        int index = archive.dir_begin(dir_index);
        int end_index = archive.dir_end(dir_index);
        if((index < 0) || (end_index < 0))
        {
            error(L"Error enumerating map data directory");
            return false;
        }

        int step = (end_index - index) / 25;
        int count = 0;
        for(; index < end_index; ++index)
        {
            if(++count > step)
            {
                increment_progress_bar();
                count = 0;
            }

            if(!archive.is_dir(index))
                continue;

            int subdir_index = archive.dir_begin(index);
            int subdir_end = archive.dir_end(index);
            if((subdir_index < 0) || (subdir_end < 0))
            {
                error(L"Error enumerating map data subdirectory");
                return false;
            }

            for(; subdir_index < subdir_end; ++subdir_index)
            {
                if(archive.get_filename(subdir_index).find(".MAD") == std::string::npos)
                    continue;

                ArcFile file;
                if(!(file = archive.get_file(subdir_index)))
                {
                    error(L"Error iterating map data subdirectory");
                    return false;
                }

                randomize_mad_file(file.data());

                if(rand_encounters_ || (level_mod_ != 100)) /* don't repack if we're just dumping catch locations */
                {
                    if(!archive.repack_file(file))
                    {
                        error(L"Error repacking .mad file");
                        return false;
                    }
                }

                break;
            }
        }
    }

    return true;
}

bool Randomizer::parse_puppet_names(Archive& archive)
{
    ArcFile file;
    if(!(file = archive.get_file("name/DollName.csv")))
    {
        error(L"Error unpacking DollName.csv from game data");
        return false;
    }

    auto utf = sjis_to_utf(file.data(), file.size());

    std::size_t pos = 0;
    std::size_t endpos = utf.find(L"\r\n");

    while(endpos != std::string::npos)
    {
        puppet_names_.push_back(utf.substr(pos, endpos - pos));
        pos = endpos + 2;
        endpos = utf.find(L"\r\n", pos);
    }

    return true;
}

bool Randomizer::randomize(const std::wstring& dir, unsigned int seed)
{
    Archive archive;
    std::wstring path;

    /* here we will store modified files until randomization is complete.
     * this prevents leaving the game files partially randomized if we encounter
     * an error mid-randomization.
     * most of the files are less than 10MB so this should be fine */
    std::map<std::wstring, Archive> write_queue; // Key is filepath

    if(!path_exists(dir))
    {
        error(L"Invalid folder selected, please locate the game folder");
        return false;
    }

    gen_.seed(seed);

    rand_skills_ = rand_skill_element_ || rand_skill_power_ || rand_skill_acc_ || rand_skill_sp_ || rand_skill_prio_ || rand_skill_type_;
    rand_puppets_ = rand_skillsets_ || rand_stats_ || rand_types_ || rand_abilities_ || rand_cost_;

    path = dir + L"/dat/gn_dat1.arc";

    if(!open_archive(archive, path))
        return false;

    is_ynk_ = archive.is_ynk();

    /* ---encryption random data source--- */
    ArcFile rand_data;

    if(!(rand_data = archive.get_file("common/EFile.bin")))
    {
        error(L"Error unpacking EFile.bin from game data");
        return false;
    }

    path = dir + (is_ynk_ ? L"/dat/gn_dat6.arc" : L"/dat/gn_dat3.arc");

    if(!open_archive(archive, path))
        return false;

    if(!parse_puppets(archive))
        return false;

    if(!parse_items(archive))
        return false;

    if(!parse_skill_names(archive))
        return false;

    if(!parse_ability_names(archive))
        return false;

    if(!randomize_skills(archive))
        return false;

    set_progress_bar(25);

    if(!randomize_puppets(archive))
        return false;

    if(!randomize_compatibility(archive))
        return false;

    if(!export_compat(archive, dir + L"/type_chart.txt"))
        return false;

    set_progress_bar(50);

    if(is_ynk_)
    {
        write_queue[path] = std::move(archive);

        path = dir + L"/dat/gn_dat5.arc";

        if(!open_archive(archive, path))
            return false;
    }

    if(!parse_puppet_names(archive))
        return false;

    if(!randomize_trainers(archive, rand_data))
        return false;

    set_progress_bar(75);

    if(!randomize_wild_puppets(archive))
        return false;

    if(!save_archive(archive, path))
        return false;

    for(auto& it : write_queue)
    {
        if(!save_archive(it.second, it.first))
            return false;
    }

    if(rand_export_locations_)
        export_locations(dir + L"/catch_locations.txt");

    if(rand_export_puppets_)
        export_puppets(dir + L"/puppets.txt");

    return true;
}

void Randomizer::decrypt_puppet(void *src, const void *rand_data, std::size_t len)
{
    uint8_t *buf = (uint8_t*)src;
    const uint8_t *randbuf = (const uint8_t*)rand_data;

    for(unsigned int i = 0; i < (len / 3); ++i)
    {
        int index = (i * 3) % len;
        uint32_t crypto = read_le32(&randbuf[(i * 4) & 0x3fff]);

        /* need the higher half of this multiplication, right shifted 1 */
        uint32_t temp = (uint64_t(uint64_t(0xAAAAAAABu) * uint64_t(crypto)) >> 33);
        temp *= 3;

        if(crypto - temp == 0)
            buf[index] = ~buf[index];
        buf[index] -= uint8_t(crypto);
    }
}

void Randomizer::encrypt_puppet(void *src, const void *rand_data, std::size_t len)
{
    uint8_t *buf = (uint8_t*)src;
    const uint8_t *randbuf = (const uint8_t*)rand_data;

    for(unsigned int i = 0; i < (len / 3); ++i)
    {
        int index = (i * 3) % len;
        uint32_t crypto = read_le32(&randbuf[(i * 4) & 0x3fff]);

        uint32_t temp = (uint64_t(uint64_t(0xAAAAAAABu) * uint64_t(crypto)) >> 33);
        temp *= 3;

        buf[index] += uint8_t(crypto);
        if(crypto - temp == 0)
            buf[index] = ~buf[index];
    }
}

void Randomizer::error(const std::wstring& msg)
{
    gui_->error(msg.c_str());
}

unsigned int Randomizer::level_from_exp(const PuppetData& data, unsigned int exp) const
{
    return level_from_exp(data.cost, exp);
}

unsigned int Randomizer::level_from_exp(unsigned int cost, unsigned int exp) const
{
    int ret = 1;
    while(exp_for_level(cost, ret + 1) <= exp)
        ++ret;

    if(ret > 100)
        ret = 100;
    return ret;
}

unsigned int Randomizer::exp_for_level(const PuppetData& data, unsigned int level) const
{
    return exp_for_level(data.cost, level);
}

unsigned int Randomizer::exp_for_level(unsigned int cost, unsigned int level) const
{
    if(level <= 1)
        return 0;

    const int *mods = (is_ynk_) ? g_cost_exp_modifiers_ynk : g_cost_exp_modifiers;

    unsigned int ret = level * level * level * (unsigned int)mods[cost] / 100u;

    return ret;
}
