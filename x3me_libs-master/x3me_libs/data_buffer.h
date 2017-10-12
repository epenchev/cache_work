#pragma once

namespace x3me
{
namespace utilities
{

#define CHECK_BUFFER_STATE(db)	\
	assert((db->m_buffer && (db->m_data_size != 0) && (db->m_buffer_size != 0) && (db->m_data_size <= db->m_buffer_size)) || (!db->m_buffer && (db->m_data_size == 0) && (db->m_data_size == 0)))

// deprecated functionality
class data_buffer
{
private:
	char*	m_buffer;
	size_t	m_data_size;
	size_t	m_buffer_size;

private:
	data_buffer(const data_buffer&);
	data_buffer& operator =(const data_buffer&);

	void move_data_buffer(data_buffer&& other)
	{
		CHECK_BUFFER_STATE((&other));

		X3ME_DELETE_ARRAY(m_buffer);

		m_buffer		= other.m_buffer;
		m_data_size		= other.m_data_size;
		m_buffer_size	= other.m_buffer_size;

		other.m_buffer		= nullptr;
		other.m_data_size	= 0;
		other.m_buffer_size	= 0;
	}

public:
	data_buffer()
	: m_buffer(nullptr)
	, m_data_size(0)
	, m_buffer_size(0)
	{
	}

	explicit data_buffer(size_t buffer_size)
	: m_buffer(X3ME_NEW_ARRAY(char, buffer_size))
	, m_data_size(0)
	, m_buffer_size(buffer_size)
	{
		assert(buffer_size != 0);
	}

	data_buffer(data_buffer&& other)
	{
		move_data_buffer(std::move(other));
	}

	data_buffer& operator =(data_buffer&& other)
	{
		if(this != &other)
		{
			move_data_buffer(std::move(other));
		}
		return *this;
	}

	~data_buffer()
	{
		clear();
	}

	void reset(size_t buffer_size)
	{
		CHECK_BUFFER_STATE(this);
		assert(buffer_size != 0);

		X3ME_DELETE_ARRAY(m_buffer);

		m_buffer		= X3ME_NEW_ARRAY(char, buffer_size);
		m_data_size		= 0;
		m_buffer_size	= buffer_size;
	}

	void clear()
	{
		X3ME_DELETE_ARRAY(m_buffer);
		m_buffer		= nullptr;
		m_data_size		= 0;
		m_buffer_size	= 0;
	}

	bool set_data_size(size_t new_data_size)
	{
		if(new_data_size > m_buffer_size)
		{
			assert(false);
			return false;
		}
		m_data_size = new_data_size;
		return true;
	}
	bool increase_data_size(size_t with_size)
	{
		auto new_data_size = m_data_size + with_size;
		return set_data_size(new_data_size);
	}
	bool full() const
	{
		return ((m_buffer_size > 0) && (m_data_size >= m_buffer_size)); // m_data_size > m_buffer_size is bug, but still
	}
	bool allocated() const
	{
		return !!m_buffer;
	}

	char* data_begin()
	{
		return m_buffer;
	}
	const char* data_begin() const
	{
		return m_buffer;
	}
	char* data_end()
	{
		return m_buffer + m_data_size;
	}
	const char* data_end() const
	{
		return m_buffer + m_data_size;
	}

	char* get_buffer()
	{
		return m_buffer;
	}
	const char* get_buffer() const
	{
		return m_buffer;
	}

	char* get_data()
	{
		return m_buffer;
	}
	const char* get_data() const
	{
		return m_buffer;
	}

	char* get_empty_space()
	{
		assert(m_buffer && (m_data_size <= m_buffer_size));
		return (m_buffer + m_data_size);
	}
	size_t get_empty_space_size()
	{
		assert(m_data_size <= m_buffer_size);
		return (m_buffer_size - m_data_size);
	}

	char* get_data_at_offset(size_t offset)
	{
		assert(m_buffer && (offset < m_buffer_size));
		return m_buffer + offset;
	}
	const char* get_data_at_offset(size_t offset) const
	{
		assert(m_buffer && (offset < m_buffer_size));
		return m_buffer + offset;
	}

	size_t get_data_size() const
	{
		return m_data_size;
	}

	size_t get_buffer_size() const
	{
		return m_buffer_size;
	}
};

#undef CHECK_BUFFER_STATE

// deprecated functionality
// will be removed when stats_phase1 branch of P3 is removed
template<typename T>
class temp_buffer
{
public:
	typedef T data_t;

private:
	data_t*	m_data;
	size_t	m_size;

public:
	temp_buffer() : m_data(nullptr), m_size(0) {}
	temp_buffer(const data_t* data, size_t size) : m_data(nullptr)
	{
		reset(data, size);
	}
	temp_buffer(temp_buffer&& tb) : m_data(tb.m_data), m_size(tb.m_size) 
	{
		tb.m_data	= nullptr;
		tb.m_size	= 0;
	}
	~temp_buffer()
	{
		X3ME_DELETE_ARRAY(m_data);
	}
	temp_buffer& operator =(temp_buffer&& tb)
	{
		if(this != &tb)
		{
			X3ME_DELETE_ARRAY(m_data);
			m_data		= tb.m_data;
			m_size		= tb.m_size;
			tb.m_data	= nullptr;
			tb.m_size	= 0;
		}
		return *this;
	}

