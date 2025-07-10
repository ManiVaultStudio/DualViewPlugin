#include "Computation.h"

void normalizeYValues(std::vector<mv::Vector2f>& embedding)
{
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();

    for (const auto& v : embedding) {
        min_y = std::min(min_y, v.y);
        max_y = std::max(max_y, v.y);
    }

    // Normalize y-values to range (0, 1)
    for (auto& v : embedding) {
        v.y = (v.y - min_y) / (max_y - min_y);
    }
}

void projectToVerticalAxis(std::vector<mv::Vector2f>& embeddings, float x_value)
{
    for (auto& point : embeddings) {
        point.x = x_value;  // Set x-coordinate to the specified value
    }
}

void scaleDataRange(const std::vector<float>& input, std::vector<float>& output, bool reverse, float ptSize)
{
    if (input.empty())
        return;

    //set size scalars
    auto min_max = std::minmax_element(input.begin(), input.end());
    float min_val = *min_max.first;
    float max_val = *min_max.second;
    float range = max_val - min_val;
    if (range == 0.0f)
    {
        range = 1.0f;
    }

    output.clear();
    output.resize(input.size());

    // In case the user wants to reverse the point size
    if (!reverse)
    {
#pragma omp parallel for
        for (int i = 0; i < input.size(); i++)
        {
            output[i] = ((input[i] - min_val) / range) * ptSize; // TODO: hardcoded factor?
        }
    }
    else
    {
#pragma omp parallel for
        for (int i = 0; i < input.size(); i++)
        {
            output[i] = (1 - (input[i] - min_val) / range) * ptSize;
        }
    }
}

void scaleDataRangeExperiment(const std::vector<float>& input, std::vector<float>& output, bool reverse, float ptSize)
{
    if (input.empty())
        return;

    //set size scalars
    auto min_max = std::minmax_element(input.begin(), input.end());
    float min_val = *min_max.first;
    float max_val = *min_max.second;
    float range = max_val - min_val;
    if (range == 0.0f)
    {
        range = 1.0f;
    }

    output.clear();
    output.resize(input.size());

    // In case the user wants to reverse the point size
    if (!reverse)
    {
#pragma omp parallel for
        for (int i = 0; i < input.size(); i++)
        {
            //output[i] = ((input[i] - min_val) / range) * ptSize; // TODO: hardcoded factor?
            float val = std::max(0.0f, input[i]);  // Set all negative values to 0
            //output[i] = std::sqrt(val / max_val) * ptSize; //exaggerates small values while compressing very large ones
            //output[i] = std::pow(val / max_val, 2.0f) * ptSize; // compress small values
            output[i] = input[i] / max_val * ptSize;
        }
    }
    else
    {
#pragma omp parallel for
        for (int i = 0; i < input.size(); i++)
        {
            output[i] = (1 - (input[i] - min_val) / range) * ptSize;
        }
    }
}

void computeDataRange(const mv::Dataset<Points> dataset, std::vector<float>& columnMins, std::vector<float>& columnRanges)
{
    if (!dataset.isValid())
    {
        qDebug() << "Datasets B should be valid to compute data range";
        return;
    }

    // Assume gene embedding is t-SNE
    // FIXME: should check if the number of points in A is the same as the number of dimensions in B

    // define lines - assume embedding A is dimension embedding, embedding B is observation embedding
    int numDimensions = dataset->getNumDimensions(); // Assume the number of points in A is the same as the number of dimensions in B
    int numDimensionsFull = dataset->getNumDimensions();
    int numPoints = dataset->getNumPoints(); // num of points in source dataset B
    int numPointsLocal = dataset->getNumPoints(); // num of points in the embedding B

    std::vector<std::uint32_t> localGlobalIndicesB;
    dataset->getGlobalIndices(localGlobalIndicesB);

    columnMins.resize(numDimensions);
    columnRanges.resize(numDimensions);

    // Find the minimum value in each column - attention! this is the minimum of the subset (if HSNE)
#pragma omp parallel for
    for (int dimIdx = 0; dimIdx < numDimensions; dimIdx++)
    {

        float minValue = std::numeric_limits<float>::max();
        float maxValue = std::numeric_limits<float>::lowest();

        for (int i = 0; i < numPoints; i++)
        {
            float val = dataset->getValueAt(i * numDimensionsFull + dimIdx);
            if (val < minValue)
                minValue = val;
            if (val > maxValue)
                maxValue = val;
        }

        columnMins[dimIdx] = minValue;
        columnRanges[dimIdx] = maxValue - minValue;
    }

    qDebug() << "Data range computed for " << numDimensions << " dimensions, " << numPoints << " points in dataset B";

    qDebug() << "Data range computed";

}

