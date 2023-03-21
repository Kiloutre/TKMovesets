#pragma once

#include <string>
#include <vector>

struct AnimationLibFile
{
	std::string name;
	std::string lowercaseName;
	std::string filepath;
	std::string duration;
	std::string size_megabytes;
};

struct AnimationLibChar
{
	std::string name;
	std::vector<AnimationLibFile*> files;
	std::vector<AnimationLibFile*> filteredFiles;
	bool forceOpen = false;
};


class EditorMove_Animations
{
private:
	// Contains a list of <character : files>
	std::vector<AnimationLibChar*> m_characters;
	// The search buffer
	char m_searchBuffer[32] { 0 };
	std::string m_lowercaseBuffer;

	// Filters the animation list according to the search buffer
	void ApplySearchFilter();
public:
	//  Whether the window/popup is opened
	bool popen = true;

	EditorMove_Animations();
	// Read the library for every animation
	void LoadAnimationList();
	// Render the popup/window. Returns false if closed.
	bool Render();
};
