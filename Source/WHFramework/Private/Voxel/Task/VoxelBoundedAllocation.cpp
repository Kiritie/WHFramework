#include "Voxel/Task/VoxelBoundedAllocation.h"

#include "HAL/MemoryBase.h"
#include "HAL/PlatformMemory.h"

uint64 VoxelBoundedAllocation::QuantizeBytes(const uint64 InBytes, const uint32 InAlignment)
{
	if (!InBytes) return 0;
#if PLATFORM_WINDOWS && PLATFORM_64BITS
	// UE 5.8 的 FMallocMimalloc 未实现 QuantizeSize。按该 SDK 的 mimalloc 2.0
	// page-queue.c、os.c、alloc-aligned.c 计算容量上界，不先分配探测块。
	// https://github.com/microsoft/mimalloc/tree/v2.0.0/src
	if (GMalloc && FCString::Strcmp(GMalloc->GetDescriptiveName(), TEXT("Mimalloc")) == 0)
	{
		const uint64 Alignment = FMath::Max<uint64>(InAlignment, InBytes >= 16 ? 16 : 8);
		if (!FMath::IsPowerOfTwo(Alignment) || InBytes > uint64(MAX_int64) - Alignment) return MAX_uint64;
		// free-list 命中可直接分配；慢路径最多额外请求 alignment-1 字节。
		const uint64 Requested = InBytes + Alignment - 1;
		if (Requested <= 128 * 1024)
		{
			const uint64 Words = (Requested + 7) / 8;
			if (Words <= 1) return 8;
			const uint64 Quantum = Words <= 8 ? 2 : uint64(1) << (FMath::FloorLog2(Words - 1) - 2);
			return Align(Words, Quantum) * 8;
		}
		const uint64 Quantum = Requested < 512 * 1024 ? FPlatformMemory::GetConstants().PageSize :
			Requested < 2 * 1024 * 1024 ? 64 * 1024 : Requested < 8 * 1024 * 1024 ? 256 * 1024 :
			Requested < 32 * 1024 * 1024 ? 1024 * 1024 : 4 * 1024 * 1024;
		return Requested > MAX_uint64 - Quantum ? MAX_uint64 : Align(Requested, Quantum);
	}
#endif
	return FMemory::QuantizeSize(InBytes, InAlignment);
}
