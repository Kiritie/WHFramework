// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <functional>

struct FOpenNode
{
	FVector2D Pos;
	float Cost;
	float Pred;
	FOpenNode* Father;
	FOpenNode() = default;

	FOpenNode(FVector2D Pos, float Cost, float Pred, FOpenNode* FatherNode)
		: Pos(Pos), Cost(Cost), Pred(Pred), Father(FatherNode)
	{
	}
};

struct FOpenPointPtrCompare
{
	bool operator()(FOpenNode* A, FOpenNode* B) { return A->Pred > B->Pred; }
};

/**
 *
 */
class WHFRAMEWORKCORE_API FPathFinder
{
public:
	FPathFinder();

	~FPathFinder();

public:
	TArray<FVector2D> FindPath(FVector2D StartPos, FVector2D EndPos);

	void SetConditionInBarrier(std::function<bool(FVector2D Pos)> Func);

	void SetWeightFormula(std::function<TPair<float, float>(FVector2D StartPos, FVector2D EndPos, float Cost)> Func);

private:
	FOpenNode* CreateOpenNode(FVector2D StartPos, FVector2D EndPos, float Cost, FOpenNode* FatherNode);

	void Open(FOpenNode& PointToOpen, FVector2D EndPos);

private:
	int32 Depth;
	int32 DepthLimit;

	std::function<bool(FVector2D Pos)> InBarrier;

	std::function<TPair<float, float>(FVector2D StartPos, FVector2D EndPos, float Cost)> WeightFormula;

	std::priority_queue<FOpenNode*, std::vector<FOpenNode*>, FOpenPointPtrCompare> Openlist;

	TArray<FOpenNode> PointList;
};