void computeSelectedGeneMeanExpression(const mv::Dataset<Points> sourceDataset, const mv::Dataset<Points> selectedDataset, std::vector<float>& meanExpressionFull)
{   // sourceDataset should be embeddingSourceDatasetB, selectedDataset should be embeddingDatasetA

    auto geneSelection = selectedDataset->getSelection<Points>();

    if (geneSelection->indices.size() == 0)
    {
        qDebug() << "DualViewPlugin: selected 0 genes";
        return;
    }

    if (!sourceDataset.isValid())
    {
        qDebug() << "DualViewPlugin: cell embedding source dataset is not valid";
        return;
    }

    int numPointsB = sourceDataset->getNumPoints();
    int numDimensionsB = sourceDataset->getNumDimensions();
    int numSelectedGenes = geneSelection->indices.size();

    qDebug() << "DualViewPlugin: computeSelectedGeneMeanExpression: numPointsB: " << numPointsB 
             << " numDimensionsB: " << numDimensionsB 
             << " numSelectedGenes: " << numSelectedGenes;

    // Output a dataset to color the spatial map by the selected gene avg. expression - always the same size as the full dataset
    mv::Dataset<Points> fullDatasetB;
    if (sourceDataset->isDerivedData())
    {
        fullDatasetB = sourceDataset->getSourceDataset<Points>()->getFullDataset<Points>();
    }
    else
    {
        fullDatasetB = sourceDataset->getFullDataset<Points>();
    }
    int totalNumPoints = fullDatasetB->getNumPoints();
    qDebug() << "totalNumPoints" << totalNumPoints << "fullDatasetB" << fullDatasetB->getGuiName();

    meanExpressionFull.resize(totalNumPoints, 0.0f);

#pragma omp parallel for  
    for (int j = 0; j < totalNumPoints; j++)
    {
        float sum = 0.0f;
        for (int i = 0; i < numSelectedGenes; i++)
        {
            int geneIndex = geneSelection->indices[i];
            sum += fullDatasetB->getValueAt(j * numDimensionsB + geneIndex);
        }
        meanExpressionFull[j] = sum / numSelectedGenes;
    }

}

void extractSelectedGeneMeanExpression(const mv::Dataset<Points> sourceDataset, const std::vector<float>& meanExpressionFull, std::vector<float>& meanExpressionLocal)
{
    // sourceDataset should be embeddingSourceDatasetB

    int numPointsB = sourceDataset->getNumPoints();

    meanExpressionLocal.clear();
    meanExpressionLocal.resize(numPointsB, 0.0f);

    std::vector<std::uint32_t> localGlobalIndicesB;
    sourceDataset->getGlobalIndices(localGlobalIndicesB);

    for (int i = 0; i < numPointsB; i++)
    {
        meanExpressionLocal[i] = meanExpressionFull[localGlobalIndicesB[i]];
    }
}

void identifyGeneSymbolsInDataset(const mv::Dataset<Points> sourceDataset, const QStringList& geneSymbols, QList<int>& foundGeneIndices)
{
    //check which genes exist in the dataset
   //find the gene index in the current embedding B source dataset dimensions
    const std::vector<QString> allDimensionNames = sourceDataset->getDimensionNames();

    foundGeneIndices.clear();

    int numNotFoundGenes = 0;
    for (int i = 0; i < geneSymbols.size(); i++)
    {
        QString gene = geneSymbols[i];
        bool found = false;
        for (int j = 0; j < allDimensionNames.size(); j++)
        {
            if (allDimensionNames[j] == gene)
            {
                foundGeneIndices.append(j);
                found = true;
                break;
            }
        }
        if (!found)
        {
            //qDebug() << "highlightInputGenes: Gene not found: " << gene;
            numNotFoundGenes++;
        }
    }

    qDebug() << "identifyGeneSymbolsInDataset: " << geneSymbols.size() << " genes, " << numNotFoundGenes << " not found";
}

void computeSelectedCellMeanExpression(const mv::Dataset<Points> sourceDataset, std::vector<float>& meanExpressionFull)
{
    int numDimensions = sourceDataset->getNumDimensions();
    meanExpressionFull.resize(numDimensions, 0.0f);

    auto fullDataset = sourceDataset->getFullDataset<Points>();
    auto selection = fullDataset->getSelection<Points>();
    std::vector<bool> selected;
    fullDataset->selectedLocalIndices(selection->indices, selected);

    std::vector<int> selectedIndices;
    for (int i = 0; i < selected.size(); ++i)
    {
        if (selected[i]) 
            selectedIndices.push_back(i);
    }

    int numSelected = static_cast<int>(selectedIndices.size());
    qDebug() << "computeSelectedCellMeanExpression: " << numSelected << " selected cells";

    if (numSelected == 0)
    {
        return;
    }

#pragma omp parallel for
    for (int i = 0; i < numDimensions; ++i)
    {
        float sum = 0.0f;
        for (int idx : selectedIndices)
        {
            sum += fullDataset->getValueAt(idx * numDimensions + i);
        }
        meanExpressionFull[i] = sum / numSelected;
    }

}

void computeMeanExpressionForAllCells(const mv::Dataset<Points> sourceDataset, std::vector<float>& meanExpressionFull)
{
    // compute the mean expression of each gene across all cells
    int numPoints = sourceDataset->getNumPoints();
    int numDimensions = sourceDataset->getNumDimensions();
    qDebug() << "computeMeanExpressionForAllCells: numPoints: " << numPoints << " numDimensions: " << numDimensions;

    meanExpressionFull.resize(numDimensions, 0.0f);

#pragma omp parallel for
    for (int i = 0; i < numDimensions; i++)
    {
        float sum = 0.0f;
        for (int j = 0; j < numPoints; j++)
        {
            sum += sourceDataset->getValueAt(j * numDimensions + i);
        }
        meanExpressionFull[i] = sum / numPoints;
    }

}
