
#include <iostream>

namespace Engine {
namespace Utils {

	template<typename ... TArgs>
	void print_base(const char* _prefix, const char* _format, TArgs ... _args)
	{
		char buffer[512];
		snprintf(buffer, sizeof(buffer), _format, _args...);
		buffer[511] = 0;
		std::cerr << "[" << _prefix << "] " << buffer << std::endl;
	}

	template<typename ... TArgs>
	void print_info(const char* _format, TArgs ... _args)
	{
		print_base("Info", _format, _args...);
	}

	template<typename ... TArgs>
	void print_debug(const char* _format, TArgs ... _args)
	{
		print_base("Debug", _format, _args...);
	}

	template<typename ... TArgs>
	void print_warning(const char* _format, TArgs ... _args)
	{
		print_base("Warning", _format, _args...);
	}

	template<typename ... TArgs>
	void print_error(const char* _format, TArgs ... _args)
	{
		print_base("ERROR", _format, _args...);
	}

	template<typename ... TArgs>
	void assert_print_error(bool _assert_result, const char* _format, TArgs ... _args)
	{
		if (!_assert_result)
		{
			print_error(_format, _args...);
			assert(false);
		}
	}

}
}