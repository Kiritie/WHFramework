#pragma once

// 临时关闭所有地形接缝生成；恢复时应整体恢复接缝路径。
namespace VoxelSeamPolicy
{
	inline bool IsEnabled()
	{
		return false;
	}
}
