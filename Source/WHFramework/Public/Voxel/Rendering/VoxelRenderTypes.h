#pragma once
#include "CoreMinimal.h"
#include "VoxelRenderTypes.generated.h"
class UTexture2D;
UENUM(BlueprintType)
enum class EVoxelRenderGroup:uint8 { Opaque,Masked,Foliage,Water,Translucent,Emissive };
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelFaceTexture
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Texture;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1",ClampMax="256")) int32 FrameCount=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="60")) int32 FramesPerSecond=0;
};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelFaceMaterialSet
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVoxelFaceTexture PositiveX;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVoxelFaceTexture NegativeX;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVoxelFaceTexture PositiveY;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVoxelFaceTexture NegativeY;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVoxelFaceTexture Top;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FVoxelFaceTexture Bottom;
    const FVoxelFaceTexture& Get(uint8 Face)const;
};
struct WHFRAMEWORK_API FVoxelRuntimeFaceRef
{
    uint16 Bank=0,Layer=0,Frames=1,FPS=0;
    bool operator==(const FVoxelRuntimeFaceRef&B)const
    {return Bank==B.Bank&&Layer==B.Layer&&Frames==B.Frames&&FPS==B.FPS;}
};
