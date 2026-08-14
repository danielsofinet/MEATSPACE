#include "World/MsEncounterVolume.h"

#include "Character/MsCharacter.h"
#include "Clankers/MsClankerBase.h"
#include "Clankers/MsClankerSmall.h"
#include "Combat/MsHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/MsObjectiveSubsystem.h"

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

	// Works out of the box with just small clankers - the onboarding ambush is a mob of them.
	FMsSpawnEntry Small;
	Small.ClankerClass = AMsClankerSmall::StaticClass();
	Small.Weight = 1.0f;
	SpawnTable = { Small };
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

	const int32 Alive = AliveClankers.Num();

	if (SpawnedSoFar < TotalToSpawn)
	{
		// Reinforce on attrition rather than a timer, so the fight paces itself to how fast
		// the player is actually killing things.
		if (Alive <= ReinforceWhenAliveAtOrBelow)
		{
			SpawnRound();
		}
	}
	else if (Alive == 0)
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

	for (int32 Attempt = 0; Attempt < PlacementAttempts; ++Attempt)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
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
	const int32 Count = FMath::Min(SpawnPerRound, Remaining);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (const TSubclassOf<AMsClankerBase> ClankerClass = PickClankerClass())
		{
			SpawnOne(ClankerClass);
		}
	}
}
