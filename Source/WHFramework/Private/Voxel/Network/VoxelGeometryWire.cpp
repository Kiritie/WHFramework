#include "Voxel/Network/VoxelGeometryWire.h"

namespace
{
	void WriteGeometryUnsigned(TArray<uint8>& OutBytes, const uint64 InValue, const int32 InWidth)
	{
		for (int32 Index = 0; Index < InWidth; ++Index)
		{
			OutBytes.Add(static_cast<uint8>((InValue >> (Index * 8)) & 0xff));
		}
	}

	uint64 ReadGeometryUnsigned(TConstArrayView<uint8> InBytes, int32& InOutPosition, const int32 InWidth)
	{
		uint64 Value = 0;
		for (int32 Index = 0; Index < InWidth; ++Index)
		{
			Value |= uint64(InBytes[InOutPosition++]) << (Index * 8);
		}
		return Value;
	}

	int32 ReadGeometrySigned(TConstArrayView<uint8> InBytes, int32& InOutPosition)
	{
		const uint64 Bits = ReadGeometryUnsigned(InBytes, InOutPosition, 4);
		return static_cast<int32>(Bits <= uint64(MAX_int32) ? int64(Bits) : int64(Bits) - (int64(1) << 32));
	}
}

bool VoxelGeometryWire::Encode(const FVoxelGeometryWireHeader& InHeader, TArray<uint8>& OutBytes, FString& OutError)
{
	if (!InHeader.Geometry.Validate(OutError))
	{
		return false;
	}
	if (InHeader.RequestId == 0 || InHeader.Geometry.GeneratorIdentity == 0)
	{
		OutError = TEXT("Representation request and generation identities must be valid");
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Reserve(HeaderBytes);
	WriteGeometryUnsigned(Bytes, 0x36524756, 4);
	WriteGeometryUnsigned(Bytes, Version, 2);
	WriteGeometryUnsigned(Bytes, static_cast<uint8>(InHeader.Geometry.Key.Representation), 1);
	WriteGeometryUnsigned(Bytes, InHeader.Geometry.Key.Level, 1);
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		WriteGeometryUnsigned(Bytes, static_cast<uint32>(InHeader.Geometry.Key.Coordinate[Axis]), 4);
	}
	WriteGeometryUnsigned(Bytes, static_cast<uint32>(InHeader.Geometry.SampleStepCells), 4);
	WriteGeometryUnsigned(Bytes, static_cast<uint32>(InHeader.Geometry.WorldMinZ), 4);
	WriteGeometryUnsigned(Bytes, static_cast<uint32>(InHeader.Geometry.WorldMaxZ), 4);
	WriteGeometryUnsigned(Bytes, InHeader.Geometry.GeneratorIdentity, 8);
	WriteGeometryUnsigned(Bytes, InHeader.Geometry.MaterialRevision, 8);
	WriteGeometryUnsigned(Bytes, InHeader.RequestId, 8);
	WriteGeometryUnsigned(Bytes, InHeader.ContentRevision, 8);
	check(Bytes.Num() == HeaderBytes);
	OutBytes = MoveTemp(Bytes);
	OutError.Reset();
	return true;
}

bool VoxelGeometryWire::Decode(TConstArrayView<uint8> InBytes, FVoxelGeometryWireHeader& OutHeader, FString& OutError)
{
	if (InBytes.Num() != HeaderBytes)
	{
		OutError = TEXT("Invalid representation geometry header length");
		return false;
	}
	int32 Position = 0;
	if (ReadGeometryUnsigned(InBytes, Position, 4) != 0x36524756 ||
		ReadGeometryUnsigned(InBytes, Position, 2) != Version)
	{
		OutError = TEXT("Unsupported representation geometry version; no legacy geometry interpretation is allowed");
		return false;
	}
	FVoxelGeometryWireHeader Header;
	Header.Geometry.Key.Representation = static_cast<EVoxelTerrainRepresentation>(ReadGeometryUnsigned(InBytes, Position, 1));
	Header.Geometry.Key.Level = static_cast<uint8>(ReadGeometryUnsigned(InBytes, Position, 1));
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Header.Geometry.Key.Coordinate[Axis] = ReadGeometrySigned(InBytes, Position);
	}
	const uint64 Step = ReadGeometryUnsigned(InBytes, Position, 4);
	if (Step > uint64(MAX_int32))
	{
		OutError = TEXT("Sample step overflows the supported integer range");
		return false;
	}
	Header.Geometry.SampleStepCells = static_cast<int32>(Step);
	Header.Geometry.WorldMinZ = ReadGeometrySigned(InBytes, Position);
	Header.Geometry.WorldMaxZ = ReadGeometrySigned(InBytes, Position);
	Header.Geometry.GeneratorIdentity = ReadGeometryUnsigned(InBytes, Position, 8);
	Header.Geometry.MaterialRevision = ReadGeometryUnsigned(InBytes, Position, 8);
	Header.RequestId = ReadGeometryUnsigned(InBytes, Position, 8);
	Header.ContentRevision = ReadGeometryUnsigned(InBytes, Position, 8);
	if (!Header.Geometry.Validate(OutError))
	{
		return false;
	}
	if (Header.RequestId == 0 || Header.Geometry.GeneratorIdentity == 0)
	{
		OutError = TEXT("Invalid representation identity");
		return false;
	}
	OutHeader = MoveTemp(Header);
	OutError.Reset();
	return true;
}

bool VoxelGeometryWire::ValidateExpected(const FVoxelGeometryWireHeader& InReply,
	const FVoxelGeometryWireHeader& InRequest, FString& OutError)
{
	if (InReply.RequestId != InRequest.RequestId || !(InReply.Geometry == InRequest.Geometry))
	{
		OutError = TEXT("Reply does not match the requested spatial and sampling identity");
		return false;
	}
	if (InReply.ContentRevision < InRequest.ContentRevision)
	{
		OutError = TEXT("Representation reply regressed the known content revision");
		return false;
	}
	OutError.Reset();
	return true;
}
