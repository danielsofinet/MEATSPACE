#include "World/MsEncounterVolume.h"

#include "Character/MsCharacter.h"
#include "Clankers/MsClankerBase.h"
#include "Clankers/MsClankerSmall.h"
#include "Combat/MsHealthComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/MsObjectiveSubsystem.h"
#include "World/MsDropPod.h"

AMsEncounterVolume::AMsEncounterVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetBoxExtent(FVector(600.0f, 600.0f, 400.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	SetRootComponent(TriggerBox);

	// Shows which way the volume faces, so the spawn arc is visible while placing it.
	FacingArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FacingArrow"));
	FacingArrow->SetupAttachment(TriggerBox);
	FacingArrow->ArrowSize = 3.0f;
	FacingArrow->ArrowColor = FColor(255, 160, 40);
	FacingArrow->SetHiddenInGame(true);

	// Works out of the box with just small clankers - the onboarding ambush is a mob of them.
	FMsSpawnEntry Small;
	Small.ClankerClass = AMsClankerSmall::StaticClass();
	Small.Weight = 1.0f;
	SpawnTable = { Small };

	// The onboarding ambush: a dropship unloads five, then a second arrives with six. Roughly
	// 30 seconds from "a problem" to "leave now".
	RoundCounts = { 5, 6 };
	RoundInterval = 5.0f;

	DropPodClass = AMsDropPod::StaticClass();
}

void AMsEncounterVolume::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMsEncounterVolume::HandleTriggerOverlap);
	}
}

void AMsEncounterVolume::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Players only - a clanker wandering through must not start the fight.
	if (!Cast<AMsCharacter>(OtherActor))
	{
		return;
	}

	if (bActive || (bCleared && bTriggerOnce))
	{
		return;
	}

	TriggerEncounter();
}

void AMsEncounterVolume::TriggerEncounter()
{
	if (!HasAuthority() || bActive)
	{
		return;
	}

	bActive = true;
	bCleared = false;
	SpawnedSoFar = 0;
	RoundIndex = 0;
	ElapsedTime = 0.0f;
	NextRoundTime = 0.0f;
	AliveClankers.Reset();

	SetActorTickEnabled(true);

	if (!ObjectiveOnTriggered.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			if (UMsObjectiveSubsystem* Objectives = World->GetSubsystem<UMsObjectiveSubsystem>())
			{
				Objectives->SetObjective(ObjectiveOnTriggered, nullptr);
			}
		}
	}

	OnEncounterTriggered.Broadcast(this);

	SpawnRound();
}

void AMsEncounterVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bActive)
	{
		return;
	}

	PruneAliveList();

	ElapsedTime += DeltaSeconds;

	const int32 Alive = AliveClankers.Num();

	if (Pacing == EMsEncounterPacing::TimedEscalation)
	{
		// Rounds arrive because time passed, not because the player earned them. That
		// indifference is the whole feeling: it escalates whether you are winning or not.
		if (RoundCounts.IsValidIndex(RoundIndex) && ElapsedTime >= NextRoundTime)
		{
			SpawnCount(RoundCounts[RoundIndex]);
			++RoundIndex;
			NextRoundTime = ElapsedTime + RoundInterval;
		}

		const bool bOutOfTime = MaxDuration > 0.0f && ElapsedTime >= MaxDuration;
		const bool bAllSpawnedAndDead = !RoundCounts.IsValidIndex(RoundIndex)
			&& Alive == 0 && PendingDeliveries == 0;

		if (bOutOfTime || bAllSpawnedAndDead)
		{
			MarkCleared();
		}

		return;
	}

	// Clear-to-proceed: reinforce on attrition, so the fight paces itself to how fast the
	// player is actually killing things.
	if (SpawnedSoFar < TotalToSpawn)
	{
		if (Alive <= ReinforceWhenAliveAtOrBelow)
		{
			SpawnRound();
		}
	}
	else if (Alive == 0 && PendingDeliveries == 0)
	{
		MarkCleared();
	}

