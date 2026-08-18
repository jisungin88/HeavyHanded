#include "Core/RunProgressSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

#include "Core/HeistLog.h"
#include "Shared/NetAuthority.h"

URunProgressSubsystem* URunProgressSubsystem::Get(const UObject* WorldContext)
{
	if (!GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<URunProgressSubsystem>() : nullptr;
}

bool URunProgressSubsystem::EnsureServerAuthority(const TCHAR* Operation) const
{
	// 서브시스템은 서버 · 클라이언트 양쪽에 하나씩 생긴다. 클라이언트 것에 값을 써 두면
	// 그 창에서만 맞는 숫자가 만들어지고, 서버와 어긋난 채로 조용히 굴러간다
	if (HasServerAuthority(GetWorld()))
	{
		return true;
	}

	UE_LOG(LogHeist, Warning,
		TEXT("%s 무시 — 런 진행 데이터는 서버만 바꿀 수 있습니다."), Operation);
	return false;
}

// ──────────────────────────────────────────────────────────────
// 팀 공용 골드
// ──────────────────────────────────────────────────────────────

void URunProgressSubsystem::AddTeamGold(int32 DeltaGold)
{
	if (DeltaGold == 0 || !EnsureServerAuthority(TEXT("AddTeamGold")))
	{
		return;
	}

	const int32 OldGold = TeamGold;
	TeamGold = FMath::Max(0, TeamGold + DeltaGold);

	UE_LOG(LogHeist, Log, TEXT("팀 골드 %+d → $%d (이전 $%d)"), DeltaGold, TeamGold, OldGold);
}

bool URunProgressSubsystem::TrySpendTeamGold(int32 Cost)
{
	if (!EnsureServerAuthority(TEXT("TrySpendTeamGold")))
	{
		return false;
	}

	if (Cost <= 0)
	{
		// 공짜이거나 잘못된 호출. 성공으로 치되 잔액은 건드리지 않는다
		return true;
	}

	if (TeamGold < Cost)
	{
		UE_LOG(LogHeist, Log, TEXT("구매 실패 — $%d 필요한데 잔액이 $%d 입니다."), Cost, TeamGold);
		return false;
	}

	TeamGold -= Cost;
	UE_LOG(LogHeist, Log, TEXT("팀 골드 -%d → $%d"), Cost, TeamGold);

	return true;
}

// ──────────────────────────────────────────────────────────────
// 참가자 명단
// ──────────────────────────────────────────────────────────────

void URunProgressSubsystem::SetConfirmedRoster(const TArray<FUniqueNetIdRepl>& InRoster)
{
	if (!EnsureServerAuthority(TEXT("SetConfirmedRoster")))
	{
		return;
	}

	ConfirmedRoster = InRoster;

	UE_LOG(LogHeist, Log, TEXT("참가자 명단 확정 — %d명"), ConfirmedRoster.Num());
}

bool URunProgressSubsystem::IsInRoster(const FUniqueNetIdRepl& PlayerId) const
{
	return ConfirmedRoster.Contains(PlayerId);
}

// ──────────────────────────────────────────────────────────────
// 역할 선택
// ──────────────────────────────────────────────────────────────

bool URunProgressSubsystem::TrySelectRole(const FUniqueNetIdRepl& PlayerId, const FGameplayTag& RoleTag)
{
	if (!EnsureServerAuthority(TEXT("TrySelectRole")))
	{
		return false;
	}

	if (!PlayerId.IsValid() || !RoleTag.IsValid())
	{
		UE_LOG(LogHeist, Warning, TEXT("역할 선택 거부 — 플레이어 또는 역할 태그가 유효하지 않습니다."));
		return false;
	}

	// 한 번 고르면 바꿀 수 없다 (기획 확정).
	// 클라이언트가 다시 요청해도 서버에서 막아야 실제로 고정된다
	if (const FGameplayTag* Existing = SelectedRoles.Find(PlayerId))
	{
		UE_LOG(LogHeist, Log, TEXT("역할 선택 거부 — %s 는 이미 %s 입니다. 역할은 바꿀 수 없습니다."),
			*PlayerId.ToDebugString(), *Existing->ToString());
		return false;
	}

	// 4역할은 실루엣으로 구분되므로 두 명이 같은 역할일 수 없다.
	// 같은 프레임에 두 명이 눌러도 여기서 순서가 정해진다 — 서버 한 곳을 지나기 때문이다
	if (IsRoleTaken(RoleTag))
	{
		UE_LOG(LogHeist, Log, TEXT("역할 선택 거부 — %s 는 이미 다른 플레이어가 선택했습니다."),
			*RoleTag.ToString());
		return false;
	}

	SelectedRoles.Add(PlayerId, RoleTag);

	UE_LOG(LogHeist, Log, TEXT("역할 확정 — %s → %s"),
		*PlayerId.ToDebugString(), *RoleTag.ToString());

	return true;
}

FGameplayTag URunProgressSubsystem::GetSelectedRole(const FUniqueNetIdRepl& PlayerId) const
{
	const FGameplayTag* Found = SelectedRoles.Find(PlayerId);
	return Found ? *Found : FGameplayTag();
}

bool URunProgressSubsystem::IsRoleTaken(const FGameplayTag& RoleTag) const
{
	for (const TPair<FUniqueNetIdRepl, FGameplayTag>& Pair : SelectedRoles)
	{
		if (Pair.Value == RoleTag)
		{
			return true;
		}
	}

	return false;
}

// ──────────────────────────────────────────────────────────────
// 구매 장비
// ──────────────────────────────────────────────────────────────

void URunProgressSubsystem::AddPurchasedEquipment(const FGameplayTag& EquipmentTag, int32 Count)
{
	if (!EnsureServerAuthority(TEXT("AddPurchasedEquipment")))
	{
		return;
	}

	if (!EquipmentTag.IsValid() || Count <= 0)
	{
		UE_LOG(LogHeist, Warning, TEXT("장비 추가 무시 — 태그가 유효하지 않거나 수량이 0 이하입니다."));
		return;
	}

	const int32 NewCount = PurchasedEquipment.FindOrAdd(EquipmentTag) + Count;
	PurchasedEquipment.Add(EquipmentTag, NewCount);

	UE_LOG(LogHeist, Log, TEXT("장비 구매 — %s x%d (누적 %d개)"),
		*EquipmentTag.ToString(), Count, NewCount);
}

int32 URunProgressSubsystem::GetPurchasedCount(FGameplayTag EquipmentTag) const
{
	const int32* Found = PurchasedEquipment.Find(EquipmentTag);
	return Found ? *Found : 0;
}

void URunProgressSubsystem::ConsumePurchasedEquipment()
{
	if (!EnsureServerAuthority(TEXT("ConsumePurchasedEquipment")))
	{
		return;
	}

	if (PurchasedEquipment.IsEmpty())
	{
		return;
	}

	UE_LOG(LogHeist, Log, TEXT("구매 장비 %d종을 스폰하고 목록을 비웠습니다."), PurchasedEquipment.Num());

	PurchasedEquipment.Reset();
}

// ──────────────────────────────────────────────────────────────
// 수명 경계
// ──────────────────────────────────────────────────────────────

void URunProgressSubsystem::BeginNewRun()
{
	if (!EnsureServerAuthority(TEXT("BeginNewRun")))
	{
		return;
	}

	// 팀 골드와 역할은 건드리지 않는다 — 둘 다 캠페인 단위다.
	// 골드는 판을 건너 누적되고, 역할은 한 번 고르면 바뀌지 않는다
	ConfirmedRoster.Reset();
	PurchasedEquipment.Reset();

	UE_LOG(LogHeist, Log, TEXT("새 판을 시작합니다 — 명단·장비 초기화 (팀 골드 $%d, 확정 역할 %d개 유지)"),
		TeamGold, SelectedRoles.Num());
}

void URunProgressSubsystem::ResetCampaign()
{
	if (!EnsureServerAuthority(TEXT("ResetCampaign")))
	{
		return;
	}

	TeamGold = 0;
	ConfirmedRoster.Reset();
	SelectedRoles.Reset();
	PurchasedEquipment.Reset();

	UE_LOG(LogHeist, Log, TEXT("새 방을 엽니다 — 팀 골드와 역할을 포함해 전부 초기화했습니다."));
}

// ──────────────────────────────────────────────────────────────
// 치트
//
// 이 데이터를 채우는 것은 은신처 상점과 로비인데 둘 다 아직 없다.
// 그때까지 이 명령들이 유일한 입력 수단이고, 상점이 붙은 뒤에도
// "골드가 모자란 상황" 처럼 손으로 만들기 번거로운 상태를 만드는 데 쓴다.
// ──────────────────────────────────────────────────────────────

namespace
{
	/** 치트가 대상으로 삼을 서브시스템. 서버가 아니면 nullptr 이고 이유를 찍는다 */
	URunProgressSubsystem* GetRunForCheat(UWorld* World, bool bRequireAuthority = true)
	{
		if (bRequireAuthority && !HasServerAuthority(World))
		{
			UE_LOG(LogHeist, Warning, TEXT("이 명령은 서버(호스트) 창에서만 동작합니다."));
			return nullptr;
		}

		URunProgressSubsystem* Run = URunProgressSubsystem::Get(World);
		if (!Run)
		{
			UE_LOG(LogHeist, Warning, TEXT("URunProgressSubsystem 을 찾지 못했습니다."));
		}

		return Run;
	}

	/**
	 * 0번 로컬 플레이어의 신원. 역할 치트가 "누구" 인지 정하는 데 쓴다.
	 *
	 * PIE 와 OnlineSubsystemNull 은 가짜 식별자를 만들어 주므로 대개 유효하다.
	 * 유효하지 않으면 TrySelectRole 이 거부하는데, 그때 원인을 알 수 있게 여기서 먼저 찍는다.
	 */
	FUniqueNetIdRepl GetLocalPlayerId(UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APlayerState* PS = PC ? PC->PlayerState : nullptr;

		if (!PS)
		{
			UE_LOG(LogHeist, Warning, TEXT("로컬 플레이어의 PlayerState 가 없습니다."));
			return FUniqueNetIdRepl();
		}

		FUniqueNetIdRepl Id = PS->GetUniqueId();
		if (!Id.IsValid())
		{
			UE_LOG(LogHeist, Warning,
				TEXT("로컬 플레이어의 네트워크 식별자가 유효하지 않습니다 — 역할 선택이 거부됩니다."));
		}

		return Id;
	}
}

static void RunShowCommand(UWorld* World)
{
	// 권위를 요구하지 않는다. 클라이언트 창에서 쳐 보면 값이 비어 있는데,
	// 그것이 곧 "서브시스템은 복제되지 않는다" 를 눈으로 확인하는 방법이다
	const URunProgressSubsystem* Run = GetRunForCheat(World, /*bRequireAuthority=*/false);
	if (!Run)
	{
		return;
	}

	const bool bIsServer = HasServerAuthority(World);

	UE_LOG(LogHeist, Log, TEXT("── 런 진행 (%s) ──"), bIsServer ? TEXT("서버") : TEXT("클라이언트"));
	UE_LOG(LogHeist, Log, TEXT("  팀 골드   $%d"), Run->GetTeamGold());
	UE_LOG(LogHeist, Log, TEXT("  참가자    %d명"), Run->GetRosterNum());

	const TMap<FGameplayTag, int32>& Equipment = Run->GetPurchasedEquipment();
	if (Equipment.IsEmpty())
	{
		UE_LOG(LogHeist, Log, TEXT("  구매 장비 없음"));
	}
	for (const TPair<FGameplayTag, int32>& Pair : Equipment)
	{
		UE_LOG(LogHeist, Log, TEXT("  구매 장비 %s x%d"), *Pair.Key.ToString(), Pair.Value);
	}

	const FGameplayTag LocalRole = Run->GetSelectedRole(GetLocalPlayerId(World));
	UE_LOG(LogHeist, Log, TEXT("  내 역할   %s"),
		LocalRole.IsValid() ? *LocalRole.ToString() : TEXT("(미선택)"));

	if (!bIsServer)
	{
		UE_LOG(LogHeist, Log,
			TEXT("  ※ 클라이언트 서브시스템은 서버와 다른 객체다. 비어 있는 것이 정상이다."));
	}
}

static FAutoConsoleCommandWithWorld GRunShowCommand(
	  TEXT("hh.Run.Show"),
	  TEXT("팀 골드 · 참가자 · 구매 장비 · 내 역할을 찍는다. 클라이언트 창에서도 동작한다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&RunShowCommand),
	  ECVF_Cheat);

static void RunAddGoldCommand(const TArray<FString>& Args, UWorld* World)
{
	URunProgressSubsystem* Run = GetRunForCheat(World);
	if (!Run)
	{
		return;
	}

	const int32 Amount = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
	Run->AddTeamGold(Amount);
}

static FAutoConsoleCommandWithWorldAndArgs GRunAddGoldCommand(
	  TEXT("hh.Run.AddGold"),
	  TEXT("hh.Run.AddGold <액수> — 팀 골드를 증감한다. 음수 가능. 예: hh.Run.AddGold 50000"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunAddGoldCommand),
	  ECVF_Cheat);

// 잔액이 모자랄 때 차감되지 않는지 확인하는 용도다.
// 상점이 붙기 전에 이 규칙이 깨져 있으면 나중에 잔액이 음수인 채로 발견된다
static void RunSpendCommand(const TArray<FString>& Args, UWorld* World)
{
	URunProgressSubsystem* Run = GetRunForCheat(World);
	if (!Run)
	{
		return;
	}

	const int32 Cost = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
	const bool bSpent = Run->TrySpendTeamGold(Cost);

	UE_LOG(LogHeist, Log, TEXT("지출 시도 $%d → %s (잔액 $%d)"),
		Cost, bSpent ? TEXT("성공") : TEXT("실패"), Run->GetTeamGold());
}

static FAutoConsoleCommandWithWorldAndArgs GRunSpendCommand(
	  TEXT("hh.Run.Spend"),
	  TEXT("hh.Run.Spend <비용> — 지출을 시도한다. 잔액이 모자라면 차감하지 않는다"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunSpendCommand),
	  ECVF_Cheat);

static void RunBuyCommand(const TArray<FString>& Args, UWorld* World)
{
	URunProgressSubsystem* Run = GetRunForCheat(World);
	if (!Run)
	{
		return;
	}

	if (!Args.IsValidIndex(0))
	{
		UE_LOG(LogHeist, Warning, TEXT("사용법: hh.Run.Buy Equipment.RubberShoes [수량]"));
		return;
	}

	// 태그가 .ini 에 없으면 무효로 잡힌다 — 오타를 여기서 걸러 준다
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound=*/false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogHeist, Warning, TEXT("'%s' 는 등록된 태그가 아닙니다."), *Args[0]);
		return;
	}

	const int32 Count = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 1;
	Run->AddPurchasedEquipment(Tag, Count);
}

