// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <functional>

struct FOpenNode{
	FVector2D pos;           // 位置
	float cost;              // 耗费值
	float pred;              // 预测值
	FOpenNode* father;        // 父节点
	FOpenNode() = default;
	FOpenNode(FVector2D pos,float cost,float pred, FOpenNode* fatherNode)
		:pos(pos),cost(cost),pred(pred),father(fatherNode){}
};

struct FOpenPointPtrCompare {
	bool operator()(FOpenNode* a, FOpenNode* b) {return a->pred > b->pred;}
};

/**
 * A*寻路器
 */
class WHFRAMEWORKCORE_API FPathFinder
{
public:
	//寻路
	static TArray<FVector2D> findPath(FVector2D startPos,FVector2D endPos);
	//设置寻路障碍条件
	static void setConditionInBarrier(std::function<bool(FVector2D pos)> func);
	//设置寻路权值公式
	static void setWeightFormula(std::function<TPair<float,float>(FVector2D pos,FVector2D endPos,float cost)> func);
private:
	//创建一个开启点
	static FOpenNode* createOpenNode(FVector2D startPos,FVector2D endPos, float cost,FOpenNode* fatherNode);
	//开启检查，检查父节点
	static void open(FOpenNode& pointToOpen,FVector2D endPos);
private:
	static int depth;          //记录深度
	static int depthLimit;     //深度限制	

	//函数接口：检查是否有障碍物（不可走）
	static std::function<bool(FVector2D pos)> inBarrier;
	
	//函数接口：计算权值公式
	static std::function<TPair<float,float>(FVector2D pos,FVector2D endPos,float cost)> weightFormula;

	//使用最大优先队列
	static std::priority_queue<FOpenNode*, std::vector<FOpenNode*>, FOpenPointPtrCompare> openlist;

	//存储OpenNode的内存空间
	static std::vector<FOpenNode> pointList;
private:
	FPathFinder() = delete;
	~FPathFinder() = delete;
};
