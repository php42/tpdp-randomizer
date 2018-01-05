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
#define NOMINMAX
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
#include <cwchar>
#include <type_traits>
#include <sstream>

#define MW_STYLE (WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
#define CB_STYLE (WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX)
#define GRP_STYLE (WS_CHILD | WS_VISIBLE | BS_GROUPBOX | WS_GROUP)
#define CB3_STYLE (WS_CHILD | WS_VISIBLE | BS_AUTO3STATE)

#define WND_WIDTH 512
#define WND_HEIGHT 758

#define IS_CHECKED(chkbox) (SendMessageW(chkbox, BM_GETCHECK, 0, 0) == BST_CHECKED)
#define SET_CHECKED(chkbox, checked) SendMessageW(chkbox, BM_SETCHECK, (checked) ? BST_CHECKED : BST_UNCHECKED, 0)
#define GET_3STATE(chkbox) ((unsigned int)SendMessageW(chkbox, BM_GETCHECK, 0, 0))
#define SET_3STATE(chkbox, state) SendMessageW(chkbox, BM_SETCHECK, (state), 0)

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
    ID_ENCOUNTERS,
    //ID_ENCOUNTER_RATE,
    ID_USE_QUOTA,
    ID_SKILLCARDS,
    ID_TRUE_RAND_STATS,
    ID_PREFER_SAME,
    ID_HEALTHY,
    ID_TRUE_RAND_SKILLS,
    ID_STARTING_MOVE,
    ID_SHARE_GEN,
    ID_SHARE_LOAD,
    ID_STAT_SCALING,
    ID_STRICT_TRAINERS,
    ID_COST
};

static const int g_cost_exp_modifiers[] = {70, 85, 100, 115, 130};
static const int g_cost_exp_modifiers_ynk[] = {85, 92, 100, 107, 115};
static const unsigned int g_sign_skills[] = {127, 179, 233, 273, 327, 375, 421, 474, 524, 566, 623, 680, 741, 782, 817};

inline bool is_sign_skill(unsigned int id)
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

/*
template<typename T>
void subtract_set(std::set<T>& s1, const std::set<T>& s2)
{
    for(auto i : s2)
        s1.erase(i);
}
*/

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

static void set_window_text(HWND hwnd, const wchar_t *str)
{
    SetWindowTextW(hwnd, str);
}

static unsigned long stoul_nothrow(const std::wstring& str, int base = 10)
{
    try
    {
        return std::stoul(str, NULL, base);
    }
    catch(const std::exception&)
    {
        return 0;
    }
}

static long stol_nothrow(const std::wstring& str, int base = 10)
{
    try
    {
        return std::stol(str, NULL, base);
    }
    catch(const std::exception&)
    {
        return 0;
    }
}

static int get_window_int(HWND hwnd, int base = 10)
{
    return stol_nothrow(get_window_text(hwnd), base);
}

static unsigned int get_window_uint(HWND hwnd, int base = 10)
{
    return stoul_nothrow(get_window_text(hwnd), base);
}

/* this method encodes BITS
 * encoding signed integers is inefficient and unreliable 
 * signed integers must be decoded as a data type of the same width
 * as it was encoded with (no sign extension is performed by the decoder)
 * 
 * this is intended to serialize integers into a short text form,
 * not to transport arbitrary binary data. */
template<typename T>
std::wstring base64_encode(const T& val)
{
    const wchar_t *chars = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-=";
    std::wstring ret;
    std::make_unsigned_t<T> bits = val;

    do
    {
        ret.insert(ret.begin(), chars[bits & 63]);
    } while(bits >>= 6);

    return ret;
}

