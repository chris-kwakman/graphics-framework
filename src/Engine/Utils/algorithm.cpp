#include "algorithm.h"
#include <utility>

namespace Engine {
namespace Utils {

std::pair<int, int> float_binary_search(float const* _array, size_t _array_size, float _value)
{
	int idx_min = 0;
	int idx_max = (int)_array_size-1;
	do
	{
		int idx_mid = (idx_max + idx_min) / 2;
		if (_value < _array[idx_mid])
			idx_max = idx_mid;
		else
			idx_min = idx_mid;
	} while (idx_max - idx_min > 1);

	return { idx_min, idx_max };
}

}
}