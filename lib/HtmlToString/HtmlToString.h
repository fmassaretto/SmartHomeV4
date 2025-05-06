#pragma once
#ifndef HTMLTOSTRING_H_
#define HTMLTOSTRING_H_

#include "Arduino.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

String htmlFileToString(const std::string& filePath);

#endif