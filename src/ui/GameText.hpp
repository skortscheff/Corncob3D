// GameText.hpp - The texts of the original that the menus show: MOAG.INF (the information about each menu)
// and CORNCOB.DOC (the documentation, in pages)

#pragma once
#include <string>
#include <vector>

namespace corncob {

enum class Info { Main, DutyRoster, Combat, Keyboard, Training, HighScores, DeleteList, ReadThis };

// The lines of an information page of MOAG.INF (with the original's backslash marks)
std::vector<std::string> infoText(Info which);

// CORNCOB.DOC split into its pages; page 0 is the table of contents and the others follow its chapter
// numbers 1-17
std::vector<std::vector<std::string>> docPages();

// The lines of a text file, without line ends or control characters
std::vector<std::string> readTextLines(const std::string& path);

} // namespace corncob