static FAutoConsoleCommandWithWorldAndArgs GRunBuyCommand(
	  TEXT("hh.Run.Buy"),
	  TEXT("hh.Run.Buy <Equipment.태그> [수량] — 구매 목록에 넣는다. 골드는 차감하지 않는다"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunBuyCommand),
	  ECVF_Cheat);

static void RunRoleCommand(const TArray<FString>& Args, UWorld* World)
{
	URunProgressSubsystem* Run = GetRunForCheat(World);
	if (!Run)
	{
		return;
	}

	if (!Args.IsValidIndex(0))
	{
		UE_LOG(LogHeist, Warning, TEXT("사용법: hh.Run.Role Role.Brute"));
		return;
	}

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound=*/false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogHeist, Warning, TEXT("'%s' 는 등록된 태그가 아닙니다."), *Args[0]);
		return;
	}

	const bool bSelected = Run->TrySelectRole(GetLocalPlayerId(World), Tag);

	UE_LOG(LogHeist, Log, TEXT("역할 선택 시도 %s → %s"),
		*Tag.ToString(), bSelected ? TEXT("확정") : TEXT("거부"));
}

static FAutoConsoleCommandWithWorldAndArgs GRunRoleCommand(
	  TEXT("hh.Run.Role"),
	  TEXT("hh.Run.Role <Role.태그> — 0번 로컬 플레이어의 역할을 정한다. 한 번 정하면 거부된다"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunRoleCommand),
	  ECVF_Cheat);

static void RunNewRunCommand(UWorld* World)
{
	if (URunProgressSubsystem* Run = GetRunForCheat(World))
	{
		Run->BeginNewRun();
	}
}

static FAutoConsoleCommandWithWorld GRunNewRunCommand(
	  TEXT("hh.Run.NewRun"),
	  TEXT("판 하나를 새로 시작한다. 명단·장비를 비우고 팀 골드와 역할은 남긴다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&RunNewRunCommand),
	  ECVF_Cheat);

static void RunResetCampaignCommand(UWorld* World)
{
	if (URunProgressSubsystem* Run = GetRunForCheat(World))
	{
		Run->ResetCampaign();
	}
}

static FAutoConsoleCommandWithWorld GRunResetCampaignCommand(
	  TEXT("hh.Run.ResetCampaign"),
	  TEXT("새 방을 연다. 팀 골드와 역할까지 전부 비운다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&RunResetCampaignCommand),
	  ECVF_Cheat);
