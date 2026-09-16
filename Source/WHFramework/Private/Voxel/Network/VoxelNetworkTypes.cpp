#include "Voxel/Network/VoxelNetworkTypes.h"
bool FVoxelRPCPacket::NetSerialize(FArchive& Ar, UPackageMap*, bool& OK)
{
	uint32 N = Ar.IsSaving() ? uint32(Bytes.Num()) : 0;
	if (Ar.IsSaving() && N > 4608)
	{
		Ar.SetError();
		OK = false;
		return true;
	}
	Ar.SerializeInt(N, 4609);
	if (Ar.IsError() || N > 4608)
	{
		Ar.SetError();
		OK = false;
		return true;
	}
	if (Ar.IsLoading())
		Bytes.SetNumUninitialized(int32(N));
	if (N)
		Ar.Serialize(Bytes.GetData(), N);
	OK = !Ar.IsError();
	return true;
}
