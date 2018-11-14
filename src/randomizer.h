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

#define VERSION_STRING "v1.1.0 BETA"

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_REVISION 0

#define MAKE_VERSION(major, minor, revision) ((major << 16) | (minor << 8) | (revision))
#define VERSION_INT MAKE_VERSION(VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION)

#include "gamedata.h"
#include "archive.h"
#include "containers.h"
#include "gui.h"
#include <string>
#include <map>
#include <set>
#include <vector>
#include <random>
#include <optional>

typedef std::set<unsigned int> IDSet;
typedef std::vector<unsigned int> IDVec;
typedef RandPool<unsigned int> IDPool;
typedef RandDeck<unsigned int> IDDeck;
typedef std::map<unsigned int, std::set<std::wstring>> LocationMap;

class Randomizer
{
private:
    LocationMap loc_map_;
    RandomizerGUI *gui_;

    std::map<int, PuppetData> puppets_;
    std::map<int, SkillData> skills_;
    std::map<unsigned int, ItemData> items_;
    IDVec valid_puppet_ids_;
    //std::vector<int> puppet_id_pool_;
    IDPool puppet_id_pool_;
    std::vector<std::wstring> puppet_names_;
    std::map<int, std::wstring> skill_names_;
    std::map<int, std::wstring> ability_names_;
    IDSet valid_skills_;
    IDSet valid_abilities_;
    IDSet skillcard_ids_;
    IDSet held_item_ids_;
    IDVec normal_stats_;
    IDVec evolved_stats_;
    std::map<unsigned int, unsigned int> old_costs_;
    std::multiset<std::wstring> location_names_;

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
    bool rand_export_locations_;
    bool rand_quota_;
    bool rand_healthy_;
    bool rand_true_rand_stats_;
    bool rand_prefer_same_type_;
    bool rand_export_puppets_;
    bool rand_true_rand_skills_;
    bool rand_stat_scaling_;
    bool rand_strict_trainers_;
    bool rand_trainer_sc_shuffle_;
    //unsigned int rand_encounter_rates_;
    unsigned int level_mod_;
    unsigned int stat_quota_;
    unsigned int trainer_sc_chance_;
    unsigned int trainer_item_chance_;
    unsigned int stat_ratio_;
    unsigned int rand_cost_;
    unsigned int rand_encounters_;
    unsigned int rand_starting_move_;
	unsigned int rand_skillcards_;

    bool parse_puppets(Archive& archive);
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

    void decrypt_puppet(void *src, const void *rand_data, std::size_t len);
    void encrypt_puppet(void *src, const void *rand_data, std::size_t len);

    unsigned int level_from_exp(const PuppetData& data, unsigned int exp) const;
    unsigned int level_from_exp(unsigned int cost, unsigned int exp) const;
    unsigned int exp_for_level(const PuppetData& data, unsigned int level) const;
    unsigned int exp_for_level(unsigned int cost, unsigned int level) const;

    /* search for a skill matching one of the given elements and remove it from the deck.
     * if a match is found, returns an optional with the skill ID.
     * if no match is found, returns empty optional */
    template <typename T>
    std::optional<unsigned int> get_stab_skill(T src, int element1, int element2)
    {
        for(auto it = src.begin(); it != src.end(); ++it)
        {
            auto e = skills_[*it].element;
            if((e == element1) || (e == element2))
            {
                auto ret = *it;
                src.erase(it);
                return ret;
            }
        }

        return {};
    }

    void error(const std::wstring& msg);

    void export_locations(const std::wstring& filepath);
    void export_puppets(const std::wstring& filepath);

    void clear();

    bool open_archive(Archive& arc, const std::wstring& path);
    bool save_archive(Archive& arc, const std::wstring& path);

    void set_progress_bar(int percent);
    void increment_progress_bar(); /* increase progress bar by 1% */

public:
    Randomizer(RandomizerGUI *gui) : gui_(gui) {};

    bool randomize(const std::wstring& dir, unsigned int seed);

    friend RandomizerGUI;
};

#endif // RANDOMIZER_H
