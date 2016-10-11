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

#ifndef RANDOMIZER_H
#define RANDOMIZER_H

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define VERSION_STRING "v1.0.0"

#include "gamedata.h"
#include <string>
#include <map>
#include <vector>
#include <random>

class Randomizer
{
private:
    HINSTANCE hInstance_;
    HWND hwnd_;
    HWND grp_dir_;
    HWND grp_rand_;
    HWND grp_seed_;
    HWND bn_Randomize_;
    HWND wnd_dir_;
    HWND bn_browse_;
    HWND wnd_seed_;
    HWND bn_generate_;
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
    HWND progress_bar_;

    HFONT hfont_;

    std::map<int, PuppetData> puppets_;
    std::vector<int> valid_puppet_ids_;
    std::vector<std::string> puppet_names_;
    std::default_random_engine gen_;
    bool is_ynk_;
    bool rand_puppets_;
    bool rand_skillsets_;
    bool rand_stats_;
    bool rand_trainers_;
    bool rand_types_;
    bool rand_compat_;
    bool rand_abilities_;
    bool rand_skills_;
    bool rand_skill_element_;
    bool rand_skill_power_;
    bool rand_skill_acc_;
    bool rand_skill_sp_;
    bool rand_skill_prio_;
    bool rand_skill_type_;


    void randomize_puppets(void *data, size_t len);
    void randomize_trainer(void *data, const void *rand_data);
    void randomize_skills(void *data, size_t len);

    void decrypt_puppet(void *src, const void *rand_data, std::size_t len);
    void encrypt_puppet(void *src, const void *rand_data, std::size_t len);

    void error(const wchar_t *msg);

    HWND set_tooltip(HWND control, wchar_t *msg);

public:
	Randomizer(HINSTANCE hInstance);

	~Randomizer();

    static bool register_window_class(HINSTANCE hInstance);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool randomize();
};

#endif // RANDOMIZER_H
