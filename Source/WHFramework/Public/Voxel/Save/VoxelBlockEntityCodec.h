#pragma once

#include "CoreMinimal.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

class WHFRAMEWORK_API FVoxelBlockEntityCodec
{
public:
    // 默认实体只保存生成标记；完整持久实体使用 DeltaCodec 的独立上限。
    static constexpr int32 MaximumDefaultPayloadBytes = 32 * 1024;

    using FMakeDefault = bool (*)(FVoxelBlockEntityState&, uint8);
    // Worker 验证只读有界字节，不创建名称、资产或可变库存。
    using FValidate = bool (*)(const FVoxelBlockEntityState&);

    static bool Register(uint16 InKind, FMakeDefault InMakeDefault, FValidate InValidate);
    static bool IsRegistered(uint16 InKind);
    static bool MakeDefault(uint16 InKind, FVoxelBlockEntityState& OutState, uint8 InVariant = 0);
    static bool Validate(const FVoxelBlockEntityState& InState);
    static bool IncrementCounter(const FVoxelBlockEntityState& InState, FVoxelBlockEntityState& OutState);
};
