// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Generators/RainGenerator.h"

#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"

TSet<uint64> RainGenerator::waters = TSet<uint64>();

void RainGenerator::GenerateRain(AVoxelChunk* chunk,UVoxelModule* info){

    UVoxelModuleStatics::setSeed(2183);
    waters.Reset();

    //初始化雨滴（随机生成雨滴）
    for(int n = 1;n>0;--n){
        float possible = UVoxelModuleStatics::rand(chunk->GetIndex().ToVector2D()+FVector2D(13.51f,2.16f));
        if(possible <= 0.95f) continue;
        int32 x = UVoxelModuleStatics::randInt(chunk->GetIndex().ToVector2D())%15+1;
        int32 y = UVoxelModuleStatics::randInt(chunk->GetIndex().ToVector2D()+FVector2D(-1.512f,41.421f))%15+1;
        flow(chunk,info,60.0f,x,y,chunk->GetTopography(FIndex(x, y)).Height+1);
    }
}


void RainGenerator::flow(AVoxelChunk* chunk,UVoxelModule* info,float rain,int i,int j,int k){
    if(rain<0.5f/* || k<=SeaLevel*/)return;

    uint64 index = UVoxelModuleStatics::Index(chunk->GetIndex().X*16+i,chunk->GetIndex().Y*16+j,k);
    waters.Emplace(index);

    //垂流：检测下方踏空
    FVector pos = FVector(chunk->GetIndex().X*16+i,chunk->GetIndex().Y*16+j,k-1);
    index = UVoxelModuleStatics::Index(pos.X,pos.Y,pos.Z);
    if(waters.Find(index)){return;}
    if(!info->HasVoxelByIndex(pos)){
        flow(chunk,info,rain-0.3f,i,j,k-1);
        return;
    }

    const int32 dx[4] = {1,-1,0,0};
    const int32 dy[4] = {0,0,1,-1};

    for(int d = 0;d<4;++d){
        int32 x = i+dx[d];
        int32 y = j+dy[d];

        pos = FVector(chunk->GetIndex().X*16+x,chunk->GetIndex().Y*16+y,k);
        index = UVoxelModuleStatics::Index(pos.X,pos.Y,pos.Z);

        if(waters.Find(index))continue;

        //水平流向无障碍
        if(!info->HasVoxelByIndex(pos)){
            pos = FVector(chunk->GetIndex().X*16+x,chunk->GetIndex().Y*16+y,k-1);
            index = UVoxelModuleStatics::Index(pos.X,pos.Y,pos.Z);
            if(waters.Find(index)){continue;}
        
            if(!info->HasVoxelByIndex(pos)){
                flow(chunk,info,rain-1.0f,x,y,k-1);
            }
            else{
                flow(chunk,info,rain-1.0f,x,y,k);
            }
        }
    }

    for(uint64 index1:waters){
        auto vec = UVoxelModuleStatics::UnIndex(index1);
        info->SetVoxelByIndex(FVector(vec.X,vec.Y,vec.Z-1),EVoxelType::Water);
    }
}
