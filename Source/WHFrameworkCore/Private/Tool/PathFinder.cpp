// Fill out your copyright notice in the Description page of Project Settings.

#include "Tool/PathFinder.h"

FPathFinder::FPathFinder()
{
	//记录深度
	Depth = 0;

	//深度限制
	DepthLimit = 400;

	//存储OpenNode的内存空间
	PointList = {};

	Openlist = {};

	//函数接口：检查是否有障碍物（不可走）
	InBarrier = nullptr;

	//函数接口：计算权值预测公式
	WeightFormula = nullptr;
}

FPathFinder::~FPathFinder()
{
	
}

TArray<FVector2D> FPathFinder::FindPath(FVector2D StartPos, FVector2D EndPos)
{
	//清理数据结构
	PointList.Empty(DepthLimit);
	while (!Openlist.empty())
	{
		Openlist.pop();
	}
	Depth = 0;

	TArray<FVector2D> PathList;
	//创建并开启一个父节点
	Openlist.push(CreateOpenNode(StartPos, EndPos, 0, nullptr));

	FOpenNode* ToOpen = nullptr;
	bool bFound = false;
	//重复寻找预测和花费之和最小节点开启检查
	while (!Openlist.empty())
	{
		ToOpen = Openlist.top();
		//将父节点从openlist移除
		Openlist.pop();
		//找到终点后，则停止搜索
		if (ToOpen->Pos.X == EndPos.X && ToOpen->Pos.Y == EndPos.Y)
		{
			bFound = true;
			break;
		}
		//若超出一定深度，则搜索失败
		if (Depth >= DepthLimit)
		{
			break;
		}
		Open(*ToOpen, EndPos);
	}

	if (bFound)
	{
		// UE_LOG(LogTemp, Warning, TEXT("path found %d"), Depth);
		while (ToOpen)
		{
			PathList.Push(ToOpen->Pos);
			ToOpen = ToOpen->Father;
		}
	}
	else
	{
		// UE_LOG(LogTemp, Warning, TEXT("path unfound %d"), Depth);
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
	//八方的位置
	const int Direction[8][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
	//深度+1
	Depth++;

	//检查p点八方的点
	for (int i = 0; i < 4; ++i)
	{
		FVector2D NewPos = OpenNode.Pos + FVector2D(Direction[i][0], Direction[i][1]);
		if (!InBarrier(NewPos))
		{
			Openlist.push(CreateOpenNode(NewPos, EndPos, OpenNode.Cost + 1.0f, &OpenNode));
		}
	}
}