#if ENABLE_DRAW_DEBUG
	if (bShowDebugReadout && GEngine)
	{
		// Cast the key: GetUniqueID() is uint32, ambiguous between the int32 and uint64
		// overloads. Offsetting per-actor gives each encounter its own line.
		const uint64 MessageKey = static_cast<uint64>(9200 + GetUniqueID() % 50);

		GEngine->AddOnScreenDebugMessage(MessageKey, 0.0f, FColor::Emerald,
			FString::Printf(TEXT("ENCOUNTER %s  alive %d   spawned %d / %d"),
				*GetName(), Alive, SpawnedSoFar, TotalToSpawn));
	}
#endif
}

void AMsEncounterVolume::MarkCleared()
{
	bActive = false;
	bCleared = true;
	SetActorTickEnabled(false);

	// A timed encounter that ends while the player is surrounded has to actually end, or
	// "it is over, move on" is a lie and they get chased through the next beat.
	if (bDespawnRemainingOnEnd && Pacing == EMsEncounterPacing::TimedEscalation)
	{
		for (const TWeakObjectPtr<AMsClankerBase>& Entry : AliveClankers)
		{
			if (AMsClankerBase* Clanker = Entry.Get())
			{
				Clanker->Destroy();
			}
		}
	}

	AliveClankers.Reset();

	if (!ObjectiveOnCleared.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			if (UMsObjectiveSubsystem* Objectives = World->GetSubsystem<UMsObjectiveSubsystem>())
			{
				Objectives->SetObjective(ObjectiveOnCleared, ObjectiveTargetOnCleared);
			}
		}
	}

	OnEncounterCleared.Broadcast(this);
}

void AMsEncounterVolume::PruneAliveList()
{
	AliveClankers.RemoveAll([](const TWeakObjectPtr<AMsClankerBase>& Entry)
	{
		const AMsClankerBase* Clanker = Entry.Get();
		if (!Clanker)
		{
			return true;
		}

		const UMsHealthComponent* Health = Clanker->GetHealth();
		return Health && Health->IsDead();
	});
}

int32 AMsEncounterVolume::GetAliveCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AMsClankerBase>& Entry : AliveClankers)
	{
		if (Entry.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

int32 AMsEncounterVolume::CountAliveOfClass(TSubclassOf<AMsClankerBase> ClankerClass) const
{
	if (!ClankerClass)
	{
		return 0;
	}

	int32 Count = 0;
	for (const TWeakObjectPtr<AMsClankerBase>& Entry : AliveClankers)
	{
		if (const AMsClankerBase* Clanker = Entry.Get())
		{
			if (Clanker->GetClass() == ClankerClass)
			{
				++Count;
			}
		}
	}
	return Count;
}

TSubclassOf<AMsClankerBase> AMsEncounterVolume::PickClankerClass() const
{
	const int32 EffectiveWave = RoundIndex + 1;

	auto IsEligible = [this, EffectiveWave](const FMsSpawnEntry& Entry)
	{
		if (!Entry.ClankerClass || Entry.Weight <= 0.0f || EffectiveWave < Entry.FirstWave)
		{
			return false;
		}

		if (Entry.MaxAlive > 0 && CountAliveOfClass(Entry.ClankerClass) >= Entry.MaxAlive)
		{
			return false;
		}

		return true;
	};

	float TotalWeight = 0.0f;
	for (const FMsSpawnEntry& Entry : SpawnTable)
	{
		if (IsEligible(Entry))
		{
			TotalWeight += Entry.Weight;
		}
	}

	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const FMsSpawnEntry& Entry : SpawnTable)
	{
		if (!IsEligible(Entry))
		{
			continue;
		}

		Roll -= Entry.Weight;
		if (Roll <= 0.0f)
		{
			return Entry.ClankerClass;
		}
	}

	return nullptr;
}

bool AMsEncounterVolume::FindSpawnLocation(FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MsEncounterGround), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(this);

	const FVector Centre = GetActorLocation();

	// Spawns are confined to an arc around the volume's forward direction, so rotating the
	// actor in the level decides where reinforcements come from.
	const float BaseYaw = GetActorRotation().Yaw + SpawnArcOffsetDegrees;
	const float HalfArc = FMath::Clamp(SpawnArcDegrees, 10.0f, 360.0f) * 0.5f;

	for (int32 Attempt = 0; Attempt < PlacementAttempts; ++Attempt)
	{
		const float Yaw = BaseYaw + FMath::FRandRange(-HalfArc, HalfArc);
		const float Angle = FMath::DegreesToRadians(Yaw);

		const float Radius = FMath::FRandRange(FMath::Min(MinSpawnRadius, MaxSpawnRadius),
			FMath::Max(MinSpawnRadius, MaxSpawnRadius));

		const FVector Candidate = Centre + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);

		// Must land on real ground. On a sculpted landscape this also keeps clankers out of
		// hillsides and off cliff edges.
		const FVector TraceStart = Candidate + FVector(0.0f, 0.0f, GroundTraceDistance * 0.5f);
		const FVector TraceEnd = Candidate - FVector(0.0f, 0.0f, GroundTraceDistance);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			OutLocation = Hit.ImpactPoint + FVector(0.0f, 0.0f, SpawnHeightOffset);
			return true;
		}
	}

	return false;
}

