#pragma once

#include <string>
#include <thread>
#include <vector>

class TEditorMove_Animations
{
private:
	struct AnimationLibFile
	{
		std::string name;
		std::string lowercaseName;
		std::wstring filepath;
		std::string duration;
		std::string size_megabytes;
		unsigned int size;
	};

	struct AnimationLibChar
	{
		std::string name;
		std::vector<AnimationLibFile*> files;
		std::vector<AnimationLibFile*> filteredFiles;
		bool forceOpen = false;
		bool forceClose = false;
	};

	// Contains a list of <character : files>
	std::vector<AnimationLibChar*> m_characters;
	// The search buffer
	char m_searchBuffer[32] { 0 };
	std::string m_lowercaseBuffer;
	// Set to true to exit the animation loading early
	bool m_destructionRequested = false;
	// Thread that will take care of the animation loading
	std::thread m_loadingThread;
	// Amount of animations in total
	size_t m_animCount = 0;
	// Amount of animations in total, during the last render. Used to determine if the list needs to be re-computed
	size_t m_prevRenderAnimCount = 0;

	// Filters the animation list according to the search buffer
	void ApplySearchFilter();
public:
	//  Whether the window/popup is opened
	bool popen = true;
	// True if the thread we created finished loading the list
	bool loadedList = false;
	// Stores the animation filename that needs to be imported
	const wchar_t* animationToImport = nullptr;

	TEditorMove_Animations();
	~TEditorMove_Animations();
	// Read the library for every animation
	void LoadAnimationList();
	// Render the popup/window. Returns false if closed.
	bool Render();
};
