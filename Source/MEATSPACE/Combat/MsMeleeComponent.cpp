#include "Combat/MsMeleeComponent.h"

#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

UMsMeleeComponent::UMsMeleeComponent()
{
	// Tick only while a swing is in flight - see BeginSwing / TickComponent.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
}

void UMsMeleeComponent::StartSwing()
{
	if (IsSwinging())
	{
		return;
	}

	// Run locally for immediate feel...
	BeginSwing();

	// ...and tell the server, which runs its own authoritative copy. On a listen-server host
	// this executes inline and is ignored by the IsSwinging() guard above.
	ServerSwing();
}

void UMsMeleeComponent::ServerSwing_Implementation()
{
	if (IsSwinging())
	{
		return;
	}

	BeginSwing();
}

void UMsMeleeComponent::BeginSwing()
{
	Phase = EMsSwingPhase::Windup;
	PhaseTime = 0.0f;
	SwingAlpha = 0.0f;
	HitActorsThisSwing.Reset();

	SetComponentTickEnabled(true);
	PlaySwingMontage();
}

void UMsMeleeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Phase == EMsSwingPhase::Idle)
	{
		SetComponentTickEnabled(false);
		return;
	}

	PhaseTime += DeltaTime;

	switch (Phase)
	{
	case EMsSwingPhase::Windup:
	{
		if (PhaseTime >= WindupTime)
		{
			Phase = EMsSwingPhase::Active;
			PhaseTime = 0.0f;
			SwingAlpha = 0.0f;
		}
		break;
	}

	case EMsSwingPhase::Active:
	{
		const float PrevAlpha = SwingAlpha;
		SwingAlpha = FMath::Clamp(PhaseTime / FMath::Max(ActiveTime, 0.01f), 0.0f, 1.0f);

		// Sweep the segment the blade covered since last frame. Doing it as a segment rather
		// than a point test is what stops a fast swing tunnelling past a target.
		SweepSegment(PrevAlpha, SwingAlpha);

		if (PhaseTime >= ActiveTime)
		{
			Phase = EMsSwingPhase::Recovery;
			PhaseTime = 0.0f;
		}
		break;
	}

	case EMsSwingPhase::Recovery:
	{
		if (PhaseTime >= RecoveryTime)
		{
			Phase = EMsSwingPhase::Idle;
			PhaseTime = 0.0f;
			HitActorsThisSwing.Reset();
			SetComponentTickEnabled(false);
		}
		break;
	}

	default:
		break;
	}
}

FVector UMsMeleeComponent::GetBladePoint(float Alpha) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	// Sweep from one side of the arc to the other, relative to where the character faces.
	const float Angle = FMath::Lerp(-ArcDegrees * 0.5f, ArcDegrees * 0.5f, Alpha);
	const FVector LocalDir = FRotator(0.0f, Angle, 0.0f).Vector();
	const FVector WorldDir = Owner->GetActorRotation().RotateVector(LocalDir);

	return Owner->GetActorLocation() + FVector(0.0f, 0.0f, HeightOffset) + WorldDir * Reach;
}

void UMsMeleeComponent::SweepSegment(float PrevAlpha, float NewAlpha)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	const FVector Start = GetBladePoint(PrevAlpha);
	const FVector End = GetBladePoint(NewAlpha);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MsMeleeSweep), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Owner);

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(BladeRadius), Params);

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == Owner)
		{
			continue;
		}

		// One hit per actor per swing.
		if (HitActorsThisSwing.Contains(HitActor))
		{
			continue;
		}
		HitActorsThisSwing.Add(HitActor);

		// Only the server's copy of this component actually hurts anything.
		if (Owner->HasAuthority())
		{
			APawn* OwnerPawn = Cast<APawn>(Owner);
			const FVector HitDir = (End - Start).GetSafeNormal();
			FPointDamageEvent DamageEvent(Damage, Hit, HitDir, nullptr);
			HitActor->TakeDamage(Damage, DamageEvent,
				OwnerPawn ? OwnerPawn->GetController() : nullptr, Owner);
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebugSwing)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, BladeRadius * 0.6f, 8, FColor::Green, false, 0.4f);
		}
#endif
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugSwing)
	{
		DrawDebugLine(World, Start, End, FColor::Cyan, false, 0.25f, 0, 3.0f);
	}
#endif
}

void UMsMeleeComponent::PlaySwingMontage()
{
	if (!SwingMontage)
	{
		return;
	}

	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				AnimInstance->Montage_Play(SwingMontage);
			}
		}
	}
}
