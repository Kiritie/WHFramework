#pragma once
#include "CoreMinimal.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
#include "Voxel/Interaction/VoxelEditTypes.h"
#include "VoxelNetworkTypes.generated.h"
class UPackageMap;
USTRUCT()
struct WHFRAMEWORK_API FVoxelRPCPacket
{
    GENERATED_BODY()
    TArray<uint8> Bytes;
    bool NetSerialize(FArchive& Ar,UPackageMap* Map,bool& bOutSuccess);
};
template<>struct TStructOpsTypeTraits<FVoxelRPCPacket>:TStructOpsTypeTraitsBase2<FVoxelRPCPacket>
{enum{WithNetSerializer=true};};
enum class EVoxelMessage:uint8{Hello=1,Welcome,Ready,Interest,Snapshots,Ack,Edit,Reply,Resync,Closed,Inventory,ProxyRequest=12,ProxyReply=13,ProxyInvalidate=14};
struct WHFRAMEWORK_API FVoxelWireMessage
{
    EVoxelMessage Kind=EVoxelMessage::Closed;FGuid Session;
    TArray<uint8> Payload;
};
struct WHFRAMEWORK_API FVoxelSnapshotBatch
{
    FGuid Id;
    TArray<FVoxelSectionOverlay> Sections;
};
