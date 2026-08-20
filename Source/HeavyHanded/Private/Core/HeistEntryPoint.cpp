#include "Core/HeistEntryPoint.h"

#include "Components/ArrowComponent.h"
#include "Core/HeistLog.h"
#include "EngineUtils.h"                // TActorIterator
#include "Engine/World.h"

#if WITH_EDITOR
#include "Logging/MessageLog.h"         // FMessageLog — 맵 체크 경고
#include "Misc/UObjectToken.h"          // FUObjectToken — 경고에서 액터로 점프
#endif

AHeistEntryPoint::AHeistEntryPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	VanAnchor = CreateDefaultSubobject<UArrowComponent>(TEXT("VanAnchor"));
	VanAnchor->SetupAttachment(GetRootComponent());

	// 진입점 뒤로 4m. 겹쳐 두면 스폰한 플레이어가 밴 콜리전에 낀다.
	// 실제 위치는 레벨에서 화살표를 끌어 정한다
	VanAnchor->SetRelativeLocation(FVector(-400.f, 0.f, 0.f));

	// 뷰포트에서 눈에 띄어야 옮긴다. 밴을 연상시키는 색으로 두고 크게 그린다
	VanAnchor->SetArrowColor(FLinearColor(1.f, 0.55f, 0.f));
	VanAnchor->ArrowSize = 2.f;
	VanAnchor->ArrowLength = 120.f;
	VanAnchor->bTreatAsASprite = false;

	// 게임 중에는 보이면 안 된다 (UArrowComponent 기본값이지만 의도를 남긴다).
	//
	// [SetIsVisualizationComponent 를 쓰지 않는다] 그렇게 표시한 컴포넌트는 쿠킹에서 빠진다.
	//   이 앵커는 장식이 아니라 **런타임에 서버가 읽는 좌표**다 (GetVanTransform).
	//   패키징한 빌드에서만 앵커가 사라져 밴이 플레이어 위에 서게 되고, 에디터에서는 멀쩡하다 —
	//   가장 늦게 발견되는 종류의 사고다. UArrowComponent 자체는 에디터 전용이 아니다.
	VanAnchor->SetHiddenInGame(true);
}

bool AHeistEntryPoint::TryGetVanTransform(FTransform& OutTransform) const
{
	// 앵커가 없을 리는 없지만(생성자에서 만든다), 옛 버전으로 직렬화된 액터나
	// 앵커를 지운 BP 서브클래스에서는 없을 수 있다.
	//
	// 그때 진입점 자리를 돌려주면 밴이 스폰 지점 위에 서고 폰이 아예 스폰되지 않는다.
	// 실패를 실패로 알리고, 밴은 레벨에 놓인 자리에 그대로 두게 한다
	if (!IsValid(VanAnchor))
	{
		UE_LOG(LogHeist, Warning,
			TEXT("진입점 %s 에 VanAnchor 가 없습니다. 밴을 옮기지 않습니다 — "
				 "액터를 지우고 새로 배치하면 화살표가 다시 붙습니다."),
			*GetName());

		return false;
	}

	OutTransform = VanAnchor->GetComponentTransform();
	return true;
}

void AHeistEntryPoint::CollectEntryPoints(const UObject* WorldContext, TArray<AHeistEntryPoint*>& OutEntries)
{
	OutEntries.Reset();

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;

	if (!World)
	{
		return;
	}

	for (TActorIterator<AHeistEntryPoint> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AHeistEntryPoint* Entry = *It;

		if (!IsValid(Entry))
		{
			continue;
		}

		// 태그 없는 진입점은 고를 수 없다. 조용히 빼면 "왜 목록에 안 뜨지" 가 되므로 알린다
		if (!Entry->EntryTag.IsValid())
		{
			UE_LOG(LogHeist, Warning,
				TEXT("진입점 %s 에 EntryTag 가 없습니다. 선택 목록에서 제외합니다."),
				*Entry->GetName());

			continue;
		}

		OutEntries.Add(Entry);
	}

	// 정렬이 계약이다 — 폴백이 "첫 번째" 를 고른다 (HeistEntryGate).
	// TActorIterator 순서는 보장되지 않아서, 정렬하지 않으면 폴백이 실행마다 달라진다
	OutEntries.Sort([](const AHeistEntryPoint& A, const AHeistEntryPoint& B)
	{
		return A.EntryTag.ToString() < B.EntryTag.ToString();
	});
}

