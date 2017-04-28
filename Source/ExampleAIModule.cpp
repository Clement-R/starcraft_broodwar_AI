#include "ExampleAIModule.h"
#include <iostream>
#include <vector>
#include <algorithm>

using namespace BWAPI;
using namespace Filter;

bool extractor = false;
bool firstPool = false;
bool firstDrone = false;
bool buildingConstructed = false;
bool enemyFound = false;

Position enemyBase = Position();
Position rallyPoint = Position();

Unit u_commandCenter = nullptr;

Unit u_builder = nullptr;
Unit u_ongoingBuilding = nullptr;
Unit u_firstExtractor = nullptr;
Unit u_firstPool = nullptr;

// diagonal, diagonal-top, diagonal-bot
std::vector<Unit> launchedScouts;

// All points that need to be scouted
std::vector<TilePosition> scoutingPoints;

void ExampleAIModule::onStart()
{
	// Print the map name.
	// BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
	Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

	// Enable the UserInput flag, which allows us to control the bot and type messages.
	Broodwar->enableFlag(Flag::UserInput);

	// Uncomment the following line and the bot will know about everything through the fog of war (cheat).
	//Broodwar->enableFlag(Flag::CompleteMapInformation);

	// Set the command optimization level so that common commands can be grouped
	// and reduce the bot's APM (Actions Per Minute).
	Broodwar->setCommandOptimizationLevel(2);

	// Check if this is a replay
	if (Broodwar->isReplay())
	{

		// Announce the players in the replay
		Broodwar << "The following players are in this replay:" << std::endl;

		// Iterate all the players in the game using a std:: iterator
		Playerset players = Broodwar->getPlayers();
		for (auto p : players)
		{
			// Only print the player if they are not an observer
			if (!p->isObserver())
				Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
		}

	}
	else // if this is not a replay
	{
		// Retrieve you and your enemy's races. enemy() will just return the first enemy.
		// If you wish to deal with multiple enemies then you must use enemies().
		if (Broodwar->enemy()) // First make sure there is an enemy
			Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;
	}

	// Set speed to the highest
	Broodwar->setLocalSpeed(0);

	// Search our hatchery for later use
	for (auto &u : Broodwar->self()->getUnits())
	{
		if (u->getType().isResourceDepot())
		{
			u_commandCenter = u;
		}
	}

	// Given our starting position we search all the positions that we want to scout, ordered by priority
	TilePosition start = Broodwar->self()->getStartLocation();

	for each (auto pos in Broodwar->getStartLocations())
	{	
		if (pos != start) {
			scoutingPoints.push_back(pos);
		}
	}
}

void ExampleAIModule::onEnd(bool isWinner)
{
	// Called when the game ends
	if (isWinner)
	{
		Broodwar->sendText("gg ez");
	}
	else
	{
		Broodwar->sendText("gg wp, cheater");
	}
}

void ExampleAIModule::createBuilding(Unit worker, UnitType building) {
	if (u_ongoingBuilding == nullptr && u_builder == nullptr)
	{
		if ((Broodwar->self()->minerals() >= building.mineralPrice()))
		{
			// Find location for the pool
			TilePosition buildPosition = Broodwar->getBuildLocation(building, worker->getTilePosition());

			// Build building
			worker->build(building, buildPosition);
			// TODO : Create a lock that avoid the consumption of ressources before the building is created
			u_builder = worker;
		}
	}
	else if (u_ongoingBuilding != nullptr)
	{
		// Search for the new building that's in construction
		for (auto &u : Broodwar->self()->getUnits())
		{
			if (u->getType() == building)
			{
				// Keep a reference to the building
				if (u->isBeingConstructed() && !buildingConstructed)
				{
					u_ongoingBuilding = u;
				}
				// If it's constructed we free the reference
				else if (!u->isBeingConstructed() && !buildingConstructed)
				{
					buildingConstructed = true;
					u_builder = nullptr;
				}
			}
		}
	}
}

bool ExampleAIModule::createUnit(Unit building, UnitType unit)
{
	if (building->isIdle())
	{
		// Check if we've enough resources to create the unit
		if ((Broodwar->self()->minerals() >= unit.mineralPrice()) && (Broodwar->self()->gas() >= unit.gasPrice()))
		{
			// Launch creation of the unit
			if (!building->train(unit)) {
				Error lastErr = Broodwar->getLastError();

				// If we are supply blocked
				if (lastErr == Errors::Insufficient_Supply && Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Overlord) == 0)
				{
					u_commandCenter->train(UnitTypes::Zerg_Overlord);
					return false;
				} // closure: insufficient supply
			}
			else
			{
				return true;
			}
		}
		else
		{
			return false;
		}

		return false;
	}

	return false;
}