	void reset(const data_t* data, size_t size)
	{
		X3ME_DELETE_ARRAY(m_data);
		m_data	= nullptr;
		m_size	= 0;
		if(size > 0)
		{
			assert(data);
			m_data	= X3ME_NEW_ARRAY(data_t, size);
			m_size	= size;
			::memcpy(m_data, data, m_size);
		}
	}

	data_t& operator [](size_t idx) { assert(idx < m_size); return *(m_data + idx); }
	const data_t& operator [](size_t idx) const { assert(idx < m_size); return *(m_data + idx); }

	data_t* begin() { return m_data;}
	const data_t* cbegin() const { return m_data;}
	data_t* end() { return m_data+m_size;}
	const data_t* cend() const { return m_data+m_size;}
	data_t* data() { return m_data; }
	const data_t* data() const { return m_data; }
	size_t size() const { return m_size; }
	bool empty() const { return (m_size == 0); }

private:
	temp_buffer(const temp_buffer&); // enable them if needed
	temp_buffer& operator =(const temp_buffer&);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO_ buffers should have allocator support some day, when needed
template<typename T> class pod_temp_buffer;
template<typename T>
class pod_buffer // this buffer will replace the above deprecated data_buffer
{
	template<typename U> friend class pod_temp_buffer;
public:
	typedef T data_t;

private:
	data_t*	m_buffer;
	size_t	m_data_size;
	size_t	m_buffer_size;

public:
	pod_buffer()
	: m_buffer(nullptr)
	, m_data_size(0)
	, m_buffer_size(0)
	{
	}
	explicit pod_buffer(size_t buffer_size)
	: m_buffer(X3ME_NEW_ARRAY(data_t, buffer_size))
	, m_data_size(0)
	, m_buffer_size(buffer_size)
	{
	}
	pod_buffer(pod_buffer&& pb)
	: m_buffer(pb.m_buffer)
	, m_data_size(pb.m_data_size)
	, m_buffer_size(pb.m_buffer_size)
	{
		pb.m_buffer			= nullptr;
		pb.m_data_size		= 0;
		pb.m_buffer_size	= 0;

	}
	explicit pod_buffer(pod_temp_buffer<T>&& ptb)
	: m_buffer(ptb.m_data)
	, m_data_size(ptb.m_size)
	, m_buffer_size(ptb.m_size)
	{
		ptb.m_data	= nullptr;
		ptb.m_size	= 0;
	}
	~pod_buffer()
	{
		X3ME_DELETE_ARRAY(m_buffer);
		// the name of this class is not correct, because it is supposed to work with trivial data, not with POD data
		// but this name is more shorter, and the difference is not so big between the two data groups
		static_assert(std::is_trivial<T>::value, "this class works only with trivial data types");
	}

	pod_buffer& operator =(pod_buffer&& pb)
	{
		if(this != &pb)
		{
			X3ME_DELETE_ARRAY(m_buffer);

			m_buffer		= pb.m_buffer;
			m_data_size		= pb.m_data_size;
			m_buffer_size	= pb.m_buffer_size;

			pb.m_buffer			= nullptr;
			pb.m_data_size		= 0;
			pb.m_buffer_size	= 0;
		}
		return *this;
	}
	pod_buffer& operator =(pod_temp_buffer<T>&& ptb)
	{
		X3ME_DELETE_ARRAY(m_buffer);

		m_buffer		= ptb.m_data;
		m_data_size		= ptb.m_size;
		m_buffer_size	= ptb.m_size;

		ptb.m_data		= nullptr;
		ptb.m_size		= 0;
		
		return *this;
	}

	void reset(size_t buffer_size)
	{
		X3ME_DELETE_ARRAY(m_buffer);
		m_buffer		= nullptr;
		m_data_size		= 0;
		m_buffer_size	= 0;
		if(buffer_size > 0)
		{
			m_buffer		= X3ME_NEW_ARRAY(data_t, buffer_size);
			m_buffer_size	= buffer_size;
		}
	}

	data_t* buffer()						{ return m_buffer; }
	data_t* data()							{ return m_buffer; }
	const data_t* data() const				{ return m_buffer; }
	data_t* free_space()					{ assert(m_buffer && (m_data_size <= m_buffer_size)); return (m_buffer + m_data_size); }

	data_t* data_at_offset(size_t offset)				{ assert(m_buffer && (offset < m_data_size)); return (m_buffer + offset); }
	const data_t* data_at_offset(size_t offset) const	{ assert(m_buffer && (offset < m_data_size)); return (m_buffer + offset); }

	void data_size(size_t new_data_size)
	{
		assert(new_data_size <= m_buffer_size);
		m_data_size = new_data_size;
	}
	void increase_data_size(size_t with_size)
	{
		data_size(m_data_size + with_size);
	}

