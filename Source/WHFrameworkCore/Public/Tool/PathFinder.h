// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <functional>

struct FOpenNode
{
	FVector2D Pos; // 位置
	float Cost; // 耗费值
	float Pred; // 预测值
	FOpenNode* Father; // 父节点
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
 * A*寻路器
 */
class WHFRAMEWORKCORE_API FPathFinder
{
public:
	//寻路
	static TArray<FVector2D> FindPath(FVector2D StartPos, FVector2D EndPos);

	//设置寻路障碍条件
	static void SetConditionInBarrier(std::function<bool(FVector2D Pos)> Func);

	//设置寻路权值公式
	static void SetWeightFormula(std::function<TPair<float, float>(FVector2D Pos, FVector2D EndPos, float Cost)> Func);

private:
	//创建一个开启点
	static FOpenNode* CreateOpenNode(FVector2D StartPos, FVector2D EndPos, float Cost, FOpenNode* FatherNode);

	//开启检查，检查父节点
	static void Open(FOpenNode& PointToOpen, FVector2D EndPos);

private:
	static int Depth; //记录深度
	static int DepthLimit; //深度限制	

	//函数接口：检查是否有障碍物（不可走）
	static std::function<bool(FVector2D Pos)> InBarrier;

	//函数接口：计算权值公式
	static std::function<TPair<float, float>(FVector2D Pos, FVector2D EndPos, float Cost)> WeightFormula;

	//使用最大优先队列
	static std::priority_queue<FOpenNode*, std::vector<FOpenNode*>, FOpenPointPtrCompare> Openlist;

	//存储OpenNode的内存空间
	static std::vector<FOpenNode> PointList;

private:
	FPathFinder() = delete;

	~FPathFinder() = delete;
};
