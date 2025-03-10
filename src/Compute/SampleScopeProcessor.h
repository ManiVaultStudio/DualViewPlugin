#pragma once
#include <vector>
#include <QString>

#include <ClusterData/ClusterData.h>

std::tuple<QStringList, QStringList, QStringList> computeMetadataCounts(QVector<Cluster>& metadata, std::vector<std::uint32_t>& sampledPoints);
