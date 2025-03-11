#pragma once
#include <vector>

#include "graphics/Vector2f.h"
#include <Dataset.h>
#include <PointData/PointData.h>


void normalizeYValues(std::vector<mv::Vector2f>& embedding);

void projectToVerticalAxis(std::vector<mv::Vector2f>& embeddings, float x_value);

// precompute the range of each column every time the dataset changes, for computing line connections
void computeDataRange(const mv::Dataset<Points> dataset, std::vector<float>& columnMins, std::vector<float>& columnRanges);