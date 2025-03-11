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
    //qDebug() << "totalNumPoints" << totalNumPoints << "fullDatasetB" << fullDatasetB->getGuiName();

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