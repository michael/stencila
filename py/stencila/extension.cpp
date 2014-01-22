#include "extension.hpp"

#include "exception.cpp"
#include "package.cpp"

// Define converters
template<typename Type>
struct vector_to_list {
	static PyObject* convert(const std::vector<Type>& vec) {
		list* l = new list();
		for(size_t i = 0; i < vec.size(); i++) (*l).append(vec[i]);
		return l->ptr();
	}
};

BOOST_PYTHON_MODULE(extension){
	using namespace bp;

	// Declare converters
	to_python_converter<std::vector<std::string>, vector_to_list<std::string>>();

	// Declare exception translation and general Stencila functions
	def_Exception();
	def("version",Stencila_version);

	// Declare classes
    def_Package();
}