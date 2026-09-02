// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "InputActionValue.h"
#include "GameplayTagContainer.h"   // FGameplayTag — OnSprintTagChanged 시그니처에 값으로 쓴다
#include "BaseCharacter.generated.h"

USTRUCT(BlueprintType)
struct FAbilityInputBinding
{
	GENERATED_BODY()

public:
	// 에디터에서 지정할 입력 액션 (예: IA_Interact)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Input")
	TObjectPtr<class UInputAction> InputAction;

	// 그 입력에 대응할 어빌리티 클래스 (예: GAB_Interact)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Input")
	TSubclassOf<class UGameplayAbility> AbilityClass;
};

// 지금 중량형 노획물을 어떤 방식으로 운반 중인가. AnimBP 가 Idle/Walk 블렌드스페이스를
// 고를 때 폴링하는 용도 — crouch 가 ACharacter::bIsCrouched 를 폴링하는 것과 같은 패턴이다.
// 주 운반자 쪽 None<->Solo 전환은 SetHeldActor 가, Coop 전환은 GA_HeavyCarryAssist 가 맡는다.
UENUM(BlueprintType)
enum class EHeavyCarryState : uint8
{
	None, // 중량형을 들고 있지 않음
	Solo, // 혼자 운반 중
	Coop  // 2인 협력 운반 중
};

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UNoiseEmitterComponent;

UCLASS()
class HEAVYHANDED_API ABaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter();

	// IAbilitySystemInterface 필수 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	/**
	 * 컨트롤러가 바뀌었다. **입력 매핑 컨텍스트를 붙이는 자리다.**
	 *
	 * [왜 BeginPlay 가 아닌가] GameMode 가 스폰하는 폰은 BeginPlay 시점에 아직 빙의 전이라
	 *   GetController() 가 nullptr 이다 — 엔진이 SpawnDefaultPawnFor(→ BeginPlay) 를 먼저 하고
	 *   FinishRestartPlayer 에서 Possess 하기 때문이다. BeginPlay 에서 붙이면 그 경로에서는
	 *   매핑이 영영 안 붙고, **마우스도 키도 전혀 안 먹는다.**
	 *
	 *   레벨에 배치한 캐릭터(Auto Possess)는 빙의가 PreInitializeComponents 에서 일어나
	 *   BeginPlay 보다 먼저라 문제가 드러나지 않았다. 그래서 개발용 테스트 맵에서는 멀쩡하고
	 *   진입점 스폰을 쓰는 작업 레벨(저택)에서만 터졌다.
	 *
	 * 서버·클라이언트 양쪽에서, 빙의와 빙의 해제 때 모두 불린다.
	 */
	virtual void NotifyControllerChanged() override;

	/**
	 * 로컬 플레이어의 입력을 세운다 — 매핑 컨텍스트 부착 + 게임플레이 입력 모드 복구.
	 * 로컬 플레이어가 아니면 조용히 아무것도 하지 않는다.
	 */
	void SetupLocalPlayerInput();

	// --- 카메라 및 시점 컴포넌트 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// 소음 발행 — 노획물 충돌 소음과 같은 컴포넌트다. 캐릭터는 물리 충돌 경로(HandleHit)
	// 대신 ReportTaggedNoise 를 직접 불러 쓴다 (걷기/뛰기는 물리 충돌이 아니라서).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Noise")
	TObjectPtr<UNoiseEmitterComponent> NoiseEmitter;

	// --- Enhanced Input Actions ---
	// 에디터에서 블루프린트로 생성한 Input Action 에셋을 할당할 수 있습니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_MoveForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_MoveRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Turn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_LookUp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Crouch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Jump;

	UPROPERTY(EditAnywhere, Category = "Input") 
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 추가 부분 08.18 start **/
	// --- 2인 운반 (Heavy Loot) — 상태만 들고 있고, 판정 로직은 GA_HeavyCarryAssist 가 담당 ---
	UPROPERTY(ReplicatedUsing = OnRep_HeavyCarryAssistant)
	TObjectPtr<ABaseCharacter> HeavyCarryAssistant;

	UPROPERTY(ReplicatedUsing = OnRep_AssistingPrimaryCarrier)
	TObjectPtr<ABaseCharacter> AssistingPrimaryCarrier;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> AssistedHeavyItem;

	// AnimBP 폴링용 — crouch 의 bIsCrouched 와 같은 성격이라 OnRep 콜백은 필요 없다.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Interaction")
	EHeavyCarryState HeavyCarryState = EHeavyCarryState::None;

	UFUNCTION()
	void OnRep_HeavyCarryAssistant();

	UFUNCTION()
	void OnRep_AssistingPrimaryCarrier();

	UPROPERTY(EditDefaultsOnly, Category = "Carry|Heavy")
	TSubclassOf<UGameplayEffect> SoloHeavyCarryPenaltyEffectClass;

	// GAB_Interact 로는 못 누르고, 이벤트로만 발동되는 어빌리티들 (예: GA_HeavyCarryAssist)
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> EventTriggeredAbilities;

