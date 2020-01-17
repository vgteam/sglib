#include <bdsg/packed_structs.hpp>
#include <bits/types/__mbstate_t.h>
#include <ios>
#include <istream>
#include <ostream>
#include <sstream> // __str__
#include <streambuf>
#include <string>

#include <pybind11/pybind11.h>
#include <functional>
#include <string>
#include <pybind11/stl.h>
#include <fstream>


#ifndef BINDER_PYBIND11_TYPE_CASTER
	#define BINDER_PYBIND11_TYPE_CASTER
	PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>);
	PYBIND11_DECLARE_HOLDER_TYPE(T, T*);
	PYBIND11_MAKE_OPAQUE(std::shared_ptr<void>);
#endif

void bind_bdsg_packed_structs(std::function< pybind11::module &(std::string const &namespace_) > &M)
{
	{ // bdsg::PackedVector file:bdsg/packed_structs.hpp line:26
		pybind11::class_<bdsg::PackedVector, std::shared_ptr<bdsg::PackedVector>> cl(M("bdsg"), "PackedVector", "");
		cl.def( pybind11::init( [](){ return new bdsg::PackedVector(); } ) );
		cl.def( pybind11::init<class std::basic_istream<char> &>(), pybind11::arg("in") );

		cl.def( pybind11::init( [](bdsg::PackedVector const &o){ return new bdsg::PackedVector(o); } ) );
		cl.def("assign", (class bdsg::PackedVector & (bdsg::PackedVector::*)(const class bdsg::PackedVector &)) &bdsg::PackedVector::operator=, "Copy assignment operator\n\nC++: bdsg::PackedVector::operator=(const class bdsg::PackedVector &) --> class bdsg::PackedVector &", pybind11::return_value_policy::automatic, pybind11::arg("other"));
		cl.def("deserialize", (void (bdsg::PackedVector::*)(class std::basic_istream<char> &)) &bdsg::PackedVector::deserialize, "Clear current contents and load from contents in a stream\n\nC++: bdsg::PackedVector::deserialize(class std::basic_istream<char> &) --> void", pybind11::arg("in"));
		cl.def("serialize", (void (bdsg::PackedVector::*)(std::ostream &) const) &bdsg::PackedVector::serialize, "Output contents to a stream\n\nC++: bdsg::PackedVector::serialize(std::ostream &) const --> void", pybind11::arg("out"));
		cl.def("set", (void (bdsg::PackedVector::*)(const unsigned long &, const unsigned long &)) &bdsg::PackedVector::set, "Set the i-th value\n\nC++: bdsg::PackedVector::set(const unsigned long &, const unsigned long &) --> void", pybind11::arg("i"), pybind11::arg("value"));
		cl.def("get", (unsigned long (bdsg::PackedVector::*)(const unsigned long &) const) &bdsg::PackedVector::get, "Returns the i-th value\n\nC++: bdsg::PackedVector::get(const unsigned long &) const --> unsigned long", pybind11::arg("i"));
		cl.def("append", (void (bdsg::PackedVector::*)(const unsigned long &)) &bdsg::PackedVector::append, "Add a value to the end\n\nC++: bdsg::PackedVector::append(const unsigned long &) --> void", pybind11::arg("value"));
		cl.def("pop", (void (bdsg::PackedVector::*)()) &bdsg::PackedVector::pop, "Remove the last value\n\nC++: bdsg::PackedVector::pop() --> void");
		cl.def("resize", (void (bdsg::PackedVector::*)(const unsigned long &)) &bdsg::PackedVector::resize, "Either shrink the vector or grow the vector to the new size. New\n entries created by growing are filled with 0.\n\nC++: bdsg::PackedVector::resize(const unsigned long &) --> void", pybind11::arg("new_size"));
		cl.def("reserve", (void (bdsg::PackedVector::*)(const unsigned long &)) &bdsg::PackedVector::reserve, "If necessary, expand capacity so that the given number of entries can\n be included in the vector without reallocating. Never shrinks capacity.\n\nC++: bdsg::PackedVector::reserve(const unsigned long &) --> void", pybind11::arg("future_size"));
		cl.def("size", (unsigned long (bdsg::PackedVector::*)() const) &bdsg::PackedVector::size, "Returns the number of values.\n\nC++: bdsg::PackedVector::size() const --> unsigned long");
		cl.def("empty", (bool (bdsg::PackedVector::*)() const) &bdsg::PackedVector::empty, "Returns true if there are no entries and false otherwise.\n\nC++: bdsg::PackedVector::empty() const --> bool");
		cl.def("clear", (void (bdsg::PackedVector::*)()) &bdsg::PackedVector::clear, "Clears the backing vector.\n\nC++: bdsg::PackedVector::clear() --> void");
		cl.def("memory_usage", (unsigned long (bdsg::PackedVector::*)() const) &bdsg::PackedVector::memory_usage, "Reports the amount of memory consumed by this object in bytes.\n\nC++: bdsg::PackedVector::memory_usage() const --> unsigned long");
		cl.def("__eq__", (bool (bdsg::PackedVector::*)(const class bdsg::PackedVector &) const) &bdsg::PackedVector::operator==, "Returns true if the contents are identical (but not necessarily storage\n parameters, such as pointer to data, capacity, bit width, etc.).\n\nC++: bdsg::PackedVector::operator==(const class bdsg::PackedVector &) const --> bool", pybind11::arg("other"));
	}
	{ // bdsg::PagedVector file:bdsg/packed_structs.hpp line:105
		pybind11::class_<bdsg::PagedVector, std::shared_ptr<bdsg::PagedVector>> cl(M("bdsg"), "PagedVector", "");
		cl.def( pybind11::init<unsigned long>(), pybind11::arg("page_size") );

		cl.def( pybind11::init<class std::basic_istream<char> &>(), pybind11::arg("in") );

		cl.def("deserialize", (void (bdsg::PagedVector::*)(class std::basic_istream<char> &)) &bdsg::PagedVector::deserialize, "Clear current contents and load from contents in a stream\n\nC++: bdsg::PagedVector::deserialize(class std::basic_istream<char> &) --> void", pybind11::arg("in"));
		cl.def("serialize", (void (bdsg::PagedVector::*)(std::ostream &) const) &bdsg::PagedVector::serialize, "Output contents to a stream\n\nC++: bdsg::PagedVector::serialize(std::ostream &) const --> void", pybind11::arg("out"));
		cl.def("set", (void (bdsg::PagedVector::*)(const unsigned long &, const unsigned long &)) &bdsg::PagedVector::set, "Set the i-th value\n\nC++: bdsg::PagedVector::set(const unsigned long &, const unsigned long &) --> void", pybind11::arg("i"), pybind11::arg("value"));
		cl.def("get", (unsigned long (bdsg::PagedVector::*)(const unsigned long &) const) &bdsg::PagedVector::get, "Returns the i-th value\n\nC++: bdsg::PagedVector::get(const unsigned long &) const --> unsigned long", pybind11::arg("i"));
		cl.def("append", (void (bdsg::PagedVector::*)(const unsigned long &)) &bdsg::PagedVector::append, "Add a value to the end\n\nC++: bdsg::PagedVector::append(const unsigned long &) --> void", pybind11::arg("value"));
		cl.def("pop", (void (bdsg::PagedVector::*)()) &bdsg::PagedVector::pop, "Remove the last value\n\nC++: bdsg::PagedVector::pop() --> void");
		cl.def("resize", (void (bdsg::PagedVector::*)(const unsigned long &)) &bdsg::PagedVector::resize, "Either shrink the vector or grow the vector to the new size. New\n entries created by growing are filled with 0.\n\nC++: bdsg::PagedVector::resize(const unsigned long &) --> void", pybind11::arg("new_size"));
		cl.def("reserve", (void (bdsg::PagedVector::*)(const unsigned long &)) &bdsg::PagedVector::reserve, "If necessary, expand capacity so that the given number of entries can\n be included in the vector without reallocating. Never shrinks capacity.\n\nC++: bdsg::PagedVector::reserve(const unsigned long &) --> void", pybind11::arg("future_size"));
		cl.def("size", (unsigned long (bdsg::PagedVector::*)() const) &bdsg::PagedVector::size, "Returns the number of values\n\nC++: bdsg::PagedVector::size() const --> unsigned long");
		cl.def("empty", (bool (bdsg::PagedVector::*)() const) &bdsg::PagedVector::empty, "Returns true if there are no entries and false otherwise\n\nC++: bdsg::PagedVector::empty() const --> bool");
		cl.def("clear", (void (bdsg::PagedVector::*)()) &bdsg::PagedVector::clear, "Clears the backing vector\n\nC++: bdsg::PagedVector::clear() --> void");
		cl.def("page_width", (unsigned long (bdsg::PagedVector::*)() const) &bdsg::PagedVector::page_width, "Returns the page width of the vector\n\nC++: bdsg::PagedVector::page_width() const --> unsigned long");
		cl.def("memory_usage", (unsigned long (bdsg::PagedVector::*)() const) &bdsg::PagedVector::memory_usage, "Reports the amount of memory consumed by this object in bytes\n\nC++: bdsg::PagedVector::memory_usage() const --> unsigned long");
	}
	{ // bdsg::RobustPagedVector file:bdsg/packed_structs.hpp line:187
		pybind11::class_<bdsg::RobustPagedVector, std::shared_ptr<bdsg::RobustPagedVector>> cl(M("bdsg"), "RobustPagedVector", "");
		cl.def( pybind11::init<unsigned long>(), pybind11::arg("page_size") );

		cl.def( pybind11::init<class std::basic_istream<char> &>(), pybind11::arg("in") );

		cl.def("deserialize", (void (bdsg::RobustPagedVector::*)(class std::basic_istream<char> &)) &bdsg::RobustPagedVector::deserialize, "Clear current contents and load from contents in a stream\n\nC++: bdsg::RobustPagedVector::deserialize(class std::basic_istream<char> &) --> void", pybind11::arg("in"));
		cl.def("serialize", (void (bdsg::RobustPagedVector::*)(std::ostream &) const) &bdsg::RobustPagedVector::serialize, "Output contents to a stream\n\nC++: bdsg::RobustPagedVector::serialize(std::ostream &) const --> void", pybind11::arg("out"));
		cl.def("set", (void (bdsg::RobustPagedVector::*)(const unsigned long &, const unsigned long &)) &bdsg::RobustPagedVector::set, "Set the i-th value\n\nC++: bdsg::RobustPagedVector::set(const unsigned long &, const unsigned long &) --> void", pybind11::arg("i"), pybind11::arg("value"));
		cl.def("get", (unsigned long (bdsg::RobustPagedVector::*)(const unsigned long &) const) &bdsg::RobustPagedVector::get, "Returns the i-th value\n\nC++: bdsg::RobustPagedVector::get(const unsigned long &) const --> unsigned long", pybind11::arg("i"));
		cl.def("append", (void (bdsg::RobustPagedVector::*)(const unsigned long &)) &bdsg::RobustPagedVector::append, "Add a value to the end\n\nC++: bdsg::RobustPagedVector::append(const unsigned long &) --> void", pybind11::arg("value"));
		cl.def("pop", (void (bdsg::RobustPagedVector::*)()) &bdsg::RobustPagedVector::pop, "Remove the last value\n\nC++: bdsg::RobustPagedVector::pop() --> void");
		cl.def("resize", (void (bdsg::RobustPagedVector::*)(const unsigned long &)) &bdsg::RobustPagedVector::resize, "Either shrink the vector or grow the vector to the new size. New\n entries created by growing are filled with 0.\n\nC++: bdsg::RobustPagedVector::resize(const unsigned long &) --> void", pybind11::arg("new_size"));
		cl.def("reserve", (void (bdsg::RobustPagedVector::*)(const unsigned long &)) &bdsg::RobustPagedVector::reserve, "If necessary, expand capacity so that the given number of entries can\n be included in the vector without reallocating. Never shrinks capacity.\n\nC++: bdsg::RobustPagedVector::reserve(const unsigned long &) --> void", pybind11::arg("future_size"));
		cl.def("size", (unsigned long (bdsg::RobustPagedVector::*)() const) &bdsg::RobustPagedVector::size, "Returns the number of values\n\nC++: bdsg::RobustPagedVector::size() const --> unsigned long");
		cl.def("empty", (bool (bdsg::RobustPagedVector::*)() const) &bdsg::RobustPagedVector::empty, "Returns true if there are no entries and false otherwise\n\nC++: bdsg::RobustPagedVector::empty() const --> bool");
		cl.def("clear", (void (bdsg::RobustPagedVector::*)()) &bdsg::RobustPagedVector::clear, "Clears the backing vector\n\nC++: bdsg::RobustPagedVector::clear() --> void");
		cl.def("page_width", (unsigned long (bdsg::RobustPagedVector::*)() const) &bdsg::RobustPagedVector::page_width, "Returns the page width of the vector\n\nC++: bdsg::RobustPagedVector::page_width() const --> unsigned long");
		cl.def("memory_usage", (unsigned long (bdsg::RobustPagedVector::*)() const) &bdsg::RobustPagedVector::memory_usage, "Reports the amount of memory consumed by this object in bytes\n\nC++: bdsg::RobustPagedVector::memory_usage() const --> unsigned long");
	}
	{ // bdsg::PackedDeque file:bdsg/packed_structs.hpp line:259
		pybind11::class_<bdsg::PackedDeque, std::shared_ptr<bdsg::PackedDeque>> cl(M("bdsg"), "PackedDeque", "");
		cl.def( pybind11::init( [](){ return new bdsg::PackedDeque(); } ) );
		cl.def( pybind11::init<class std::basic_istream<char> &>(), pybind11::arg("in") );

		cl.def("deserialize", (void (bdsg::PackedDeque::*)(class std::basic_istream<char> &)) &bdsg::PackedDeque::deserialize, "Clear current contents and load from contents in a stream\n\nC++: bdsg::PackedDeque::deserialize(class std::basic_istream<char> &) --> void", pybind11::arg("in"));
		cl.def("serialize", (void (bdsg::PackedDeque::*)(std::ostream &) const) &bdsg::PackedDeque::serialize, "Output contents to a stream\n\nC++: bdsg::PackedDeque::serialize(std::ostream &) const --> void", pybind11::arg("out"));
		cl.def("set", (void (bdsg::PackedDeque::*)(const unsigned long &, const unsigned long &)) &bdsg::PackedDeque::set, "Set the i-th value\n\nC++: bdsg::PackedDeque::set(const unsigned long &, const unsigned long &) --> void", pybind11::arg("i"), pybind11::arg("value"));
		cl.def("get", (unsigned long (bdsg::PackedDeque::*)(const unsigned long &) const) &bdsg::PackedDeque::get, "Returns the i-th value\n\nC++: bdsg::PackedDeque::get(const unsigned long &) const --> unsigned long", pybind11::arg("i"));
		cl.def("append_front", (void (bdsg::PackedDeque::*)(const unsigned long &)) &bdsg::PackedDeque::append_front, "Add a value to the front\n\nC++: bdsg::PackedDeque::append_front(const unsigned long &) --> void", pybind11::arg("value"));
		cl.def("append_back", (void (bdsg::PackedDeque::*)(const unsigned long &)) &bdsg::PackedDeque::append_back, "Add a value to the back\n\nC++: bdsg::PackedDeque::append_back(const unsigned long &) --> void", pybind11::arg("value"));
		cl.def("pop_front", (void (bdsg::PackedDeque::*)()) &bdsg::PackedDeque::pop_front, "Remove the front value\n\nC++: bdsg::PackedDeque::pop_front() --> void");
		cl.def("pop_back", (void (bdsg::PackedDeque::*)()) &bdsg::PackedDeque::pop_back, "Remove the back value\n\nC++: bdsg::PackedDeque::pop_back() --> void");
		cl.def("reserve", (void (bdsg::PackedDeque::*)(const unsigned long &)) &bdsg::PackedDeque::reserve, "If necessary, expand capacity so that the given number of entries can\n be included in the deque without reallocating. Never shrinks capacity.\n\nC++: bdsg::PackedDeque::reserve(const unsigned long &) --> void", pybind11::arg("future_size"));
		cl.def("size", (unsigned long (bdsg::PackedDeque::*)() const) &bdsg::PackedDeque::size, "Returns the number of values\n\nC++: bdsg::PackedDeque::size() const --> unsigned long");
		cl.def("empty", (bool (bdsg::PackedDeque::*)() const) &bdsg::PackedDeque::empty, "Returns true if there are no entries and false otherwise\n\nC++: bdsg::PackedDeque::empty() const --> bool");
		cl.def("clear", (void (bdsg::PackedDeque::*)()) &bdsg::PackedDeque::clear, "Empty the contents\n\nC++: bdsg::PackedDeque::clear() --> void");
		cl.def("memory_usage", (unsigned long (bdsg::PackedDeque::*)() const) &bdsg::PackedDeque::memory_usage, "Reports the amount of memory consumed by this object in bytes.\n\nC++: bdsg::PackedDeque::memory_usage() const --> unsigned long");
	}
}
