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

/* this could go in the command line, but i'll dump it here so it's conspicuous */
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "randomizer.h"
#include "filesystem.h"
#include "archive.h"
#include "gamedata.h"
#include "puppet.h"
#include "util.h"
#include "textconvert.h"
#include <memory>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <CommCtrl.h>
#include <ShlObj.h>
#include <cassert>
#include <iterator>

#define MW_STYLE (WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
#define CB_STYLE (WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX)
#define GRP_STYLE (WS_CHILD | WS_VISIBLE | BS_GROUPBOX | WS_GROUP)

#define WND_WIDTH 512
#define WND_HEIGHT 620

#define IS_CHECKED(chkbox) (SendMessageW(chkbox, BM_GETCHECK, 0, 0) == BST_CHECKED)
#define SET_CHECKED(chkbox, checked) SendMessageW(chkbox, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0)

enum
{
    ID_BROWSE = 1,
    ID_RANDOMIZE,
    ID_SKILLSETS,
    ID_STATS,
    ID_TYPES,
    ID_TRAINERS,
    ID_COMPAT,
    ID_ABILITIES,
    ID_GENERATE,
    ID_SKILL_ELEMENT,
    ID_SKILL_POWER,
    ID_SKILL_ACC,
    ID_SKILL_SP,
    ID_SKILL_PRIO,
    ID_SKILL_TYPE,
    ID_WILD_PUPPETS,
    ID_WILD_STYLE,
    ID_USE_QUOTA,
    ID_SKILLCARDS,
    ID_TRUE_RAND_STATS,
    ID_PREFER_SAME,
    ID_HEALTHY,
    ID_TRUE_RAND_SKILLS,
    ID_STAB
};

static const int cost_exp_modifiers[] = {70, 85, 100, 115, 130};
static const int cost_exp_modifiers_ynk[] = {85, 92, 100, 107, 115};

template<typename T> void subtract_set(std::vector<T>& vec, std::set<T>& s)
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