template<typename T = unsigned int>
T base64_decode(const std::wstring& str)
{
    typedef std::make_unsigned_t<T> value_type;
    const std::wstring chars(L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-=");
    value_type ret = 0;
    const auto max_bits = (sizeof(value_type) * 8U);

    if(str.empty())
        throw std::invalid_argument("Invalid Base64 sequence");

    /* we don't care about performance, just use the easiest implementation */
    unsigned int pos = 0;
    for(auto it = str.crbegin(); it != str.crend(); ++it)
    {
        auto val = chars.find(*it);
        if(val == std::wstring::npos)
            throw std::invalid_argument("Invalid Base64 sequence");

        auto bits = (6U * (pos + 1U));

        /* throw if the encoded value is too large for our data type */
        if(bits > max_bits)
        {
            auto diff = (bits - max_bits);

            /* throw if we have too many digits */
            if(diff >= 6)
                throw std::out_of_range("Encoded Base64 value too large for target data type");

            /* since each digit encodes 6 bits, there will be a case where an extra digit is required
             * to represent less than 6 bits of the most significant byte.
             * throw if any of the extra bits are set (i.e. conversion would truncate). */
            if(val >> (6 - diff))
                throw std::out_of_range("Encoded Base64 value too large for target data type");
        }

        ret |= ((value_type)val << (6 * pos++));
    }

    return ret;
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
                    set_window_text(rnd->wnd_dir_, buf);
                }
                break;
            case ID_RANDOMIZE:
                if(MessageBoxW(hwnd, L"This will permanently modify game files.\r\nIf you have not backed up your game folder, you may wish to do so now.\r\n"
                                     "Also note that randomization is cumulative. Re-randomizing the same game data may have unexpected results.",
                                     L"Notice", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
                    break;

                try
                {
					if(rnd->randomize())
					{
						rnd->set_progress_bar(100);
						MessageBoxW(hwnd, L"Complete!", L"Success", MB_OK);
					}
					else
						MessageBoxW(hwnd, L"An error occurred, randomization aborted", L"Error", MB_OK);
                }
                catch(const std::exception& ex)
                {
					rnd->error(utf_widen(ex.what()));
                }

                rnd->set_progress_bar(0);
                rnd->clear();
                break;
            case ID_STATS:
                if(!IS_CHECKED(rnd->cb_stats_))
                {
                    SET_CHECKED(rnd->cb_true_rand_stats_, false);
                    SET_CHECKED(rnd->cb_use_quota_, false);
                    EnableWindow(rnd->wnd_quota_, false);
                    EnableWindow(rnd->wnd_stat_ratio_, false);
                    SET_CHECKED(rnd->cb_proportional_stats_, false);
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
                        SET_CHECKED(rnd->cb_proportional_stats_, false);
                        EnableWindow(rnd->wnd_stat_ratio_, false);
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
                        EnableWindow(rnd->wnd_stat_ratio_, false);
                        SET_CHECKED(rnd->cb_use_quota_, false);
                        SET_CHECKED(rnd->cb_proportional_stats_, false);
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
                    SET_CHECKED(rnd->cb_starting_move_, false);
                }
                break;
            case ID_TRUE_RAND_SKILLS:
                if(IS_CHECKED(rnd->cb_true_rand_skills_))
                {
                    SET_CHECKED(rnd->cb_skills_, true);
                }
                break;
            case ID_STARTING_MOVE:
                if(GET_3STATE(rnd->cb_starting_move_))
                {
                    SET_CHECKED(rnd->cb_skills_, true);
                }
                break;
            case ID_SHARE_GEN:
                rnd->generate_share_code();
                break;
            case ID_SHARE_LOAD:
                rnd->load_share_code();
                break;
            case ID_STAT_SCALING:
                {
                    bool checked = IS_CHECKED(rnd->cb_proportional_stats_);
                    EnableWindow(rnd->wnd_stat_ratio_, checked);
                    if(checked)
                    {
                        SET_CHECKED(rnd->cb_stats_, true);
                        EnableWindow(rnd->wnd_quota_, false);
                        SET_CHECKED(rnd->cb_use_quota_, false);
                        SET_CHECKED(rnd->cb_true_rand_stats_, false);
                    }
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
    SendMessageW(progress_bar_, PBM_SETPOS, percent, 0);
}

void Randomizer::increment_progress_bar()
{
    SendMessageW(progress_bar_, PBM_STEPIT, 0, 0);
}

void Randomizer::get_child_rect(HWND hwnd, RECT& rect)
{
    POINT tl, br;
    GetWindowRect(hwnd, &rect);

    tl.x = rect.left;
    tl.y = rect.top;
    br.x = rect.right;
    br.y = rect.bottom;

    ScreenToClient(hwnd_, &tl);
    ScreenToClient(hwnd_, &br);

    rect.left = tl.x;
    rect.top = tl.y;
    rect.right = br.x;
    rect.bottom = br.y;
}

int Randomizer::get_child_x(HWND hwnd)
{
    RECT rect;
    get_child_rect(hwnd, rect);
    return rect.left;
}

int Randomizer::get_child_y(HWND hwnd)
{
    RECT rect;
    get_child_rect(hwnd, rect);
    return rect.top;
}

int Randomizer::get_child_bottom(HWND hwnd)
{
    RECT rect;
    get_child_rect(hwnd, rect);
    return rect.bottom;
}

int Randomizer::get_child_right(HWND hwnd)
{
    RECT rect;
    get_child_rect(hwnd, rect);
    return rect.right;
}

/* detect valid puppet/skill/etc IDs from the game data.
 * this eliminates a dependecy on prebuilt tables and should work
 * for any version of the game. */
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
            std::vector<unsigned int> skill_deck;
            if(rand_true_rand_skills_)
                skill_deck.assign(valid_skills_.begin(), valid_skills_.end());
            else
                skill_deck.assign(valid_base_skills.begin(), valid_base_skills.end());
            std::shuffle(skill_deck.begin(), skill_deck.end(), gen_);

            /* moves shared by all styles of a particular puppet */
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
                if((style.style_type == STYLE_NORMAL) && !skill_deck.empty() && rand_starting_move_)
                {
                    style.style_skills[0] = 56; /* default to yin energy if we don't find a match below */
                    for(auto it = skill_deck.begin(); it != skill_deck.end(); ++it)
                    {
                        auto e = skills_[*it].element;
                        if((skills_[*it].type != SKILL_TYPE_STATUS) && (skills_[*it].power > 0) && ((rand_starting_move_ != BST_CHECKED) || (e == style.element1) || (e == style.element2)))
                        {
                            style.style_skills[0] = *it;
                            style.skillset.insert(*it);
                            skill_deck.erase(it);
                            break;
                        }
                    }
                }

                for(int j = (((style.style_type == STYLE_NORMAL) && rand_starting_move_) ? 1 : 0); j < 11; ++j)
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

            /* randomize abilities */
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
    std::vector<unsigned int> item_ids(held_item_ids_.begin(), held_item_ids_.end());
    std::uniform_int_distribution<int> iv(0, 0xf);
    std::uniform_int_distribution<int> ev(0, 64);
    std::uniform_int_distribution<int> pick_ev(0, 5);
    std::uniform_int_distribution<int> id(0, valid_puppet_ids_.size() - 1);
    std::bernoulli_distribution item_chance(trainer_item_chance_ / 100.0);
    std::bernoulli_distribution coin_flip(0.5);
    std::bernoulli_distribution skillcard_chance(trainer_sc_chance_ / 100.0);

    std::shuffle(item_ids.begin(), item_ids.end(), gen_);

    unsigned int max_lvl = 0;
    double lvl_mul = double(level_mod_) / 100.0;
    for(char *pos = buf; pos < endbuf; pos += PUPPET_SIZE_BOX)
    {
        decrypt_puppet(pos, rand_data, PUPPET_SIZE);

        Puppet puppet(pos, false);

        /* if we've changed puppet costs, trainer puppets will have exp based on a different cost value.
         * use the old cost value to determine the correct level. */
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

        if(((puppet.puppet_id == 0) && rand_full_party_) || ((puppet.puppet_id != 0) && rand_trainers_))
        {
            PuppetData& data(puppets_[valid_puppet_ids_[id(gen_)]]);
            puppet.puppet_id = data.id;

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

            std::vector<unsigned int> skill_deck(skill_set.begin(), skill_set.end());
            std::vector<unsigned int> skillcard_deck(skillcards.begin(), skillcards.end());

            std::shuffle(skill_deck.begin(), skill_deck.end(), gen_);
            if(!rand_trainer_sc_shuffle_)
                std::shuffle(skillcard_deck.begin(), skillcard_deck.end(), gen_);

            bool has_sign_skill = false;
            for(auto& i : puppet.skills)
            {
                if(!rand_trainer_sc_shuffle_ && skillcard_chance(gen_))
                {
                    if(!skillcard_deck.empty())
                    {
                        i = skillcard_deck.back();
                        skillcard_deck.pop_back();
                    }
                    else
                    {
                        i = 0;
                    }
                }
                else
                {
                    if(!skill_deck.empty())
                    {
                        i = skill_deck.back();
                        skill_deck.pop_back();
                    }
                    else
                    {
                        i = 0;
                    }
                }
                
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

            if(coin_flip(gen_))
            {
                puppet.ability_index ^= 1;
                if(data.styles[puppet.style_index].abilities[puppet.ability_index] == 0)
                    puppet.ability_index = 0;
            }

            if(!item_ids.empty() && item_chance(gen_))
            {
                puppet.held_item_id = item_ids.back();
                item_ids.pop_back();
            }
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
		if(rand_encounters_ == BST_CHECKED)
		{
			/* since there's no way to tell what areas have what type of grass,
			 * we won't add any puppets to any grass type if we don't find some there already */
			unsigned int num_encounters = encounters.size() ? gen_normal(gen_) : 0;
			unsigned int num_special = special_encounters.size() ? gen_special(gen_) : 0;
			unsigned int max_level = 0;
			unsigned int max_special_level = 0;

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
			}
			for(auto& i : special_encounters)
			{
				i.level = max_special_level;
				i.weight = gen_weight(gen_);
			}
		}

		/* randomize puppets and styles */
		for(auto& i : encounters)
		{
			if(puppet_id_pool_.empty())
			{
				puppet_id_pool_ = valid_puppet_ids_;
				std::shuffle(puppet_id_pool_.begin(), puppet_id_pool_.end(), gen_);
			}

			i.id = puppet_id_pool_.back();
			puppet_id_pool_.pop_back();

			if(i.level >= 32)
				i.style = std::uniform_int_distribution<int>(0, puppets_[i.id].max_style_index())(gen_);
			else
				i.style = 0;
		}
		for(auto& i : special_encounters)
		{
			if(puppet_id_pool_.empty())
			{
				puppet_id_pool_ = valid_puppet_ids_;
				std::shuffle(puppet_id_pool_.begin(), puppet_id_pool_.end(), gen_);
			}

			i.id = puppet_id_pool_.back();
			puppet_id_pool_.pop_back();

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
    set_window_text(wnd_seed_, std::to_wstring(seed).c_str());
}

/* serialize the randomization settings into a text string */
void Randomizer::generate_share_code()
{
    unsigned int bitfield = 0;

    assert((sizeof(bitfield) * 8) >= (checkboxes_.size() + (checkboxes_3state_.size() * 2)));

    for(std::size_t i = 0; i < checkboxes_.size(); ++i)
    {
        if(IS_CHECKED(checkboxes_[i]))
            bitfield |= (1U << i);
    }

    for(std::size_t i = 0; i < checkboxes_3state_.size(); ++i)
        bitfield |= (GET_3STATE(checkboxes_3state_[i]) << (checkboxes_.size() + (i * 2)));

    if(!validate())
        return;

    unsigned int seed = get_window_uint(wnd_seed_);
    unsigned int lvl_mod = get_window_uint(wnd_lvladjust_);
    unsigned int quota = get_window_uint(wnd_quota_);
    unsigned int sc_chance = get_window_uint(wnd_sc_chance_);
    unsigned int item_chance = get_window_uint(wnd_item_chance_);
    unsigned int stat_variance = get_window_uint(wnd_stat_ratio_);

    if(item_chance > 100)
        item_chance = 100;

    if(sc_chance > 100)
        sc_chance = 100;

    bool sc_shuffle = get_window_text(wnd_sc_chance_).empty();

    if(!sc_shuffle)
    {
        if(!validate_uint_window(wnd_sc_chance_, L"Skillcard Chance"))
            return;
    }

    std::wstring code = base64_encode(VERSION_INT) + L':' +
                        base64_encode(bitfield) + L':' +
                        base64_encode(seed) + L':' +
                        base64_encode(lvl_mod) + L':' +
                        base64_encode(quota) + L':' +
                        (sc_shuffle ? L"" : base64_encode(sc_chance)) + L':' +
                        base64_encode(item_chance) + L':' +
                        base64_encode(stat_variance);

    set_window_text(wnd_share_, code.c_str());
}

bool Randomizer::load_share_code()
{
    try
    {
        auto code = get_window_text(wnd_share_);
        std::vector<std::wstring> code_segs;

        /* strip all whitespace */
        for(auto it = code.begin(); it != code.end();)
        {
            if(iswspace(*it))
                it = code.erase(it);
            else
                ++it;
        }

        auto pos = code.find(L':');
        if(pos == std::wstring::npos)
            throw std::exception();

        auto ver = base64_decode(code.substr(0, pos));
        if((ver >= MAKE_VERSION(1, 0, 8)) && (ver != VERSION_INT))
        {
            std::wstring version_string;
            version_string += std::to_wstring((uint8_t)(ver >> 16));
            version_string += L'.';
            version_string += std::to_wstring((uint8_t)(ver >> 8));
            version_string += L'.';
            version_string += std::to_wstring((uint8_t)ver);

            error(L"This code appears to be for a different version of the randomizer, or may not be a share code at all.\r\n"
                   "The encoded version number appears to be: \"" + version_string + L"\"\r\n"
                   "If you really want to use it, check for a matching version of the randomizer from https://github.com/php42/tpdp-randomizer/releases");
            return false;
        }
        else if(ver != VERSION_INT)
            throw std::exception();

        ++pos;
        for(;;)
        {
            auto endpos = code.find(L':', pos);

            code_segs.push_back(code.substr(pos, endpos - pos));

            if(endpos >= code.size())
                break;
            pos = endpos + 1;
        }

        bool sc_shuffle = code_segs.at(4).empty();

        auto bitfield = base64_decode(code_segs.at(0));
        auto seed = base64_decode(code_segs.at(1));
        auto lvl_mod = base64_decode(code_segs.at(2));
        auto quota = base64_decode(code_segs.at(3));
        auto sc_chance = (sc_shuffle ? 0u : base64_decode(code_segs.at(4)));
        auto item_chance = base64_decode(code_segs.at(5));
        auto stat_variance = base64_decode(code_segs.at(6));

        assert((sizeof(bitfield) * 8) >= (checkboxes_.size() + (checkboxes_3state_.size() * 2)));

        for(std::size_t i = 0; i < checkboxes_.size(); ++i)
        {
            SET_CHECKED(checkboxes_[i], (bitfield & (1U << i)));
        }

        for(std::size_t i = 0; i < checkboxes_3state_.size(); ++i)
        {
            auto state = ((bitfield >> (checkboxes_.size() + (i * 2))) & 3);
            if(state > 2)
                throw std::exception();

            SET_3STATE(checkboxes_3state_[i], state);
        }

        /* manually generate button click notifications so our wndproc gets called */
        for(auto i : checkboxes_)
            SendMessageW(hwnd_, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(i), BN_CLICKED), (LPARAM)i);
        for(auto i : checkboxes_3state_)
            SendMessageW(hwnd_, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(i), BN_CLICKED), (LPARAM)i);

        set_window_text(wnd_seed_, std::to_wstring(seed).c_str());
        set_window_text(wnd_lvladjust_, std::to_wstring(lvl_mod).c_str());
        set_window_text(wnd_quota_, std::to_wstring(quota).c_str());
        set_window_text(wnd_item_chance_, std::to_wstring(item_chance).c_str());
        set_window_text(wnd_stat_ratio_, std::to_wstring(stat_variance).c_str());

        if(code_segs.at(4).empty())
            set_window_text(wnd_sc_chance_, L"");
        else
            set_window_text(wnd_sc_chance_, std::to_wstring(sc_chance).c_str());

        return true;
    }
    catch(const std::exception&)
    {
        error(L"Invalid share code.");
        return false;
    }
}

bool Randomizer::validate()
{
    if(!validate_uint_window(wnd_seed_, L"Seed") ||
       !validate_uint_window(wnd_lvladjust_, L"Level adjustment") ||
       !validate_uint_window(wnd_quota_, L"Quota") ||
       !validate_uint_window(wnd_item_chance_, L"Item Chance") ||
       !validate_uint_window(wnd_stat_ratio_, L"Stat ratio") ||
       (!get_window_text(wnd_sc_chance_).empty() && !validate_uint_window(wnd_sc_chance_, L"Skillcard chance")))
    {
        return false;
    }

    if(get_window_uint(wnd_lvladjust_) <= 0)
    {
        error(L"Invalid trainer level modifier. Must be an integer greater than zero");
        return false;
    }

    auto stat_quota = get_window_uint(wnd_quota_);
    if(stat_quota <= 0)
    {
        error(L"Stat quota must be an integer greater than zero.");
        return false;
    }
    else if(stat_quota > (0xFF * 6))
    {
        error(L"Stat quota too large! Maximum quota is 1530.");
        return false;
    }

    return true;
}

bool Randomizer::validate_uint_window(HWND hwnd, const std::wstring& name)
{
    auto str = get_window_text(hwnd);
    for(auto it : str)
    {
        if(!iswdigit(it))
        {
            error(name + L" must contain only digits");
            return false;
        }
    }

    try
    {
        /* volatile so the compiler doesn't optimize the entire call away (unlikely but possible?) */
        volatile unsigned long unused = std::stoul(str);
    }
    catch(const std::invalid_argument&)
    {
        error(L"Invalid " + name + L" value.");
        return false;
    }
    catch(const std::out_of_range&)
    {
        error(name + L" value too large!\r\nValues must be less than 2^32");
        return false;
    }

    return true;
}

/* raw winapi so we don't have any dependencies */
/* FIXME: there's probably a way to do all this with resource files or something
 * I'm not exerienced enough with Windows GUI stuff to know how this is normally done */
Randomizer::Randomizer(HINSTANCE hInstance)
{
    hInstance_ = hInstance;
    RECT rect, pos;

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rect, 0);

    hwnd_ = CreateWindowW(L"MainWindow", L"TPDP Randomizer "  VERSION_STRING, MW_STYLE, (rect.right - WND_WIDTH) / 2, (rect.bottom - WND_HEIGHT) / 2, WND_WIDTH, WND_HEIGHT, NULL, NULL, hInstance, NULL);
    if(hwnd_ == NULL)
        abort();

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);

    GetClientRect(hwnd_, &rect);

    /* [spaghetti code intensifies] */
    /* FIXME: replace with wxWidgets or something, not worth doing it this way anymore */

    grp_dir_ = CreateWindowW(L"Button", L"Game Folder", GRP_STYLE, 10, 10, rect.right - 20, 55, hwnd_, NULL, hInstance, NULL);
    grp_rand_ = CreateWindowW(L"Button", L"Randomization", GRP_STYLE, 10, get_child_bottom(grp_dir_) + 10, rect.right - 20, 245, hwnd_, NULL, hInstance, NULL);
    grp_other_ = CreateWindowW(L"Button", L"Other", GRP_STYLE, 10, get_child_bottom(grp_rand_) + 10, rect.right - 20, 188, hwnd_, NULL, hInstance, NULL);
    grp_seed_ = CreateWindowW(L"Button", L"Seed", GRP_STYLE, 10, get_child_bottom(grp_other_) + 10, rect.right - 20, 55, hwnd_, NULL, hInstance, NULL);
    grp_share_ = CreateWindowW(L"Button", L"Share Code", GRP_STYLE, 10, get_child_bottom(grp_seed_) + 10, rect.right - 20, 55, hwnd_, NULL, hInstance, NULL);
    bn_Randomize_ = CreateWindowW(L"Button", L"Randomize", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, (rect.right / 2) - 50, get_child_bottom(grp_share_) + 10, 100, 30, hwnd_, (HMENU)ID_RANDOMIZE, hInstance, NULL);
    progress_bar_ = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE, 10, rect.bottom - 30, rect.right - 20, 20, hwnd_, NULL, hInstance, NULL);
    SendMessageW(progress_bar_, PBM_SETSTEP, 1, 0);

    GetClientRect(grp_dir_, &rect);
    get_child_rect(grp_dir_, pos);
    wnd_dir_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"C:\\game\\FocasLens\\幻想人形演舞", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, pos.left + 10, pos.top + 20, rect.right - 90, 25, hwnd_, NULL, hInstance, NULL);
    bn_browse_ = CreateWindowW(L"Button", L"Browse", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, pos.right - 75, pos.top + 18, 65, 29, hwnd_, (HMENU)ID_BROWSE, hInstance, NULL);

    GetClientRect(grp_other_, &rect);
    get_child_rect(grp_other_, pos);
    wnd_lvladjust_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"100", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, pos.left + 10, pos.top + 20, 50, 23, hwnd_, NULL, hInstance, NULL);
    tx_lvladjust_ = CreateWindowW(L"Static", L"% Enemy level adjustment", WS_CHILD | WS_VISIBLE | SS_WORDELLIPSIS, get_child_right(wnd_lvladjust_) + 5, pos.top + 22, (rect.right / 2) - 75, 23, hwnd_, NULL, hInstance, NULL);
    cb_trainer_party_ = CreateWindowW(L"Button", L"Full trainer party", CB_STYLE, (rect.right / 2) + pos.left + 10, pos.top + 23, (rect.right / 2) - 25, 15, hwnd_, NULL, hInstance, NULL);
    cb_export_locations_ = CreateWindowW(L"Button", L"Export catch locations", CB_STYLE, pos.left + 10, pos.top + 55, (rect.right / 2) - 25, 15, hwnd_, NULL, hInstance, NULL);
    wnd_quota_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"500", WS_CHILD | WS_VISIBLE | ES_NUMBER | WS_DISABLED | ES_AUTOHSCROLL, (rect.right / 2) + pos.left + 10, pos.top + 53, 50, 23, hwnd_, NULL, hInstance, NULL);
    tx_quota_ = CreateWindowW(L"Static", L"Stat quota", WS_CHILD | WS_VISIBLE | SS_WORDELLIPSIS, (rect.right / 2) + pos.left + 65, pos.top + 55, (rect.right / 2) - 75, 23, hwnd_, NULL, hInstance, NULL);
    cb_healthy_ = CreateWindowW(L"Button", L"Healthy", CB_STYLE, pos.left + 10, pos.top + 87, (rect.right / 2) - 25, 15, hwnd_, (HMENU)ID_HEALTHY, hInstance, NULL);
    cb_export_puppets_ = CreateWindowW(L"Button", L"Dump puppet stats", CB_STYLE, (rect.right / 2) + pos.left + 10, pos.top + 87, (rect.right / 2) - 25, 15, hwnd_, NULL, hInstance, NULL);
    cb_strict_trainers_ = CreateWindowW(L"Button", L"Strict trainers", CB_STYLE, pos.left + 10, pos.top + 120, (rect.right / 2) - 25, 15, hwnd_, (HMENU)ID_STRICT_TRAINERS, hInstance, NULL);
    wnd_sc_chance_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, (rect.right / 2) + pos.left + 10, pos.top + 119, 50, 23, hwnd_, NULL, hInstance, NULL);
    tx_sc_chance_ = CreateWindowW(L"Static", L"% Trainer skillcard chance", WS_CHILD | WS_VISIBLE | SS_WORDELLIPSIS, (rect.right / 2) + pos.left + 65, pos.top + 121, (rect.right / 2) - 75, 23, hwnd_, NULL, hInstance, NULL);
    wnd_item_chance_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"25", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, pos.left + 10, pos.top + 152, 50, 23, hwnd_, NULL, hInstance, NULL);
    tx_item_chance_ = CreateWindowW(L"Static", L"% Trainer item chance", WS_CHILD | WS_VISIBLE | SS_WORDELLIPSIS, pos.left + 65, pos.top + 154, (rect.right / 2) - 75, 23, hwnd_, NULL, hInstance, NULL);
    wnd_stat_ratio_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"25", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL | WS_DISABLED, (rect.right / 2) + pos.left + 10, pos.top + 152, 50, 23, hwnd_, NULL, hInstance, NULL);
    tx_stat_ratio_ = CreateWindowW(L"Static", L"% Stat ratio", WS_CHILD | WS_VISIBLE | SS_WORDELLIPSIS, (rect.right / 2) + pos.left + 65, pos.top + 154, (rect.right / 2) - 75, 23, hwnd_, NULL, hInstance, NULL);

    GetClientRect(grp_seed_, &rect);
    get_child_rect(grp_seed_, pos);
    wnd_seed_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"0", WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, pos.left + 10, pos.top + 20, rect.right - 125, 25, hwnd_, NULL, hInstance, NULL);
    bn_generate_ = CreateWindowW(L"Button", L"Generate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, pos.left + (rect.right - 110), pos.top + 18, 100, 29, hwnd_, (HMENU)ID_GENERATE, hInstance, NULL);
    generate_seed();

    GetClientRect(grp_share_, &rect);
    get_child_rect(grp_share_, pos);
    wnd_share_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, pos.left + 10, pos.top + 20, rect.right - 195, 25, hwnd_, NULL, hInstance, NULL);
    bn_share_gen_ = CreateWindowW(L"Button", L"Generate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, pos.left + (rect.right - 175), pos.top + 18, 80, 29, hwnd_, (HMENU)ID_SHARE_GEN, hInstance, NULL);
    bn_share_load_ = CreateWindowW(L"Button", L"Load", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, pos.left + (rect.right - 90), pos.top + 18, 80, 29, hwnd_, (HMENU)ID_SHARE_LOAD, hInstance, NULL);

    GetClientRect(grp_rand_, &rect);
    get_child_rect(grp_rand_, pos);
    int x = pos.left + 15;
    int y = pos.top + 30;
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
    cb_encounters_ = CreateWindowW(L"Button", L"Wild Puppets", CB3_STYLE, x, y + 120, width_cb, 15, hwnd_, (HMENU)ID_ENCOUNTERS, hInstance, NULL);
    //cb_encounter_rates_ = CreateWindowW(L"Button", L"Encounter Rates", CB3_STYLE, x + (width / 3), y + 120, width_cb, 15, hwnd_, (HMENU)ID_ENCOUNTER_RATE, hInstance, NULL);
	cb_cost_ = CreateWindowW(L"Button", L"Puppet Cost", CB3_STYLE, x + (width / 3), y + 120, width_cb, 15, hwnd_, (HMENU)ID_COST, hInstance, NULL);
    cb_use_quota_ = CreateWindowW(L"Button", L"Use stat quota", CB_STYLE, x + ((width / 3) * 2), y + 120, width_cb, 15, hwnd_, (HMENU)ID_USE_QUOTA, hInstance, NULL);
    cb_skillcards_ = CreateWindowW(L"Button", L"Skill Cards", CB3_STYLE, x, y + 150, width_cb, 15, hwnd_, (HMENU)ID_SKILLCARDS, hInstance, NULL);
    cb_true_rand_stats_ = CreateWindowW(L"Button", L"True random stats", CB_STYLE, x + (width / 3), y + 150, width_cb, 15, hwnd_, (HMENU)ID_TRUE_RAND_STATS, hInstance, NULL);
    cb_prefer_same_type_ = CreateWindowW(L"Button", L"Prefer STAB moves", CB_STYLE, x + ((width / 3) * 2), y + 150, width_cb, 15, hwnd_, (HMENU)ID_PREFER_SAME, hInstance, NULL);
    cb_true_rand_skills_ = CreateWindowW(L"Button", L"True random skills", CB_STYLE, x, y + 180, width_cb, 15, hwnd_, (HMENU)ID_TRUE_RAND_SKILLS, hInstance, NULL);
    cb_starting_move_ = CreateWindowW(L"Button", L"STAB starting move", CB3_STYLE, x + (width / 3), y + 180, width_cb, 15, hwnd_, (HMENU)ID_STARTING_MOVE, hInstance, NULL);
    cb_proportional_stats_ = CreateWindowW(L"Button", L"Proportional stats", CB_STYLE, x + ((width / 3) * 2), y + 180, width_cb, 15, hwnd_, (HMENU)ID_STAT_SCALING, hInstance, NULL);

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
    set_tooltip(cb_encounters_, L"This is a 3-state checkbox. Click twice to get to the \"middle\" state.\r\nChecked: Full randomization.\r\nMiddle: Partial randomization. Encounter rates and number of puppets will not be changed.");
    //set_tooltip(cb_encounter_rates_, L"This is a 3-state checkbox. Click twice to get to the \"middle\" state.\nChecked: Randomize encounter rates and number of wild puppets in an area.\r\nMiddle: Randomize encounter rates only.");
    set_tooltip(cb_export_locations_, L"Export the locations where each puppet can be caught in the wild.\r\nThis will be written to catch_locations.txt in the game folder.");
    set_tooltip(cb_use_quota_, L"When stat randomization is enabled, each puppet will recieve the same total sum of stat points (distributed randomly)\r\nThe number of stat points can be adjusted in the \"Stat quota\" field below");
    set_tooltip(wnd_quota_, L"When \"Use stat quota\" is enabled, this number dictates the total sum of stat points each puppet recieves");
    set_tooltip(cb_healthy_, L"Remove \"Frail Health\" from the ability pool");
    set_tooltip(cb_skillcards_, L"This is a 3-state checkbox. Click twice to get to the \"middle\" state.\nChecked: Skill Cards will teach random skills\r\nMiddle: Don't randomize sign skills");
    set_tooltip(cb_true_rand_stats_, L"Puppet base stats will be completely random");
    set_tooltip(cb_prefer_same_type_, L"Randomization will favor skills that match a puppets typing");
    set_tooltip(cb_export_puppets_, L"Write puppet stats/skillsets/etc to puppets.txt in the game folder");
    set_tooltip(cb_true_rand_skills_, L"Puppet skills are totally random\r\nThe default behaviour preserves the \"move pools\" of puppets, meaning that normal puppets will generally have less powerful moves.\r\nThis option disables that.");
    set_tooltip(cb_starting_move_, L"This is a 3-state checkbox. Click twice to get to the \"middle\" state.\nChecked: All puppets are guaranteed a damaging same-type starting move (so long as such a move exists in the pool)\r\nMiddle: Guaranteed damaging starting move of any type");
    set_tooltip(wnd_share_, L"Share codes allow you to easily share your randomization settings and seed with others. Click generate to create a code for your current settings, or paste in a code and click load to apply those settings.");
    set_tooltip(bn_share_gen_, L"Generate a share code for your current settings and seed");
    set_tooltip(bn_share_load_, L"Load settings and seed from the supplied share code");
    set_tooltip(cb_proportional_stats_, L"Generate stats proportional to the puppets original stats.\nAdjust with the \"Stat ratio\" option below.");
    set_tooltip(cb_strict_trainers_, L"Trainer puppets will not have skills for which they are too low level.\nWhen unchecked, level requirement is ignored and trainer puppets may have any move in their moveset at any level.\nHas no effect on skill card moves.");
    set_tooltip(wnd_sc_chance_, L"Chance for trainer puppets to have a skill card move (per move slot).\nBlank = shuffle.");
    set_tooltip(wnd_item_chance_, L"Chance for trainer puppets to have a held item.");
    set_tooltip(wnd_stat_ratio_, L"Adjust maximum stat variance when using proportional stats.\nA value of 25 allows stats to change up to a maximum of +/- 25% of the original stat.");
    set_tooltip(cb_cost_, L"This is a 3-state checkbox. Click twice to get to the \"middle\" state.\nChecked: randomized puppet cost\nMiddle: all puppets set to 120 cost");

    NONCLIENTMETRICSW ncm = {0};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    hfont_ = CreateFontIndirectW(&ncm.lfMessageFont);

    EnumChildWindows(hwnd_, set_font, (LPARAM)hfont_);

    checkboxes_.push_back(cb_skills_);
    checkboxes_.push_back(cb_stats_);
    checkboxes_.push_back(cb_trainers_);
    checkboxes_.push_back(cb_types_);
    checkboxes_.push_back(cb_compat_);
    checkboxes_.push_back(cb_abilities_);
    checkboxes_.push_back(cb_trainer_party_);
    checkboxes_.push_back(cb_export_locations_);
    checkboxes_.push_back(cb_use_quota_);
    checkboxes_.push_back(cb_healthy_);
    checkboxes_.push_back(cb_true_rand_stats_);
    checkboxes_.push_back(cb_prefer_same_type_);
    checkboxes_.push_back(cb_export_puppets_);
    checkboxes_.push_back(cb_true_rand_skills_);
    checkboxes_.push_back(cb_skill_element_);
    checkboxes_.push_back(cb_skill_power_);
    checkboxes_.push_back(cb_skill_acc_);
    checkboxes_.push_back(cb_skill_sp_);
    checkboxes_.push_back(cb_skill_prio_);
    checkboxes_.push_back(cb_skill_type_);
    checkboxes_.push_back(cb_proportional_stats_);
    checkboxes_.push_back(cb_strict_trainers_);

    checkboxes_3state_.push_back(cb_cost_);
    checkboxes_3state_.push_back(cb_encounters_);
    checkboxes_3state_.push_back(cb_starting_move_);
	//checkboxes_3state_.push_back(cb_encounter_rates_);
	checkboxes_3state_.push_back(cb_skillcards_);
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

    if(!validate())
        return false;

    rand_trainer_sc_shuffle_ = get_window_text(wnd_sc_chance_).empty();
    stat_ratio_ = get_window_uint(wnd_stat_ratio_);
    level_mod_ = get_window_uint(wnd_lvladjust_);
    stat_quota_ = get_window_uint(wnd_quota_);

    trainer_sc_chance_ = get_window_uint(wnd_sc_chance_);
    if(trainer_sc_chance_ > 100)
        trainer_sc_chance_ = 100;

    trainer_item_chance_ = get_window_uint(wnd_item_chance_);
    if(trainer_item_chance_ > 100)
        trainer_item_chance_ = 100;

    gen_.seed(get_window_uint(wnd_seed_));

    rand_skillsets_ = IS_CHECKED(cb_skills_);
    rand_stats_ = IS_CHECKED(cb_stats_);
    rand_trainers_ = IS_CHECKED(cb_trainers_);
    rand_types_ = IS_CHECKED(cb_types_);
    rand_compat_ = IS_CHECKED(cb_compat_);
    rand_abilities_ = IS_CHECKED(cb_abilities_);
    rand_full_party_ = IS_CHECKED(cb_trainer_party_);
    rand_encounters_ = GET_3STATE(cb_encounters_);
    //rand_encounter_rates_ = GET_3STATE(cb_encounter_rates_);
    rand_export_locations_ = IS_CHECKED(cb_export_locations_);
    rand_quota_ = IS_CHECKED(cb_use_quota_);
    rand_healthy_ = IS_CHECKED(cb_healthy_);
    rand_skillcards_ = GET_3STATE(cb_skillcards_);
    rand_true_rand_stats_ = IS_CHECKED(cb_true_rand_stats_);
    rand_prefer_same_type_ = IS_CHECKED(cb_prefer_same_type_);
    rand_export_puppets_ = IS_CHECKED(cb_export_puppets_);
    rand_true_rand_skills_ = IS_CHECKED(cb_true_rand_skills_);
    rand_cost_ = GET_3STATE(cb_cost_);
    rand_puppets_ = rand_skillsets_ || rand_stats_ || rand_types_ || rand_abilities_ || rand_cost_;
    rand_skill_element_ = IS_CHECKED(cb_skill_element_);
    rand_skill_power_ = IS_CHECKED(cb_skill_power_);
    rand_skill_acc_ = IS_CHECKED(cb_skill_acc_);
    rand_skill_sp_ = IS_CHECKED(cb_skill_sp_);
    rand_skill_prio_ = IS_CHECKED(cb_skill_prio_);
    rand_skill_type_ = IS_CHECKED(cb_skill_type_);
    //rand_stab_ = IS_CHECKED(cb_stab_);
    //rand_dmg_starting_move_ = IS_CHECKED(cb_dmg_starting_move_);
    rand_starting_move_ = GET_3STATE(cb_starting_move_);
    rand_stat_scaling_ = IS_CHECKED(cb_proportional_stats_);
    rand_strict_trainers_ = IS_CHECKED(cb_strict_trainers_);
    rand_skills_ = rand_skill_element_ || rand_skill_power_ || rand_skill_acc_ || rand_skill_sp_ || rand_skill_prio_ || rand_skill_type_;

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

    set_progress_bar(25);

    if(!randomize_puppets(archive))
        return false;

    if(!randomize_compatibility(archive))
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
    MessageBoxW(hwnd_, msg.c_str(), L"Error", MB_OK | MB_ICONERROR);
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
