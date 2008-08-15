#ifndef RANGE_H__INCLUDED
#define RANGE_H__INCLUDED

// Range class defines the boundaries of tiles in a single dimension.
// The tiling data is constructed with and stored in an array with
// the format {a, b, c, ...}, where 0 <= a < b < c < ... Each tile is
// defined by [a,b), [b,c), ... The number of tiles in the range is
// defined as one less than the number of elements in the array.

class Range
{
public:
	typedef std::pair<unsigned int, unsigned int>	tile;

private:
	// Iterator spec for RangeIterator class.
	class RangeIteratorSpec
	{
	public:
		typedef int							iterator_type;
		typedef Range						collection_type;
		typedef std::input_iterator_tag		iterator_category;  
		typedef tile						value;
		typedef value*						pointer;
		typedef const value*				const_pointer;
		typedef value&						reference;
		typedef const value&				const_reference;
	};


	// RangeIterator is an input iterator that iterates over the tiles
	// of a range.

	class RangeIterator : public Iterator<RangeIteratorSpec>
	{
	public:
		// Iterator typedef's
		typedef Iterator<RangeIteratorSpec>::iterator_type		iterator_type;
		typedef Iterator<RangeIteratorSpec>::collection_type	collection_type;
		typedef Iterator<RangeIteratorSpec>::iterator_category	iterator_catagory;
		typedef Iterator<RangeIteratorSpec>::reference			reference;
		typedef Iterator<RangeIteratorSpec>::const_reference	const_reference;
		typedef Iterator<RangeIteratorSpec>::pointer			pointer;
		typedef Iterator<RangeIteratorSpec>::const_pointer		const_pointer;
		typedef Iterator<RangeIteratorSpec>::value				value;
		typedef Iterator<RangeIteratorSpec>::difference_type	difference_type;

	private:

		const collection_type& m_coll;	// Reference to the collection that will be iterated over

	public:

// 		// Default construction not allowed (required by forward iterators)
// 		RangeIterator() :
// 			Iterator<RangeIteratorSpec>(),
// 			m_coll(collection_type())
// 		{assert(false);}

		// Main constructor function
		RangeIterator(const collection_type& coll, const iterator_type& cur = 0) : 
		Iterator<RangeIteratorSpec>(cur), 
			m_coll(coll)
		{}

		// Copy constructor (required by all iterators)
		RangeIterator(const RangeIterator& it) :
		Iterator<RangeIteratorSpec>(it.m_current), 
			m_coll(it.m_coll)
		{}

		// Prefix increment (required by all iterators)
		RangeIterator&
		operator ++() 
		{     
			this->advance();
			return *this;
		}


		// Postfix increment (required by all iterators)
		RangeIterator
		operator ++(int) 
		{
			assert(this->m_current != -1);
			RangeIterator tmp(*this);
			this->advance();
			return tmp;
		}

		// Equality operator (required by input iterators)
		inline bool
			operator ==(const RangeIterator& it) const
		{
			return this->m_current == it.m_current;
		}

		// Inequality operator (required by input iterators)
		inline bool
			operator !=(const RangeIterator& it) const
		{
			return ! (this->operator ==(it));
		}

		// Dereference operator (required by input iterators)
		const value
		operator *() const 
		{
			assert(this->m_current != -1);
			return this->m_coll.get_tile(this->m_current);
		}

		// Dereference operator (required by input iterators)
		const value
		operator ->() const
		{
			assert(this->m_current != -1);
			return this->m_coll.get_tile(this->m_current);
		}

		/* This is for debugging only. Not doen in an overload of operator<<
		* because it seems that gcc 3.4 does not manage inner class declarations of 
		* template classes correctly
		*/
		char
			Print(std::ostream& ost) const
		{
			ost << "Range::iterator("
				<< "current=" << this->m_current << ")";
			return '\0';
		}

	private:

		void
			advance(int n = 1) 
		{

		}


	}; // RangeIterator
	
	// class data
	std::vector<unsigned int> m_ranges;

	// Validates the data in m_ranges meets the requirements of Range.
	bool
	valid() const
	{
		// Check minimum number of elements
		if(this->m_ranges.size() < 2)
			return false;
		
		// Verify the requirement that a < b < c < ...
		for(std::vector<unsigned int>::const_iterator it = this->m_ranges.begin() + 1; it != this->m_ranges.end(); ++it)
			if(*it <= *(it - 1))
				return false;
		
		return true;
	}
public:
	typedef RangeIterator							iterator;
	typedef const RangeIterator						const_iterator;

	
	// Default constructor, range with a single tile [0,1)
	Range() :
		m_ranges(2, 0)
	{
		m_ranges[1] = 1;
	}

	// Constructs range from a vector
	Range(std::vector<unsigned int> ranges) :
		m_ranges(ranges)
	{
		assert(this->valid());
	}

	// Construct range from a C-style array, ranges must have at least
	// tiles + 1 elements.
	Range(unsigned int* ranges, size_t tiles) :
		m_ranges(ranges, ranges + tiles + 1)
	{
		assert(this->valid());
	}

	// Copy constructor
	Range(const Range& rng) :
		m_ranges(rng.m_ranges)
	{}
	
	inline unsigned int
	tile_low(const unsigned int index) const
	{
		assert(index < this->m_ranges.size() - 1);
		return this->m_ranges[index];
	}

	inline unsigned int
	tile_high(const unsigned int index) const
	{
		assert(index < this->m_ranges.size() - 1);
		return this->m_ranges[index - 1];
	}
	
	inline const size_t
	tile_size(const unsigned int index) const
	{
		assert(index < this->m_ranges.size() - 1);
		return (this->m_ranges[index + 1] - this->m_ranges[index]);
	}
	
	inline const size_t
	tile_count() const
	{
		return (this->m_ranges.size() - 1);
	}

	inline const tile
	get_tile(const unsigned int index) const
	{
		assert(index < this->m_ranges.size() - 1);
		return tile(this->m_ranges[index], this->m_ranges[index + 1]);
	}

	inline unsigned int
	low() const
	{
		return this->m_ranges[0];
	}

	inline unsigned int
	high() const
	{
		return this->m_ranges[this->m_ranges.size() - 1];
	}

	inline unsigned int
	size() const
	{
		return (this->high() - this->low());
	}

	iterator
	begin()
	{
		return iterator(*this);
	}
	
	const_iterator
	begin() const
	{
		return const_iterator(*this);
	}

	iterator
	end()
	{
		return iterator(*this, -1);
	}

	const_iterator
	end() const
	{
		return const_iterator(*this, -1);
	}

}; // Range


#endif // RANGE_H__INCLUDED