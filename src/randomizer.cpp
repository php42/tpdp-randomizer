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
#include <memory>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <CommCtrl.h>
#include <ShlObj.h>

#define MW_STYLE (WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
#define CB_STYLE (WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX)

#define WND_WIDTH 512
#define WND_HEIGHT 470

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
    ID_SKILL_TYPE
};

static Randomizer *g_wnd = NULL;

static const int cost_exp_modifiers[] = {70, 85, 100, 115, 130};
static const int cost_exp_modifiers_ynk[] = {85, 92, 100, 107, 115};

LRESULT CALLBACK Randomizer::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_COMMAND:
        if(HIWORD(wParam) == BN_CLICKED)
        {
            switch(LOWORD(wParam))
            {
            case ID_GENERATE:
                {
                    unsigned int seed = (unsigned int)std::chrono::system_clock::now().time_since_epoch().count();
                    SetWindowTextW(g_wnd->wnd_seed_, std::to_wstring(seed).c_str());
                }
                break;
            case ID_BROWSE:
                {
                    wchar_t buf[MAX_PATH] = {0};
                    BROWSEINFOW bi = {0};
                    bi.hwndOwner = g_wnd->hwnd_;
                    bi.pszDisplayName = buf;
                    bi.lpszTitle = L"Select Game Folder";
                    bi.ulFlags = BIF_USENEWUI;

                    PIDLIST_ABSOLUTE pl = SHBrowseForFolderW(&bi);
                    if(!pl)
                        break;

                    SHGetPathFromIDListW(pl, buf);
                    CoTaskMemFree(pl);
                    SetWindowTextW(g_wnd->wnd_dir_, buf);
                }
                break;
            case ID_RANDOMIZE:
                if(MessageBoxW(g_wnd->hwnd_, L"This will permanently modify game files.\r\nIf you have not backed up your game folder, you may wish to do so now.", L"Notice", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
                    break;
                if(g_wnd->randomize())
                {
                    SendMessageW(g_wnd->progress_bar_, PBM_SETPOS, 100, 0);
                    MessageBoxW(g_wnd->hwnd_, L"Complete!", L"Success", MB_OK);
                }
                else
                    MessageBoxW(g_wnd->hwnd_, L"An error occurred, randomization aborted", L"Error", MB_OK);
                SendMessageW(g_wnd->progress_bar_, PBM_SETPOS, 0, 0);
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
    ti.lpszText = msg;
    SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, 400);

    return tip;
}

void Randomizer::randomize_puppets(void *data, size_t len)
{
    char *buf = (char*)data;
    uint8_t valid_bits[16] = {0};
    std::set<unsigned int> valid_skills;
    std::set<unsigned int> valid_abilities;
    std::vector<unsigned int> stats;

    /* there's a lot of unimplemented stuff floating around the files,
     * so find all skills which are actually used by puppets */
    for(unsigned int i = 0; i < len; i += PUPPET_DATA_SIZE)
    {
        PuppetData puppet(&buf[i]);
        puppet.id = (i / PUPPET_DATA_SIZE);
        if(puppet.styles[0].style_type == 0) /* not a real puppet */
            continue;

        valid_puppet_ids_.push_back(puppet.id);

        for(auto i : puppet.base_skills)
            valid_skills.insert(i);

        for(auto& style : puppet.styles)
        {
            valid_skills.insert(style.lv100_skill);
            style.skillset.insert(style.lv100_skill);
            for(auto i : style.style_skills)
            {
                valid_skills.insert(i);
                style.skillset.insert(i);
            }

            for(auto i : style.lv70_skills)
            {
                valid_skills.insert(i);
                style.skillset.insert(i);
            }

            for(auto i : puppet.base_skills)
                style.skillset.insert(i);

            for(int i = 0; i < 16; ++i)
            {
                valid_bits[i] |= style.skill_compat_table[i];
            }

            for(auto i : style.abilities)
                valid_abilities.insert(i);

            for(auto i : style.base_stats)
                stats.push_back(i);
        }

        puppets_[puppet.id] = std::move(puppet);
    }

    if(!rand_puppets_)
        return;

    valid_skills.erase(0);
    valid_abilities.erase(0);

    /* create a "deck" of all valid skills which we will shuffle for each puppet and assign the first n skills from the deck */
    std::vector<unsigned int> skill_deck(valid_skills.begin(), valid_skills.end());
    std::vector<unsigned int> ability_deck(valid_abilities.begin(), valid_abilities.end());
    std::bernoulli_distribution compat_chance(0.5); /* 50% chance to invert skill card compatibility */
    std::bernoulli_distribution element2_chance(0.75); /* 75% chance to get a secondary type */
    std::uniform_int_distribution<int> element(1, is_ynk_ ? ELEMENT_WARPED : ELEMENT_SOUND);

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

        std::shuffle(skill_deck.begin(), skill_deck.end(), gen_);
        int index = 0;

        if(rand_skillsets_)
        {
            for(auto& i : puppet.base_skills)
            {
                if(i != 0)
                    i = skill_deck[index++];
            }
        }

        for(auto& style : puppet.styles)
        {
            int index = 0;

            if(rand_skillsets_)
            {
                style.skillset.clear();
                style.skillset = puppet.styles[0].skillset;
                std::shuffle(skill_deck.begin(), skill_deck.end(), gen_);

                if(style.lv100_skill != 0)
                {
                    style.lv100_skill = skill_deck[index++];
                    style.skillset.insert(style.lv100_skill);
                }

                for(auto& i : style.style_skills)
                {
                    if(i != 0)
                    {
                        i = skill_deck[index++];
                        style.skillset.insert(i);
                    }
                }

                for(auto& i : style.lv70_skills)
                {
                    if(i != 0)
                    {
                        i = skill_deck[index++];
                        style.skillset.insert(i);
                    }
                }

                for(auto& i : puppet.base_skills)
                {
                    if(i != 0)
                        style.skillset.insert(i);
                }

                for(int i = 0; i < 16; ++i)
                {
                    for(int j = 0; j < 8; ++j)
                    {
                        if(((1 << j) & valid_bits[i]) && compat_chance(gen_))
                        {
                            style.skill_compat_table[i] ^= (1 << j);
                        }
                    }
                }
            }

            if(rand_abilities_)
            {
                std::shuffle(ability_deck.begin(), ability_deck.end(), gen_);
                index = 0;
                for(auto& i : style.abilities)
                {
                    if(i != 0)
                        i = ability_deck[index++];
                }
            }

            if(rand_stats_)
            {
                std::shuffle(stats.begin(), stats.end(), gen_);
                for(int i = 0; i < 6; ++i)
                    style.base_stats[i] = stats[i];
            }

            if(rand_types_)
            {
                style.element1 = element(gen_);
                if(!element2_chance(gen_))
                    style.element2 = 0;
                else
                {
                    style.element2 = element(gen_);
                    if(style.element1 == style.element2)
                        style.element2 = 0;
                }
            }
        }

        puppet.write(&buf[puppet.id * PUPPET_DATA_SIZE]);
    }
}

void Randomizer::randomize_trainer(void *src, const void *rand_data)
{
    char *buf = (char*)src + 0x2C;
    char *endbuf = buf + (6 * PUPPET_SIZE_BOX);
    std::uniform_int_distribution<int> iv(0, 0xf);
    std::uniform_int_distribution<int> ev(0, 64);
    std::uniform_int_distribution<int> id(0, valid_puppet_ids_.size() - 1);
    std::uniform_int_distribution<int> style(0, 3);
    std::bernoulli_distribution ability(0.5);

    unsigned int max_lvl = 0;
    double lvl_mul = double(level_mod_) / 100.0;
    for(char *pos = buf; pos < endbuf; pos += PUPPET_SIZE_BOX)
    {
        decrypt_puppet(pos, rand_data, PUPPET_SIZE);

        Puppet puppet(pos, false);
        unsigned int lvl = level_from_exp(puppets_[puppet.puppet_id], puppet.exp);
        if(lvl > max_lvl)
            max_lvl = lvl;
        if(((puppet.puppet_id == 0) && rand_full_party_) || ((puppet.puppet_id != 0) && rand_trainers_))
        {
            PuppetData& data(puppets_[valid_puppet_ids_[id(gen_)]]);

            puppet.style_index = style(gen_);
            while(data.styles[puppet.style_index].style_type == 0)
            {
                if(puppet.style_index == 0)
                    break;
                --puppet.style_index;
            }

            std::vector<unsigned int> skills(data.styles[puppet.style_index].skillset.begin(), data.styles[puppet.style_index].skillset.end());
            std::shuffle(skills.begin(), skills.end(), gen_);

            puppet.puppet_id = data.id;

            int skill_index = 0;
            for(auto& i : puppet.skills)
                i = skills[skill_index++];

            for(auto& i : puppet.ivs)
                i = iv(gen_);

            int total = 0;
            for(auto& i : puppet.evs)
            {
                int j = ev(gen_);
                if((total + j) > 130)
                    j = 130 - total;
                i = j;
                total += j;
            }

            if(ability(gen_))
            {
                puppet.ability_index ^= 1;
                if(data.styles[puppet.style_index].abilities[puppet.ability_index] == 0)
                    puppet.ability_index = 0;
            }

            if(puppet.puppet_id < puppet_names_.size())
                puppet.set_puppet_nickname(puppet_names_[puppet.puppet_id]);

            if(puppet.exp == 0)
                lvl = max_lvl;
        }

        puppet.exp = exp_for_level(puppets_[puppet.puppet_id], (unsigned int)(double(lvl) * lvl_mul));
        puppet.write(pos, false);

        encrypt_puppet(pos, rand_data, PUPPET_SIZE);
    }
}

void Randomizer::randomize_skills(void * data, size_t len)
{
    std::set<unsigned int> valid_power, valid_acc, valid_sp, valid_prio;
    char *buf = (char*)data;

    for(size_t i = 0; i < len; i += SKILL_DATA_SIZE)
    {
        SkillData skill(&buf[i]);

        valid_power.insert(skill.power);
        valid_acc.insert(skill.accuracy);
        valid_sp.insert(skill.sp);
        valid_prio.insert(skill.priority);
    }

    valid_power.erase(0);
    valid_acc.erase(0);
    valid_sp.erase(0);

    std::vector<unsigned int> power_deck(valid_power.begin(), valid_power.end());
    std::vector<unsigned int> acc_deck(valid_acc.begin(), valid_acc.end());
    std::vector<unsigned int> sp_deck(valid_sp.begin(), valid_sp.end());
    std::vector<unsigned int> prio_deck(valid_prio.begin(), valid_prio.end());
    std::uniform_int_distribution<int> element(1, is_ynk_ ? ELEMENT_WARPED : ELEMENT_DREAM);
    std::uniform_int_distribution<int> power(0, power_deck.size() - 1);
    std::uniform_int_distribution<int> prio(0, prio_deck.size() - 1);
    std::uniform_int_distribution<int> acc(0, acc_deck.size() - 1);
    std::uniform_int_distribution<int> sp(0, sp_deck.size() - 1);
    std::bernoulli_distribution type(0.5);

    int step = (len / SKILL_DATA_SIZE) / 25;
    int count = 0;

    for(size_t i = 0; i < len; i += SKILL_DATA_SIZE)
    {
        SkillData skill(&buf[i]);

        if(count++ == step)
        {
            SendMessageW(progress_bar_, PBM_STEPIT, 0, 0);
            count = 0;
        }

        if(rand_skill_element_)
            skill.element = element(gen_);

        if(rand_skill_power_)
            skill.power = power_deck[power(gen_)];

        if(rand_skill_acc_)
            skill.accuracy = acc_deck[acc(gen_)];

        if(rand_skill_sp_)
            skill.sp = sp_deck[sp(gen_)];

        if(rand_skill_prio_)
            skill.priority = prio_deck[prio(gen_)];

        if(rand_skill_type_ && (skill.type != SKILL_TYPE_STATUS))
            skill.type = type(gen_) ? SKILL_TYPE_FOCUS : SKILL_TYPE_SPREAD;

        skill.write(&buf[i]);
    }
}

/* raw winapi so we don't have any dependencies */
Randomizer::Randomizer(HINSTANCE hInstance)
{
    g_wnd = this;
    hInstance_ = hInstance;
    RECT rect;

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rect, 0);

    hwnd_ = CreateWindowW(L"MainWindow", L"TPDP Randomizer "  VERSION_STRING, MW_STYLE, (rect.right - WND_WIDTH) / 2, (rect.bottom - WND_HEIGHT) / 2, WND_WIDTH, WND_HEIGHT, NULL, NULL, hInstance, NULL);

    GetClientRect(hwnd_, &rect);

    grp_dir_ = CreateWindowW(L"Button", L"Game Folder", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | WS_GROUP, 10, 10, rect.right - 20, 55, hwnd_, NULL, hInstance, NULL);
    grp_rand_ = CreateWindowW(L"Button", L"Randomization", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | WS_GROUP, 10, 75, rect.right - 20, 155, hwnd_, NULL, hInstance, NULL);
    grp_other_ = CreateWindowW(L"Button", L"Other", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | WS_GROUP, 10, 240, rect.right - 20, 55, hwnd_, NULL, hInstance, NULL);
    grp_seed_ = CreateWindowW(L"Button", L"Seed", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | WS_GROUP, 10, 305, rect.right - 20, 55, hwnd_, NULL, hInstance, NULL);
    bn_Randomize_ = CreateWindowW(L"Button", L"Randomize", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, (rect.right / 2) - 50, 370, 100, 30, hwnd_, (HMENU)ID_RANDOMIZE, hInstance, NULL);
    progress_bar_ = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE, 10, rect.bottom - 30, rect.right - 20, 20, hwnd_, NULL, hInstance, NULL);
    SendMessageW(progress_bar_, PBM_SETSTEP, 1, 0);

    GetClientRect(grp_dir_, &rect);
    wnd_dir_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"C:\\game\\FocasLens\\Œ¶‘zlŒ`‰‰•‘", WS_CHILD | WS_VISIBLE, 20, 30, rect.right - 90, 25, hwnd_, NULL, hInstance, NULL);
    bn_browse_ = CreateWindowW(L"Button", L"Browse", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, rect.right - 65, 28, 65, 29, hwnd_, (HMENU)ID_BROWSE, hInstance, NULL);

    GetClientRect(grp_other_, &rect);
    wnd_trainerlvl_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"100", WS_CHILD | WS_VISIBLE | ES_NUMBER, 20, 260, 50, 25, hwnd_, NULL, hInstance, NULL);
    tx_trainerlvl_ = CreateWindowW(L"Static", L"% trainer level adjustment", WS_CHILD | WS_VISIBLE | SS_WORDELLIPSIS, 75, 262, (rect.right / 2) - 75, 23, hwnd_, NULL, hInstance, NULL);
    cb_trainer_party_ = CreateWindowW(L"Button", L"Full trainer party", CB_STYLE, (rect.right / 2) + 20, 263, (rect.right / 2) - 25, 15, hwnd_, NULL, hInstance, NULL);

    GetClientRect(grp_seed_, &rect);
    unsigned int seed = (unsigned int)std::chrono::system_clock::now().time_since_epoch().count();
    wnd_seed_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", std::to_wstring(seed).c_str(), WS_CHILD | WS_VISIBLE | ES_NUMBER, 20, 325, rect.right - 125, 25, hwnd_, NULL, hInstance, NULL);
    bn_generate_ = CreateWindowW(L"Button", L"Generate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, rect.right - 100, 323, 100, 29, hwnd_, (HMENU)ID_GENERATE, hInstance, NULL);

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
    set_tooltip(wnd_trainerlvl_, L"Trainer puppets will have their level adjusted to the specified percentage\r\n100% = no change");
    set_tooltip(cb_trainer_party_, L"Trainers will always have 6 puppets.\r\nnew puppets will be randomly generated at the same level as the highest level existing puppet");

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
    std::wstring filepath = get_window_text(wnd_dir_);
    if(!path_exists(filepath))
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

    puppets_.clear();
    valid_puppet_ids_.clear();
    puppet_names_.clear();

    unsigned int seed = std::stoul(seedtext);
    gen_.seed(seed);

    level_mod_ = std::stol(get_window_text(wnd_trainerlvl_));
    if(level_mod_ <= 0)
    {
        error(L"Invalid trainer level modifier. Must be a positive integer greater than zero");
        return false;
    }

    rand_skillsets_ = (SendMessageW(cb_skills_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_stats_ = (SendMessageW(cb_stats_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_trainers_ = (SendMessageW(cb_trainers_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_types_ = (SendMessageW(cb_types_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_compat_ = (SendMessageW(cb_compat_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_abilities_ = (SendMessageW(cb_abilities_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_full_party_ = (SendMessageW(cb_trainer_party_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_puppets_ = rand_skillsets_ || rand_stats_ || rand_types_ || rand_abilities_;
    rand_skill_element_ = (SendMessageW(cb_skill_element_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_skill_power_ = (SendMessageW(cb_skill_power_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_skill_acc_ = (SendMessageW(cb_skill_acc_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_skill_sp_ = (SendMessageW(cb_skill_sp_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_skill_prio_ = (SendMessageW(cb_skill_prio_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_skill_type_ = (SendMessageW(cb_skill_type_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    rand_skills_ = rand_skill_element_ || rand_skill_power_ || rand_skill_acc_ || rand_skill_sp_ || rand_skill_prio_ || rand_skill_type_;

    if(!archive.open(filepath + L"/dat/gn_dat1.arc"))
    {
        error(L"Failed to open gn_dat1.arc\nMake sure the specified folder is correct");
        return false;
    }

    is_ynk_ = archive.is_ynk();

    /* ---encryption random data source--- */

    size_t len = archive.get_file("common/EFile.bin", NULL);
    if(!len)
    {
        error(L"EFile.bin missing from game data");
        return false;
    }

    std::unique_ptr<char[]> rand_data(new char[len]);
    size_t rand_data_len = len;

    if(archive.get_file("common/EFile.bin", rand_data.get()) != len)
    {
        error(L"Error unpacking EFile.bin from game data");
        return false;
    }

    /* ---puppet randomization--- */

    if(!archive.open(filepath + (is_ynk_ ? L"/dat/gn_dat6.arc" : L"/dat/gn_dat3.arc")))
    {
        std::wstring msg(L"Failed to open ");
        msg += (is_ynk_ ? L"gn_dat6.arc" : L"gn_dat3.arc");
        msg += L"\nMake sure the specified folder is correct";
        error(msg.c_str());
        return false;
    }

    len = archive.get_file("doll/dolldata.dbs", NULL);
    if(!len)
    {
        error(L"dolldata.dbs missing from game data");
        return false;
    }

    std::unique_ptr<char[]> buf(new char[len]);

    if(archive.get_file("doll/dolldata.dbs", buf.get()) != len)
    {
        error(L"Error unpacking dolldata.dbs from game data");
        return false;
    }

    randomize_puppets(buf.get(), len);

    if(rand_puppets_)
    {
        if(!archive.repack_file("doll/dolldata.dbs", buf.get(), len))
        {
            error(L"Error repacking dolldata.dbs");
            return false;
        }
    }

    SendMessageW(progress_bar_, PBM_SETPOS, 25, 0);
    /* ---skill randomization--- */

    if(rand_skills_)
    {
        int index = archive.get_index(is_ynk_ ? "doll/SkillData.sbs" : "doll/skill/SkillData.sbs");
        if(index < 0)
        {
            error(L"SkillData.sbs missing from game data");
            return false;
        }

        len = archive.get_file(index, NULL);
        if(!len)
        {
            error(L"Failed to unpack SkillData.sbs");
            return false;
        }

        buf.reset();
        buf.reset(new char[len]);

        if(archive.get_file(index, buf.get()) != len)
        {
            error(L"Error unpacking SkillData.sbs from game data");
            return false;
        }

        randomize_skills(buf.get(), len);

        if(!archive.repack_file(index, buf.get(), len))
        {
            error(L"Error repacking SkillData.sbs");
            return false;
        }
    }

    SendMessageW(progress_bar_, PBM_SETPOS, 50, 0);
    /* ---type effectiveness randomization--- */

    if(rand_compat_)
    {
        int index = archive.get_index(is_ynk_ ? "doll/Compatibility.csv" : "doll/elements/Compatibility.csv");
        if(index < 0)
        {
            error(L"compatibility.csv missing from game data");
            return false;
        }

        len = archive.get_file(index, NULL);
        if(!len)
        {
            error(L"Failed to unpack compatibility.csv");
            return false;
        }

        buf.reset();
        buf.reset(new char[len]);

        if(archive.get_file(index, buf.get()) != len)
        {
            error(L"Error unpacking compatibility.csv from game data");
            return false;
        }

        char chars[] = {'0', '1', '2', '4'};
        int counts[4] = {0};    /* just for statistics */

        /* we don't want tons of immunities, so weight randomization towards neutral */
        std::normal_distribution<double> dist(2.0, 0.9);

        for(char *pos = buf.get(); pos < (buf.get() + len); ++pos)
        {
            if((*pos >= '0') && (*pos <= '9'))
            {
                int r = lround(dist(gen_));
                if(r < 0)
                    r = 0;  /* toss these into the immunities since the immunity count should be pretty low anyway */
                if(r > 3)
                    r = 2;  /* make these neutral so we don't have too many weaknesses */
                *pos = chars[r];
                ++counts[r];
            }
        }

        if(!archive.repack_file(index, buf.get(), len))
        {
            error(L"Error repacking compatibility.csv");
            return false;
        }
    }

    if(!rand_trainers_ && !rand_full_party_ && (level_mod_ == 100))
    {
        std::wstring path = filepath + (is_ynk_ ? L"/dat/gn_dat6.arc" : L"/dat/gn_dat3.arc");
        if(!archive.save(path))
        {
            error((std::wstring(L"Could not write to file: ") + path + L"\nPlease make sure you have write permission to the game folder.").c_str());
            return false;
        }

        return true;
    }

    if(is_ynk_)
    {
        std::wstring path = filepath + L"/dat/gn_dat6.arc";
        if(!archive.save(path))
        {
            error((std::wstring(L"Could not write to file: ") + path + L"\nPlease make sure you have write permission to the game folder.").c_str());
            return false;
        }

        if(!archive.open(filepath + L"/dat/gn_dat5.arc"))
        {
            error(L"Failed to open gn_dat5.arc\nMake sure the specified folder is correct");
            return false;
        }
    }

    /* ---puppet names--- */

    int index = archive.get_index("name/DollName.csv");
    if(index < 0)
    {
        error(L"DollName.csv missing from game data");
        return false;
    }

    len = archive.get_file(index, NULL);
    if(!len)
    {
        error(L"Failed to unpack DollName.csv");
        return false;
    }

    buf.reset();
    buf.reset(new char[len]);

    if(archive.get_file(index, buf.get()) != len)
    {
        error(L"Error unpacking DollName.csv from game data");
        return false;
    }

    const char *pos = buf.get();
    const char *eof = pos + len;
    for(const char *endpos = pos; endpos < eof; ++endpos)
    {
        if(*endpos == '\r')
        {
            puppet_names_.push_back(std::move(std::string(pos, (size_t)(endpos - pos))));
            pos = endpos + 2;
        }
    }

    SendMessageW(progress_bar_, PBM_SETPOS, 75, 0);
    /* ---trainer randomization--- */

    int dir_index = archive.get_index("script/dollOperator");
    if(dir_index < 0)
    {
        error(L"dollOperator directory missing from game data");
        return false;
    }

    index = archive.dir_begin(dir_index);
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

        len = archive.get_file(index, NULL);
        if(!len)
        {
            error(L"Error iterating dollOperator directory");
            return false;
        }

        buf.reset();
        buf.reset(new char[len]);

        if(archive.get_file(index, buf.get()) != len)
        {
            error(L"Error iterating dollOperator directory");
            return false;
        }

        randomize_trainer(buf.get(), rand_data.get());

        if(!archive.repack_file(index, buf.get(), len))
        {
            error(L"Error repacking .dod file");
            return false;
        }
    }

    std::wstring path = filepath + (is_ynk_ ? L"/dat/gn_dat5.arc" : L"/dat/gn_dat3.arc");
    if(!archive.save(path))
    {
        error((std::wstring(L"Could not write to file: ") + path + L"\nPlease make sure you have write permission to the game folder.").c_str());
        return false;
    }

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

void Randomizer::error(const wchar_t * msg)
{
    MessageBoxW(hwnd_, msg, L"Error", MB_OK | MB_ICONERROR);
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