void ExampleAIModule::checkStrategy() {
	// First we create a pool
	if (!firstPool) {
		bool constructionLaunched = false;
		for (auto &u : Broodwar->self()->getUnits())
		{
			if (u->getType() == UnitTypes::Zerg_Spawning_Pool)
			{
				constructionLaunched = true;
				if (!u->isBeingConstructed())
				{
					u_firstPool = u;
					firstPool = true;
				}
			}

			if (!constructionLaunched) {
				Unit worker = nullptr;

				for (auto &u : Broodwar->self()->getUnits())
				{
					if (u->getType().isWorker())
					{
						worker = u;
						break;
					}
				}

				createBuilding(worker, UnitTypes::Zerg_Spawning_Pool);
			}
		}
	}
	// Then we create a drone
	else if (!firstDrone) {
		// Launch creation of a drone
		if (createUnit(u_commandCenter, UnitTypes::Zerg_Drone)) {
			firstDrone = true;
		}
	}
	else
	{
		// If some workers died we replace them
		int workerCounter = 0;
		for each (auto u in Broodwar->self()->getUnits())
		{
			if (u->getType() == UnitTypes::Zerg_Drone) {
				workerCounter++;
			}
		}

		if (workerCounter < 5) {
			createUnit(u_commandCenter, UnitTypes::Zerg_Drone);
		}
		// YOLO ZERG
		else {
			createUnit(u_commandCenter, UnitTypes::Zerg_Zergling);
		}
	}
}

void ExampleAIModule::onFrame()
{
	if (Broodwar->getFrameCount() % 5) {
		checkStrategy();
	}

	// Called once every game frame

	// Display the game frame rate as text in the upper left area of the screen
	Broodwar->drawTextScreen(200, 0, "FPS: %d", Broodwar->getFPS());
	Broodwar->drawTextScreen(200, 20, "Average FPS: %f", Broodwar->getAverageFPS());

	// Return if the game is a replay or is paused
	if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
		return;

	// Prevent spamming by only running our onFrame once every number of latency frames.
	// Latency frames are the number of frames before commands are processed.
	if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
		return;

	// Iterate through all the units that we own
	for (auto &u : Broodwar->self()->getUnits())
	{
		// Ignore the unit if it no longer exists
		// Make sure to include this block when handling any Unit pointer!
		if (!u->exists())
			continue;

		// Ignore the unit if it has one of the following status ailments
		if (u->isLockedDown() || u->isMaelstrommed() || u->isStasised())
			continue;

		// Ignore the unit if it is in one of the following states
		if (u->isLoaded() || !u->isPowered() || u->isStuck())
			continue;

		// Ignore the unit if it is incomplete or busy constructing
		if (!u->isCompleted() || u->isConstructing())
			continue;

		// Finally make the unit do some stuff!

		// If the unit is a worker unit
		if (u->getType().isWorker())
		{
			/*
			if (!firstPool)
			{
				createBuilding(u, UnitTypes::Zerg_Spawning_Pool);
			}
			*/
			
			// if our worker is idle
			if (u->isIdle())
			{
				// Order workers carrying a resource to return them to the center,
				// otherwise find a mineral patch to harvest.
				if (u->isCarryingGas() || u->isCarryingMinerals())
				{
					u->returnCargo();
				}
				else if (!u->getPowerUp())  // The worker cannot harvest anything if it is carrying a powerup such as a flag
				{
					// Harvest from the nearest mineral patch or gas refinery
					if (!u->gather(u->getClosestUnit(IsMineralField || IsRefinery)))
					{
						// If the call fails, then print the last error message
						Broodwar << Broodwar->getLastError() << std::endl;
					}
				} // closure: has no powerup
			} // closure: if idle
		}
		else if (u->getType() == UnitTypes::Zerg_Overlord)
		{
		}
		else if (u->getType() == UnitTypes::Zerg_Zergling)
		{
			zerglingBehavior(u);
		}
		else if (u->getType().isResourceDepot()) // A resource depot is a Command Center, Nexus, or Hatchery
		{
		}
	} // closure: unit iterator
}

