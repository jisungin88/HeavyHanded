// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseCharacter.h"
#include "PlayerSessionState.h"
#include "BaseAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    // --- 1인칭 카메라 설정 (스프링 암 관련 코드는 제거) ---
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(RootComponent);
    FollowCamera->bUsePawnControlRotation = true; // 마우스 회전에 따라 카메라가 같이 회전

    // 카메라 위치를 캐릭터의 눈높이(약 Z축 64cm 위)로 설정
    FollowCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));

    // --- 캐릭터 이동 및 회전 방향 설정 ---
    bUseControllerRotationYaw = true; // 1인칭은 시선과 몸통 방향을 일치시키기 위해 true로 설정
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

    if (GetController() == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Controller is nullptr!"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Controller"));
    }

	// ★ 클라이언트든 서버든 '로컬 플레이어'인 경우에만 입력 매핑을 추가해야 합니다.
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        UE_LOG(LogTemp, Log, TEXT("True"));
        // 이 캐릭터를 조종하는 로컬 플레이어인지 확인 (AI나 다른 플레이어 소유일 때 오류 방지)
        if (PlayerController->IsLocalController())
        {
            if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
                {
                    // 에디터에서 UPROPERTY로 IMC를 할당해 둔 경우 (예: DefaultMappingContext)
                    if (DefaultMappingContext)
                    {
                        Subsystem->AddMappingContext(DefaultMappingContext, 0);
                    }
                }
            }
        }
    }
}

void ABaseCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // 서버 권한에서 AbilityInputBindings에 등록된 모든 스킬들을 ASC에 부여합니다.
    if (HasAuthority())
    {
        APlayerSessionState* SessionState = GetPlayerState<APlayerSessionState>();
        if (SessionState)
        {
            UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
            if (ASC)
            {
                ASC->InitAbilityActorInfo(SessionState, this);

                for (const FAbilityInputBinding& Binding : AbilityInputBindings)
                {
                    if (Binding.AbilityClass)
                    {
                        ASC->GiveAbility(FGameplayAbilitySpec(Binding.AbilityClass, 1, 0, this));
                    }
                }
            }
        }
    }

    // 서버 측에서 컨트롤러 소유가 끝났을 때 바인딩 실행
    BindAttributeDelegates();
}

// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // 전후 이동 바인딩 (W, S)
        if (IA_MoveForward)
        {
            EnhancedInputComponent->BindAction(IA_MoveForward, ETriggerEvent::Triggered, this, &ABaseCharacter::MoveForward);
        }

        // 좌우 이동 바인딩 (A, D)
        if (IA_MoveRight)
        {
            EnhancedInputComponent->BindAction(IA_MoveRight, ETriggerEvent::Triggered, this, &ABaseCharacter::MoveRight);
        }

        // Turn (좌우) 바인딩
        if (IA_Turn)
        {
            EnhancedInputComponent->BindAction(IA_Turn, ETriggerEvent::Triggered, this, &ABaseCharacter::Turn);
        }

        // LookUp (상하) 바인딩
        if (IA_LookUp)
        {
            EnhancedInputComponent->BindAction(IA_LookUp, ETriggerEvent::Triggered, this, &ABaseCharacter::LookUp);
        }

        if (IA_Sprint)
        {
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Started, this, &ABaseCharacter::StartSprint);
            EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ABaseCharacter::StopSprint);
        }
        if (IA_Crouch)
        {
            EnhancedInputComponent->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ABaseCharacter::StartCrouch);
            EnhancedInputComponent->BindAction(IA_Crouch, ETriggerEvent::Completed, this, &ABaseCharacter::StopCrouch);
        }

        for (const FAbilityInputBinding& Binding : AbilityInputBindings)
        {
            if (Binding.InputAction && Binding.AbilityClass)
            {
                EnhancedInputComponent->BindAction(
                    Binding.InputAction,
                    ETriggerEvent::Triggered,
                    this,
                    &ABaseCharacter::AbilityInputPressed,
                    Binding.AbilityClass   // ← 페이로드로 전달, 클로저처럼 각 바인딩마다 고정됨
                );
            }
        }
    }
}

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const
{
    // --- 캐릭터 본인 대신 PlayerSessionState의 ASC를 리턴하도록 변경 ---
    if (APlayerSessionState* SessionState = GetPlayerState<APlayerSessionState>())
    {
        return SessionState->GetAbilitySystemComponent();
    }
    return nullptr;
}

// 2. 전진/후진 처리 함수 (Axis 1D 값 활용)
void ABaseCharacter::MoveForward(const FInputActionValue& Value)
{
    const float DirectionValue = Value.Get<float>();

    if (Controller != nullptr && DirectionValue != 0.0f)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

        AddMovementInput(Direction, DirectionValue);
    }
}

// 3. 좌우 이동 처리 함수 (Axis 1D 값 활용)
void ABaseCharacter::MoveRight(const FInputActionValue& Value)
{
    const float DirectionValue = Value.Get<float>();

    if (Controller != nullptr && DirectionValue != 0.0f)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        AddMovementInput(Direction, DirectionValue);
    }
}

void ABaseCharacter::Turn(const FInputActionValue& Value)
{
    float AxisValue = Value.Get<float>();
    if (AxisValue != 0.0f)
    {
        AddControllerYawInput(AxisValue);
    }
}

