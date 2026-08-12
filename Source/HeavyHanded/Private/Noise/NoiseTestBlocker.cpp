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

	// NoiseOcclusion 은 Block 이 아니라 Overlap 이어야 한다.
	//
	// 소음 채널은 "막는" 채널이 아니라 "세는" 채널이다. LineTraceMulti 는 첫 Block 히트에서
	// 광선을 끊고 그보다 먼 히트를 전부 버리므로, 여기를 Block 으로 두면 이 벽이 몇 겹이든
	// 차폐물이 항상 1개로만 세어지고 MaxOccluders 세팅이 통째로 죽는다.
	// DefaultEngine.ini 의 NoiseOcclusion 기본 응답 · VisionBlocker 프로파일과 같은 이유다.
	//
	// 실제 벽(VisionBlocker 프로파일)과 다르게 굴면 테스트 도구로서 의미가 없다
	Body->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
}