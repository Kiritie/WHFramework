// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/VoxelTree.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Datas/VoxelTreeData.h"

UVoxelTree::UVoxelTree()
{
	TreePart = EVoxelTreePart::None;
	TreeHeight = 0;
	LeavesHeight = 0;
	LeavesWidth = 0;
}

void UVoxelTree::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	TreePart = EVoxelTreePart::None;
	TreeHeight = 0;
	LeavesHeight = 0;
	LeavesWidth = 0;
}

void UVoxelTree::LoadData(const FString& InData)
{
	TArray<FString> Datas;
	InData.ParseIntoArray(Datas, TEXT(";"));
	TreePart = (EVoxelTreePart)FCString::Atoi(*Datas[0]);
	switch (TreePart)
	{
		case EVoxelTreePart::Root:
		{
			TreeHeight = FCString::Atoi(*Datas[1]);
			LeavesHeight = FCString::Atoi(*Datas[2]);
			LeavesWidth = FCString::Atoi(*Datas[3]);
			break;
		}
		default: break;
	}
}

FString UVoxelTree::ToData()
{
	switch (TreePart)
	{
		case EVoxelTreePart::Root:
		{
			return FString::Printf(TEXT("%d;%d;%d;%d"), TreePart, TreeHeight, LeavesHeight, LeavesWidth);
		}
		case EVoxelTreePart::Bole:
		case EVoxelTreePart::Leaves:
		{
			return FString::Printf(TEXT("%d"), TreePart);
		}
		default: break;
	}
	return TEXT("");
}

FString UVoxelTree::ToData(EVoxelTreePart InTreePart, const UVoxelTreeData& InTreeData)
{
	TreePart = InTreePart;
	switch (TreePart)
	{
		case EVoxelTreePart::Root:
		{
			const FRandomStream& RandomStream = AVoxelModule::Get()->GetWorldData().RandomStream;
			TreeHeight = RandomStream.RandRange(InTreeData.TreeHeight.X, InTreeData.TreeHeight.Y);
			LeavesHeight = RandomStream.RandRange(InTreeData.LeavesHeight.X, InTreeData.LeavesHeight.Y);
			LeavesWidth = RandomStream.FRand() < 0.5f ? InTreeData.LeavesWidth.X : InTreeData.LeavesWidth.Y;
			break;
		}
		default: break;
	}
	return ToData();
}

void UVoxelTree::OnGenerate(IVoxelAgentInterface* InAgent)
{
	Super::OnGenerate(InAgent);

	if(GetOwner() && !IsGenerated() && TreePart == EVoxelTreePart::Root)
	{
		const auto& TreeData = GetData<UVoxelTreeData>();
		TMap<FIndex, FVoxelItem> VoxelItems;
		for(int32 OffsetZ = TreeHeight - LeavesHeight / 2; OffsetZ <= TreeHeight + LeavesHeight / 2; OffsetZ++)
		{
			for(int32 OffsetX = -LeavesWidth / 2; OffsetX <= LeavesWidth / 2; OffsetX++)
			{
				for(int32 OffsetY = -LeavesWidth / 2; OffsetY <= LeavesWidth / 2; OffsetY++)
				{
					VoxelItems.Emplace(GetIndex() + FIndex(OffsetX, OffsetY, OffsetZ - 1), FVoxelItem(TreeData.LeavesData->VoxelType, ToData(EVoxelTreePart::Leaves, TreeData)));
				}
			}
		}
		DON_WITHINDEX(TreeHeight - 1, i,
			VoxelItems.Emplace(GetIndex() + FIndex(0, 0, i + 1), FVoxelItem(TreeData.VoxelType, ToData(EVoxelTreePart::Bole, TreeData)));
		)
		GetOwner()->SetVoxelComplex(VoxelItems);
	}
}

void UVoxelTree::OnDestroy(IVoxelAgentInterface* InAgent)
{
	Super::OnDestroy(InAgent);

	if(AVoxelChunk* Owner = GetOwner())
	{
		switch (TreePart)
		{
			case EVoxelTreePart::Root:
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
				for(auto Iter : TMap(VoxelItems))
				{
					if(Owner->GetVoxel<UVoxelTree>(Iter.Key).TreePart == EVoxelTreePart::None)
					{
						VoxelItems.Remove(Iter.Key);
					}
				}
				Owner->SetVoxelComplex(VoxelItems, true, false, InAgent);
				break;
			}
			default: break;
		}
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

bool UVoxelTree::OnAgentInteract(IVoxelAgentInterface* InAgent, EVoxelInteractType InActionType, const FVoxelHitResult& InHitResult)
{
	return Super::OnAgentInteract(InAgent, InActionType, InHitResult);
}
