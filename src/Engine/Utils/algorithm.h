#ifndef ENGINE_UTILS_ALGORITHM
#define ENGINE_UTILS_ALGORITHM

#include <utility>

namespace Engine {
namespace Utils
{

	/*
	* Helper method to perform binary search for containing bracket in float value array
	* @param	float *		Pointer to floating value array with keys to search through
	* @param	size_t		Size of array
	* @param	float		Key value to search for
	* @param	std::pair<int,int>	Indices that contain the given value to search for.
	*/
	std::pair<int, int> float_binary_search(float const* _array, size_t _array_size, float _value);

}
}
#endif // !ENGINE_UTILS_ALGORITHM
