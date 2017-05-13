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

#define VERSION_STRING "v1.0.8"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_REVISION 8

#define MAKE_VERSION(major, minor, revision) ((major << 16) | (minor << 8) | (revision))
#define VERSION_INT MAKE_VERSION(VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION)

#include "gamedata.h"
#include "archive.h"
#include <string>
#include <map>
#include <set>
#include <vector>
#include <random>

class Randomizer
{
private:
    HINSTANCE hInstance_;
    HWND hwnd_;
    HWND grp_dir_;
    HWND grp_rand_;
    HWND grp_other_;
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
    HWND tx_lvladjust_;
    HWND wnd_lvladjust_;
    HWND cb_trainer_party_;
    HWND cb_wild_puppets_;
    HWND cb_wild_style_;
    HWND cb_export_locations_;
    HWND cb_use_quota_;
    HWND cb_healthy_;
    HWND cb_skillcards_;
    HWND cb_true_rand_stats_;
    HWND cb_prefer_same_type_;
    HWND cb_export_puppets_;
    HWND wnd_quota_;
    HWND tx_quota_;
    HWND cb_true_rand_skills_;
    HWND cb_stab_;
    HWND grp_share_;
    HWND bn_share_gen_;
    HWND bn_share_load_;
    HWND wnd_share_;
    HWND cb_dmg_starting_move_;

    HFONT hfont_;

    typedef std::map<unsigned int, std::set<std::wstring>> LocationMap;
    LocationMap loc_map_;

    std::map<int, PuppetData> puppets_;
    std::map<int, SkillData> skills_;
    std::map<unsigned int, ItemData> items_;
    std::vector<int> valid_puppet_ids_;
    std::vector<int> puppet_id_pool_;
    std::vector<std::wstring> puppet_names_;
    std::map<int, std::wstring> skill_names_;
    std::map<int, std::wstring> ability_names_;
    std::set<unsigned int> valid_skills_;
    std::set<unsigned int> valid_abilities_;
    std::set<unsigned int> skillcard_ids_;
    std::set<unsigned int> held_item_ids_;
    std::vector<unsigned int> normal_stats_;
    std::vector<unsigned int> evolved_stats_;
    std::vector<HWND> checkboxes_;
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
    bool rand_full_party_;
    bool rand_wild_puppets_;
    bool rand_wild_style_;
    bool rand_export_locations_;
    bool rand_quota_;
    bool rand_healthy_;
    bool rand_skillcards_;
    bool rand_true_rand_stats_;
    bool rand_prefer_same_type_;
    bool rand_export_puppets_;
    bool rand_true_rand_skills_;
    bool rand_stab_;
    bool rand_dmg_starting_move_;
    unsigned int level_mod_;
    unsigned int stat_quota_;

    bool read_puppets(Archive& archive);
    bool parse_items(Archive& archive);
    bool parse_skill_names(Archive& archive);
    bool parse_ability_names(Archive& archive);
    bool randomize_puppets(Archive& archive);
    void randomize_dod_file(void *src, const void *rand_data);
    bool randomize_trainers(Archive& archive, ArcFile& rand_data);
    bool randomize_skills(Archive& archive);
    void randomize_mad_file(void *data);
    bool randomize_compatibility(Archive& archive);
    bool randomize_wild_puppets(Archive& archive);
    bool parse_puppet_names(Archive& archive);

    void generate_seed();

    void generate_share_code();
    bool load_share_code();

    void sanity_check();
    bool check_numeric_window(HWND hwnd, const std::wstring& name);

    void decrypt_puppet(void *src, const void *rand_data, std::size_t len);
    void encrypt_puppet(void *src, const void *rand_data, std::size_t len);

    unsigned int level_from_exp(const PuppetData& data, unsigned int exp) const;
    unsigned int exp_for_level(const PuppetData& data, unsigned int level) const;

    void error(const std::wstring& msg);

    HWND set_tooltip(HWND control, wchar_t *msg);

    void export_locations(const std::wstring& filepath);
    void export_puppets(const std::wstring& filepath);

    void clear();

    bool open_archive(Archive& arc, const std::wstring& path);
    bool save_archive(Archive& arc, const std::wstring& path);

public:
	Randomizer(HINSTANCE hInstance);

	~Randomizer();

    static bool register_window_class(HINSTANCE hInstance);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool randomize();
};

#endif // RANDOMIZER_H
