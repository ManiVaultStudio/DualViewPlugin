#pragma once
#include <vector>

#include "graphics/Vector2f.h"
#include <Dataset.h>
#include <PointData/PointData.h>


void normalizeYValues(std::vector<mv::Vector2f>& embedding);

void projectToVerticalAxis(std::vector<mv::Vector2f>& embeddings, float x_value);

// for plotting 
void scaleDataRange(const std::vector<float>& input, std::vector<float>& output, bool reverse, float ptSize);

void scaleDataRangeExperiment(const std::vector<float>& input, std::vector<float>& output, bool reverse, float ptSize);

// precompute the range of each column every time the dataset changes, for computing line connections
void computeDataRange(const mv::Dataset<Points> dataset, std::vector<float>& columnMins, std::vector<float>& columnRanges);

// compute the mean expression of the selected gene across all cells
void computeSelectedGeneMeanExpression(const mv::Dataset<Points> sourceDataset, const mv::Dataset<Points> selectedDataset, std::vector<float>& meanExpressionFull);

// extract the mean expression of the selected genes for the current embedding
void extractSelectedGeneMeanExpression(const mv::Dataset<Points> sourceDataset, const std::vector<float>& meanExpressionFull, std::vector<float>& meanExpressionLocal);

void identifyGeneSymbolsInDataset(const mv::Dataset<Points> sourceDataset, const QStringList& geneSymbols, QList<int>& foundGeneIndices);

// compute the mean expression of the selected cells across all genes
void computeSelectedCellMeanExpression(const mv::Dataset<Points> sourceDataset, std::vector<float>& meanExpressionFull);

// pre-compute the mean expression of each gene across all cells
void computeMeanExpressionForAllCells(const mv::Dataset<Points> sourceDataset, std::vector<float>& meanExpressionFull);