int32 AHeistEntryPoint::FindDefaultIndex(const TArray<AHeistEntryPoint*>& Entries)
{
	int32 DefaultIndex = INDEX_NONE;
	int32 MarkedNum = 0;

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const AHeistEntryPoint* Entry = Entries[Index];

		if (!IsValid(Entry) || !Entry->bIsDefaultEntry)
		{
			continue;
		}

		++MarkedNum;

		// 앞선 것을 쓴다. Entries 는 태그 이름순이라 이 선택도 결정적이다
		if (DefaultIndex == INDEX_NONE)
		{
			DefaultIndex = Index;
		}
	}

	// 하나를 고르는 것이 이 플래그의 뜻이라, 둘 다 존중하는 해석이 없다.
	// 조용히 하나를 고르면 "왜 저기서 시작하지" 가 되므로 알린다
	if (MarkedNum > 1)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("기본 진입점이 %d 개 체크돼 있습니다. %s 하나만 쓰입니다 — 하나만 남겨 주세요."),
			MarkedNum, *Entries[DefaultIndex]->GetEntryTag().ToString());
	}

	return DefaultIndex;
}

AHeistEntryPoint* AHeistEntryPoint::FindByTag(const UObject* WorldContext, const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return nullptr;
	}

	TArray<AHeistEntryPoint*> Entries;
	CollectEntryPoints(WorldContext, Entries);

	for (AHeistEntryPoint* Entry : Entries)
	{
		// 정확히 일치하는 것만. 부모 매칭을 쓰면 Entry.Mansion 하나가
		// Entry.Mansion.Front 를 가리키게 되어, 진입점이 늘어날 때 예전 선택이 조용히 옮겨간다
		if (Entry->EntryTag == Tag)
		{
			return Entry;
		}
	}

	return nullptr;
}

void AHeistEntryPoint::BeginPlay()
{
	Super::BeginPlay();

	// 중복 태그는 "고른 대로 시작했는데 가끔 다른 곳" 으로만 드러난다.
	// 서버에서 한 번만 확인한다 — 클라이언트에서도 찍으면 같은 경고가 인원수만큼 나온다
	if (!HasAuthority() || !EntryTag.IsValid())
	{
		return;
	}

	TArray<AHeistEntryPoint*> Entries;
	CollectEntryPoints(this, Entries);

	int32 SameTagNum = 0;

	for (const AHeistEntryPoint* Entry : Entries)
	{
		if (Entry->EntryTag == EntryTag)
		{
			++SameTagNum;
		}
	}

	if (SameTagNum > 1)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("진입점 태그 %s 가 이 레벨에 %d 개 있습니다. 먼저 찾은 하나만 쓰입니다 — 태그를 갈라 주세요."),
			*EntryTag.ToString(), SameTagNum);
	}
}

#if WITH_EDITOR
void AHeistEntryPoint::CheckForErrors()
{
	Super::CheckForErrors();

	// 맵 체크(Build > Map Check)에서 잡히게 한다. 런타임 경고는 플레이해야 보이지만
	// 이건 레벨을 저장하는 사람이 그 자리에서 알아야 하는 것이다
	if (!EntryTag.IsValid())
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(NSLOCTEXT("HeavyHanded", "EntryPointNoTag",
				"진입점에 EntryTag 가 없습니다. 선택 목록에 뜨지 않습니다.")));
	}
}
#endif