	size_t data_size() const { return m_data_size; }
	size_t buffer_size() const { return m_buffer_size; }
	size_t free_size() const { assert(m_data_size <= m_buffer_size); return (m_buffer_size - m_data_size); }

	bool allocated() const { return !!m_buffer; }
	bool full() const { return ((m_buffer_size > 0) && (m_data_size >= m_buffer_size)); } // m_data_size > m_buffer_size is bug, but still

private:
	pod_buffer(const pod_buffer&);
	pod_buffer& operator =(const pod_buffer&);
};

// NOTE_ these functions are unsafe, the outer side must take care for correct usage
// all of them will only assert when used with incorrect input parameters
template<typename T>
void copy_to_buffer(pod_buffer<T>& db, const T* data, size_t size)
{
	::memcpy(db.buffer(), data, size);
	db.data_size(size);
}
template<typename T>
void copy_add_to_buffer(pod_buffer<T>& db, const T* data, size_t size)
{
	::memcpy(db.free_space(), data, size);
	db.increase_data_size(size);
}
template<typename T>
void memmove_buffer_end_data(pod_buffer<T>& db, size_t remaining_data_size)
{
	auto remaining_data_offset = db.data_size() - remaining_data_size;
	::memmove(db.buffer(), db.data_at_offset(remaining_data_offset), remaining_data_size);
	db.data_size(remaining_data_size);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
class pod_temp_buffer
{
	template<typename U> friend class pod_buffer;
public:
	typedef T data_t;

private:
	data_t*	m_data;
	size_t	m_size;

public:
	pod_temp_buffer() : m_data(nullptr), m_size(0) 
	{
	}
	explicit pod_temp_buffer(size_t size) 
	: m_data(X3ME_NEW_ARRAY(data_t, size))
	, m_size(size)
	{
	}
	pod_temp_buffer(const data_t* data, size_t size) : m_data(nullptr)
	{
		reset(data, size);
	}
	pod_temp_buffer(pod_temp_buffer&& tb) : m_data(tb.m_data), m_size(tb.m_size) 
	{
		tb.m_data	= nullptr;
		tb.m_size	= 0;
	}
	// space can be loosed here, because pod_buffer buffer_size can be bigger, but the outside should think about this
	explicit pod_temp_buffer(pod_buffer<T>&& pb) : m_data(pb.m_buffer), m_size(pb.m_data_size)
	{
		pb.m_buffer			= nullptr;
		pb.m_data_size		= 0;
		pb.m_buffer_size	= 0;
	}
	~pod_temp_buffer()
	{
		X3ME_DELETE_ARRAY(m_data);
		// the name of this class is not correct, because it is supposed to work with trivial data, not with POD data
		// but this name is more shorter, and the difference is not so big between the two data groups
		static_assert(std::is_trivial<T>::value, "this class works only with trivial data types");
	}

	pod_temp_buffer& operator =(pod_temp_buffer&& tb)
	{
		if(this != &tb)
		{
			X3ME_DELETE_ARRAY(m_data);
			m_data		= tb.m_data;
			m_size		= tb.m_size;
			tb.m_data	= nullptr;
			tb.m_size	= 0;
		}
		return *this;
	}
	// space can be lost here, because pod_buffer buffer_size can be bigger, but the outside should think about this
	pod_temp_buffer& operator =(pod_buffer<T>&& pb)
	{
		X3ME_DELETE_ARRAY(m_data);
		m_data				= pb.m_buffer;
		m_size				= pb.m_data_size;
		pb.m_buffer			= nullptr;
		pb.m_data_size		= 0;
		pb.m_buffer_size	= 0;
		return *this;
	}

	void reset(size_t size)
	{
		X3ME_DELETE_ARRAY(m_data);
		m_data	= nullptr;
		m_size	= 0;
		if(size > 0)
		{
			m_data	= X3ME_NEW_ARRAY(data_t, size);
			m_size	= size;
		}
	}

	void reset(const data_t* data, size_t size)
	{
		X3ME_DELETE_ARRAY(m_data);
		m_data	= nullptr;
		m_size	= 0;
		if(size > 0)
		{
			assert(data);
			m_data	= X3ME_NEW_ARRAY(data_t, size);
			m_size	= size;
			::memcpy(m_data, data, m_size);
		}
	}

	data_t& operator [](size_t idx) { assert(idx < m_size); return *(m_data + idx); }
	const data_t& operator [](size_t idx) const { assert(idx < m_size); return *(m_data + idx); }

	data_t* begin() { return m_data;}
	const data_t* cbegin() const { return m_data;}
	data_t* end() { return m_data+m_size;}
	const data_t* cend() const { return m_data+m_size;}
	data_t* data() { return m_data; }
	const data_t* data() const { return m_data; }
	size_t size() const { return m_size; }
	bool empty() const { return (m_size == 0); }

private:
	pod_temp_buffer(const pod_temp_buffer&);
	pod_temp_buffer& operator =(const pod_temp_buffer&);
};

} // namespace utilities
} // namespace x3me
