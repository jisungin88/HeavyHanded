#include "Noise/NoiseTestBlocker.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ANoiseTestBlocker::ANoiseTestBlocker()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Body->SetStaticMesh(CubeMesh.Object);
	}

	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_WorldStatic);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);          // 에디터에서 클릭 선택
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);                // 플레이어가 통과 못하게
	Body->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);   // NoiseOcclusion
}