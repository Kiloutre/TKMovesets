#pragma once

// -- Behaviour -- //

void run_gui();
bool handle_arguments();

// -- Other -- //

void ApplyUpdate(const std::string& filename);
void CleanupUpdateFiles(const std::string& filename);