void AMsEncounterVolume::SpawnOne(TSubclassOf<AMsClankerBase> ClankerClass)
{
	UWorld* World = GetWorld();
	if (!World || !ClankerClass)
	{
		return;
	}

	FVector SpawnLocation;
	if (!FindSpawnLocation(SpawnLocation))
	{
		return;
	}

	FVector ToCentre = GetActorLocation() - SpawnLocation;
	ToCentre.Z = 0.0f;
	const FRotator SpawnRotation = ToCentre.IsNearlyZero() ? FRotator::ZeroRotator : ToCentre.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = this;

	if (AMsClankerBase* Spawned = World->SpawnActor<AMsClankerBase>(ClankerClass, SpawnLocation, SpawnRotation, SpawnParams))
	{
		AliveClankers.Add(Spawned);
		++SpawnedSoFar;
	}
}

void AMsEncounterVolume::SpawnRound()
{
	++RoundIndex;

	const int32 Remaining = FMath::Max(0, TotalToSpawn - SpawnedSoFar);
	SpawnCount(FMath::Min(SpawnPerRound, Remaining));
}

void AMsEncounterVolume::SpawnCount(int32 Count)
{
	if (Count <= 0)
	{
		return;
	}

	if (bDeliverByDropPod && DropPodClass)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		// Split the round across as many pods as it takes.
		int32 Remaining = Count;
		while (Remaining > 0)
		{
			const int32 ThisPod = FMath::Min(Remaining, FMath::Max(PodCapacity, 1));
			Remaining -= ThisPod;

			TArray<TSubclassOf<AMsClankerBase>> Payload;
			for (int32 Index = 0; Index < ThisPod; ++Index)
			{
				if (const TSubclassOf<AMsClankerBase> ClankerClass = PickClankerClass())
				{
					Payload.Add(ClankerClass);
				}
			}

			if (Payload.Num() == 0)
			{
				break;
			}

			FVector ImpactLocation;
			if (!FindSpawnLocation(ImpactLocation))
			{
				break;
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.Owner = this;

			if (AMsDropPod* Pod = World->SpawnActor<AMsDropPod>(
				DropPodClass, ImpactLocation, FRotator::ZeroRotator, SpawnParams))
			{
				// The pod spawns the clankers when it lands, so we count them then rather
				// than now - otherwise the encounter would think it was already fighting.
				Pod->OnClankerDelivered.AddDynamic(this, &AMsEncounterVolume::HandleClankerDelivered);
				Pod->Deliver(Payload, ImpactLocation);

				SpawnedSoFar += Payload.Num();
				PendingDeliveries += Payload.Num();
			}
		}

		return;
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (const TSubclassOf<AMsClankerBase> ClankerClass = PickClankerClass())
		{
			SpawnOne(ClankerClass);
		}
	}
}

void AMsEncounterVolume::HandleClankerDelivered(AMsClankerBase* Clanker)
{
	PendingDeliveries = FMath::Max(0, PendingDeliveries - 1);

	if (Clanker)
	{
		AliveClankers.Add(Clanker);
	}
}
