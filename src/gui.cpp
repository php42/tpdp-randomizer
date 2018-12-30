/*
    Copyright (C) 2018 php42

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

/* this could go in the command line, but i'll dump it here so it's conspicuous */
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "gui.h"
#include "randomizer.h"
#include "resource.h"
#include "textconvert.h"
#include "filesystem.h"
#include <CommCtrl.h>
#include <ShlObj.h>

#define IS_CHECKED(chkbox) (SendMessageW(chkbox, BM_GETCHECK, 0, 0) == BST_CHECKED)
#define SET_CHECKED(chkbox, checked) SendMessageW(chkbox, BM_SETCHECK, (checked) ? BST_CHECKED : BST_UNCHECKED, 0)
#define GET_3STATE(chkbox) ((unsigned int)SendMessageW(chkbox, BM_GETCHECK, 0, 0))
#define SET_3STATE(chkbox, state) SendMessageW(chkbox, BM_SETCHECK, (state), 0)

static std::wstring get_window_text(HWND hwnd)
{
    int len = GetWindowTextLengthW(hwnd);
    if(!len)
        return std::wstring();

    len += 1; // make room for null terminator

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
 * this is intended to serialize positive integers into a short text form,
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

HWND RandomizerGUI::set_tooltip(HWND control, wchar_t *msg)
{
    HWND tip = CreateWindowExW(NULL, TOOLTIPS_CLASSW,
        NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_USEVISUALSTYLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, hwnd_, NULL, hInstance_, NULL);

    TOOLINFOW ti = { 0 };
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

void RandomizerGUI::init_hwnd_member(HWND& hwnd, int id)
{
    hwnd = GetDlgItem(hwnd_, id);
}

INT_PTR CALLBACK RandomizerGUI::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_COMMAND:
        if(HIWORD(wParam) == BN_CLICKED)
        {
            RandomizerGUI *gui = (RandomizerGUI*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            switch(LOWORD(wParam))
            {
            case IDC_GENERATE_SEED:
            {
                gui->generate_seed();
            }
            break;
            case IDC_BROWSE:
            {
                wchar_t buf[MAX_PATH] = { 0 };
                BROWSEINFOW bi = { 0 };
                bi.hwndOwner = hwnd;
                bi.pszDisplayName = buf;
                bi.lpszTitle = L"Select Game Folder";
                bi.ulFlags = BIF_USENEWUI;

                PIDLIST_ABSOLUTE pl = SHBrowseForFolderW(&bi);
                if(!pl)
                    break;

                SHGetPathFromIDListW(pl, buf);
                CoTaskMemFree(pl);
                set_window_text(gui->wnd_dir_, buf);
            }
            break;
            case IDC_RANDOMIZE:
                if(!gui->validate())
                    break;

                if(!path_exists(get_window_text(gui->wnd_dir_)))
                {
                    gui->error(L"Invalid folder selected, please locate the game folder");
                    break;
                }

                if(MessageBoxW(hwnd, L"This will permanently modify game files.\r\nIf you have not backed up your game folder, you may wish to do so now.\r\n"
                    "Also note that randomization is cumulative. Re-randomizing the same game data may have unexpected results.",
                    L"Notice", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
                    break;

                try
                {
                    Randomizer rnd(gui);

                    rnd.rand_trainer_sc_shuffle_ = get_window_text(gui->wnd_sc_chance_).empty();
                    rnd.stat_ratio_ = get_window_uint(gui->wnd_stat_ratio_);
                    rnd.level_mod_ = get_window_uint(gui->wnd_lvladjust_);
                    rnd.stat_quota_ = get_window_uint(gui->wnd_quota_);

                    rnd.trainer_sc_chance_ = get_window_uint(gui->wnd_sc_chance_);
                    if(rnd.trainer_sc_chance_ > 100)
                        rnd.trainer_sc_chance_ = 100;

                    rnd.trainer_item_chance_ = get_window_uint(gui->wnd_item_chance_);
                    if(rnd.trainer_item_chance_ > 100)
                        rnd.trainer_item_chance_ = 100;

                    rnd.rand_skillsets_ = IS_CHECKED(gui->cb_skills_);
                    rnd.rand_stats_ = IS_CHECKED(gui->cb_stats_);
                    rnd.rand_trainers_ = IS_CHECKED(gui->cb_trainers_);
                    rnd.rand_types_ = IS_CHECKED(gui->cb_types_);
                    rnd.rand_compat_ = IS_CHECKED(gui->cb_compat_);
                    rnd.rand_abilities_ = IS_CHECKED(gui->cb_abilities_);
                    rnd.rand_full_party_ = IS_CHECKED(gui->cb_trainer_party_);
                    rnd.rand_encounters_ = GET_3STATE(gui->cb_encounters_);
                    rnd.rand_export_locations_ = IS_CHECKED(gui->cb_export_locations_);
                    rnd.rand_quota_ = IS_CHECKED(gui->cb_use_quota_);
                    rnd.rand_healthy_ = IS_CHECKED(gui->cb_healthy_);
                    rnd.rand_skillcards_ = GET_3STATE(gui->cb_skillcards_);
                    rnd.rand_true_rand_stats_ = IS_CHECKED(gui->cb_true_rand_stats_);
                    rnd.rand_prefer_same_type_ = IS_CHECKED(gui->cb_prefer_same_type_);
                    rnd.rand_export_puppets_ = IS_CHECKED(gui->cb_export_puppets_);
                    rnd.rand_true_rand_skills_ = IS_CHECKED(gui->cb_true_rand_skills_);
                    rnd.rand_cost_ = GET_3STATE(gui->cb_cost_);
                    rnd.rand_skill_element_ = IS_CHECKED(gui->cb_skill_element_);
                    rnd.rand_skill_power_ = IS_CHECKED(gui->cb_skill_power_);
                    rnd.rand_skill_acc_ = IS_CHECKED(gui->cb_skill_acc_);
                    rnd.rand_skill_sp_ = IS_CHECKED(gui->cb_skill_sp_);
                    rnd.rand_skill_prio_ = IS_CHECKED(gui->cb_skill_prio_);
                    rnd.rand_skill_type_ = IS_CHECKED(gui->cb_skill_type_);
                    rnd.rand_starting_move_ = GET_3STATE(gui->cb_starting_move_);
                    rnd.rand_stat_scaling_ = IS_CHECKED(gui->cb_proportional_stats_);
                    rnd.rand_strict_trainers_ = IS_CHECKED(gui->cb_strict_trainers_);

                    if(rnd.randomize(get_window_text(gui->wnd_dir_), get_window_uint(gui->wnd_seed_)))
                    {
                        gui->set_progress_bar(100);
                        MessageBoxW(hwnd, L"Complete!", L"Success", MB_OK);
                    }
                    else
                        MessageBoxW(hwnd, L"An error occurred, randomization aborted", L"Error", MB_OK | MB_ICONERROR);
                }
                catch(const std::exception& ex)
                {
                    gui->error(utf_widen(ex.what()));
                }

                gui->set_progress_bar(0);
                break;
            case IDC_STATS:
                if(!IS_CHECKED(gui->cb_stats_))
                {
                    SET_CHECKED(gui->cb_true_rand_stats_, false);
                    SET_CHECKED(gui->cb_use_quota_, false);
                    EnableWindow(gui->wnd_quota_, false);
                    EnableWindow(gui->wnd_stat_ratio_, false);
                    SET_CHECKED(gui->cb_proportional_stats_, false);
                }
                break;
            case IDC_STAT_QUOTA:
            {
                bool checked = IS_CHECKED(gui->cb_use_quota_);
                EnableWindow(gui->wnd_quota_, checked);
                if(checked)
                {
                    SET_CHECKED(gui->cb_stats_, true);
                    SET_CHECKED(gui->cb_true_rand_stats_, false);
                    SET_CHECKED(gui->cb_proportional_stats_, false);
                    EnableWindow(gui->wnd_stat_ratio_, false);
                }
            }
            break;
            case IDC_TRUE_RAND_STATS:
            {
                bool checked = IS_CHECKED(gui->cb_true_rand_stats_);
                if(checked)
                {
                    SET_CHECKED(gui->cb_stats_, true);
                    EnableWindow(gui->wnd_quota_, false);
                    EnableWindow(gui->wnd_stat_ratio_, false);
                    SET_CHECKED(gui->cb_use_quota_, false);
                    SET_CHECKED(gui->cb_proportional_stats_, false);
                }
            }
            break;
            case IDC_PREFER_STAB:
                if(IS_CHECKED(gui->cb_prefer_same_type_))
                    SET_CHECKED(gui->cb_skills_, true);
                break;
            case IDC_HEALTHY:
                if(IS_CHECKED(gui->cb_healthy_))
                    SET_CHECKED(gui->cb_abilities_, true);
                break;
            case IDC_ABILITIES:
                if(!IS_CHECKED(gui->cb_abilities_))
                    SET_CHECKED(gui->cb_healthy_, false);
                break;
            case IDC_SKILLSETS:
                if(!IS_CHECKED(gui->cb_skills_))
                {
                    SET_CHECKED(gui->cb_prefer_same_type_, false);
                    SET_CHECKED(gui->cb_true_rand_skills_, false);
                    SET_CHECKED(gui->cb_starting_move_, false);
                }
                break;
            case IDC_TRUE_RAND_SKILLS:
                if(IS_CHECKED(gui->cb_true_rand_skills_))
                {
                    SET_CHECKED(gui->cb_skills_, true);
                }
                break;
            case IDC_STAB_STARTING_MOVE:
                if(GET_3STATE(gui->cb_starting_move_))
                {
                    SET_CHECKED(gui->cb_skills_, true);
                }
                break;
            case IDC_GENERATE_SHARE:
                gui->generate_share_code();
                break;
            case IDC_LOAD_SHARE:
                gui->load_share_code();
                break;
            case IDC_PROPORTIONAL_STATS:
                {
                    bool checked = IS_CHECKED(gui->cb_proportional_stats_);
                    EnableWindow(gui->wnd_stat_ratio_, checked);
                    if(checked)
                    {
                        SET_CHECKED(gui->cb_stats_, true);
                        EnableWindow(gui->wnd_quota_, false);
                        SET_CHECKED(gui->cb_use_quota_, false);
                        SET_CHECKED(gui->cb_true_rand_stats_, false);
                    }
                }
                break;
            case IDC_FULL_PARTY:
                if(!IS_CHECKED(gui->cb_trainers_) && !IS_CHECKED(gui->cb_trainer_party_))
                {
                    SET_CHECKED(gui->cb_strict_trainers_, false);
                    EnableWindow(gui->cb_strict_trainers_, false);
                }
                else if(IS_CHECKED(gui->cb_trainer_party_))
                    EnableWindow(gui->cb_strict_trainers_, true);
                break;
            case IDC_TRAINERS:
                if(!IS_CHECKED(gui->cb_trainers_) && !IS_CHECKED(gui->cb_trainer_party_))
                {
                    SET_CHECKED(gui->cb_strict_trainers_, false);
                    EnableWindow(gui->cb_strict_trainers_, false);
                }
                else if(IS_CHECKED(gui->cb_trainers_))
                    EnableWindow(gui->cb_strict_trainers_, true);
                break;
            default:
                break;
            }
        }
        break;
    case WM_DESTROY: // fallthrough
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        break;
    }

    return FALSE;
}

RandomizerGUI::RandomizerGUI(HINSTANCE hInstance)
{
    hInstance_ = hInstance;

    /* XXX: do we actually need this when using dialogs from a resource file? */
    INITCOMMONCONTROLSEX icc = { 0 };
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    hwnd_ = CreateDialogParamW(hInstance, MAKEINTRESOURCEW(IDD_MAINWINDOW), NULL, DialogProc, 0);
    if(hwnd_ == NULL)
        abort();

    /* store pointer to this object in the userdata field of the window
     * this is used by the window callback procedure, and is a bit more proper than a global variable */
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);

    set_window_text(hwnd_, L"TPDP Randomizer " VERSION_STRING);

    /* delicious spaghetti */
    init_hwnd_member(bn_randomize_, IDC_RANDOMIZE);
    init_hwnd_member(bn_browse_, IDC_BROWSE);
    init_hwnd_member(bn_generate_, IDC_GENERATE_SEED);
    init_hwnd_member(bn_share_gen_, IDC_GENERATE_SHARE);
    init_hwnd_member(bn_share_load_, IDC_LOAD_SHARE);

    init_hwnd_member(wnd_dir_, IDC_FOLDER_BOX);
    init_hwnd_member(wnd_seed_, IDC_SEED_BOX);
    init_hwnd_member(wnd_lvladjust_, IDC_LVL_BOX);
    init_hwnd_member(wnd_quota_, IDC_QUOTA_BOX);
    init_hwnd_member(wnd_share_, IDC_SHARE_BOX);
    init_hwnd_member(wnd_sc_chance_, IDC_SKILLCARD_BOX);
    init_hwnd_member(wnd_item_chance_, IDC_ITEM_BOX);
    init_hwnd_member(wnd_stat_ratio_, IDC_RATIO_BOX);

    init_hwnd_member(cb_skills_, IDC_SKILLSETS);
    init_hwnd_member(cb_stats_, IDC_STATS);
    init_hwnd_member(cb_types_, IDC_TYPES);
    init_hwnd_member(cb_trainers_, IDC_TRAINERS);
    init_hwnd_member(cb_compat_, IDC_COMPAT);
    init_hwnd_member(cb_abilities_, IDC_ABILITIES);
    init_hwnd_member(cb_skill_element_, IDC_SKILL_ELEMENT);
    init_hwnd_member(cb_skill_power_, IDC_SKILL_POWER);
    init_hwnd_member(cb_skill_acc_, IDC_SKILL_ACC);
    init_hwnd_member(cb_skill_sp_, IDC_SKILL_SP);
    init_hwnd_member(cb_skill_prio_, IDC_SKILL_PRIO);
    init_hwnd_member(cb_skill_type_, IDC_SKILL_TYPE);
    init_hwnd_member(cb_trainer_party_, IDC_FULL_PARTY);
    init_hwnd_member(cb_encounters_, IDC_WILD_PUPPETS);
    init_hwnd_member(cb_export_locations_, IDC_EXPORT_LOCATIONS);
    init_hwnd_member(cb_use_quota_, IDC_STAT_QUOTA);
    init_hwnd_member(cb_healthy_, IDC_HEALTHY);
    init_hwnd_member(cb_skillcards_, IDC_SKILLCARDS);
    init_hwnd_member(cb_true_rand_stats_, IDC_TRUE_RAND_STATS);
    init_hwnd_member(cb_prefer_same_type_, IDC_PREFER_STAB);
    init_hwnd_member(cb_export_puppets_, IDC_DUMP_STATS);
    init_hwnd_member(cb_true_rand_skills_, IDC_TRUE_RAND_SKILLS);
    init_hwnd_member(cb_proportional_stats_, IDC_PROPORTIONAL_STATS);
    init_hwnd_member(cb_strict_trainers_, IDC_STRICT_TRAINERS);
    init_hwnd_member(cb_cost_, IDC_PUPPET_COST);
    init_hwnd_member(cb_starting_move_, IDC_STAB_STARTING_MOVE);

    init_hwnd_member(progress_bar_, IDC_PROG_BAR);

    generate_seed();

    set_window_text(wnd_dir_, L"C:\\game\\FocasLens\\幻想人形演舞");
    set_window_text(wnd_lvladjust_, L"100");
    set_window_text(wnd_quota_, L"500");
    set_window_text(wnd_stat_ratio_, L"25");
    set_window_text(wnd_item_chance_, L"25");

    SendMessageW(progress_bar_, PBM_SETSTEP, 1, 0);

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
    set_tooltip(wnd_share_, L"Randomization codes allow for quick copy/paste of your randomization settings and seed. Click generate to create a code for your current settings, or paste in a code and click load to apply those settings.");
    set_tooltip(bn_share_gen_, L"Generate a randomization code for your current settings and seed");
    set_tooltip(bn_share_load_, L"Load settings and seed from the supplied randomization code");
    set_tooltip(cb_proportional_stats_, L"Generate stats proportional to the puppets original stats.\nAdjust with the \"Stat ratio\" option below.");
    set_tooltip(cb_strict_trainers_, L"Trainer puppets will not have skills for which they are too low level.\nWhen unchecked, level requirement is ignored and trainer puppets may have any move in their moveset at any level.\nHas no effect on skill card moves.");
    set_tooltip(wnd_sc_chance_, L"Chance for trainer puppets to have a skill card move (per move slot).\nBlank = shuffle.");
    set_tooltip(wnd_item_chance_, L"Chance for trainer puppets to have a held item.");
    set_tooltip(wnd_stat_ratio_, L"Adjust maximum stat variance when using proportional stats.\nA value of 25 allows stats to change up to a maximum of +/- 25% of the original stat.");
    set_tooltip(cb_cost_, L"This is a 3-state checkbox. Click twice to get to the \"middle\" state.\nChecked: randomized puppet cost\nMiddle: all puppets set to 120 cost");

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
    checkboxes_3state_.push_back(cb_skillcards_);
}

void RandomizerGUI::generate_seed()
{
    std::random_device rdev;
    unsigned int seed = rdev();
    set_window_text(wnd_seed_, std::to_wstring(seed).c_str());
}

/* serialize the randomization settings into a text string */
void RandomizerGUI::generate_share_code()
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

bool RandomizerGUI::load_share_code()
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

            if(!msg_yesno(L"This code appears to be for a different version of the randomizer and may not work as expected.\r\n"
                "The encoded version number appears to be: \"" + version_string + L"\"\r\n"
                "If you really want to use it, check for a matching version of the randomizer from https://github.com/php42/tpdp-randomizer/releases\r\n"
                "Would you like to try to load this code anyway?"))
                return false;
        }
        else if((ver == 0) && (ver != VERSION_INT))
        {
            if(!msg_yesno(L"This code looks like it's from a beta or git build of the randomizer and may not work as expected.\r\n"
                "Would you like to try to load this code anyway?"))
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

bool RandomizerGUI::validate()
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

bool RandomizerGUI::validate_uint_window(HWND hwnd, const std::wstring& name)
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
        volatile auto unused = std::stoul(str);
    }
    catch(const std::out_of_range&)
    {
        error(name + L" value too large!\r\nValues must be less than 2^32");
        return false;
    }
    catch(const std::exception&)
    {
        error(L"Invalid " + name + L" value.");
        return false;
    }

    return true;
}

void RandomizerGUI::error(const std::wstring& msg)
{
    MessageBoxW(hwnd_, msg.c_str(), L"Error", MB_OK | MB_ICONERROR);
}

bool RandomizerGUI::msg_yesno(const std::wstring& msg)
{
    auto ret = MessageBoxW(hwnd_, msg.c_str(), L"Error", MB_YESNO | MB_ICONERROR);
    return (ret == IDYES);
}

void RandomizerGUI::set_progress_bar(int percent)
{
    SendMessageW(progress_bar_, PBM_SETPOS, percent, 0);
}

void RandomizerGUI::increment_progress_bar()
{
    SendMessageW(progress_bar_, PBM_STEPIT, 0, 0);
}