void ExampleAIModule::scout(Unit u)
{
	if (launchedScouts.size() != scoutingPoints.size()) {
		launchedScouts.push_back(u);
	}

	if (std::find(launchedScouts.begin(), launchedScouts.end(), u) != launchedScouts.end()) {
		if (!u->isMoving()) {
			if (scoutingPoints.size() > 0) {
				TilePosition pos = scoutingPoints.back();
				u->move(Position(pos.x * 32, pos.y * 32));
				scoutingPoints.pop_back();
			}
		}

		if (u->getUnitsInRadius(4096, IsEnemy && IsBuilding).size() > 0) {
			for (Unit i : u->getUnitsInRadius(4096, IsEnemy && IsBuilding)) {
				enemyBase = i->getPosition();
				enemyFound = true;
				break;
			}
		}
	}
}

void ExampleAIModule::zerglingBehavior(Unit u)
{	
	// Count how many zerglings we owned
	int zeCounter = 0;
	for each (auto u in Broodwar->self()->getUnits())
	{
		if (u->getType() == UnitTypes::Zerg_Zergling) {
			zeCounter++;
		}
	} 

	// Strike back
	if (u->isUnderAttack())
	{
		u->attack(u->getClosestUnit(IsEnemy));
	}

	// If the zergling is not moving and that we've not found the enemy we scout else if we've enough zerglings we go to the enemy base
	// Else the zergling is moving (scouting) and we've found the enemy, we rush him if we've enough zerglings
	if (!u->isMoving())
	{
		if (enemyFound) {
			if (zeCounter > 12) {
				u->move(enemyBase);
			}
		}
		else {
			if (!u->isAttacking()) {
				scout(u);
			}
		}	
	}
	else
	{
		if (u->getUnitsInRadius(128, IsEnemy).size() > 0) {
			for (Unit i : u->getUnitsInRadius(128, IsEnemy)) {
				u->attack(i);
				break;
			}
		}

		if (enemyFound) {
			if (zeCounter > 12) {
				u->move(enemyBase);
			}
		}
	}

	// Attack workers
	for (auto &a : u->getUnitsInRadius(512, IsEnemy && IsWorker))
	{
		if (u->canAttackUnit(a))
		{
			u->attack(a);
		}
	}

	// Attack units
	if (!u->isAttacking()) {
		for (auto &a : u->getUnitsInRadius(512, IsEnemy && !IsBuilding))
		{
			if (u->canAttackUnit(a))
			{
				u->attack(a);
			}
		}
	}

	// Attack buildings
	if (!u->isAttacking()) {
		for (auto &a : u->getUnitsInRadius(512, IsEnemy && IsBuilding))
		{
			if (u->canAttackUnit(a))
			{
				u->attack(a);
			}
		}
	}
}

void ExampleAIModule::onSendText(std::string text)
{

	// Send the text to the game if it is not being processed.
	Broodwar->sendText("%s", text.c_str());


	// Make sure to use %s and pass the text as a parameter,
	// otherwise you may run into problems when you use the %(percent) character!

}

void ExampleAIModule::onReceiveText(BWAPI::Player player, std::string text)
{
	// Parse the received text
	Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void ExampleAIModule::onPlayerLeft(BWAPI::Player player)
{
	// Interact verbally with the other players in the game by
	// announcing that the other player has left.
	Broodwar->sendText("Goodbye %s!", player->getName().c_str());
}

void ExampleAIModule::onNukeDetect(BWAPI::Position target)
{

	// Check if the target is a valid position
	if (target)
	{
		// if so, print the location of the nuclear strike target
		Broodwar << "Nuclear Launch Detected at " << target << std::endl;
	}
	else
	{
		// Otherwise, ask other players where the nuke is!
		Broodwar->sendText("Where's the nuke?");
	}

	// You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void ExampleAIModule::onUnitDiscover(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitEvade(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitShow(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitHide(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitCreate(BWAPI::Unit unit)
{
	if (Broodwar->isReplay())
	{
		// if we are in a replay, then we will print out the build order of the structures
		if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
		}
	}
}

void ExampleAIModule::onUnitDestroy(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitMorph(BWAPI::Unit unit)
{
	if (Broodwar->isReplay())
	{
		// if we are in a replay, then we will print out the build order of the structures
		if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
		}
	}
}

void ExampleAIModule::onUnitRenegade(BWAPI::Unit unit)
{
}

void ExampleAIModule::onSaveGame(std::string gameName)
{
	Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void ExampleAIModule::onUnitComplete(BWAPI::Unit unit)
{
}
