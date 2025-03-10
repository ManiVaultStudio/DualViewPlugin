#include "SampleScopeProcessor.h"



std::tuple<QStringList, QStringList, QStringList> computeMetadataCounts(QVector<Cluster>& metadata, std::vector<std::uint32_t>& sampledPoints)
{
    QStringList labels;
    QStringList data;
    QStringList backgroundColors;

    std::unordered_map<int, int> cellToClusterMap; // maps global cell index to cluster index
    for (int clusterIndex = 0; clusterIndex < metadata.size(); ++clusterIndex) {
        const auto& ptIndices = metadata[clusterIndex].getIndices();
        for (int cellIndex : ptIndices) {
            cellToClusterMap[cellIndex] = clusterIndex;
        }
    }

    std::unordered_map<int, int> clusterCount; // maps cluster index to count
    for (int i = 0; i < sampledPoints.size(); ++i) {
        int globalCellIndex = sampledPoints[i];
        if (cellToClusterMap.find(globalCellIndex) != cellToClusterMap.end()) {
            int clusterIndex = cellToClusterMap[globalCellIndex];
            clusterCount[clusterIndex]++;
        }
    }

    std::vector<std::pair<int, int>> sortedClusterCount(clusterCount.begin(), clusterCount.end());
    std::sort(sortedClusterCount.begin(), sortedClusterCount.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [clusterIndex, count] : sortedClusterCount) 
    {
        QString clusterName = metadata[clusterIndex].getName();
        labels << clusterName;
        data << QString::number(count);

        QString color = metadata[clusterIndex].getColor().name();
        backgroundColors << color;
    }

    return { labels, data, backgroundColors };
}