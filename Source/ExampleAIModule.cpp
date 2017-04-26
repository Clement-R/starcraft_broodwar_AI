#include "ExampleAIModule.h"
#include <iostream>

using namespace BWAPI;
using namespace Filter;

bool extractor = false;
bool firstPool = false;
bool firstDrone = false;
bool buildingConstructed = false;

Unit u_commandCenter = nullptr;

Unit u_builder = nullptr;
Unit u_ongoingBuilding = nullptr;
Unit u_firstExtractor = nullptr;
Unit u_firstPool = nullptr;

// diagonal, diagonal-top, diagonal-bot
bool launchedScouts[] = {false, false, false};

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

	// Search our hatchery to later use
	for (auto &u : Broodwar->self()->getUnits())
	{
		if (u->getType().isResourceDepot())
		{
			u_commandCenter = u;
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

				Broodwar << lastErr << std::endl;

				// If we are supply blocked
				if (lastErr == Errors::Insufficient_Supply && Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Overlord) == 0)
				{
					Broodwar << "Go go supply" << std::endl;
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
	// YOLO zerg
	else
	{
		createUnit(u_commandCenter, UnitTypes::Zerg_Zergling);
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
			if (u->getUnitsInRadius(40, IsEnemy).size() > 0) {
				Broodwar << "WE'VE FOUND THEM !" << std::endl;
			}

			if (!u->isMoving())
			{
				scout(u);
			}
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

// 0 - bottom left
// 1 - bottom right
// 2 - top right
// 3 - top left
void ExampleAIModule::scoutMove(int direction, Unit u) {
	switch (direction)
	{
		case 0:
			// Go bottom left
			u->move(Position(0, Broodwar->mapHeight() * 32));
			break;
		case 1:
			// Go bottom right
			u->move(Position(Broodwar->mapWidth() * 32, Broodwar->mapHeight() * 32));
			break;
		case 2:
			// Go top right
			u->move(Position(Broodwar->mapWidth() * 32, 0));
			break;
		case 3:
			// Go top left
			u->move(Position(0, 0));
			break;
	}
}

void ExampleAIModule::scout(Unit u)
{
	TilePosition start = Broodwar->self()->getStartLocation();
	
	if (!launchedScouts[0])
	{
		// Detect in which corner of the map we are
		if (start.x < Broodwar->mapWidth() / 2)
		{
			// Go in diagonal
			// Top left
			if (start.y < Broodwar->mapHeight() / 2)
			{
				Broodwar << "Go bottom right" << std::endl;
				scoutMove(1, u);
			}
			// Bottom left
			else
			{
				Broodwar << "Go top right" << std::endl;
				scoutMove(2, u);
			}
		}
		else
		{
			// Go in diagonal
			// Top right
			if (start.y < Broodwar->mapHeight() / 2)
			{
				Broodwar << "Go bottom left" << std::endl;
				scoutMove(0, u);
			}
			// Bottom right
			else
			{
				Broodwar << "Go top left" << std::endl;
				scoutMove(3, u);
			}
		}

		launchedScouts[0] = true;
	}
	else if (launchedScouts[0] && !launchedScouts[1])
	{
		Broodwar << "Second scout !" << std::endl;
		// Detect in which corner of the map we are
		// Left side of the map
		if (start.x < Broodwar->mapWidth() / 2)
		{
			// Go in diagonal
			// Top left
			if (start.y < Broodwar->mapHeight() / 2)
			{
				// Go bottom left
				scoutMove(0, u);
			}
			// Bottom left
			else
			{
				// Go bottom right
				scoutMove(1, u);
			}
		}
		// Right side of the map
		else
		{
			// Go in diagonal
			// Top right
			if (start.y < Broodwar->mapHeight() / 2)
			{
				Broodwar << "Go bottom left" << std::endl;
				scoutMove(3, u);
			}
			// Bottom right
			else
			{
				Broodwar << "Go top left" << std::endl;
				scoutMove(2, u);
			}
		}

		launchedScouts[1] = true;
	} 
	else if (launchedScouts[1] && !launchedScouts[2])
	{
		Broodwar << "Third scout !" << std::endl;
		// Detect in which corner of the map we are
		if (start.x < Broodwar->mapWidth() / 2)
		{
			// Go in diagonal
			// Top left
			if (start.y < Broodwar->mapHeight() / 2)
			{
				// Top right
				scoutMove(2, u);
			}
			// Bottom left
			else
			{
				// Top left
				scoutMove(3, u);
			}
		}
		else
		{
			// Go in diagonal
			// Top right
			if (start.y < Broodwar->mapHeight() / 2)
			{
				// Bottom right
				scoutMove(1, u);
			}
			// Bottom right
			else
			{
				// Bottom left
				scoutMove(0, u);
			}
		}

		launchedScouts[2] = true;
	}
	
	// Go on mid-x  0-y
	// Go on full-x mid-y
}

void ExampleAIModule::zerglingBehavior(Unit u)
{	
	// Attack if not scouting
	// TODO : Scout in a better way with a class and a flag
	if (!u->isMoving())
	{
		for (auto &a : u->getUnitsInRadius(20, IsWorker && !IsOwned))
		{
			// Attack da worker
			if (u->canAttackUnit(a))
			{
				Broodwar << Broodwar->getLastError() << std::endl;
			}
		}

		if (!u->isAttacking()) {
			scout(u);
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
