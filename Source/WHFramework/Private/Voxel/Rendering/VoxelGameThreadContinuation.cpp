#include "Voxel/Rendering/VoxelGameThreadContinuation.h"

// 保留同步驱动边界，避免 MSVC 内联嵌套协程时触发 C1001；微步执行顺序不变。
FORCENOINLINE void FVoxelGameThreadContinuation::RunToCompletion()
{
	while (!IsComplete())
	{
		Resume();
	}
}