LRESULT CALLBACK Randomizer::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_COMMAND:
        if(HIWORD(wParam) == BN_CLICKED)
        {
            Randomizer *rnd = (Randomizer*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            switch(LOWORD(wParam))
            {
            case ID_GENERATE:
                {
                    rnd->generate_seed();
                }
                break;
            case ID_BROWSE:
                {
                    wchar_t buf[MAX_PATH] = {0};
                    BROWSEINFOW bi = {0};
                    bi.hwndOwner = hwnd;
                    bi.pszDisplayName = buf;
                    bi.lpszTitle = L"Select Game Folder";
                    bi.ulFlags = BIF_USENEWUI;

                    PIDLIST_ABSOLUTE pl = SHBrowseForFolderW(&bi);
                    if(!pl)
                        break;

                    SHGetPathFromIDListW(pl, buf);
                    CoTaskMemFree(pl);
                    SetWindowTextW(rnd->wnd_dir_, buf);
                }
                break;
            case ID_RANDOMIZE:
                if(MessageBoxW(hwnd, L"This will permanently modify game files.\r\nIf you have not backed up your game folder, you may wish to do so now.\r\n"
                                     "Also note that randomization is cumulative. Re-randomizing the same game data may have unexpected results.",
                                     L"Notice", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
                    break;
                if(rnd->randomize())
                {
                    SendMessageW(rnd->progress_bar_, PBM_SETPOS, 100, 0);
                    MessageBoxW(hwnd, L"Complete!", L"Success", MB_OK);
                }
                else
                    MessageBoxW(hwnd, L"An error occurred, randomization aborted", L"Error", MB_OK);
                SendMessageW(rnd->progress_bar_, PBM_SETPOS, 0, 0);

                rnd->clear();
                break;
            case ID_STATS:
                if(!IS_CHECKED(rnd->cb_stats_))
                {
                    SET_CHECKED(rnd->cb_true_rand_stats_, false);
                    SET_CHECKED(rnd->cb_use_quota_, false);
                    EnableWindow(rnd->wnd_quota_, false);
                }
                break;
            case ID_USE_QUOTA:
                {
                    bool checked = IS_CHECKED(rnd->cb_use_quota_);
                    EnableWindow(rnd->wnd_quota_, checked);
                    if(checked)
                    {
                        SET_CHECKED(rnd->cb_stats_, true);
                        SET_CHECKED(rnd->cb_true_rand_stats_, false);
                    }
                }
                break;
            case ID_TRUE_RAND_STATS:
                {
                    bool checked = IS_CHECKED(rnd->cb_true_rand_stats_);
                    if(checked)
                    {
                        SET_CHECKED(rnd->cb_stats_, true);
                        EnableWindow(rnd->wnd_quota_, false);
                        SET_CHECKED(rnd->cb_use_quota_, false);
                    }
                }
                break;
            case ID_PREFER_SAME:
                if(IS_CHECKED(rnd->cb_prefer_same_type_))
                    SET_CHECKED(rnd->cb_skills_, true);
                break;
            case ID_HEALTHY:
                if(IS_CHECKED(rnd->cb_healthy_))
                    SET_CHECKED(rnd->cb_abilities_, true);
                break;
            case ID_ABILITIES:
                if(!IS_CHECKED(rnd->cb_abilities_))
                    SET_CHECKED(rnd->cb_healthy_, false);
                break;
            case ID_SKILLSETS:
                if(!IS_CHECKED(rnd->cb_skills_))
                {
                    SET_CHECKED(rnd->cb_prefer_same_type_, false);
                    SET_CHECKED(rnd->cb_true_rand_skills_, false);
                    SET_CHECKED(rnd->cb_stab_, false);
                }
                break;
            case ID_TRUE_RAND_SKILLS:
                if(IS_CHECKED(rnd->cb_true_rand_skills_))
                {
                    SET_CHECKED(rnd->cb_skills_, true);
                    SET_CHECKED(rnd->cb_stab_, false);
                }
                break;
            case ID_STAB:
                if(IS_CHECKED(rnd->cb_stab_))
                {
                    SET_CHECKED(rnd->cb_skills_, true);
                    SET_CHECKED(rnd->cb_true_rand_skills_, false);
                }
                break;
            default:
                break;
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

BOOL CALLBACK set_font(HWND hwnd, LPARAM lparam)
{
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)lparam, TRUE);
    return TRUE;
}

static std::wstring get_window_text(HWND hwnd)
{
    int len = GetWindowTextLengthW(hwnd) + 1;
    if(!len)
        return std::wstring();

    wchar_t *buf = new wchar_t[len];
    memset(buf, 0, len * sizeof(wchar_t));

    GetWindowTextW(hwnd, buf, len);
    std::wstring ret = buf;
    delete[] buf;
    return ret;
}

HWND Randomizer::set_tooltip(HWND control, wchar_t *msg)
{
    HWND tip = CreateWindowExW(NULL, TOOLTIPS_CLASSW,
                NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_USEVISUALSTYLE,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, hwnd_, NULL, hInstance_, NULL);

    TOOLINFOW ti = {0};
    ti.cbSize = sizeof(ti);
    ti.hwnd = hwnd_;
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.uId = (UINT_PTR)control;
    ti.hinst = hInstance_;
    ti.lpszText = msg;
    SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, 400);
    SendMessageW(tip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 30 * 1000);

    return tip;
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
            temp += style.style_string() + L' ' + puppet_names_[puppet.id] + L" (";
            temp += element_string(style.element1);
            if(style.element2)
                temp += L'/' + element_string(style.element2);
            temp += L")\r\n";
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
                        temp += L"\t\t#" + std::to_wstring((i * 8) + j + 1) + L' ';
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
}

bool Randomizer::read_puppets(Archive& archive)
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

    return true;
}

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
        for(auto& it : csv.data())
        {
            if((it[3] == L"4") && (it[9] != L"0"))
                skill_pool.insert(stoul(it[9]));
        }

        skill_pool.erase(0);
        std::vector<unsigned int> skills(skill_pool.begin(), skill_pool.end());

        for(auto& it : csv.data())
        {
            if((it[3] == L"4") && (it[9] != L"0"))
            {
                std::shuffle(skills.begin(), skills.end(), gen_);
                it[9] = std::to_wstring(skills.back());
                skills.pop_back();
            }
        }

        std::string temp = csv.to_string();
        archive.repack_file("item/ItemData.csv", temp.c_str(), temp.length());
    }

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
        for(auto& it : csv.data())
            skill_names_[std::stol(it[0])] = it[1];
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

    for(auto& it : csv.data())
        ability_names_[std::stol(it[0])] = it[1];

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
    std::uniform_int_distribution<int> element(1, is_ynk_ ? ELEMENT_WARPED : ELEMENT_SOUND);
    std::uniform_int_distribution<int> pick_stat(0, 5);
    std::uniform_int_distribution<int> gen_stat(0, 0xff);
    std::uniform_int_distribution<int> gen_quota(0, 64);

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

    for(auto& it : puppets_)
    {
        PuppetData& puppet(it.second);

        if(count++ == step)
        {
            SendMessageW(progress_bar_, PBM_STEPIT, 0, 0);
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

        if(rand_skillsets_)
        {
            std::vector<unsigned int> skill_deck;
            if(rand_true_rand_skills_)
                skill_deck.assign(valid_skills_.begin(), valid_skills_.end());
            else
                skill_deck.assign(valid_base_skills.begin(), valid_base_skills.end());
            std::shuffle(skill_deck.begin(), skill_deck.end(), gen_);
            for(auto& i : puppet.base_skills)
            {
                if((i != 0) && !skill_deck.empty())
                {
                    if(rand_prefer_same_type_ && chance60(gen_))
                    {
                        bool found = false;
                        for(auto it = skill_deck.begin(); it != skill_deck.end(); ++it)
                        {
                            auto e = skills_[*it].element;
                            if((e == puppet.styles[0].element1) || (e == puppet.styles[0].element2))
                            {
                                i = *it;
                                skill_deck.erase(it);
                                found = true;
                                break;
                            }
                        }
                        if(!found)
                        {
                            i = skill_deck.back();
                            skill_deck.pop_back();
                        }
                    }
                    else
                    {
                        i = skill_deck.back();
                        skill_deck.pop_back();
                    }
                }
            }
        }

        for(auto& style : puppet.styles)
        {
            if(style.style_type == 0)
                continue;

            if(rand_skillsets_)
            {
                style.skillset.clear();
                style.skillset = puppet.styles[0].skillset;
                std::vector<unsigned int> skill_deck;

                for(auto& i : puppet.base_skills)
                    style.skillset.insert(i);

                if((style.lv100_skill != 0) && !valid_lv100_skills.empty())
                {
                    if(rand_true_rand_skills_)
                        skill_deck.assign(valid_skills_.begin(), valid_skills_.end());
                    else
                        skill_deck.assign(valid_lv100_skills.begin(), valid_lv100_skills.end());
                    subtract_set(skill_deck, style.skillset);
                    if(!skill_deck.empty())
                    {
                        std::shuffle(skill_deck.begin(), skill_deck.end(), gen_);
                        style.lv100_skill = skill_deck[0];
                        if(rand_prefer_same_type_ && chance60(gen_))
                        {
                            for(auto i : skill_deck)
                            {
                                auto e = skills_[i].element;
                                if((e == style.element1) || (e == style.element2))
                                {
                                    style.lv100_skill = i;
                                    break;
                                }
                            }
                        }
                    }
                }
                style.skillset.insert(style.lv100_skill);

                if(rand_true_rand_skills_)
                    skill_deck.assign(valid_skills_.begin(), valid_skills_.end());
                else if(style.style_type == STYLE_NORMAL)
                {
                    skill_deck.assign(valid_base_skills.begin(), valid_base_skills.end());
                    for(auto i : valid_normal_skills)
                        skill_deck.push_back(i);
                }
                else
                    skill_deck.assign(valid_evolved_skills.begin(), valid_evolved_skills.end());
                subtract_set(skill_deck, style.skillset);
                std::shuffle(skill_deck.begin(), skill_deck.end(), gen_);

                /* ensure every puppet starts with at least one damaging move */
                if((style.style_type == STYLE_NORMAL) && !skill_deck.empty() && !rand_true_rand_skills_)
                {
                    style.style_skills[0] = 56; /* default to yin energy if we don't find a match below */
                    for(auto it = skill_deck.begin(); it != skill_deck.end(); ++it)
                    {
                        auto e = skills_[*it].element;
                        if((skills_[*it].type != SKILL_TYPE_STATUS) && (skills_[*it].power > 0) && (!rand_stab_ || (e == style.element1) || (e == style.element2)))
                        {
                            style.style_skills[0] = *it;
                            style.skillset.insert(*it);
                            skill_deck.erase(it);
                            break;
                        }
                    }
                }

                for(int j = (((style.style_type == STYLE_NORMAL) && !rand_true_rand_skills_) ? 1 : 0); j < 11; ++j)
                {
                    auto& i(style.style_skills[j]);
                    if((i != 0) && !skill_deck.empty())
                    {
                        if(rand_prefer_same_type_ && chance60(gen_))
                        {
                            bool found = false;
                            for(auto it = skill_deck.begin(); it != skill_deck.end(); ++it)
                            {
                                auto e = skills_[*it].element;
                                if((e == style.element1) || (e == style.element2))
                                {
                                    i = *it;
                                    skill_deck.erase(it);
                                    found = true;
                                    break;
                                }
                            }
                            if(!found)
                            {
                                i = skill_deck.back();
                                skill_deck.pop_back();
                            }
                        }
                        else
                        {
                            i = skill_deck.back();
                            skill_deck.pop_back();
                        }
                    }
                    style.skillset.insert(i);
                }

                if(rand_true_rand_skills_)
                    skill_deck.assign(valid_skills_.begin(), valid_skills_.end());
                else
                    skill_deck.assign(valid_lv70_skills.begin(), valid_lv70_skills.end());
                subtract_set(skill_deck, style.skillset);
                std::shuffle(skill_deck.begin(), skill_deck.end(), gen_);

                for(auto& i : style.lv70_skills)
                {
                    if((i != 0) && !skill_deck.empty())
                    {
                        if(rand_prefer_same_type_ && chance60(gen_))
                        {
                            bool found = false;
                            for(auto it = skill_deck.begin(); it != skill_deck.end(); ++it)
                            {
                                auto e = skills_[*it].element;
                                if((e == style.element1) || (e == style.element2))
                                {
                                    i = *it;
                                    skill_deck.erase(it);
                                    found = true;
                                    break;
                                }
                            }
                            if(!found)
                            {
                                i = skill_deck.back();
                                skill_deck.pop_back();
                            }
                        }
                        else
                        {
                            i = skill_deck.back();
                            skill_deck.pop_back();
                        }
                    }
                    style.skillset.insert(i);
                }

                style.skillset.erase(0);

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
                        if((same_element && chance75(gen_)) || chance25(gen_))
                            style.skill_compat_table[compat_index] |= (1 << offset);
                    }
                    else if(chance35(gen_))
                    {
                        style.skill_compat_table[compat_index] |= (1 << offset);
                    }
                }
            }

            if(rand_abilities_)
            {
                memset(style.abilities, 0, sizeof(style.abilities));
                std::shuffle(ability_deck.begin(), ability_deck.end(), gen_);
                int index = 0;
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

                    if((i == 0) || chance90(gen_))
                        style.abilities[i] = ability_deck[index++];
                }
            }

            if(rand_stats_)
            {
                if(rand_quota_)
                {
                    memset(style.base_stats, 0, sizeof(style.base_stats));
                    int sum = 0;
                    while(sum < stat_quota_)
                    {
                        auto& i(style.base_stats[pick_stat(gen_)]);

                        int temp = gen_quota(gen_);
                        if((temp + (int)i) > 0xff)
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

        puppet.write(&file.data()[puppet.id * PUPPET_DATA_SIZE]);
    }

    if(!archive.repack_file(file))
    {
        error(L"Error repacking dolldata.dbs");
        return false;
    }

    return true;
}

void Randomizer::randomize_dod_file(void *src, const void *rand_data)
{
    char *buf = (char*)src + 0x2C;
    char *endbuf = buf + (6 * PUPPET_SIZE_BOX);
    std::vector<unsigned int> item_ids(held_item_ids_.begin(), held_item_ids_.end());
    std::uniform_int_distribution<int> iv(0, 0xf);
    std::uniform_int_distribution<int> ev(0, 64);
    std::uniform_int_distribution<int> pick_ev(0, 5);
    std::uniform_int_distribution<int> id(0, valid_puppet_ids_.size() - 1);
    std::bernoulli_distribution chance25(0.25);
    std::bernoulli_distribution coin_flip(0.5);

    std::shuffle(item_ids.begin(), item_ids.end(), gen_);

    unsigned int max_lvl = 0;
    double lvl_mul = double(level_mod_) / 100.0;
    for(char *pos = buf; pos < endbuf; pos += PUPPET_SIZE_BOX)
    {
        decrypt_puppet(pos, rand_data, PUPPET_SIZE);

        Puppet puppet(pos, false);
        unsigned int lvl = level_from_exp(puppets_[puppet.puppet_id], puppet.exp);
        lvl = (unsigned int)(double(lvl) * lvl_mul);
        if(lvl > 100)
            lvl = 100;
        if(lvl > max_lvl)
            max_lvl = lvl;

        if(puppet.exp == 0)
            lvl = max_lvl;

        if(((puppet.puppet_id == 0) && rand_full_party_) || ((puppet.puppet_id != 0) && rand_trainers_))
        {
            PuppetData& data(puppets_[valid_puppet_ids_[id(gen_)]]);

            if(lvl >= 30)
            {
                int max_index = 0;
                for(int j = 0; j < 4; ++j)
                {
                    if(data.styles[j].style_type > 0)
                        max_index = j;
                    else
                        break;
                }

                puppet.style_index = std::uniform_int_distribution<int>(0, max_index)(gen_);
            }
            else
                puppet.style_index = 0;

            const StyleData& style(data.styles[puppet.style_index]);

            std::vector<unsigned int> skills(style.skillset.begin(), style.skillset.end());

            for(unsigned int i = 0; i < 16; ++i)
            {
                for(unsigned int j = 0; j < 8; ++j)
                {
                    if(style.skill_compat_table[i] & (1 << j))
                        skills.push_back(items_[385 + (8 * i) + j].skill_id);
                }
            }

            std::shuffle(skills.begin(), skills.end(), gen_);

            puppet.puppet_id = data.id;

            int skill_index = 0;
            for(auto& i : puppet.skills)
                i = skills[skill_index++];

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

            if(coin_flip(gen_))
            {
                puppet.ability_index ^= 1;
                if(data.styles[puppet.style_index].abilities[puppet.ability_index] == 0)
                    puppet.ability_index = 0;
            }

            if(!item_ids.empty() && chance25(gen_))
            {
                puppet.held_item_id = item_ids.back();
                item_ids.pop_back();
            }
            else
                puppet.held_item_id = 0;

            if(puppet.puppet_id < puppet_names_.size())
                puppet.set_puppet_nickname(puppet_names_[puppet.puppet_id]);
        }

        if(puppet.puppet_id)
            puppet.exp = exp_for_level(puppets_[puppet.puppet_id], lvl);
        puppet.write(pos, false);

        encrypt_puppet(pos, rand_data, PUPPET_SIZE);
    }
}

bool Randomizer::randomize_trainers(Archive& archive, ArcFile& rand_data)
{
    if(rand_trainers_ || rand_full_party_ || (level_mod_ != 100))
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
            if(count++ == step)
            {
                SendMessageW(progress_bar_, PBM_STEPIT, 0, 0);
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

        if(count++ == step)
        {
            SendMessageW(progress_bar_, PBM_STEPIT, 0, 0);
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

void Randomizer::randomize_mad_file(void *data)
{
    char *buf = (char*)data;
    char *area_name = &buf[0x59];

    if(rand_wild_puppets_ || rand_wild_style_)
    {
        uint16_t *puppet_table = (uint16_t*)&buf[0x0E];
        char *style_table = &buf[0x2C];

        for(int i = 0; i < 10; ++i)
        {
            if(!puppet_table[i])
                continue;

            if(rand_wild_puppets_)
            {
                if(puppet_id_pool_.empty())
                {
                    puppet_id_pool_ = valid_puppet_ids_;
                    std::shuffle(puppet_id_pool_.begin(), puppet_id_pool_.end(), gen_);
                }

                puppet_table[i] = puppet_id_pool_.back();
                puppet_id_pool_.pop_back();
            }

            const PuppetData& puppet(puppets_[puppet_table[i]]);
            if(rand_wild_style_ || !puppet.styles[style_table[i]].style_type)
            {
                int max_index = 0;
                for(int j = 0; j < 4; ++j)
                {
                    if(puppet.styles[j].style_type > 0)
                        max_index = j;
                    else
                        break;
                }

                style_table[i] = std::uniform_int_distribution<int>(0, max_index)(gen_);
            }
        }

        puppet_table = (uint16_t*)&buf[0x40];
        style_table = &buf[0x4F];
        for(int i = 0; i < 5; ++i)
        {
            if(!puppet_table[i])
                continue;

            if(rand_wild_puppets_)
            {
                if(puppet_id_pool_.empty())
                {
                    puppet_id_pool_ = valid_puppet_ids_;
                    std::shuffle(puppet_id_pool_.begin(), puppet_id_pool_.end(), gen_);
                }

                puppet_table[i] = puppet_id_pool_.back();
                puppet_id_pool_.pop_back();
            }

            const PuppetData& puppet(puppets_[puppet_table[i]]);
            if(rand_wild_style_ || !puppet.styles[style_table[i]].style_type)
            {
                int max_index = 0;
                for(int j = 0; j < 4; ++j)
                {
                    if(puppet.styles[j].style_type > 0)
                        max_index = j;
                    else
                        break;
                }

                style_table[i] = std::uniform_int_distribution<int>(0, max_index)(gen_);
            }
        }
    }

    double mod = double(level_mod_) / 100.0;

    char *lvls = &buf[0x22];
    char *style_table = &buf[0x2C];
    uint16_t *puppet_table = (uint16_t*)&buf[0x0E];
    std::wstring loc_name;
    if(area_name[0])
        loc_name = sjis_to_utf(area_name);
    for(int i = 0; i < 10; ++i)
    {
        double newlvl = double(lvls[i]) * mod;
        if(newlvl > 100)
            newlvl = 100;
        lvls[i] = (char)newlvl;
        if(lvls[i] < 30)
            style_table[i] = 0;

        if(rand_export_locations_ && puppet_table[i] && !loc_name.empty())
            loc_map_[puppet_table[i]].insert(loc_name + L" (" + puppets_[puppet_table[i]].styles[style_table[i]].style_string() + L')');
    }

    lvls = &buf[0x4A];
    style_table = &buf[0x4F];
    puppet_table = (uint16_t*)&buf[0x40];
    if(!loc_name.empty())
        loc_name += L" (blue grass)";
    for(int i = 0; i < 5; ++i)
    {
        double newlvl = double(lvls[i]) * mod;
        if(newlvl > 100)
            newlvl = 100;
        lvls[i] = (char)newlvl;
        if(lvls[i] < 30)
            style_table[i] = 0;

        if(rand_export_locations_ && puppet_table[i] && !loc_name.empty())
            loc_map_[puppet_table[i]].insert(loc_name + L" (" + puppets_[puppet_table[i]].styles[style_table[i]].style_string() + L')');
    }
}

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

    char chars[] = {'0', '1', '2', '4'};

    /* we don't want tons of immunities, so weight randomization towards neutral */
    std::normal_distribution<double> dist(2.0, 0.9);

    for(char *pos = file.data(); pos < (file.data() + file.size()); ++pos)
    {
        if((*pos >= '0') && (*pos <= '9'))
        {
            int r = lround(dist(gen_));
            if(r < 0)
                r = 0;  /* toss these into the immunities since the immunity count should be pretty low anyway */
            if(r > 3)
                r = 2;  /* make these neutral so we don't have too many weaknesses */
            *pos = chars[r];
        }
    }

    if(!archive.repack_file(file))
    {
        error(L"Error repacking compatibility.csv");
        return false;
    }

    return true;
}

bool Randomizer::randomize_wild_puppets(Archive& archive)
{
    if(rand_wild_puppets_ || rand_wild_style_ || rand_export_locations_ || (level_mod_ != 100))
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
            if(count++ == step)
            {
                SendMessageW(progress_bar_, PBM_STEPIT, 0, 0);
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

                if(rand_wild_puppets_ || rand_wild_style_ || (level_mod_ != 100)) /* don't repack if we're just dumping catch locations */
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

    const char *pos = file.data();
    const char *eof = pos + file.size();
    for(const char *endpos = pos; endpos < eof; ++endpos)
    {
        if(*endpos == '\r')
        {
            puppet_names_.push_back(std::move(sjis_to_utf(pos, endpos)));
            pos = endpos + 2;
            ++endpos;
        }
    }

    return true;
}

void Randomizer::generate_seed()
{
    std::random_device rdev;
    unsigned int seed = rdev();
    SetWindowTextW(wnd_seed_, std::to_wstring(seed).c_str());
}

/* raw winapi so we don't have any dependencies */
/* FIXME: mistakes were made, raw winapi is an abomination */
Randomizer::Randomizer(HINSTANCE hInstance)
{
    hInstance_ = hInstance;
    RECT rect;

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rect, 0);

    hwnd_ = CreateWindowW(L"MainWindow", L"TPDP Randomizer "  VERSION_STRING, MW_STYLE, (rect.right - WND_WIDTH) / 2, (rect.bottom - WND_HEIGHT) / 2, WND_WIDTH, WND_HEIGHT, NULL, NULL, hInstance, NULL);
    if(hwnd_ == NULL)
        abort();

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);

    GetClientRect(hwnd_, &rect);

    grp_dir_ = CreateWindowW(L"Button", L"Game Folder", GRP_STYLE, 10, 10, rect.right - 20, 55, hwnd_, NULL, hInstance, NULL);
    grp_rand_ = CreateWindowW(L"Button", L"Randomization", GRP_STYLE, 10, 75, rect.right - 20, 245, hwnd_, NULL, hInstance, NULL);
    grp_other_ = CreateWindowW(L"Button", L"Other", GRP_STYLE, 10, 330, rect.right - 20, 115, hwnd_, NULL, hInstance, NULL);
    grp_seed_ = CreateWindowW(L"Button", L"Seed", GRP_STYLE, 10, 455, rect.right - 20, 55, hwnd_, NULL, hInstance, NULL);
    bn_Randomize_ = CreateWindowW(L"Button", L"Randomize", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, (rect.right / 2) - 50, 520, 100, 30, hwnd_, (HMENU)ID_RANDOMIZE, hInstance, NULL);
    progress_bar_ = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE, 10, rect.bottom - 30, rect.right - 20, 20, hwnd_, NULL, hInstance, NULL);
    SendMessageW(progress_bar_, PBM_SETSTEP, 1, 0);

    GetClientRect(grp_dir_, &rect);
    wnd_dir_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"C:\\game\\FocasLens\\幻想人形演舞", WS_CHILD | WS_VISIBLE, 20, 30, rect.right - 90, 25, hwnd_, NULL, hInstance, NULL);
    bn_browse_ = CreateWindowW(L"Button", L"Browse", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, rect.right - 65, 28, 65, 29, hwnd_, (HMENU)ID_BROWSE, hInstance, NULL);

    GetClientRect(grp_other_, &rect);
    wnd_lvladjust_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"100", WS_CHILD | WS_VISIBLE | ES_NUMBER, 20, 350, 50, 23, hwnd_, NULL, hInstance, NULL);
    tx_lvladjust_ = CreateWindowW(L"Static", L"% enemy level adjustment", WS_CHILD | WS_VISIBLE | SS_WORDELLIPSIS, 75, 352, (rect.right / 2) - 75, 23, hwnd_, NULL, hInstance, NULL);
    cb_trainer_party_ = CreateWindowW(L"Button", L"Full trainer party", CB_STYLE, (rect.right / 2) + 20, 353, (rect.right / 2) - 25, 15, hwnd_, NULL, hInstance, NULL);
    cb_export_locations_ = CreateWindowW(L"Button", L"Export catch locations", CB_STYLE, 20, 385, (rect.right / 2) - 25, 15, hwnd_, NULL, hInstance, NULL);
    wnd_quota_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"500", WS_CHILD | WS_VISIBLE | ES_NUMBER | WS_DISABLED, (rect.right / 2) + 20, 383, 50, 23, hwnd_, NULL, hInstance, NULL);
    tx_lvladjust_ = CreateWindowW(L"Static", L"Stat quota", WS_CHILD | WS_VISIBLE | SS_WORDELLIPSIS, (rect.right / 2) + 75, 385, (rect.right / 2) - 75, 23, hwnd_, NULL, hInstance, NULL);
    cb_healthy_ = CreateWindowW(L"Button", L"Healthy", CB_STYLE, 20, 415, (rect.right / 2) - 25, 15, hwnd_, (HMENU)ID_HEALTHY, hInstance, NULL);
    cb_export_puppets_ = CreateWindowW(L"Button", L"Dump puppet stats", CB_STYLE, (rect.right / 2) + 20, 415, (rect.right / 2) - 25, 15, hwnd_, NULL, hInstance, NULL);

    GetClientRect(grp_seed_, &rect);
    wnd_seed_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"0", WS_CHILD | WS_VISIBLE | ES_NUMBER, 20, 475, rect.right - 125, 25, hwnd_, NULL, hInstance, NULL);
    bn_generate_ = CreateWindowW(L"Button", L"Generate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, rect.right - 100, 473, 100, 29, hwnd_, (HMENU)ID_GENERATE, hInstance, NULL);
    generate_seed();

    GetClientRect(grp_rand_, &rect);
    int x = 25;
    int y = 105;
    int width = rect.right - 30;
    int height = rect.bottom - 60;
    int width_cb = (width / 3) - 5;
    cb_abilities_ = CreateWindowW(L"Button", L"Puppet Abilities", CB_STYLE, x, y, width_cb, 15, hwnd_, (HMENU)ID_ABILITIES, hInstance, NULL);
    cb_skills_ = CreateWindowW(L"Button", L"Skillsets", CB_STYLE, (width / 3) + x, y, width_cb, 15, hwnd_, (HMENU)ID_SKILLSETS, hInstance, NULL);
    cb_stats_ = CreateWindowW(L"Button", L"Base Stats", CB_STYLE, ((width / 3) * 2) + x, y, width_cb, 15, hwnd_, (HMENU)ID_STATS, hInstance, NULL);
    cb_types_ = CreateWindowW(L"Button", L"Puppet Types", CB_STYLE, x, y + 30, width_cb, 15, hwnd_, (HMENU)ID_TYPES, hInstance, NULL);
    cb_trainers_ = CreateWindowW(L"Button", L"Trainers", CB_STYLE, (width / 3) + x, y + 30, width_cb, 15, hwnd_, (HMENU)ID_TRAINERS, hInstance, NULL);
    cb_compat_ = CreateWindowW(L"Button", L"Type Effectiveness", CB_STYLE, ((width / 3) * 2) + x, y + 30, width_cb, 15, hwnd_, (HMENU)ID_COMPAT, hInstance, NULL);
    cb_skill_element_ = CreateWindowW(L"Button", L"Skill Element", CB_STYLE, x, y + 60, width_cb, 15, hwnd_, (HMENU)ID_SKILL_ELEMENT, hInstance, NULL);
    cb_skill_power_ = CreateWindowW(L"Button", L"Skill Power", CB_STYLE, x + (width / 3), y + 60, width_cb, 15, hwnd_, (HMENU)ID_SKILL_POWER, hInstance, NULL);
    cb_skill_acc_ = CreateWindowW(L"Button", L"Skill Accuracy", CB_STYLE, x + ((width / 3) * 2), y + 60, width_cb, 15, hwnd_, (HMENU)ID_SKILL_ACC, hInstance, NULL);
    cb_skill_sp_ = CreateWindowW(L"Button", L"Skill SP", CB_STYLE, x, y + 90, width_cb, 15, hwnd_, (HMENU)ID_SKILL_SP, hInstance, NULL);
    cb_skill_prio_ = CreateWindowW(L"Button", L"Skill Priority", CB_STYLE, x + (width / 3), y + 90, width_cb, 15, hwnd_, (HMENU)ID_SKILL_PRIO, hInstance, NULL);
    cb_skill_type_ = CreateWindowW(L"Button", L"Skill Type", CB_STYLE, x + ((width / 3) * 2), y + 90, width_cb, 15, hwnd_, (HMENU)ID_SKILL_TYPE, hInstance, NULL);
    cb_wild_puppets_ = CreateWindowW(L"Button", L"Wild Puppets", CB_STYLE, x, y + 120, width_cb, 15, hwnd_, (HMENU)ID_WILD_PUPPETS, hInstance, NULL);
    cb_wild_style_ = CreateWindowW(L"Button", L"Wild Puppet Styles", CB_STYLE, x + (width / 3), y + 120, width_cb, 15, hwnd_, (HMENU)ID_WILD_STYLE, hInstance, NULL);
    cb_use_quota_ = CreateWindowW(L"Button", L"Use stat quota", CB_STYLE, x + ((width / 3) * 2), y + 120, width_cb, 15, hwnd_, (HMENU)ID_USE_QUOTA, hInstance, NULL);
    cb_skillcards_ = CreateWindowW(L"Button", L"Skill Cards", CB_STYLE, x, y + 150, width_cb, 15, hwnd_, (HMENU)ID_SKILLCARDS, hInstance, NULL);
    cb_true_rand_stats_ = CreateWindowW(L"Button", L"True random stats", CB_STYLE, x + (width / 3), y + 150, width_cb, 15, hwnd_, (HMENU)ID_TRUE_RAND_STATS, hInstance, NULL);
    cb_prefer_same_type_ = CreateWindowW(L"Button", L"Prefer same type", CB_STYLE, x + ((width / 3) * 2), y + 150, width_cb, 15, hwnd_, (HMENU)ID_PREFER_SAME, hInstance, NULL);
    cb_true_rand_skills_ = CreateWindowW(L"Button", L"True random skills", CB_STYLE, x, y + 180, width_cb, 15, hwnd_, (HMENU)ID_TRUE_RAND_SKILLS, hInstance, NULL);
    cb_stab_ = CreateWindowW(L"Button", L"STAB starting move", CB_STYLE, x + (width / 3), y + 180, width_cb, 15, hwnd_, (HMENU)ID_STAB, hInstance, NULL);

    set_tooltip(cb_skills_, L"Randomize the skills each puppet can learn");
    set_tooltip(cb_stats_, L"Randomize puppet base stats");
    set_tooltip(cb_types_, L"Randomize puppet typings");
    set_tooltip(cb_trainers_, L"Randomize trainer puppets");
    set_tooltip(cb_compat_, L"Randomize how effective each element is against other elements (weighted towards neutral)");
    set_tooltip(cb_abilities_, L"Randomize the abilities each puppet can have");
    set_tooltip(wnd_seed_, L"The seed used for randomization (must be an integer).\r\nA given seed will always generate the same result when using the same settings");
    set_tooltip(cb_skill_element_, L"Randomize skill elements");
    set_tooltip(cb_skill_power_, L"Randomize skill base power");
    set_tooltip(cb_skill_acc_, L"Randomize skill accuracy");
    set_tooltip(cb_skill_sp_, L"Randomize skill SP");
    set_tooltip(cb_skill_prio_, L"Randomize skill priority");
    set_tooltip(cb_skill_type_, L"Randomize skill type (focus or spread. does not affect status skills)");
    set_tooltip(wnd_lvladjust_, L"Trainer puppets and wild puppets will have their level adjusted to the specified percentage\r\n100% = no change");
    set_tooltip(cb_trainer_party_, L"Trainers will always have 6 puppets.\r\nnew puppets will be randomly generated at the same level as the highest level existing puppet");
    set_tooltip(cb_wild_puppets_, L"Randomize which puppets appear in the wild");
    set_tooltip(cb_wild_style_, L"Randomize which style of puppets appear in the wild (power, defence, etc)");
    set_tooltip(cb_export_locations_, L"Export the locations where each puppet can be caught in the wild.\r\nThis will be written to catch_locations.txt in the game folder.");
    set_tooltip(cb_use_quota_, L"When stat randomization is enabled, each puppet will recieve the same total sum of stat points (distributed randomly)\r\nThe number of stat points can be adjusted in the \"Stat quota\" field below");
    set_tooltip(wnd_quota_, L"When \"Use stat quota\" is enabled, this number dictates the total sum of stat points each puppet recieves");
    set_tooltip(cb_healthy_, L"Remove \"Frail Health\" from the ability pool");
    set_tooltip(cb_skillcards_, L"Skill Cards will teach random skills");
    set_tooltip(cb_true_rand_stats_, L"Puppet base stats will be completely random");
    set_tooltip(cb_prefer_same_type_, L"Randomization will favor skills that match a puppets typing");
    set_tooltip(cb_export_puppets_, L"Write puppet stats/skillsets/etc to puppets.txt in the game folder");
    set_tooltip(cb_true_rand_skills_, L"Any puppet can have any skill\r\nThe default behaviour preserves the \"move pools\" of puppets, meaning that normal puppets will generally have less powerful moves.\r\nThis option disables that.");
    set_tooltip(cb_stab_, L"Puppets are guaranteed a damaging same-type starting move (so long as such a move exists in the pool)");

    NONCLIENTMETRICSW ncm = {0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    hfont_ = CreateFontIndirectW(&ncm.lfMessageFont);

    EnumChildWindows(hwnd_, set_font, (LPARAM)hfont_);
}

Randomizer::~Randomizer()
{
    /* since we're only instatiating this once, and it's only destroyed when main returns,
     * we can just do this */
    DeleteObject(hfont_);
}

bool Randomizer::register_window_class(HINSTANCE hInstance)
{
    /* just throw this in here since it's part of initialization anyway */
    INITCOMMONCONTROLSEX icc = {0};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASSW wc = {0};
    wc.lpszClassName = L"MainWindow";
    wc.hInstance = hInstance;
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(0, IDC_ARROW);

    return (RegisterClassW(&wc) != 0);
}

bool Randomizer::randomize()
{
    Archive archive;
    std::wstring dir = get_window_text(wnd_dir_);
    std::wstring path;
    if(!path_exists(dir))
    {
        error(L"Invalid folder selected, please locate the game folder");
        return false;
    }

    std::wstring seedtext = get_window_text(wnd_seed_);
    for(auto it : seedtext)
    {
        if(!iswdigit(it))
        {
            error(L"Seed must contain only digits");
            return false;
        }
    }

    unsigned int seed = std::stoul(seedtext);
    gen_.seed(seed);

    level_mod_ = std::stol(get_window_text(wnd_lvladjust_));
    if(level_mod_ <= 0)
    {
        error(L"Invalid trainer level modifier. Must be an integer greater than zero");
        return false;
    }

    rand_skillsets_ = IS_CHECKED(cb_skills_);
    rand_stats_ = IS_CHECKED(cb_stats_);
    rand_trainers_ = IS_CHECKED(cb_trainers_);
    rand_types_ = IS_CHECKED(cb_types_);
    rand_compat_ = IS_CHECKED(cb_compat_);
    rand_abilities_ = IS_CHECKED(cb_abilities_);
    rand_full_party_ = IS_CHECKED(cb_trainer_party_);
    rand_wild_puppets_ = IS_CHECKED(cb_wild_puppets_);
    rand_wild_style_ = IS_CHECKED(cb_wild_style_);
    rand_export_locations_ = IS_CHECKED(cb_export_locations_);
    rand_quota_ = IS_CHECKED(cb_use_quota_);
    rand_healthy_ = IS_CHECKED(cb_healthy_);
    rand_skillcards_ = IS_CHECKED(cb_skillcards_);
    rand_true_rand_stats_ = IS_CHECKED(cb_true_rand_stats_);
    rand_prefer_same_type_ = IS_CHECKED(cb_prefer_same_type_);
    rand_export_puppets_ = IS_CHECKED(cb_export_puppets_);
    rand_true_rand_skills_ = IS_CHECKED(cb_true_rand_skills_);
    rand_puppets_ = rand_skillsets_ || rand_stats_ || rand_types_ || rand_abilities_;
    rand_skill_element_ = IS_CHECKED(cb_skill_element_);
    rand_skill_power_ = IS_CHECKED(cb_skill_power_);
    rand_skill_acc_ = IS_CHECKED(cb_skill_acc_);
    rand_skill_sp_ = IS_CHECKED(cb_skill_sp_);
    rand_skill_prio_ = IS_CHECKED(cb_skill_prio_);
    rand_skill_type_ = IS_CHECKED(cb_skill_type_);
    rand_stab_ = IS_CHECKED(cb_stab_);
    rand_skills_ = rand_skill_element_ || rand_skill_power_ || rand_skill_acc_ || rand_skill_sp_ || rand_skill_prio_ || rand_skill_type_;

    if(rand_quota_)
    {
        std::wstring quota_text = get_window_text(wnd_quota_);
        stat_quota_ = std::stol(quota_text);
        if(stat_quota_ < 1)
        {
            error(L"Stat quota must be an integer greater than zero.");
            return false;
        }
        else if(stat_quota_ > (0xFF * 6))
        {
            error(L"Stat quota too large! Maximum quota is 1530.");
            return false;
        }
    }

    path = dir + L"/dat/gn_dat1.arc";

    try
    {
        archive.open(path);
    }
    catch(const ArcError& ex)
    {
        error(L"failed to open file: " + path + L"\r\n" + utf_widen(ex.what()));
        return false;
    }

    is_ynk_ = archive.is_ynk();

    /* ---encryption random data source--- */

    ArcFile rand_data;

    if(!(rand_data = archive.get_file("common/EFile.bin")))
    {
        error(L"Error unpacking EFile.bin from game data");
        return false;
    }

    path = dir + (is_ynk_ ? L"/dat/gn_dat6.arc" : L"/dat/gn_dat3.arc");

    try
    {
        archive.open(path);
    }
    catch(const ArcError& ex)
    {
        error(L"failed to open file: " + path + L"\r\n" + utf_widen(ex.what()));
        return false;
    }

    if(!read_puppets(archive))
        return false;

    if(!parse_items(archive))
        return false;

    if(!parse_skill_names(archive))
        return false;

    if(!parse_ability_names(archive))
        return false;

    if(!randomize_skills(archive))
        return false;

    SendMessageW(progress_bar_, PBM_SETPOS, 25, 0);

    if(!randomize_puppets(archive))
        return false;

    if(!randomize_compatibility(archive))
        return false;

    SendMessageW(progress_bar_, PBM_SETPOS, 50, 0);

    if(is_ynk_)
    {
        if(!archive.save(path))
        {
            error(std::wstring(L"Could not write to file: ") + path + L"\nPlease make sure you have write permission to the game folder.");
            return false;
        }

        path = dir + L"/dat/gn_dat5.arc";

        try
        {
            archive.open(path);
        }
        catch(const ArcError& ex)
        {
            error(L"failed to open file: " + path + L"\r\n" + utf_widen(ex.what()));
            return false;
        }
    }

    if(!parse_puppet_names(archive))
        return false;

    if(!randomize_trainers(archive, rand_data))
        return false;

    SendMessageW(progress_bar_, PBM_SETPOS, 75, 0);

    if(!randomize_wild_puppets(archive))
        return false;

    if(!archive.save(path))
    {
        error(std::wstring(L"Could not write to file: ") + path + L"\nPlease make sure you have write permission to the game folder.");
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
    MessageBoxW(hwnd_, msg.c_str(), L"Error", MB_OK | MB_ICONERROR);
}

unsigned int Randomizer::level_from_exp(const PuppetData& data, unsigned int exp) const
{
    int ret = 1;
    while(exp_for_level(data, ret + 1) <= exp)
        ++ret;

    if(ret > 100)
        ret = 100;
    return ret;
}

unsigned int Randomizer::exp_for_level(const PuppetData& data, unsigned int level) const
{
    if(level <= 1)
        return 0;

    const int *mods = cost_exp_modifiers;
    if(is_ynk_)
        mods = cost_exp_modifiers_ynk;

    unsigned int ret = level * level * level * mods[data.cost] / 100;

    return ret;
}
