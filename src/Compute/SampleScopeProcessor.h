#pragma once
#include <vector>
#include <QString>

#include <ClusterData/ClusterData.h>

// count the number of cells in each metadata cluster and return the counts, labels, and colors for the chart
std::tuple<QStringList, QStringList, QStringList> computeMetadataCounts(QVector<Cluster>& metadata, std::vector<std::uint32_t>& sampledPoints);

// build html for the selected items
QString buildHtmlForSelection(const bool isASelected, const QString colorDatasetName, const QStringList& geneSymbols, QStringList& labels, QStringList& data, QStringList& backgroundColors);

// build html for the enrichment results
QString buildHtmlForEnrichmentResults(const QVariantList& data);

// build html for no enrichment results
QString buildHtmlForEmpty();