public:


	ABaseCharacter* GetHeavyCarryAssistant() const { return HeavyCarryAssistant; }
	void SetHeavyCarryAssistant(ABaseCharacter* InAssistant) { HeavyCarryAssistant = InAssistant; }
	void ClearHeavyCarryAssistant() { HeavyCarryAssistant = nullptr; }

	void SetAssistingPrimaryCarrier(ABaseCharacter* InPrimary, AActor* InItem) { AssistingPrimaryCarrier = InPrimary; AssistedHeavyItem = InItem; }
	void ClearAssistingPrimaryCarrier() { AssistingPrimaryCarrier = nullptr; AssistedHeavyItem = nullptr; }
	AActor* GetAssistedHeavyItem() const { return AssistedHeavyItem; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	EHeavyCarryState GetHeavyCarryState() const { return HeavyCarryState; }
	void SetHeavyCarryState(EHeavyCarryState NewState) { HeavyCarryState = NewState; }

	void ApplyHeavySoloPenalty() { ApplyGameplayEffectToSelf(SoloHeavyCarryPenaltyEffectClass); }
	void RemoveHeavySoloPenalty() { RemoveGameplayEffectFromSelf(SoloHeavyCarryPenaltyEffectClass); }

	/** 추가 부분 08.18 end **/

	// --- 입력 처리 함수 (Enhanced Input 방식) ---
	// 2차원 축 입력 (WASD 이동 등)
	void MoveForward(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);

	// 마우스 회전 입력 등
	void Turn(const FInputActionValue& Value);
	void LookUp(const FInputActionValue& Value);


protected:
	// 클라이언트가 입력했을 때 서버로 요청을 보내는 함수
	UFUNCTION(Server, Reliable)
	void Server_ApplyGameplayEffect(TSubclassOf<class UGameplayEffect> EffectClass, bool bApply);

	// 2. 언리얼 빌드 시스템이 찾는 내부 구현체 함수 (_Implementation 필수 붙이기)
	//void Server_ApplyGameplayEffect_Implementation(TSubclassOf<class UGameplayEffect> EffectClass, bool bApply);

	// 기존 입력 함수 수정
	virtual void StartCrouch(const FInputActionValue& Value);
	virtual void StopCrouch(const FInputActionValue& Value);
	virtual void StartSprint(const FInputActionValue& Value);
	virtual void StopSprint(const FInputActionValue& Value);

protected:
	// MovementSpeed 속성이 변할 때 호출될 콜백 함수
	virtual void OnMovementSpeedChanged(const struct FOnAttributeChangeData& Data);

	// ASC 초기화 시 속성 바인딩을 수행할 함수
	void BindAttributeDelegates();

	// 입력에 따라 적용할 달리기/앉기 Gameplay Effect 클래스 (블루프린트에서 지정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> SprintGameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> CrouchGameplayEffectClass;

	// State.Sprinting 태그가 붙고 떨어질 때 호출된다 (ASC->RegisterGameplayTagEvent 구독).
	// SprintGameplayEffectClass 가 이 태그를 부여하도록 만들어져 있어야 동작한다.
	void OnSprintTagChanged(const FGameplayTag Tag, int32 NewCount);

	// SprintNoiseTimerHandle 이 반복 호출하는 함수. Noise.Player.Run 을 발행한다.
	void EmitSprintNoise();

	// Sprint 중 소음을 반복 발행하는 타이머. State.Sprinting 이 붙어 있는 동안만 돈다.
	FTimerHandle SprintNoiseTimerHandle;

	// Sprint 소음 발행 간격(초). 발소리 박자에 맞춰 디자이너가 튜닝할 수 있게 노출한다.
	// 나중에 발소리 애님 노티파이로 옮기면 이 타이머 자체가 필요 없어진다.
	UPROPERTY(EditDefaultsOnly, Category = "Noise", meta = (ClampMin = "0.05", Units = "s"))
	float SprintNoiseInterval = 1.0f;

public:
	void ApplyGameplayEffectToSelf(TSubclassOf<class UGameplayEffect> EffectClass);
	void RemoveGameplayEffectFromSelf(TSubclassOf<class UGameplayEffect> EffectClass);

protected:
	// ★ [통합됨] 에디터 디테일 패널에서 입력과 스킬을 1:1로 매핑하는 리스트 (DefaultAbilities 삭제됨)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Abilities")
	TArray<FAbilityInputBinding> AbilityInputBindings;

	// 인프라 입력 감지 시 실행될 콜백 함수
	//void AbilityInputPressed(TSubclassOf<class UGameplayAbility> AbilityClass);
	void AbilityInputPressed(int32 InputID);
	void AbilityInputReleased(int32 InputID);

protected:
	// 현재 손에 들고 있는 액터.
	// 물리/콜리전 토글도 부착도 복제되지 않는 로컬 호출이므로, OnRep 에서
	// 클라이언트가 서버와 완전히 같은 순서를 다시 밟는다.
	UPROPERTY(ReplicatedUsing = OnRep_HeldActor, VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> HeldActor = nullptr;

	UFUNCTION()
	virtual void OnRep_HeldActor(AActor* PreviousHeldActor);

	// GAB_Interact 의 부활 채널링 진행률(0~1). 지금은 빈 훅 — UI는 나중에 연결한다.
	UPROPERTY(ReplicatedUsing = OnRep_ReviveProgress, BlueprintReadOnly, Category = "State")
	float ReviveProgress = 0.f;

	UFUNCTION()
	void OnRep_ReviveProgress();

	// 물건을 붙일 손 소켓 이름.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	FName CarrySocketName = TEXT("Hand_R_Socket");

	/**
	 * 중량형을 들고 있는 동안(솔로·협동 둘 다) 마우스 좌우 회전 입력에 곱하는 배율.
	 *
	 * 무빙아웃처럼 "마우스로 홱 돌리기보다 좌우 이동으로 방향을 맞춘다"를 유도하려는
	 * 값이다. 0으로 완전히 막으면(하드 클램프) 답답해지고 조작감도 나빠지므로,
	 * 대신 감도만 크게 죽인다 — 여전히 돌아는 가지만 느리다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeavyCarryYawInputScale = 0.15f;

	// 방금 던진 물건을 이 시간 동안은 본인이 다시 잡을 수 없다.
	//
	// 던지고 곧바로 낚아채는 것을 반복하면 중량형의 "2인 필수" 규칙(Loot.ini)과
	// 착지 소음·파손 판정을 통째로 우회할 수 있다. 동료에게 패스하는 것은 막지 않도록
	// 던진 본인에게만, 짧게 건다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "s"))
	float RecatchBlockSeconds = 0.75f;

	// 운반 상태를 실제로 적용한다. 물리/콜리전 게이팅과 부착/분리를 한 묶음으로 처리하며
	// 순서가 중요하다 — 런타임에는 물리 시뮬레이션 중인 컴포넌트를 웰드 없이 부착할 수 없다
	// (엔진 SceneComponent.cpp:2151, 이중 트랜스폼 갱신 방지). 반드시 물리를 먼저 끈다.
	//
	// 서버와 OnRep 이 이 함수 하나만 쓰도록 해 양쪽 순서를 동일하게 맞춘다.
	void ApplyCarryState(AActor* Target, bool bCarried);

	// 운반 가능한 상태인지 검사한다(루트 컴포넌트 존재, Mobility 가 Movable).
	bool CanCarryActor(const AActor* Target) const;

	/**
	 * 중량형을 들고 있는 동안 매 틱, 물건의 강체 트랜스폼을 계산해 직접 옮긴다.
	 *
	 * [왜 여기 있나] ALootBase::ComputeHeavyCarryTransform 은 순수 함수다. 그 계산을
	 * 언제·누가 돌리느냐가 LootHeavyComponent.h 의 2026-08-20 결정 사항이다 — 폰을 실제로
	 * 옮기는 CharacterMovementComponent 의 예측 경로를 쥔 쪽(여기)이 계산해야, 서버·클라
	 * 두 벌의 손 위치가 어긋나지 않는다.
	 *
	 * [누가 실행하나] HeldActor 가 채워진 주 운반자만 실행한다. 보조자는 자기 손 위치를
	 * 제공하기만 할 뿐 물건을 직접 옮기지 않는다 — 둘 다 SetActorLocation 을 부르면
	 * 서로 다른 프레임에 값을 덮어써 물건이 떨린다.
	 */
	void UpdateHeavyCarryTransform();

public:
	// HeldActor를 반환하는 함수
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetHeldActor() const { return HeldActor; }

	// 운반 대상을 설정한다. 서버 권한 전용.
	// 물리/콜리전 게이팅과 부착/분리까지 전부 여기서 처리하므로 호출하는 쪽은
	// 따로 AttachToComponent / DetachFromActor 를 부르지 않는다.
	// 실패하면(부착 거부, Mobility 부적합 등) false 를 돌려주고 상태를 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	//bool SetHeldActor(AActor* NewHeldActor);
	bool SetHeldActor(AActor* NewHeldActor, bool bIsHeavyLoot = false);

	// 방금 던진 물건을 RecatchBlockSeconds 동안 다시 잡지 못하게 한다.
	// 던지기 어빌리티가 임펄스를 준 뒤 호출한다. 놓기(Drop)에는 걸지 않는다 —
	// 그쪽은 우회 수단이 되지 않고, 막으면 조작감만 나빠진다.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void BlockRecatch(AActor* ThrownActor);

private:
	// 재획득 금지 대상과 던진 시각. 서버 권한 로직에서만 쓰이므로 복제하지 않는다.
	UPROPERTY()
	TObjectPtr<AActor> RecentlyThrownActor = nullptr;

	float RecentlyThrownTime = -1.f;

public:
	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsCarryingHeavyItem() const;

	// Heavy 캐리 포즈(BS_Haevy_Brute)로 안 가는 나머지 전부 — 즉 무언가를 들고 있지만
	// 무겁지는 않은 상태. AnimBP 의 일반(한손) 캐리 Idle/Walk/Run 분기용 폴링 지점이다.
	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsCarryingLightLoot() const;

	// State.Downed 태그 보유 여부를 ASC에서 조회한다. 새 태그를 만들지 않고
	// 이미 있는 State.Downed 를 조회만 하는 헬퍼 — AnimBP 폴링 지점으로도 쓴다.
	UFUNCTION(BlueprintPure, Category = "State")
	bool IsDowned() const;

	// GAB_Interact 의 부활 채널링 진행률(0~1). UI는 나중에 연결 — 지금은 값만 존재한다.
	UFUNCTION(BlueprintCallable, Category = "State")
	void SetReviveProgress(float NewProgress);
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
