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

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <vector>
#include <string>

class RandomizerGUI
{
private:
    HINSTANCE hInstance_;
    HWND hwnd_;

    HWND bn_randomize_;
    HWND bn_browse_;
    HWND bn_generate_;
    HWND bn_share_gen_;
    HWND bn_share_load_;

    HWND wnd_dir_;
    HWND wnd_seed_;
    HWND wnd_lvladjust_;
    HWND wnd_quota_;
    HWND wnd_share_;
    HWND wnd_sc_chance_;
    HWND wnd_item_chance_;
    HWND wnd_stat_ratio_;

    HWND cb_skills_;
    HWND cb_stats_;
    HWND cb_types_;
    HWND cb_trainers_;
    HWND cb_compat_;
    HWND cb_abilities_;
    HWND cb_skill_element_;
    HWND cb_skill_power_;
    HWND cb_skill_acc_;
    HWND cb_skill_sp_;
    HWND cb_skill_prio_;
    HWND cb_skill_type_;
    HWND cb_trainer_party_;
    HWND cb_encounters_;
    HWND cb_export_locations_;
    HWND cb_use_quota_;
    HWND cb_healthy_;
    HWND cb_skillcards_;
    HWND cb_true_rand_stats_;
    HWND cb_prefer_same_type_;
    HWND cb_export_puppets_;
    HWND cb_true_rand_skills_;
    HWND cb_proportional_stats_;
    HWND cb_strict_trainers_;
    HWND cb_cost_;
    HWND cb_starting_move_;

    HWND progress_bar_;

    std::vector<HWND> checkboxes_;
    std::vector<HWND> checkboxes_3state_;

    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND set_tooltip(HWND control, wchar_t *msg);

    void init_hwnd_member(HWND& hwnd, int id); // ugly wrapper to initialize hwnd data members from id

    void generate_seed();

    void generate_share_code();
    bool load_share_code();

    bool validate();
    bool validate_uint_window(HWND hwnd, const std::wstring& name);

public:
    RandomizerGUI(HINSTANCE hInstance);

    void set_progress_bar(int percent);
    void increment_progress_bar(); /* increase progress bar by 1% */

    void error(const std::wstring& msg);
    bool msg_yesno(const std::wstring& msg);

    //~RandomizerGUI() = default;
};