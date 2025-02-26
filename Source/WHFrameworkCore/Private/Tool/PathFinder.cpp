// Fill out your copyright notice in the Description page of Project Settings.

#include "Tool/PathFinder.h"

FPathFinder::FPathFinder()
{
	Depth = 0;

	DepthLimit = 400;

	PointList = {};

	Openlist = {};

	InBarrier = nullptr;

	WeightFormula = nullptr;
}

FPathFinder::~FPathFinder()
{
	
}

TArray<FVector2D> FPathFinder::FindPath(FVector2D StartPos, FVector2D EndPos)
{
	PointList.Empty(DepthLimit);
	while (!Openlist.empty())
	{
		Openlist.pop();
	}
	Depth = 0;

	TArray<FVector2D> PathList;
	Openlist.push(CreateOpenNode(StartPos, EndPos, 0, nullptr));

	FOpenNode* ToOpen = nullptr;
	bool bFound = false;
	while (!Openlist.empty())
	{
		ToOpen = Openlist.top();
		Openlist.pop();
		if (ToOpen->Pos.X == EndPos.X && ToOpen->Pos.Y == EndPos.Y)
		{
			bFound = true;
			break;
		}
		if (Depth >= DepthLimit)
		{
			break;
		}
		Open(*ToOpen, EndPos);
	}

	if (bFound)
	{
		while (ToOpen)
		{
			PathList.Push(ToOpen->Pos);
			ToOpen = ToOpen->Father;
		}
	}

	return PathList;
}

void FPathFinder::SetConditionInBarrier(std::function<bool(FVector2D Pos)> Func)
{
	InBarrier = Func;
}

void FPathFinder::SetWeightFormula(std::function<TPair<float, float>(FVector2D StartPos, FVector2D EndPos, float Cost)> Func)
{
	WeightFormula = Func;
}

FOpenNode* FPathFinder::CreateOpenNode(FVector2D StartPos, FVector2D EndPos, float Cost, FOpenNode* FatherNode)
{
	TPair<float, float> Pair = WeightFormula(StartPos, EndPos, Cost);
	PointList.Push(FOpenNode(StartPos, Pair.Get<0>(), Pair.Get<1>(), FatherNode));
	return &PointList.Last();
}

void FPathFinder::Open(FOpenNode& OpenNode, FVector2D EndPos)
{
	const int32 Direction[8][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
	Depth++;

	for (int i = 0; i < 4; ++i)
	{
		FVector2D NewPos = OpenNode.Pos + FVector2D(Direction[i][0], Direction[i][1]);
		if (!InBarrier(NewPos))
		{
			Openlist.push(CreateOpenNode(NewPos, EndPos, OpenNode.Cost + 1.0f, &OpenNode));
		}
	}
}
