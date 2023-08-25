// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelTree.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Datas/VoxelTreeData.h"

UVoxelTree::UVoxelTree()
{
	bRoot = false;
	TreeHeight = 0;
	LeavesHeight = 0;
	LeavesWidth = 0;
}

void UVoxelTree::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	bRoot = false;
	TreeHeight = 0;
	LeavesHeight = 0;
	LeavesWidth = 0;
}

void UVoxelTree::LoadData(const FString& InData)
{
	TArray<FString> Datas;
	InData.ParseIntoArray(Datas, TEXT(","));
	if(Datas.IsValidIndex(0))
	{
		bRoot = (bool)FCString::Atoi(*Datas[0]);
	}
	if(Datas.IsValidIndex(1))
	{
		TreeHeight = FCString::Atoi(*Datas[1]);
	}
	if(Datas.IsValidIndex(2))
	{
		LeavesHeight = FCString::Atoi(*Datas[2]);
	}
	if(Datas.IsValidIndex(3))
	{
		LeavesWidth = FCString::Atoi(*Datas[3]);
	}
}

FString UVoxelTree::ToData()
{
	return FString::Printf(TEXT("%d,%d,%d,%d"), bRoot, TreeHeight, LeavesHeight, LeavesWidth);
}

void UVoxelTree::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);
}

void UVoxelTree::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);

	if(GetOwner() && IsRoot())
	{
		TMap<FIndex, FVoxelItem> VoxelItems;
		DON_WITHINDEX(TreeHeight - 1, i,
			VoxelItems.Emplace(GetIndex() + FIndex(0, 0, i + 1), FVoxelItem::Empty);
		)
		for(int32 OffsetZ = TreeHeight - LeavesHeight / 2; OffsetZ <= TreeHeight + LeavesHeight / 2; OffsetZ++)
		{
			for(int32 OffsetX = -LeavesWidth / 2; OffsetX <= LeavesWidth / 2; OffsetX++)
			{
				for(int32 OffsetY = -LeavesWidth / 2; OffsetY <= LeavesWidth / 2; OffsetY++)
				{
					VoxelItems.Emplace(GetIndex() + FIndex(OffsetX, OffsetY, OffsetZ - 1), FVoxelItem::Empty);
				}
			}
		}
		GetOwner()->SetVoxelComplex(VoxelItems, true, false, InAgent);
	}
}

void UVoxelTree::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentHit(InAgent, InHitResult);
}

void UVoxelTree::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentEnter(InAgent, InHitResult);
}

void UVoxelTree::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentStay(InAgent, InHitResult);
}

void UVoxelTree::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	Super::OnAgentExit(InAgent, InHitResult);
}

bool UVoxelTree::OnAgentAction(IVoxelAgentInterface* InAgent, EVoxelActionType InActionType, const FVoxelHitResult& InHitResult)
{
	return Super::OnAgentAction(InAgent, InActionType, InHitResult);
}

void UVoxelTree::InitTree(bool bIsRoot)
{
	const FRandomStream& RandomStream = AVoxelModule::Get()->GetWorldData().RandomStream;
	const auto& TreeData = GetData<UVoxelTreeData>();
	if(bIsRoot)
	{
		bRoot = true;
		TreeHeight = RandomStream.RandRange(TreeData.TreeHeight.X, TreeData.TreeHeight.Y);
		LeavesHeight = RandomStream.RandRange(TreeData.LeavesHeight.X, TreeData.LeavesHeight.Y);
		LeavesWidth = RandomStream.FRand() < 0.5f ? TreeData.LeavesWidth.X : TreeData.LeavesWidth.Y;
	}
}

void UVoxelTree::BuildTree()
{
	if(GetOwner())
	{
		const auto& TreeData = GetData<UVoxelTreeData>();
		TMap<FIndex, FVoxelItem> VoxelItems;
		for(int32 OffsetZ = TreeHeight - LeavesHeight / 2; OffsetZ <= TreeHeight + LeavesHeight / 2; OffsetZ++)
		{
			for(int32 OffsetX = -LeavesWidth / 2; OffsetX <= LeavesWidth / 2; OffsetX++)
			{
				for(int32 OffsetY = -LeavesWidth / 2; OffsetY <= LeavesWidth / 2; OffsetY++)
				{
					VoxelItems.Emplace(GetIndex() + FIndex(OffsetX, OffsetY, OffsetZ - 1), TreeData.LeavesData->VoxelType);
				}
			}
		}
		DON_WITHINDEX(TreeHeight - 1, i,
			VoxelItems.Emplace(GetIndex() + FIndex(0, 0, i + 1), TreeData.VoxelType);
		)
		GetOwner()->SetVoxelComplex(VoxelItems);
	}
}
