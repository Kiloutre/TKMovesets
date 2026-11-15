#include "helpers.hpp"

#include "constants.h"
#include "GameInteraction.hpp"
#include "AttachRetryBackoff.hpp"

// -- Private methods -- //


// -- Public methods -- //

void GameInteraction::SetTargetProcess(const GameInfo* gameInfo)
{
	auto state = LockState();
	if (!gameInfo || IsBusy()) {
		return;
	}

    // The worker applies target changes between operations, never during I/O.
    m_canStart = false;
    m_pendingGame = gameInfo;
    m_resetRequested = false;
}

void GameInteraction::Update()
{
	// Executed in its own thread, is the one in charge of extraction, that way it won't interrupt the GUI rendering
    AttachRetryBackoff retry;
	while (m_threadStarted)
	{
		{
			auto state = LockState();
            if (m_resetRequested || m_pendingGame) {
                auto requested = m_pendingGame;
                m_pendingGame = nullptr;
                m_resetRequested = false;
                m_canStart = false;
                retry.Reset();
                if (process.IsAttached()) {
                    PreProcessDetach(); process.Detach(); OnProcessDetach();
                    m_lastStatus = process.status;
                }
                currentGame = requested;
                currentGameProcess = requested ? requested->processName : "";
            }
			// Ensure the process is still open and valid before possibly extracting
			if (process.IsAttached() && process.CheckRunning()) {
				process.FreeOldGameMemory();
				if (CanStart(false)) {
					actionStartDate = Helpers::getCurrentTimestamp();
					m_canStart = true;
                    state.unlock();
                    RunningUpdate();
                    state.lock();
				}
				else {
					m_canStart = false;
				}
			}
            else if (currentGame != nullptr && retry.Ready(std::chrono::steady_clock::now())) {
				// Process closed, try to attach again in case it is restarted
				if (process.Attach(currentGameProcess.c_str(), m_processExtraFlags)) {
					DEBUG_LOG("Process is now attached - OnProcessAttach()\n");
                    OnProcessAttach();
                    retry.Reset();
                } else {
                    retry.Failed(std::chrono::steady_clock::now());
                }
            }

			if (m_lastStatus != process.status) {
				if (m_lastStatus == GameProcessErrcode_PROC_ATTACHED) {
					DEBUG_LOG("Process is not attached anymore - OnProcessDetach()\n");
					OnProcessDetach();
				}
				m_lastStatus = process.status;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(GAME_INTERACTION_THREAD_SLEEP_MS));
	}
}

void GameInteraction::Init(GameAddressesFile* addrFile, LocalStorage* t_storage)
{
	// todo: Clean this up, we don't need to instantiate multiple GameData classes.
    // or maybe we do for data strings?
	game.SetProcess(&process, addrFile);
	storage = t_storage;
}

bool GameInteraction::IsAttached() const
{
	auto state = LockState();
	return process.IsAttached();
}

void GameInteraction::StopThreadAndCleanup()
{
	// Order thread to stop
	ThreadedClass::StopThreadAndCleanup();
}

uint8_t GameInteraction::GetCharacterCount() const
{
	auto state = LockState();
	if (currentGame != nullptr) {
		return currentGame->characterCount;
	}
	return 2;
}
