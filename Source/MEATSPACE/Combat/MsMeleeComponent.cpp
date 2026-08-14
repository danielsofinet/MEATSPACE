#include "Combat/MsMeleeComponent.h"

#include "Animation/AnimMontage.h"
#include "Character/MsCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

UMsMeleeComponent::UMsMeleeComponent()
{
	// Tick only while a swing is in flight - see BeginSwing / TickComponent.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);

	// Default chain: right, then left, then a full spin finisher.
	FMsSwingStep Right;
	Right.ArcDegrees = 160.0f;
	Right.bRightToLeft = false;
	Right.DamageMultiplier = 1.0f;

	FMsSwingStep Left;
	Left.ArcDegrees = 160.0f;
	Left.bRightToLeft = true;
	Left.DamageMultiplier = 1.0f;

	FMsSwingStep Spin;
	Spin.ArcDegrees = 360.0f;
	Spin.bRightToLeft = false;
	Spin.DamageMultiplier = 1.5f;
	Spin.ReachMultiplier = 1.15f;
	Spin.WindupTime = 0.14f;
	Spin.ActiveTime = 0.34f;
	Spin.RecoveryTime = 0.40f;

	ComboSteps = { Right, Left, Spin };
}

const FMsSwingStep& UMsMeleeComponent::GetCurrentStep() const
{
	static const FMsSwingStep Fallback;

	if (ComboSteps.IsValidIndex(ComboIndex))
	{
		return ComboSteps[ComboIndex];
	}
	return Fallback;
}

void UMsMeleeComponent::StartSwing()
{
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	// On a listen-server host we ARE the authority, so BeginSwing below is already the
	// authoritative swing. Calling ServerSwing as well would execute inline, see a swing
	// already in progress, and queue the next combo step off a single click.
	const bool bHasAuthority = Owner->HasAuthority();

	if (IsSwinging())
	{
		// Mid-swing click: queue the next link in the chain rather than dropping the input.
		bComboQueued = true;

		// A remote client must tell the server about queued clicks too, or the server's copy
		// finishes the current swing and stops while the client carries on into the next link.
		if (!bHasAuthority)
		{
			ServerSwing(static_cast<uint8>(ComboIndex + 1));
		}
		return;
	}

	// Chain continues only if the click lands inside the combo window.
	const bool bChainAlive = (World->GetTimeSeconds() - LastSwingEndTime) <= ComboResetTime;
	const int32 NextIndex = bChainAlive ? ComboIndex : 0;

	BeginSwing(NextIndex);

	if (!bHasAuthority)
	{
		ServerSwing(static_cast<uint8>(NextIndex));
	}
}

void UMsMeleeComponent::ServerSwing_Implementation(uint8 StepIndex)
{
	if (IsSwinging())
	{
		bComboQueued = true;
		return;
	}

	// Trust the client for which step it thinks it is on, but never past the end of the chain.
	BeginSwing(FMath::Clamp<int32>(StepIndex, 0, FMath::Max(ComboSteps.Num() - 1, 0)));
}

