/*
	Copyright (C) 2017 php42

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

#ifndef CONTAINERS_H
#define CONTAINERS_H
#include <vector>
#include <random>
#include <cassert>
#include <type_traits>
#include <optional>

struct ContainerError : public std::runtime_error
{
	using std::runtime_error::runtime_error;
};

/* A randomized "pool" that will refresh itself when depleted.
 * Values are copied from a provided source container. */
template<typename T>
class RandPool
{
private:
	std::vector<T> pool_;
	std::vector<T> src_;

public:
    RandPool() = default;

    template<typename Src, typename Rng>
	RandPool(const Src& src, Rng& gen) : src_(src.begin(), src.end())
	{
		assert(src.size());
        reset(gen);
	}

    template<typename Src, typename Rng>
    void assign(const Src& src, Rng& gen)
    {
        assert(src.size());
        src_.assign(src.begin(), src.end());
        reset(gen);
    }

    template<typename Rng>
    void reset(Rng& gen)
    {
        assert(src_.size());
        pool_ = src_;
        std::shuffle(pool_.begin(), pool_.end(), gen);
    }

    void clear()
    {
        pool_.clear();
        src_.clear();
    }

    using iterator = typename std::vector<T>::iterator;

    auto begin()
    {
        return pool_.begin();
    }

    auto end()
    {
        return pool_.end();
    }

    auto erase(iterator it)
    {
        return pool_.erase(it);
    }

    bool empty()
    {
        return pool_.empty();
    }

	/* Draw a value from the pool. the returned value is removed from the pool.
	 * If there are no more values in the pool, the pool is refilled with all the original
	 * values and shuffled again.
     * Throws ContainerError if pool and source container are both empty. */
    template<typename Rng>
	T draw(Rng& gen)
	{
        assert(src_.size());
        if(pool_.empty() && src_.empty())
            throw ContainerError("Tried to draw from an empty pool");

		if(pool_.empty())
            reset(gen);

		T ret = pool_.back();
		pool_.pop_back();

		return ret;
	}
};

/* Create a randomized "deck" from a source container.
 * This deck does not refresh itself when empty. */
template<typename T>
class RandDeck
{
private:
	std::vector<T> deck_;

public:
    RandDeck() = default;

    template<typename Src, typename Rng>
	RandDeck(const Src& src, Rng& gen) : deck_(src.begin(), src.end())
	{
		std::shuffle(deck_.begin(), deck_.end(), gen);
	}

    template<typename Src, typename Rng>
    void assign(const Src& src, Rng& gen)
    {
        deck_.assign(src.begin(), src.end());
        std::shuffle(deck_.begin(), deck_.end(), gen);
    }

    using iterator = typename std::vector<T>::iterator;

    auto begin()
    {
        return deck_.begin();
    }

    auto end()
    {
        return deck_.end();
    }

    auto erase(iterator it)
    {
        return deck_.erase(it);
    }

    bool empty()
    {
        return deck_.empty();
    }

    void clear()
    {
        deck_.clear();
    }

    /* Draw a value from the deck. The returned value is removed from the deck.
     * Returns empty optional if the deck is empty */
    std::optional<T> draw()
    {
        if(deck_.empty())
            return {};

        T ret;
        ret = deck_.back();
        deck_.pop_back();
        return ret;
    }

	/* Draw a value from the deck. The returned value is removed from the deck.
	 * Returns default_value if the deck is empty */
	T draw(T default_value)
	{
		if(deck_.empty())
			return default_value;

		T ret;
		ret = deck_.back();
		deck_.pop_back();
		return ret;
	}

    /* Draw a value from the deck. The returned value is removed from the deck.
     * Throws ContainerError if the deck is empty */
    T draw_throw()
    {
        if(deck_.empty())
            throw ContainerError("Tried to draw from an empty deck");

        T ret;
        ret = deck_.back();
        deck_.pop_back();
        return ret;
    }
};

#endif // !CONTAINERS_H