void ABaseCharacter::LookUp(const FInputActionValue& Value)
{
    float AxisValue = Value.Get<float>();
    if (AxisValue != 0.0f)
    {
        AddControllerPitchInput(AxisValue);
    }
}

// 예시: 캐릭터가 컨트롤러를 소유하거나 ASC가 초기화될 때 호출되는 함수 내부에서 바인딩 실행
void ABaseCharacter::BindAttributeDelegates()
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC) return;

    // ASC로부터 UBaseAttributeSet 가져오기
    const UBaseAttributeSet* BaseAttrSet = ASC->GetSet<UBaseAttributeSet>();
    if (BaseAttrSet)
    {
        // MovementSpeed 속성 변화를 감지하는 델리게이트 구독
        ASC->GetGameplayAttributeValueChangeDelegate(BaseAttrSet->GetMovementSpeedAttribute()).AddUObject(this, &ABaseCharacter::OnMovementSpeedChanged);
    }
}

// 속성이 변경될 때 자동 호출되어 실제 무브먼트 속도에 적용
void ABaseCharacter::OnMovementSpeedChanged(const FOnAttributeChangeData& Data)
{
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        // Data.NewValue는 변경된 MovementSpeed의 새로운 값입니다.
        MoveComp->MaxWalkSpeed = Data.NewValue;
    }
}

void ABaseCharacter::ApplyGameplayEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (ASC && EffectClass)
    {
        FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
        ContextHandle.AddSourceObject(this);
        ASC->ApplyGameplayEffectToSelf(EffectClass.GetDefaultObject(), 1.0f, ContextHandle);
    }
}

void ABaseCharacter::RemoveGameplayEffectFromSelf(TSubclassOf<UGameplayEffect> EffectClass)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (ASC && EffectClass)
    {
        ASC->RemoveActiveGameplayEffectBySourceEffect(EffectClass, ASC);
    }
}

void ABaseCharacter::StartCrouch(const FInputActionValue& Value)
{
    Server_ApplyGameplayEffect(CrouchGameplayEffectClass, true);
}

void ABaseCharacter::StopCrouch(const FInputActionValue& Value)
{
    Server_ApplyGameplayEffect(CrouchGameplayEffectClass, false);
}

void ABaseCharacter::StartSprint(const FInputActionValue& Value)
{
    Server_ApplyGameplayEffect(SprintGameplayEffectClass, true);
}

void ABaseCharacter::StopSprint(const FInputActionValue& Value)
{
    Server_ApplyGameplayEffect(SprintGameplayEffectClass, false);
}

// --- 서버 RPC 실제 동작 구현 ---
void ABaseCharacter::Server_ApplyGameplayEffect_Implementation(TSubclassOf<UGameplayEffect> EffectClass, bool bApply)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC || !EffectClass) return;

    if (bApply)
    {
        FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
        ContextHandle.AddSourceObject(this);
        ASC->ApplyGameplayEffectToSelf(EffectClass.GetDefaultObject(), 1.0f, ContextHandle);
    }
    else
    {
        // 제거할 때 핸들 방식이거나 소스 이펙트 방식 사용
        ASC->RemoveActiveGameplayEffectBySourceEffect(EffectClass, ASC);
    }
}

void ABaseCharacter::AbilityInputPressed(TSubclassOf<UGameplayAbility> AbilityClass)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC || !AbilityClass) return;

    ASC->TryActivateAbilityByClass(AbilityClass);
}

void ABaseCharacter::Multicast_AttachItem_Implementation(AActor* ItemToAttach)
{
    if (!ItemToAttach) return;

    // 1. 물리 시뮬레이션 및 콜리전 끄기
    if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(ItemToAttach->GetRootComponent()))
    {
        PrimComp->SetSimulatePhysics(false);
        PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 2. 캐릭터 손 소켓에 부착
    ItemToAttach->AttachToComponent(
        GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        FName("Hand_R_Socket")
    );

    // 3. 들고 있는 액터로 기억
    HeldActor = ItemToAttach;
}

void ABaseCharacter::DropItem()
{
    // 만약 내가 서버라면 바로 멀티캐스트를 태워 실행하고,
    if (HasAuthority())
    {
        Multicast_DropItem();
    }
    // 만약 내가 클라이언트라면 서버에게 "나 아이템 버릴래!" 하고 요청(Server RPC)을 보냅니다.
    else
    {
        Server_DropItem();
    }
}

// 1단계: 클라이언트의 요청을 받아 서버에서 실행되는 함수
void ABaseCharacter::Server_DropItem_Implementation()
{
    // 서버가 권한을 가지고 안전하게 멀티캐스트를 호출합니다.
    Multicast_DropItem();
}

void ABaseCharacter::Multicast_DropItem_Implementation()
{
    if (!HeldActor) return;

    // 1. 손에서 떼어내기 (월드 좌표계 유지)
    FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
    HeldActor->DetachFromActor(DetachRules);

    // 2. 물리 엔진 다시 켜기 (바닥으로 툭 떨어지게)
    if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(HeldActor->GetRootComponent()))
    {
        PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        PrimComp->SetSimulatePhysics(true);
    }

    // 3. 들고 있던 액터 비우기
    HeldActor = nullptr;
    UE_LOG(LogTemp, Log, TEXT("Item Drop (Multicast)"));
}