void UMsMeleeComponent::BeginSwing(int32 StepIndex)
{
	ComboIndex = ComboSteps.IsValidIndex(StepIndex) ? StepIndex : 0;

	Phase = EMsSwingPhase::Windup;
	PhaseTime = 0.0f;
	SwingAlpha = 0.0f;
	bComboQueued = false;
	HitActorsThisSwing.Reset();

	SetComponentTickEnabled(true);
	PlayStepMontage();

	// The weight of the swing itself, felt before anything is hit.
	if (AMsCharacter* OwnerCharacter = Cast<AMsCharacter>(GetOwner()))
	{
		OwnerCharacter->OnSwordSwing();
	}
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

	const FMsSwingStep& Step = GetCurrentStep();
	PhaseTime += DeltaTime;

	switch (Phase)
	{
	case EMsSwingPhase::Windup:
	{
		if (PhaseTime >= Step.WindupTime)
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
		SwingAlpha = FMath::Clamp(PhaseTime / FMath::Max(Step.ActiveTime, 0.01f), 0.0f, 1.0f);

		SweepSlice(PrevAlpha, SwingAlpha);

		if (PhaseTime >= Step.ActiveTime)
		{
			Phase = EMsSwingPhase::Recovery;
			PhaseTime = 0.0f;
		}
		break;
	}

	case EMsSwingPhase::Recovery:
	{
		if (PhaseTime >= Step.RecoveryTime)
		{
			const UWorld* World = GetWorld();
			LastSwingEndTime = World ? World->GetTimeSeconds() : 0.0f;

			const int32 NextIndex = ComboIndex + 1;
			const bool bHasNextStep = ComboSteps.IsValidIndex(NextIndex);

			if (bComboQueued && bHasNextStep)
			{
				// Flow straight into the next link with no gap - this is what makes a combo
				// feel like one motion instead of three separate attacks.
				BeginSwing(NextIndex);
			}
			else
			{
				Phase = EMsSwingPhase::Idle;
				PhaseTime = 0.0f;
				bComboQueued = false;
				HitActorsThisSwing.Reset();

				// Finished the chain, or let it lapse - either way the next swing starts over.
				ComboIndex = bHasNextStep ? NextIndex : 0;

				SetComponentTickEnabled(false);
			}
		}
		break;
	}

	default:
		break;
	}
}

void UMsMeleeComponent::GetStepAngles(float& OutStartAngle, float& OutEndAngle) const
{
	const FMsSwingStep& Step = GetCurrentStep();
	const float Half = Step.ArcDegrees * 0.5f;

	OutStartAngle = Step.bRightToLeft ? Half : -Half;
	OutEndAngle = Step.bRightToLeft ? -Half : Half;
}

float UMsMeleeComponent::SignedAngleToTarget(const FVector& TargetLocation) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0.0f;
	}

	FVector ToTarget = TargetLocation - Owner->GetActorLocation();
	ToTarget.Z = 0.0f;

	FVector Forward = Owner->GetActorForwardVector();
	Forward.Z = 0.0f;

	if (!ToTarget.Normalize() || !Forward.Normalize())
	{
		return 0.0f;
	}

	const float Dot = FVector::DotProduct(Forward, ToTarget);
	const float Cross = Forward.X * ToTarget.Y - Forward.Y * ToTarget.X;

	return FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));
}

