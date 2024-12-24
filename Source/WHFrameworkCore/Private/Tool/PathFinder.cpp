// Fill out your copyright notice in the Description page of Project Settings.

#include "Tool/PathFinder.h"

//记录深度
int FPathFinder::depth = 0;        

//深度限制
int FPathFinder::depthLimit = 400;     

//存储OpenNode的内存空间
std::vector<FOpenNode> FPathFinder::pointList = std::vector<FOpenNode>(depthLimit);

//函数接口：检查是否有障碍物（不可走）
std::function<bool(FVector2D pos)> FPathFinder::inBarrier = nullptr;
	
//函数接口：计算权值预测公式
std::function<TPair<float,float>(FVector2D pos,FVector2D endPos,float cost)> FPathFinder::weightFormula = nullptr;

std::priority_queue<FOpenNode*, std::vector<FOpenNode*>, FOpenPointPtrCompare> FPathFinder::openlist = {};

TArray<FVector2D> FPathFinder::findPath(FVector2D startPos,FVector2D endPos) {
    //清理数据结构
    pointList.clear();
    while(!openlist.empty())
        openlist.pop();
    depth = 0;

    TArray<FVector2D> road;
    //创建并开启一个父节点
    openlist.push(createOpenNode(startPos, endPos, 0, nullptr));

    FOpenNode* toOpen = nullptr;
    bool found = false;
    //重复寻找预测和花费之和最小节点开启检查
    while (!openlist.empty()){
        toOpen = openlist.top();        
        //将父节点从openlist移除
        openlist.pop();
        // 找到终点后，则停止搜索
        if (toOpen->pos.X == endPos.X && toOpen->pos.Y ==endPos.Y) {
            found = true;
            break;
        }
        //若超出一定深度，则搜索失败
        if (depth >= depthLimit) {
            break;
        }
        open(*toOpen,endPos);
    }

    if(!found){
	    UE_LOG(LogTemp, Warning, TEXT("path unfound %d"),depth);
    }
    else{
	    UE_LOG(LogTemp, Warning, TEXT("path found %d"),depth);
        for (auto rs = toOpen; rs != nullptr; rs = rs->father) {
            road.Push(rs->pos);
        }
    }

    return road;
}


void FPathFinder::setConditionInBarrier(std::function<bool(FVector2D pos)> func){
    inBarrier = func;
}

void FPathFinder::setWeightFormula(std::function<TPair<float,float>(FVector2D pos,FVector2D endPo,float cost)> func){
    weightFormula = func;
}

FOpenNode* FPathFinder::createOpenNode(FVector2D pos,FVector2D endPos, float cost, FOpenNode* fatherNode){
    TPair<float,float> pair = weightFormula(pos,endPos,cost);
    pointList.push_back(FOpenNode(pos,pair.Get<0>(),pair.Get<1>(),fatherNode));
    return &pointList.back();
}

void FPathFinder::open(FOpenNode& OpenNode,FVector2D endPos) {
	//八方的位置
	const int direction[8][2] = {{1,0},{0,1},{-1,0},{0,-1}};
    //深度+1
    depth++;

    //检查p点八方的点
    for (int i = 0; i < 4; ++i)
    {   
        FVector2D newPos = OpenNode.pos + FVector2D(direction[i][0],direction[i][1]);
        if (!inBarrier(newPos)) {
            openlist.push(createOpenNode(newPos,endPos, OpenNode.cost+1.0f, &OpenNode));
        }
    }
}
