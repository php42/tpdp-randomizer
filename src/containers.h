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

struct ContainerError : public std::runtime_error
{
	using std::runtime_error::runtime_error;
};

/* a randomized "pool" that will refresh itself when depleted
 * values are copied from a provided source container
 * takes a reference to a PRNG engine, this reference must remain valid
 * for the duration of this objects lifespan */
template<typename T, typename Src, typename Gen>
class RandPool
{
private:
	std::vector<T> pool_;
	Src src_;
	Gen& gen_;

	void refresh()
	{
		pool_.assign(src_.begin(), src_.end());
		std::shuffle(pool_.begin(), pool_.end(), gen_);
	}

public:
	RandPool(const Src& src, Gen& gen) : src_(src), gen_(gen)
	{
		assert(src.size());
		refresh();
	}

	/* draw a value from the pool. the returned value is removed from the pool.
	 * if there are no more values in the pool, the pool is refilled with all the original
	 * values and shuffled again. */
	T draw()
	{
		if(pool_.empty())
			refresh();

		T ret = pool_.back();
		pool_.pop_back();

		return ret;
	}

	static_assert(std::is_copy_constructible<Src>::value, "source container must be copy-constructible");
};

/* create a randomized "deck" from a source container
 * drawing from this deck returns a specified default value when empty */
template<typename T, typename Src, typename Gen, T default_value>
class RandDeck
{
private:
	std::vector<T> deck_;

public:
	RandDeck(const Src& src, Gen& gen) : deck_(src.begin(), src.end())
	{
		std::shuffle(deck_.begin(), deck_.end(), gen);
	}

	/* draw a value from the deck. the returned value is removed from the deck.
	 * returns default_value if the deck is empty */
	T draw()
	{
		if(deck_.empty())
			return default_value;

		T ret;
		ret = deck_.back();
		deck_.pop_back();
		return ret;
	}
};

/* create a randomized "deck" from a source container
 * throws a ContainerError exception if attempting to draw from an empty deck */
template<typename T, typename Src, typename Gen>
class RandDeck
{
private:
	std::vector<T> deck_;

public:
	RandDeck(const Src& src, Gen& gen) : deck_(src.begin(), src.end())
	{
		std::shuffle(deck_.begin(), deck_.end(), gen);
	}

	/* draw a value from the deck. the returned value is removed from the deck.
	 * throws ContainerError if the deck is empty */
	T draw()
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
