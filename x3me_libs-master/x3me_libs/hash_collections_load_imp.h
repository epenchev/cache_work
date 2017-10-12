# pragma once

// The file is get from 
// boost/serialization/hash_collections_load_imp.hpp
// and has some fixes
#include <boost/config.hpp>
#include <boost/archive/detail/basic_iarchive.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/collection_size_type.hpp>
#include <boost/serialization/item_version_type.hpp>

namespace boost
{
namespace serialization 
{
namespace stl 
{

//////////////////////////////////////////////////////////////////////
// implementation of serialization for STL containers

template <class Archive, class Container, class InputFunction>
inline void load_hash_collection(Archive& ar, Container& s)
{
    s.clear();
    collection_size_type count;
    collection_size_type bucket_count;
    boost::serialization::item_version_type item_version(0);
    boost::archive::library_version_type library_version(
        ar.get_library_version()
    );
    // retrieve number of elements
    if (boost::archive::library_version_type(6) != library_version)
    {
        ar >> BOOST_SERIALIZATION_NVP(count);
        ar >> BOOST_SERIALIZATION_NVP(bucket_count);
    }
    else
    {   // Note fix-up for error in version 6.  Collection size was
        // changed to size_t BUT for hashed collections it was implemented
        // as an unsigned int.  
        // This should be a problem only on win64 machines
        // but I'll leave it for everyone just in case.
        unsigned int c;
        unsigned int bc;
        ar >> BOOST_SERIALIZATION_NVP(c);
        count = c;
        ar >> BOOST_SERIALIZATION_NVP(bc);
        bucket_count = bc;
    }
    if (boost::archive::library_version_type(3) < library_version)
    {
        ar >> BOOST_SERIALIZATION_NVP(item_version);
    }
    InputFunction ifunc;
	BOOST_DEDUCED_TYPENAME Container::iterator hint;
    while (count-- > 0)
    {
        ifunc(ar, s, item_version, hint);
    }
}

} // namespace stl 
} // namespace serialization
} // namespace boost