void UMsMeleeComponent::SweepSlice(float PrevAlpha, float NewAlpha)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	const FMsSwingStep& Step = GetCurrentStep();
	const float StepReach = Reach * Step.ReachMultiplier;

	float StartAngle, EndAngle;
	GetStepAngles(StartAngle, EndAngle);

	const float PrevAngle = FMath::Lerp(StartAngle, EndAngle, PrevAlpha);
	const float NewAngle = FMath::Lerp(StartAngle, EndAngle, NewAlpha);

	// The angular slice the blade covered this frame, widened by the generosity tolerance.
	const float MinAngle = FMath::Min(PrevAngle, NewAngle) - AngleTolerance;
	const float MaxAngle = FMath::Max(PrevAngle, NewAngle) + AngleTolerance;

	// One vertical cylinder around the character: ground to overhead, out to reach. Everything
	// inside is a candidate; the angle check below decides what the blade actually passed over.
	const FVector Centre = Owner->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MsMeleeSweep), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Owner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Centre, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeCapsule(StepReach, HitHalfHeight), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || HitActor == Owner)
		{
			continue;
		}

		if (HitActorsThisSwing.Contains(HitActor))
		{
			continue;
		}

		const FVector TargetLocation = HitActor->GetActorLocation();

		// Horizontal distance only - vertical coverage is the cylinder's job.
		FVector Flat = TargetLocation - Centre;
		Flat.Z = 0.0f;
		const float FlatDistance = Flat.Size();
		if (FlatDistance > StepReach)
		{
			continue;
		}

		float Angle = SignedAngleToTarget(TargetLocation);

		// Widen the window by however many degrees the target's own bulk subtends, so large
		// clankers are easier to clip than a centre-point test would allow.
		float SizeSlack = 0.0f;
		if (bAccountForTargetSize && FlatDistance > KINDA_SMALL_NUMBER)
		{
			const float TargetRadius = HitActor->GetSimpleCollisionRadius();
			SizeSlack = FMath::RadiansToDegrees(FMath::Atan2(TargetRadius, FlatDistance));
		}

		if (Angle < MinAngle - SizeSlack || Angle > MaxAngle + SizeSlack)
		{
			continue;
		}

		HitActorsThisSwing.Add(HitActor);

		// Connecting should feel heavier than swinging through air. Runs on every copy of
		// the component, but AddCameraShake ignores anyone who is not looking through it.
		if (AMsCharacter* OwnerCharacter = Cast<AMsCharacter>(Owner))
		{
			OwnerCharacter->OnSwordHit();
		}

		// Only the server's copy actually hurts anything.
		if (Owner->HasAuthority())
		{
			APawn* OwnerPawn = Cast<APawn>(Owner);
			const FVector HitDir = (TargetLocation - Centre).GetSafeNormal();

			FHitResult SyntheticHit;
			SyntheticHit.ImpactPoint = TargetLocation;
			SyntheticHit.ImpactNormal = -HitDir;
			SyntheticHit.Location = TargetLocation;

			FPointDamageEvent DamageEvent(Damage * Step.DamageMultiplier, SyntheticHit, HitDir, nullptr);
			HitActor->TakeDamage(Damage * Step.DamageMultiplier, DamageEvent,
				OwnerPawn ? OwnerPawn->GetController() : nullptr, Owner);
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebugSwing)
		{
			DrawDebugSphere(World, TargetLocation, 30.0f, 10, FColor::Green, false, 0.4f);
		}
#endif
	}

	DrawSwingDebug(PrevAngle, NewAngle);
}

void UMsMeleeComponent::DrawSwingDebug(float PrevAngle, float NewAngle) const
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugSwing)
	{
		return;
	}

	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	const FMsSwingStep& Step = GetCurrentStep();
	const float StepReach = Reach * Step.ReachMultiplier;
	const FVector Centre = Owner->GetActorLocation();
	const FRotator OwnerRot = Owner->GetActorRotation();

	const FVector Bottom = Centre - FVector(0.0f, 0.0f, HitHalfHeight);
	const FVector Top = Centre + FVector(0.0f, 0.0f, HitHalfHeight);

	// The leading edge of the blade, drawn full height so the vertical coverage is visible.
	const FVector EdgeDir = OwnerRot.RotateVector(FRotator(0.0f, NewAngle, 0.0f).Vector());
	const FVector EdgeBottom = Bottom + EdgeDir * StepReach;
	const FVector EdgeTop = Top + EdgeDir * StepReach;

	DrawDebugLine(World, Bottom, EdgeBottom, FColor::Cyan, false, 0.25f, 0, 3.0f);
	DrawDebugLine(World, Top, EdgeTop, FColor::Cyan, false, 0.25f, 0, 3.0f);
	DrawDebugLine(World, EdgeBottom, EdgeTop, FColor::Cyan, false, 0.25f, 0, 3.0f);

	// The slice covered this frame, at mid height.
	const FVector PrevDir = OwnerRot.RotateVector(FRotator(0.0f, PrevAngle, 0.0f).Vector());
	DrawDebugLine(World, Centre + PrevDir * StepReach, Centre + EdgeDir * StepReach,
		FColor::Blue, false, 0.5f, 0, 2.0f);
#endif
}

void UMsMeleeComponent::PlayStepMontage()
{
	UAnimMontage* Montage = GetCurrentStep().Montage;
	if (!Montage)
	{
		return;
	}

	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* MeshComp = OwnerCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				AnimInstance->Montage_Play(Montage);
			}
		}
	}
}
