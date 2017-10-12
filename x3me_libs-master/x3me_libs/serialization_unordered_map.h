# pragma once 

#include <unordered_map>

#include <boost/config.hpp> 

#include <boost/serialization/hash_collections_save_imp.hpp>
#include <boost/serialization/collections_load_imp.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/utility.hpp> 

#include "hash_collections_load_imp.h"

namespace boost 
{ 
namespace serialization 
{ 

template <class Archive, class Type, class Key, 
          class Hash, class Compare, class Allocator> 
inline
void save(Archive& ar, 
	      const std::unordered_map<Key, Type, Hash, Compare, Allocator>& t, 
		  const unsigned int /*version*/)
{ 
    boost::serialization::stl::save_hash_collection<Archive, 
        std::unordered_map<Key, Type, Hash, Compare, Allocator>>(ar, t); 
} 

template <class Archive, class Type, class Key, 
          class Hash, class Compare, class Allocator> 
inline 
void load(Archive& ar, 
		  std::unordered_map<Key, Type, Hash, Compare, Allocator>& t, 
		  const unsigned int /*version*/) 
{
    boost::serialization::stl::load_hash_collection<Archive, 
	    std::unordered_map<Key, Type, Hash, Compare, Allocator>, 
		boost::serialization::stl::archive_input_map<Archive, 
            std::unordered_map<Key, Type, Hash, Compare, Allocator>>>(ar, t);
} 

template <class Archive, class Type, class Key, 
          class Hash, class Compare, class Allocator> 
inline 
void serialize(Archive& ar,
               std::unordered_map<Key, Type, Hash, Compare, Allocator>& t, 
			   const unsigned int version) 
{
	boost::serialization::split_free(ar, t, version); 
} 

template <class Archive, class Type, class Key, 
          class Hash, class Compare, class Allocator > 
inline 
void save(Archive& ar,
		  const std::unordered_multimap<Key, Type, Hash, 
                                        Compare, Allocator>& t, 
          const unsigned int/* version*/) 
{
	boost::serialization::stl::save_hash_collection<Archive,
        std::unordered_multimap<Key, Type, Hash, Compare, Allocator>>(ar, t);
}

template <class Archive, class Type, class Key, 
          class Hash, class Compare, class Allocator> 
inline 
void load(Archive& ar, 
		  std::unordered_multimap<Key, Type, Hash, Compare, Allocator>& t, 
		  const unsigned int /*version*/) 
{
	boost::serialization::stl::load_hash_collection<Archive, 
		std::unordered_multimap<Key, Type, Hash, Compare, Allocator>, 
		boost::serialization::stl::archive_input_map<Archive, 
            std::unordered_multimap<Key, Type, Hash, 
                Compare, Allocator>>>(ar, t); 
}

template <class Archive, class Type, class Key,
          class Hash, class Compare, class Allocator> 
inline 
void serialize(Archive& ar,
			   std::unordered_multimap<Key, Type, Hash, Compare, Allocator>& t, 
			   const unsigned int version) 
{
	boost::serialization::split_free(ar, t, version); 
}

} // namespace serialization 
} // namespace boost
